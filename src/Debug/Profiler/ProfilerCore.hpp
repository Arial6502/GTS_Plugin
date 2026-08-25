#pragma once
#include <intrin.h>

#ifdef GTS_PROFILER_ENABLED

namespace GTS {

    // Slots and thread states are allocated once and never freed while the process lives.
    // That is what lets the UI thread read any counter at any time without a lock and
    // without a lifetime check - the old design's crashes all came from the reader and the
    // writers sharing a mutable keyed container.
    inline constexpr std::uint32_t kMaxProfilerSlots = 32768;
    inline constexpr std::uint32_t kMaxListenerSlots = 256;  //Must match EventDispatcher::MaxListeners.
    inline constexpr std::uint32_t kInvalidSlot = 0xFFFFFFFFu;
    inline constexpr std::uint32_t kEntrypointBit = 0x80000000u;

    // Which table a site belongs to. Entrypoint additionally drives the per thread total, which
    // is why it also has a bit in the slot encoding.
    enum class SiteKind : std::uint8_t {
        Entrypoint,
        Event,
        Scope,
        Task,
    };

    inline constexpr std::size_t kSiteKindCount = 4;

    struct alignas(16) ProfilerCounter {
        std::atomic<std::int64_t> Ticks{ 0 };
        std::atomic<std::uint32_t> Calls{ 0 };
        std::uint32_t Padding = 0;
    };

    struct ProfilerThreadState {

        ProfilerThreadState* Next = nullptr;      //Append only main list, written under the thread mutex.
        ProfilerThreadState* FreeNext = nullptr;  //Reuse queue, written under the thread mutex.
        std::atomic<bool> InUse{ true };
        std::uint32_t OsThreadId = 0;
        std::array<char, 32> Name{};

        std::atomic<std::int64_t> TotalTicks{ 0 };

        //Owner only, never read by anyone else.
        std::int64_t TotalBegin = 0;
        std::uint32_t EntrypointDepth = 0;

        std::array<ProfilerCounter, kMaxProfilerSlots> Counters{};

        void Adopt();
        void ClearCounters();
    };

    namespace ProfilerDetail {

        inline std::atomic<bool> g_Collecting{ false };

        // Trivially destructible and zero initialised, so reading it is a plain TLS load with
        // no on-demand-init guard. Cleanup rides on a separate guard touched only when a
        // thread first registers.
        inline thread_local ProfilerThreadState* t_State = nullptr;

        ProfilerThreadState* AcquireThreadState();

        //Number of slots handed out so far. Only these can have been written by anyone.
        [[nodiscard]] std::uint32_t LiveSlotCount() noexcept;

        // Seconds per TSC tick. Seeded against QPC when collection is switched on and
        // refined on every UI sample, where the baseline is long enough to be exact.
        inline std::atomic<double> g_secondsPerTick{ 0.0 };

        //True when the CPU reports an invariant TSC; false means timings drift with P states.
        [[nodiscard]] bool HasInvariantTsc() noexcept;

        void SeedCalibration();
        void RefreshCalibration();

        // Roughly 20-25 cycles, against roughly 20ns for steady_clock::now(), which reaches
        // KUSER_SHARED_DATA and does the scaling itself. At two reads per scope the clock was
        // the bulk of what a profiled scope cost.
        [[nodiscard]] inline std::int64_t Now() noexcept {
            return static_cast<std::int64_t>(__rdtsc());
        }

        [[nodiscard]] inline double TicksToSeconds(std::int64_t a_ticks) noexcept {
            return static_cast<double>(a_ticks) * g_secondsPerTick.load(std::memory_order_relaxed);
        }
    }

    //-------------------------------------------------------------------------------------
    //  Call site handles
    //-------------------------------------------------------------------------------------

    // One per call site. The ctor is constexpr and the dtor trivial, so a function local
    // static of this type is constant initialised: no magic-static guard, no atexit entry.
    class ProfilerSite {

        public:

        constexpr explicit ProfilerSite(std::string_view a_name, bool a_entrypoint, std::int32_t a_id = -1) noexcept
            : m_name(a_name), m_id(a_id), m_entrypoint(a_entrypoint) {}

        ProfilerSite(const ProfilerSite&) = delete;
        ProfilerSite& operator=(const ProfilerSite&) = delete;

        [[nodiscard]] std::uint32_t Slot() noexcept {

            const std::uint32_t cached = m_slot.load(std::memory_order_relaxed);

            if (cached == 0u) [[unlikely]] {
                return Register();
            }

            if (cached == kInvalidSlot) [[unlikely]] {
                return kInvalidSlot;   //Registration failed once; do not keep retrying under the mutex.
            }

            return m_entrypoint ? ((cached - 1u) | kEntrypointBit) : (cached - 1u);
        }

        private:

        std::uint32_t Register() noexcept;

        std::string_view m_name;
        std::int32_t m_id = -1;
        bool m_entrypoint = false;
        std::atomic<std::uint32_t> m_slot{ 0 };  //Stored biased by one so zero means unregistered.
    };

