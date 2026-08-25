#include "Debug/Profilers.hpp"

#include "Managers/GTSManager.hpp"
#include "Systems/Misc/Tasks.hpp"
#include "UI/Core/ImFontManager.hpp"

#ifdef GTS_PROFILER_ENABLED

namespace {

    using namespace GTS;

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Borders                |
        ImGuiTableFlags_HighlightHoveredColumn |
        ImGuiTableFlags_Sortable               |
        ImGuiTableFlags_BordersOuter           |
        ImGuiTableFlags_SizingFixedFit;


    constexpr float kColTotal = 21.0f;
    constexpr float kColWorstEP = 30.0f;
    constexpr float kColWorstEPTime = 47.0f;
    constexpr float kColWorstScope = 56.0f;
    constexpr float kColWorstScopeTime = 73.0f;
    constexpr std::size_t kWorstNameChars = 16;

   
    constexpr std::array<const char*, kSiteKindCount> kKindNames { "Entrypoints", "Events", "Scoped", "Tasks" };
    constexpr std::array<bool, kSiteKindCount> kKindDefaultOpen { true, false, false, true };

    void SortRows(std::vector<Profilers::Row>& a_rows, ImGuiTableSortSpecs* a_specs);

    //Rough character width.
    [[nodiscard]] float Em() {
        return ImGui::CalcTextSize("M").x;
    }

    // Pins the next item to a fixed column.
    void ColumnAt(float a_chars) {
        ImGui::SameLine(a_chars * Em());
    }

    // Names vary in length far more than numbers, so an unclipped name would push every later
    // column around even with fixed starts.
    [[nodiscard]] std::string Clip(const char* a_text, std::size_t a_max) {

        std::string text = a_text ? a_text : "-";

        if (text.size() > a_max) {
            text.resize(a_max > 1 ? a_max - 1 : 0);
            text += "~";
        }

        return text;
    }

    [[nodiscard]] std::uint32_t MainThreadId() {
        const RE::Main* main = RE::Main::GetSingleton();
        return main ? main->threadID : 0u;
    }

    [[nodiscard]] ImVec4 CostColor(double a_msPerFrame) {

        if (a_msPerFrame >= 1.0)  return ImVec4(0.95f, 0.45f, 0.40f, 1.0f);
        if (a_msPerFrame >= 0.33) return ImVec4(0.95f, 0.80f, 0.35f, 1.0f);

        return ImVec4(0.55f, 0.85f, 0.55f, 1.0f);
    }

    [[nodiscard]] const Profilers::Row* Worst(const std::vector<Profilers::Row>& a_rows) {

        const Profilers::Row* best = nullptr;

        for (const Profilers::Row& row : a_rows) {
            if (!best || row.Ticks > best->Ticks) {
                best = &row;
            }
        }

        return best;
    }
}

namespace GTS {

    bool Profilers::IsEnabled() {
        return enabled;
    }

    void Profilers::SetEnabled(bool a_enabled) {

        if (enabled == a_enabled) return;

        enabled = a_enabled;
        ProfilerDetail::g_Collecting.store(a_enabled, std::memory_order_relaxed);

        if (a_enabled) {

            ProfilerDetail::SeedCalibration();

            if (!ProfilerDetail::HasInvariantTsc()) {
                logger::warn("Profiler: CPU does not report an invariant TSC, timings will drift with clock speed.");
            }

            last_sample = std::chrono::steady_clock::now();
            frames = 0;

            for (ThreadView& view : views) {
                view.PrevTicks.assign(view.PrevTicks.size(), 0);
                view.PrevCalls.assign(view.PrevCalls.size(), 0);
                view.PrevTotal = 0;
            }

            ProfilerRegistry::ResetAll();
        }
    }

    // Writes one window into a slot's ring. The first write fills the whole ring, otherwise a
    // freshly seen slot would plot as 63 zeros and one live sample for the first 32 seconds.
    void Profilers::Record(ViewData& a_view, std::uint32_t a_slot, double a_ms) {

        const float value = static_cast<float>(a_ms);

        if (!a_view.Seeded[a_slot]) {
            a_view.History[a_slot].fill(value);
            a_view.Seeded[a_slot] = 1;
            return;
        }

        a_view.History[a_slot][history_head] = value;
    }

