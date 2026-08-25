#pragma once
#include "Debug/Profiler/ProfilerCore.hpp"
#include "Utils/Misc/Singleton.hpp"

#ifdef GTS_PROFILER_ENABLED


#define GTS_PROFILE_SCOPE(a_name)                                   \
    static GTS::ProfilerSite _gts_profile_site{ a_name, false };    \
    GTS::ScopeTimer _gts_profile_handle{ _gts_profile_site }

#define GTS_PROFILE_ENTRYPOINT(a_name)                              \
    static GTS::ProfilerSite _gts_profile_site{ a_name, true };     \
    GTS::ScopeTimer _gts_profile_handle{ _gts_profile_site }

// a_id is a template parameter at every call site, so the static is per instantiation and
// the "<id>" suffix is composed once instead of being rebuilt on every call.
#define GTS_PROFILE_ENTRYPOINT_UNIQUE(a_name, a_id)                         \
    static GTS::ProfilerSite _gts_profile_site{ a_name, true, (a_id) };     \
    GTS::ScopeTimer _gts_profile_handle{ _gts_profile_site }

// The enabled test comes first so the disabled path never reaches typeid or the slot table.
#define GTS_PROFILE_LISTENER(a_listener, a_function)                                \
    static GTS::ListenerSite _gts_profile_site{ #a_function };                      \
    GTS::ScopeTimer _gts_profile_handle{ [&]() noexcept -> std::uint32_t {          \
        if (!GTS::ProfilerDetail::g_Collecting.load(std::memory_order_relaxed)) {   \
            return GTS::kInvalidSlot;                                               \
        }                                                                           \
        const std::uint32_t Index = (a_listener)->GetProfilerIndex();               \
        const std::uint32_t Slot = _gts_profile_site.Cached(Index);                 \
        return Slot != GTS::kInvalidSlot                                            \
             ? Slot                                                                 \
             : _gts_profile_site.Resolve(Index, typeid(*(a_listener)));             \
    }() }

#define GTS_PROFILER_DISPLAY_REPORT() GTS::Profilers::DisplayReport()

namespace GTS {

    class Profilers : CInitSingleton<Profilers> {

        public:

        static void DisplayReport();

        [[nodiscard]] static bool IsEnabled();
        static void SetEnabled(bool a_enabled);

        static inline double thread_expiration_time = 30.0;
        static inline double update_interval = 0.5;
        static constexpr int kHistory = 64;

        struct Row {
            const std::string* Name = nullptr;   //Points into the registry, which never reallocates.
            const float* History = nullptr;      //kHistory ms/frame samples, oldest at history_head + 1.
            std::int64_t Ticks = 0;
            std::uint32_t Calls = 0;
            bool Idle = false;                   //Held over from an earlier window, silent in this one.
        };

        private:

        //Anything drawable: one thread, or every thread summed together.
        struct ViewData {

            std::string Name;
            std::vector<std::array<float, kHistory>> History
            std::vector<std::uint8_t> Seeded;
            std::vector<std::uint32_t> LastSeen;
            std::array<std::vector<Row>, kSiteKindCount> Rows;

            double TotalSeconds = 0.0;
        };

        struct ThreadView : ViewData {

            ProfilerThreadState* State = nullptr;

            //Previous absolute readings, used to turn free running counters into deltas.
            std::vector<std::int64_t> PrevTicks;
            std::vector<std::uint32_t> PrevCalls;
            std::int64_t PrevTotal = 0;

            std::chrono::steady_clock::time_point LastActive{};

            std::uint32_t OsThreadId = 0;
            bool IsMain = false;
        };

        static void Sample();
        static void SampleAggregate(std::uint32_t a_slots);
        static void Record(ViewData& a_view, std::uint32_t a_slot, double a_ms);
        static void DrawHeaderRow();
        static void DrawThread(ThreadView& a_view, double a_totalSeconds);
        static void DrawBody(ViewData& a_view, double a_totalSeconds);
        static void DrawTable(const char* a_id, std::vector<Row>& a_rows, double a_totalSeconds, bool a_defaultOpen);

        static inline std::vector<ThreadView> views;
        static inline ViewData aggregate;

        //Scratch for the aggregate pass, kept alive so it is not reallocated every window.
        static inline std::vector<std::int64_t> aggregate_ticks;
        static inline std::vector<std::uint32_t> aggregate_calls;

        static inline double window_seconds = 0.0;
        static inline double total_seconds = 0.0;

        // Counters accumulate across a whole sample window, so everything on screen is
        // divided by the frames in that window to stay comparable to a frame budget.
        static inline std::uint32_t frames = 0;
        static inline std::uint32_t window_frames = 1;

        static inline int history_head = 0;
        static inline std::uint32_t window_index = 0;

        // How long a row that has gone quiet stays on screen.
        static inline double row_retention_seconds = 10.0;

        static inline bool paused = false;
        static inline bool aggregate_threads = false;
        static inline bool sort_by_cost = false;
        static inline bool show_graphs = true;

        static inline std::chrono::steady_clock::time_point last_sample = std::chrono::steady_clock::now();
        static inline bool enabled = false;
    };
}

#else

#define GTS_PROFILE_ENTRYPOINT_UNIQUE(a_name, a_id)
#define GTS_PROFILE_ENTRYPOINT(a_name)
#define GTS_PROFILE_SCOPE(a_name)
#define GTS_PROFILER_DISPLAY_REPORT()
#define GTS_PROFILE_LISTENER(a_listener, a_function)

#endif //GTS_PROFILER_ENABLED