    // One per listener dispatch site. The listener type is only known at runtime, so the
    // slot is looked up in a flat array indexed by the listener's registration index -
    // a single load on the hot path instead of hashing a name into a map.
    class ListenerSite {

        public:

        constexpr explicit ListenerSite(std::string_view a_function) noexcept : m_function(a_function) {}

        ListenerSite(const ListenerSite&) = delete;
        ListenerSite& operator=(const ListenerSite&) = delete;

        // Returns kInvalidSlot both when this pair has not been resolved yet and when
        // resolving it failed. Resolve() is idempotent and re-poisons on failure, so the
        // difference only costs a mutex acquire in the exhausted case.
        [[nodiscard]] std::uint32_t Cached(std::uint32_t a_listenerIndex) const noexcept {

            if (a_listenerIndex >= kMaxListenerSlots) {
                return kInvalidSlot;
            }

            const std::uint32_t cached = m_slots[a_listenerIndex].load(std::memory_order_relaxed);
            return (cached != 0u && cached != kInvalidSlot) ? cached - 1u : kInvalidSlot;
        }

        std::uint32_t Resolve(std::uint32_t a_listenerIndex, const std::type_info& a_type) noexcept;

        private:

        std::string_view m_function;
        std::array<std::atomic<std::uint32_t>, kMaxListenerSlots> m_slots{};
    };

    //-------------------------------------------------------------------------------------
    //  Scope timer
    //-------------------------------------------------------------------------------------

    class ScopeTimer {

        public:

        // Taking the site rather than a slot keeps the disabled path down to one load and one
        // predicted branch - the slot is not even resolved unless collection is running.
        explicit ScopeTimer(ProfilerSite& a_site) noexcept {

            if (!ProfilerDetail::g_Collecting.load(std::memory_order_relaxed)) {
                return;
            }

            Begin(a_site.Slot());
        }

        explicit ScopeTimer(std::uint32_t a_slot) noexcept {

            if (a_slot == kInvalidSlot || !ProfilerDetail::g_Collecting.load(std::memory_order_relaxed)) {
                return;
            }

            Begin(a_slot);
        }

        ~ScopeTimer() noexcept {

            if (!m_state) {
                return;
            }

            const std::int64_t end = ProfilerDetail::Now();

            // A thread migrating between cores whose TSCs are not in step can read backwards.
            // Compiles to a cmov, and without it one bad read would poison a slot's total for
            // the rest of the session.
            const std::int64_t elapsed = end > m_begin ? end - m_begin : 0;

            ProfilerCounter& counter = m_state->Counters[m_index];

            // Single writer by construction, so a relaxed load/store pair is enough and
            // compiles to plain moves. fetch_add would emit a lock prefix for nothing.
            counter.Ticks.store(counter.Ticks.load(std::memory_order_relaxed) + elapsed, std::memory_order_relaxed);
            counter.Calls.store(counter.Calls.load(std::memory_order_relaxed) + 1u, std::memory_order_relaxed);

            if (m_entrypoint && --m_state->EntrypointDepth == 0u) {
                const std::int64_t total = end > m_state->TotalBegin ? end - m_state->TotalBegin : 0;
                std::atomic<std::int64_t>& sink = m_state->TotalTicks;
                sink.store(sink.load(std::memory_order_relaxed) + total, std::memory_order_relaxed);
            }
        }

        ScopeTimer(const ScopeTimer&) = delete;
        ScopeTimer& operator=(const ScopeTimer&) = delete;

        private:

        void Begin(std::uint32_t a_slot) noexcept {

            ProfilerThreadState* state = ProfilerDetail::t_State;

            if (!state) [[unlikely]] {
                state = ProfilerDetail::AcquireThreadState();
                if (!state) return;
            }

            m_state = state;
            m_index = a_slot & ~kEntrypointBit;
            m_entrypoint = (a_slot & kEntrypointBit) != 0u;
            m_begin = ProfilerDetail::Now();

            if (m_entrypoint && state->EntrypointDepth++ == 0u) {
                state->TotalBegin = m_begin;
            }
        }

        ProfilerThreadState* m_state = nullptr;
        std::int64_t m_begin = 0;
        std::uint32_t m_index = 0;
        bool m_entrypoint = false;
    };

    //-------------------------------------------------------------------------------------
    //  Registry
    //-------------------------------------------------------------------------------------

    class ProfilerRegistry {

        public:

        static std::uint32_t Register(std::string_view a_name, SiteKind a_kind, std::int32_t a_id);
        static std::uint32_t RegisterListener(std::string_view a_function, const std::type_info& a_type);

        [[nodiscard]] static std::uint32_t SlotCount();
        [[nodiscard]] static const std::string& SiteName(std::uint32_t a_index);
        [[nodiscard]] static SiteKind SiteKindOf(std::uint32_t a_index);

        [[nodiscard]] static ProfilerThreadState* ThreadListHead();
        static void ReleaseThreadState(ProfilerThreadState* a_state);
        static void ResetAll();
    };
}

#endif //GTS_PROFILER_ENABLED