    //---------------
    //  Sampling
    //---------------

    // Reads every thread's counters, converts them to deltas over the elapsed window, and
    // rebuilds the display rows. Runs on the update interval, not every frame. Nothing here
    // writes to a counter, so the threads being measured never see the reader.
    void Profilers::Sample() {

        ProfilerDetail::RefreshCalibration();

        const auto now = std::chrono::steady_clock::now();
        window_seconds = std::chrono::duration<double>(now - last_sample).count();
        last_sample = now;

        window_frames = std::max(frames, 1u);
        frames = 0;

        const std::uint32_t slots = ProfilerRegistry::SlotCount();
        const double perFrame = 1.0 / static_cast<double>(window_frames);
        const std::uint32_t mainId = MainThreadId();

        history_head = (history_head + 1) % kHistory;
        ++window_index;

        //Retention in windows. Zero drops a row the moment it goes quiet, as it used to.
        const std::uint32_t retention = static_cast<std::uint32_t>(std::max(0.0, row_retention_seconds / std::max(update_interval, 0.01)));

        aggregate_ticks.assign(slots, 0);
        aggregate_calls.assign(slots, 0);
        std::size_t index = 0;

        for (ProfilerThreadState* state = ProfilerRegistry::ThreadListHead(); state; state = state->Next) {

            if (!state->InUse.load(std::memory_order_acquire)) {
                continue;
            }

            if (index >= views.size()) {
                views.emplace_back();
            }

            ThreadView& view = views[index];

            if (view.State != state) {
                view = ThreadView{};
                view.State = state;
                view.OsThreadId = state->OsThreadId;
                view.LastActive = now;
            }

           
            view.IsMain = view.OsThreadId != 0u && view.OsThreadId == mainId;
            view.Name = view.IsMain
                ? std::format("Main Thread ({})", view.OsThreadId)
                : std::format("Thread {}", view.OsThreadId);

            view.PrevTicks.resize(slots, 0);
            view.PrevCalls.resize(slots, 0);
            view.History.resize(slots);
            view.Seeded.resize(slots, 0);
            view.LastSeen.resize(slots, 0);

            for (std::vector<Row>& rows : view.Rows) {
                rows.clear();
            }

            bool active = false;

            for (std::uint32_t slot = 0; slot < slots; ++slot) {

                const ProfilerCounter& counter = state->Counters[slot];
                const std::int64_t ticks = counter.Ticks.load(std::memory_order_relaxed);
                const std::uint32_t calls = counter.Calls.load(std::memory_order_relaxed);

                const std::int64_t deltaTicks = std::max<std::int64_t>(ticks - view.PrevTicks[slot], 0);
                const std::uint32_t deltaCalls = calls - view.PrevCalls[slot];

                view.PrevTicks[slot] = ticks;
                view.PrevCalls[slot] = calls;

                const double ms = ProfilerDetail::TicksToSeconds(deltaTicks) * perFrame * 1000.0;
                Record(view, slot, ms);

                aggregate_ticks[slot] += deltaTicks;
                aggregate_calls[slot] += deltaCalls;

                if (deltaCalls != 0) {
                    view.LastSeen[slot] = window_index;
                    active = true;
                }

                if (view.LastSeen[slot] == 0 || window_index - view.LastSeen[slot] > retention) {
                    continue;
                }

                const Row row{ &ProfilerRegistry::SiteName(slot), view.History[slot].data(), deltaTicks, deltaCalls, deltaCalls == 0 };
                view.Rows[static_cast<std::size_t>(ProfilerRegistry::SiteKindOf(slot))].push_back(row);
            }

            const std::int64_t total = state->TotalTicks.load(std::memory_order_relaxed);
            view.TotalSeconds = ProfilerDetail::TicksToSeconds(std::max<std::int64_t>(total - view.PrevTotal, 0));
            view.PrevTotal = total;

            if (active) {
                view.LastActive = now;
            }

            ++index;
        }

        views.resize(index);

        total_seconds = 0.0;
        for (const ThreadView& view : views) {
            total_seconds += view.TotalSeconds;
        }

        SampleAggregate(slots);
    }

