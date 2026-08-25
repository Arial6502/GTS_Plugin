#include "Debug/Profiler/ProfilerCore.hpp"

#ifdef GTS_PROFILER_ENABLED

namespace {

    using namespace GTS;

    struct SiteInfo {
        std::string Name;
        SiteKind Kind = SiteKind::Scope;
    };

    // Everything with a non trivial destructor lives here and is deliberately leaked. A
    // thread's cleanup guard runs during process shutdown and takes ThreadMutex, so any of
    // this being destroyed first would be a use after free on the way out.
    struct Registry {

        std::mutex SiteMutex;

        // A deque never invalidates references to existing elements when it grows, so a
        // reader can index anything below g_siteCount without a lock and without the
        // slot ceiling having to be reserved up front.
        std::deque<SiteInfo> Sites;

        //Name to slot, so registration does not linear scan. Views point into Sites.
        std::unordered_map<std::string_view, std::uint32_t> SiteIndex;

        std::mutex ThreadMutex;
        ProfilerThreadState* FreeList = nullptr;

        std::string UnknownSite = "<unregistered>";
    };

    Registry& Reg() {
        static Registry* instance = new Registry();
        return *instance;
    }

    // Trivially destructible and constant initialised, so readers can touch these at any
    // point in the process lifetime without going through Reg().
    std::atomic<std::uint32_t> g_siteCount{ 0 };
    std::atomic<ProfilerThreadState*> g_threadHead{ nullptr };

    [[nodiscard]] std::string_view StripQualification(std::string_view a_raw) {

        if (const std::size_t qualifier = a_raw.rfind("::"); qualifier != std::string_view::npos) {
            return a_raw.substr(qualifier + 2);
        }

        if (const std::size_t keyword = a_raw.rfind(' '); keyword != std::string_view::npos) {
            return a_raw.substr(keyword + 1);
        }

        return a_raw;
    }

    // Reuses an existing slot when the name matches, so two call sites sharing a name
    // aggregate the way they did before. Runs under Reg().SiteMutex.
    std::uint32_t FindOrAppendSite(std::string a_name, SiteKind a_kind) {

        Registry& registry = Reg();

        if (const auto it = registry.SiteIndex.find(std::string_view(a_name)); it != registry.SiteIndex.end()) {
            return it->second;
        }

        const std::uint32_t known = g_siteCount.load(std::memory_order_relaxed);

        if (known >= kMaxProfilerSlots) {
            logger::warn("Profiler slot limit of {} reached, dropping site: {}. Raise GTS::kMaxProfilerSlots.", kMaxProfilerSlots, a_name);
            return kInvalidSlot;
        }

        const SiteInfo& added = registry.Sites.emplace_back(std::move(a_name), a_kind);

        //Safe to key on a view: the deque owns the string and never moves or mutates it.
        registry.SiteIndex.emplace(std::string_view(added.Name), known);

        g_siteCount.store(known + 1u, std::memory_order_release);
        return known;
    }

    // Cleanup hook. Non trivial, so it is only ever touched on the cold registration path -
    // keeping the hot t_State load free of an on-demand-init guard.
    struct ThreadStateGuard {
        ~ThreadStateGuard() {
            if (ProfilerThreadState* state = ProfilerDetail::t_State) {
                ProfilerDetail::t_State = nullptr;
                ProfilerRegistry::ReleaseThreadState(state);
            }
        }
    };

    thread_local ThreadStateGuard t_guard;
}

namespace GTS {

    namespace ProfilerDetail {

        namespace {
            std::int64_t g_anchorTsc = 0;
            std::chrono::steady_clock::time_point g_anchorTime{};
        }

        bool HasInvariantTsc() noexcept {

            std::array<int, 4> regs{};

            __cpuid(regs.data(), 0x80000000);

            if (static_cast<unsigned int>(regs[0]) < 0x80000007u) {
                return false;
            }

            __cpuid(regs.data(), 0x80000007);
            return (regs[3] & (1 << 8)) != 0;
        }

        // Short blocking measurement, only ever run when the profiler is switched on, so the
        // very first sample window already has a usable tick rate instead of reading zero.
        void SeedCalibration() {

            constexpr auto kSpan = std::chrono::milliseconds(2);

            const auto startTime = std::chrono::steady_clock::now();
            const std::int64_t startTsc = Now();

            std::chrono::steady_clock::time_point endTime;

            do {
                endTime = std::chrono::steady_clock::now();
            } while (endTime - startTime < kSpan);

            const std::int64_t endTsc = Now();
            const double seconds = std::chrono::duration<double>(endTime - startTime).count();
            const std::int64_t ticks = endTsc - startTsc;

            if (ticks > 0 && seconds > 0.0) {
                g_secondsPerTick.store(seconds / static_cast<double>(ticks), std::memory_order_relaxed);
            }

            g_anchorTsc = endTsc;
            g_anchorTime = endTime;
        }

        // Refines the seed against an ever growing baseline. After a few seconds this is far
        // more accurate than any short measurement could be.
        void RefreshCalibration() {

            if (g_anchorTsc == 0) {
                SeedCalibration();
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            const std::int64_t tsc = Now();

            const double seconds = std::chrono::duration<double>(now - g_anchorTime).count();
            const std::int64_t ticks = tsc - g_anchorTsc;

            if (ticks > 0 && seconds > 0.0) {
                g_secondsPerTick.store(seconds / static_cast<double>(ticks), std::memory_order_relaxed);
            }
        }
    }