    void Profilers::SampleAggregate(std::uint32_t a_slots) {

        const double perFrame = 1.0 / static_cast<double>(window_frames);

        aggregate.Name = "All Threads";
        aggregate.TotalSeconds = total_seconds;
        aggregate.History.resize(a_slots);
        aggregate.Seeded.resize(a_slots, 0);
        aggregate.LastSeen.resize(a_slots, 0);

        const std::uint32_t retention = static_cast<std::uint32_t>(
            std::max(0.0, row_retention_seconds / std::max(update_interval, 0.01)));
        for (std::vector<Row>& rows : aggregate.Rows) {
            rows.clear();
        }

        for (std::uint32_t slot = 0; slot < a_slots; ++slot) {

            const std::int64_t ticks = aggregate_ticks[slot];
            const double ms = ProfilerDetail::TicksToSeconds(ticks) * perFrame * 1000.0;

            Record(aggregate, slot, ms);

            if (aggregate_calls[slot] != 0) {
                aggregate.LastSeen[slot] = window_index;
            }

            if (aggregate.LastSeen[slot] == 0 || window_index - aggregate.LastSeen[slot] > retention) {
                continue;
            }

            const Row row{ &ProfilerRegistry::SiteName(slot), aggregate.History[slot].data(), ticks,
                           aggregate_calls[slot], aggregate_calls[slot] == 0 };

            aggregate.Rows[static_cast<std::size_t>(ProfilerRegistry::SiteKindOf(slot))].push_back(row);
        }
    }

    //---------------
    //  Draw
    //---------------

    void Profilers::DrawTable(const char* a_id, std::vector<Row>& a_rows, double a_totalSeconds, bool a_defaultOpen) {

        const std::string label = std::format("{} ({})", a_id, a_rows.size());

        ImGui::PushID(a_id);

        const bool open = a_defaultOpen
            ? ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)
            : ImGui::TreeNode(label.c_str());

        if (!open) {
            ImGui::PopID();
            return;
        }

        const int columns = show_graphs ? 6 : 5;

        if (ImGui::BeginTable("t", columns, kTableFlags)) {

            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ms/frame", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Calls/frame", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("% DLL", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed);

            if (show_graphs) {
                ImGui::TableSetupColumn("Trend", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed);
            }

            ImGui::TableHeadersRow();

            if (auto* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0) {
                SortRows(a_rows, specs);
            }

            const double perFrame = 1.0 / static_cast<double>(window_frames);
            const float graphWidth = 12.0f * Em();
            const float graphHeight = ImGui::GetTextLineHeight() * 2.5f + ImGui::GetStyle().FramePadding.y * 2.0f;

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(a_rows.size()));

            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {

                    const Row& row = a_rows[static_cast<std::size_t>(i)];
                    const double seconds = ProfilerDetail::TicksToSeconds(row.Ticks);
                    const double ms = seconds * perFrame * 1000.0;
                    const double average = row.Calls > 0 ? seconds / static_cast<double>(row.Calls) : 0.0;
                    const double share = a_totalSeconds > 0.0 ? seconds / a_totalSeconds * 100.0 : 0.0;

                    ImGui::TableNextRow();

                    if (row.Idle) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    }

                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(row.Name->c_str());
                    ImGui::TableSetColumnIndex(1);

                    if (row.Idle) {
                        ImGui::TextUnformatted("-");
                    }
                    else {
                        ImGui::TextColored(CostColor(ms), "%.3f", ms);
                    }

                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", static_cast<double>(row.Calls) * perFrame);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.4fms", average * 1000.0);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f%%", share);

                    if (show_graphs && row.History) {

                        ImGui::TableSetColumnIndex(5);
                        ImGui::PushID(i);

                        ImGui::PlotLines("", row.History, kHistory, history_head + 1, nullptr, 0.0f, FLT_MAX, ImVec2(graphWidth, graphHeight));

                        ImGui::PopID();
                    }

                    if (row.Idle) {
                        ImGui::PopStyleColor();
                    }
                }
            }

            ImGui::EndTable();
        }

        ImGui::TreePop();
        ImGui::PopID();
    }

    void Profilers::DrawBody(ViewData& a_view, double a_totalSeconds) {

        for (std::size_t kind = 0; kind < kSiteKindCount; ++kind) {

            if (a_view.Rows[kind].empty()) {
                continue;
            }

            DrawTable(kKindNames[kind], a_view.Rows[kind], a_totalSeconds, kKindDefaultOpen[kind]);
        }
    }

    void Profilers::DrawHeaderRow() {

        ImGui::TextDisabled("Thread");
        ColumnAt(kColTotal);          ImGui::TextDisabled("ms/frame");
        ColumnAt(kColWorstEP);        ImGui::TextDisabled("worst entrypoint");
        ColumnAt(kColWorstEPTime);    ImGui::TextDisabled("ms");
        ColumnAt(kColWorstScope);     ImGui::TextDisabled("worst inner");
        ColumnAt(kColWorstScopeTime); ImGui::TextDisabled("ms");
        ImGui::Separator();
    }

    void Profilers::DrawThread(ThreadView& a_view, double a_totalSeconds) {

        const Row* worstEntrypoint = Worst(a_view.Rows[static_cast<std::size_t>(SiteKind::Entrypoint)]);
        const Row* worstInner = nullptr;

        for (const SiteKind kind : { SiteKind::Event, SiteKind::Scope, SiteKind::Task }) {

            const Row* candidate = Worst(a_view.Rows[static_cast<std::size_t>(kind)]);

            if (candidate && (!worstInner || candidate->Ticks > worstInner->Ticks)) {
                worstInner = candidate;
            }
        }

        const double perFrame = 1.0 / static_cast<double>(window_frames);
        const double totalMs = a_view.TotalSeconds * perFrame * 1000.0;

        //Keyed on the OS thread id, so a renamed or recycled slot keeps its open/closed state.
        ImGui::PushID(static_cast<int>(a_view.OsThreadId));

        if (a_view.IsMain) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.0f));
        }

        const bool open = ImGui::TreeNodeEx("node", a_view.IsMain ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", a_view.Name.c_str());

        if (a_view.IsMain) {
            ImGui::PopStyleColor();
        }

        ColumnAt(kColTotal);
        ImGui::TextColored(CostColor(totalMs), "%7.3f", totalMs);

        ColumnAt(kColWorstEP);
        ImGui::TextUnformatted(Clip(worstEntrypoint ? worstEntrypoint->Name->c_str() : nullptr, kWorstNameChars).c_str());
        ColumnAt(kColWorstEPTime);
        ImGui::Text("%7.3f", worstEntrypoint ? ProfilerDetail::TicksToSeconds(worstEntrypoint->Ticks) * perFrame * 1000.0 : 0.0);

        ColumnAt(kColWorstScope);
        ImGui::TextUnformatted(Clip(worstInner ? worstInner->Name->c_str() : nullptr, kWorstNameChars).c_str());
        ColumnAt(kColWorstScopeTime);
        ImGui::Text("%7.3f", worstInner ? ProfilerDetail::TicksToSeconds(worstInner->Ticks) * perFrame * 1000.0 : 0.0);

        if (open) {
            DrawBody(a_view, a_totalSeconds);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void Profilers::DisplayReport() {

        if (!enabled) {
            return;
        }

        ++frames;

        const auto now = std::chrono::steady_clock::now();

        if (!paused && std::chrono::duration<double>(now - last_sample).count() >= update_interval) {
            Sample();
        }

        ImFontManager::Push(ImFontManager::ActiveFontType::kSubText);

        if (!ImGui::Begin("Profiler Report", nullptr, ImGuiWindowFlags_None) || ImGui::IsWindowCollapsed()) {
            ImGui::End();
            ImFontManager::Pop();
            return;
        }

        const double dllMs = total_seconds / static_cast<double>(window_frames) * 1000.0;

        ImGui::TextDisabled("DLL");
        ColumnAt(4.0f);  ImGui::TextColored(CostColor(dllMs), "%7.3f ms/frame", dllMs);
        ColumnAt(22.0f); ImGui::Text("FPS %6.1f", static_cast<double>(ImGui::GetIO().Framerate));
        ColumnAt(34.0f); ImGui::Text("Actors %4d", GTSManager::LoadedActorCount);
        ColumnAt(47.0f); ImGui::Text("Threads %3zu", views.size());
        ColumnAt(60.0f); ImGui::Text("Tasks %5zu", TaskManager::LiveTaskCount());
        ColumnAt(73.0f); ImGui::Text("Sites %5u", ProfilerRegistry::SlotCount());
        ColumnAt(86.0f); ImGui::TextDisabled("%.2fs / %u", window_seconds, window_frames);

        if (ImGui::Button("Settings")) {
            ImGui::OpenPopup("ProfilerSettings");
        }

        if (ImGui::BeginPopup("ProfilerSettings")) {

            float expiration = static_cast<float>(thread_expiration_time);
            if (ImGui::SliderFloat("Thread Expiration (s)", &expiration, 5.0f, 300.0f, "%.1f")) {
                thread_expiration_time = expiration;
            }

            float interval = static_cast<float>(update_interval);
            if (ImGui::SliderFloat("Update Interval (s)", &interval, 0.1f, 2.0f, "%.2f")) {
                update_interval = interval;
            }

            float retention = static_cast<float>(row_retention_seconds);
            if (ImGui::SliderFloat("Row Retention (s)", &retention, 0.0f, 60.0f, "%.1f")) {
                row_retention_seconds = retention;
            }

            ImGui::SetItemTooltip("How long a row stays listed after it stops being called.\n"
                                  "Zero drops it immediately, which reorders the list as sites come and go.");

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(paused ? "Resume" : "Pause")) {
            paused = !paused;
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear")) {
            ProfilerRegistry::ResetAll();
            views.clear();
            aggregate = ViewData{};
            total_seconds = 0.0;
            last_sample = now;
            frames = 0;
        }

        ImGui::SameLine(); ImGui::Checkbox("Merge threads", &aggregate_threads);
        ImGui::SameLine(); ImGui::Checkbox("Trend", &show_graphs);

        if (!aggregate_threads) {
            ImGui::SameLine();
            ImGui::Checkbox("Sort by cost", &sort_by_cost);
        }

        ImGui::Separator();

        if (aggregate_threads) {
            DrawBody(aggregate, total_seconds);
            ImGui::End();
            ImFontManager::Pop();
            return;
        }

        if (!views.empty()) {

            DrawHeaderRow();

            // Draw order is a list of indices, views itself has to stay in thread list order to
            // remain index matched to the live threads in Sample.
            static std::vector<std::size_t> order;
            order.clear();

            for (std::size_t i = 0; i < views.size(); ++i) {

                if (std::chrono::duration<double>(now - views[i].LastActive).count() > thread_expiration_time) {
                    continue;
                }

                order.push_back(i);
            }

            std::ranges::stable_sort(order, [](std::size_t a_lhs, std::size_t a_rhs) {

                const ThreadView& lhs = views[a_lhs];
                const ThreadView& rhs = views[a_rhs];

                if (lhs.IsMain != rhs.IsMain) {
                    return lhs.IsMain;
                }

                return sort_by_cost && lhs.TotalSeconds > rhs.TotalSeconds;
            });

            for (const std::size_t i : order) {
                DrawThread(views[i], total_seconds);
            }
        }

        ImGui::End();
        ImFontManager::Pop();
    }
}

namespace {

    void SortRows(std::vector<Profilers::Row>& a_rows, ImGuiTableSortSpecs* a_specs) {

        const auto& spec = a_specs->Specs[0];
        const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
        a_specs->SpecsDirty = false;

        const auto key = [&](const Profilers::Row& a_row) -> double {

            switch (spec.ColumnIndex) {

                case 1: return static_cast<double>(a_row.Ticks);
                case 2: return static_cast<double>(a_row.Calls);
                case 3: return a_row.Calls > 0 ? static_cast<double>(a_row.Ticks) / static_cast<double>(a_row.Calls) : 0.0;
                case 4: return static_cast<double>(a_row.Ticks);
                default: return 0.0;
            }
        };

        std::ranges::sort(a_rows, [&](const Profilers::Row& a_lhs, const Profilers::Row& a_rhs) {

            if (spec.ColumnIndex == 0) {
                return ascending ? (*a_lhs.Name < *a_rhs.Name) : (*a_lhs.Name > *a_rhs.Name);
            }

            const double lhs = key(a_lhs);
            const double rhs = key(a_rhs);

            if (lhs == rhs) {
                return *a_lhs.Name < *a_rhs.Name;
            }

            return ascending ? (lhs < rhs) : (lhs > rhs);
        });
    }
}

#endif //GTS_PROFILER_ENABLED