    void ProfilerThreadState::Adopt() {

        OsThreadId = static_cast<std::uint32_t>(::GetCurrentThreadId());

        const auto written = std::format_to_n(Name.data(), Name.size() - 1, "Thread-{}", OsThreadId);
        *written.out = '\0';

        ClearCounters();
        TotalTicks.store(0, std::memory_order_relaxed);
        TotalBegin = 0;
        EntrypointDepth = 0;
        InUse.store(true, std::memory_order_release);
    }

    // Slot indices are handed out in order and the count only grows, so anything at or above
    // the current count has never been written and is still zero from construction. Touching
    // the whole array instead would mean walking half a megabyte per thread.
    void ProfilerThreadState::ClearCounters() {

        const std::uint32_t live = ProfilerDetail::LiveSlotCount();

        for (std::uint32_t i = 0; i < live; ++i) {
            Counters[i].Ticks.store(0, std::memory_order_relaxed);
            Counters[i].Calls.store(0, std::memory_order_relaxed);
        }
    }

    //-------------
    // Call sites
    //-------------

    std::uint32_t ProfilerSite::Register() noexcept {

        const std::uint32_t index = ProfilerRegistry::Register(m_name, m_entrypoint ? SiteKind::Entrypoint : SiteKind::Scope, m_id);

        if (index == kInvalidSlot) {
            m_slot.store(kInvalidSlot, std::memory_order_relaxed);
            return kInvalidSlot;
        }

        m_slot.store(index + 1u, std::memory_order_relaxed);
        return m_entrypoint ? (index | kEntrypointBit) : index;
    }

    std::uint32_t ListenerSite::Resolve(std::uint32_t a_listenerIndex, const std::type_info& a_type) noexcept {

        if (a_listenerIndex >= kMaxListenerSlots) {
            return kInvalidSlot;
        }

        const std::uint32_t index = ProfilerRegistry::RegisterListener(m_function, a_type);

        if (index == kInvalidSlot) {
            m_slots[a_listenerIndex].store(kInvalidSlot, std::memory_order_relaxed);
            return kInvalidSlot;
        }

        m_slots[a_listenerIndex].store(index + 1u, std::memory_order_relaxed);
        return index;
    }

    //-----------
    // Registry
    //-----------

    std::uint32_t ProfilerRegistry::Register(std::string_view a_name, SiteKind a_kind, std::int32_t a_id) {
        std::lock_guard lock(Reg().SiteMutex);
        return FindOrAppendSite(a_id >= 0 ? std::format("{}<{}>", a_name, a_id) : std::string(a_name), a_kind);
    }

    std::uint32_t ProfilerRegistry::RegisterListener(std::string_view a_function, const std::type_info& a_type) {
        std::lock_guard lock(Reg().SiteMutex);
        return FindOrAppendSite(std::format("::{}::{}", StripQualification(a_type.name()), a_function), SiteKind::Event);
    }

    std::uint32_t ProfilerRegistry::SlotCount() {
        return g_siteCount.load(std::memory_order_acquire);
    }

    const std::string& ProfilerRegistry::SiteName(std::uint32_t a_index) {
        return a_index < g_siteCount.load(std::memory_order_acquire) ? Reg().Sites[a_index].Name : Reg().UnknownSite;
    }

    SiteKind ProfilerRegistry::SiteKindOf(std::uint32_t a_index) {
        return a_index < g_siteCount.load(std::memory_order_acquire) ? Reg().Sites[a_index].Kind : SiteKind::Scope;
    }

    //----------------
    // Thread states
    //----------------

    ProfilerThreadState* ProfilerRegistry::ThreadListHead() {
        return g_threadHead.load(std::memory_order_acquire);
    }

    void ProfilerRegistry::ReleaseThreadState(ProfilerThreadState* a_state) {

        if (!a_state) return;

        Registry& registry = Reg();
        std::lock_guard lock(registry.ThreadMutex);

        // Stays linked into the main list forever - only the reuse queue changes. Nothing is
        // ever freed, so a reader walking the list can never see a dangling node.
        a_state->InUse.store(false, std::memory_order_release);
        a_state->FreeNext = registry.FreeList;
        registry.FreeList = a_state;
    }

    void ProfilerRegistry::ResetAll() {

        for (ProfilerThreadState* state = ThreadListHead(); state; state = state->Next) {
            state->ClearCounters();
            state->TotalTicks.store(0, std::memory_order_relaxed);
        }
    }

    namespace ProfilerDetail {

        std::uint32_t LiveSlotCount() noexcept {
            return g_siteCount.load(std::memory_order_acquire);
        }

        ProfilerThreadState* AcquireThreadState() {

            //Forces this thread's cleanup guard to be constructed so the state is recycled on exit.
            (void)&t_guard;

            ProfilerThreadState* state = nullptr;

            {
                Registry& registry = Reg();
                std::lock_guard lock(registry.ThreadMutex);

                if (registry.FreeList) {
                    state = registry.FreeList;
                    registry.FreeList = state->FreeNext;
                    state->FreeNext = nullptr;
                    state->Adopt();
                }
                else {
                    state = new (std::nothrow) ProfilerThreadState();

                    if (!state) {
                        return nullptr;
                    }

                    state->Adopt();
                    state->Next = g_threadHead.load(std::memory_order_relaxed);
                    g_threadHead.store(state, std::memory_order_release);
                }
            }

            t_State = state;
            return state;
        }
    }
}

#endif //GTS_PROFILER_ENABLED
