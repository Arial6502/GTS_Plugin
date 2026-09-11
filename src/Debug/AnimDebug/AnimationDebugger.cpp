#include "Debug/AnimDebug/AnimationDebugger.hpp"

#include "Managers/Console/ConsoleManager.hpp"
#include "UI/Controls/Button.hpp"
#include "UI/Controls/CheckBox.hpp"
#include "UI/Core/ImFontManager.hpp"
#include "UI/Core/ImUtil.hpp"

namespace {

	using namespace GTS;

	constexpr std::string_view LogName = "GTSPluginEvents";

	constexpr ImGuiTableFlags TableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_SizingStretchProp;

	// How long a changed row stays highlighted.
	constexpr double HighlightFor = 1.5;

	char g_VarFilter[128] = {};
	char g_EventFilter[128] = {};
	bool g_ChangedOnly = false;
	bool g_RecentFirst = false;
	bool g_AutoScroll = true;

	std::string ShortName(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return "none";
		}

		return std::format("{:08X} {}", a_Actor->formID, a_Actor->GetDisplayFullName());
	}
}

namespace GTS {

	bool AnimationDebugger::VarsEnabled() { return m_ShowVars.load(std::memory_order_relaxed); }
	bool AnimationDebugger::EventsEnabled() { return m_ShowEvents.load(std::memory_order_relaxed); }
	bool AnimationDebugger::AnyEnabled() { return VarsEnabled() || EventsEnabled(); }

	void AnimationDebugger::SetVarsEnabled(bool a_Enabled) {
		m_ShowVars.store(a_Enabled, std::memory_order_relaxed);
		m_Rebuild.store(true, std::memory_order_relaxed);
	}

	void AnimationDebugger::SetEventsEnabled(bool a_Enabled) {

		m_ShowEvents.store(a_Enabled, std::memory_order_relaxed);

		if (!a_Enabled) {
			CloseLog();
		}
	}

	RE::Actor* AnimationDebugger::Resolve(Scope a_Scope) {

		switch (a_Scope) {
			case Scope::kTarget:
			{
				return ConsoleArgs::SelectedActor();
			}
			case Scope::kAll:
			{
				return nullptr;
			}
			default:
			{
				return RE::PlayerCharacter::GetSingleton();
			}
		}
	}

	bool AnimationDebugger::InScope(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return false;
		}

		const Scope scope = m_EventScope.load(std::memory_order_relaxed);

		if (scope == Scope::kAll) {
			return true;
		}

		auto* wanted = Resolve(scope);
		return wanted && wanted->formID == a_Actor->formID;
	}

	bool AnimationDebugger::PassesNameFilter(std::string_view a_Name, const char* a_Filter) {

		if (!a_Filter || a_Filter[0] == '\0') {
			return true;
		}

		const std::string_view needle(a_Filter);

		return std::ranges::search(a_Name, needle, [](char a_L, char a_R) {
			return std::tolower(static_cast<unsigned char>(a_L)) == std::tolower(static_cast<unsigned char>(a_R));
		}).begin() != a_Name.end();
	}

	//--------------------------------------------------------------------------------------------
	// Capture, game thread
	//--------------------------------------------------------------------------------------------

	void AnimationDebugger::Capture(RE::Actor* a_Actor, RowKind a_Kind, std::string_view a_Tag, std::string_view a_Detail, bool a_Handled) {

		if (!EventsEnabled() || m_Paused.load(std::memory_order_relaxed) || !a_Actor || a_Tag.empty()) {
			return;
		}

		if (a_Kind == RowKind::kTrigger && !m_WantTriggers.load(std::memory_order_relaxed)) {
			return;
		}

		if (a_Kind == RowKind::kAnnotation && !m_WantAnnotations.load(std::memory_order_relaxed)) {
			return;
		}

		if (!InScope(a_Actor)) {
			return;
		}

		if (m_GtsOnly.load(std::memory_order_relaxed) && !GraphIntrospect::IsGTSName(a_Tag)) {
			return;
		}

		EventRow row{};
		row.Time = Time::WorldTimeElapsed();
		row.Actor = a_Actor->formID;
		row.ActorName = a_Actor->GetDisplayFullName();
		row.Kind = a_Kind;
		row.Tag = a_Tag;
		row.Detail = a_Detail;
		row.Handled = a_Handled;

		{
			std::scoped_lock lock(m_Lock);

			if (m_Incoming.size() >= MaxRows) {
				m_Dropped.fetch_add(1, std::memory_order_relaxed);
				return;
			}

			m_Incoming.push_back(std::move(row));
		}
	}

	void AnimationDebugger::Trigger(RE::Actor* a_Actor, std::string_view a_Trigger, std::string_view a_Detail, bool a_Sent) {
		Capture(a_Actor, RowKind::kTrigger, a_Trigger, a_Detail, a_Sent);
	}

	void AnimationDebugger::OnActorAnimationChange(RE::Actor* a_Actor, const std::string_view& a_Tag, const std::string_view& a_Payload) {
		Capture(a_Actor, RowKind::kAnnotation, a_Tag, a_Payload, true);
	}

	void AnimationDebugger::Rebuild(RE::Actor* a_Actor) {

		std::vector<GraphVarSlot> slots;

		if (!GraphIntrospect::BuildSlots(a_Actor, slots)) {
			return;
		}

		std::ranges::sort(slots, [](const GraphVarSlot& a_L, const GraphVarSlot& a_R) {
			return a_L.Display < a_R.Display;
		});

		m_Polled = std::move(slots);
		m_Built = a_Actor->formID;
	}

	void AnimationDebugger::OnMainUpdate() {

		if (!VarsEnabled() || m_Paused.load(std::memory_order_relaxed)) {
			return;
		}

		auto* actor = Resolve(m_VarScope.load(std::memory_order_relaxed));
		if (!actor || !actor->Is3DLoaded()) {
			return;
		}

		if (m_Rebuild.exchange(false, std::memory_order_relaxed)) {
			m_Built = 0;
			m_Polled.clear();
		}

		if (m_Built != actor->formID) {
			Rebuild(actor);
		}
		else {
			GraphIntrospect::Poll(actor, m_Polled, Time::WorldTimeElapsed(), [](const GraphVarSlot&, std::uint32_t, std::uint32_t) {});
		}

		{
			std::scoped_lock lock(m_Lock);
			m_Published = m_Polled;
		}
	}

	//--------------------------------------------------------------------------------------------
	// Log
	//--------------------------------------------------------------------------------------------

	bool AnimationDebugger::OpenLog() {

		if (m_Log) {
			return true;
		}

		auto path = SKSE::log::log_directory();
		if (!path) {
			logger::error("AnimDebug: no log directory");
			return false;
		}

		*path /= LogName;
		*path += L".log";

		try {
			m_Log = std::make_shared<spdlog::logger>("AnimDebug", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
			m_Log->set_pattern("%v");
			m_Log->set_level(spdlog::level::trace);
			m_Log->flush_on(spdlog::level::trace);
		}
		catch (const std::exception& e) {
			logger::error("AnimDebug: could not open {}.log: {}", LogName, e.what());
			m_Log = nullptr;
			return false;
		}

		return true;
	}

	void AnimationDebugger::CloseLog() {
		m_Log = nullptr;
	}

	void AnimationDebugger::WriteLog(const EventRow& a_Row) {

		if (!m_Log) {
			return;
		}

		m_Log->trace("{:9.3f}|{}|{:08X}|{}|{}|{}",
			a_Row.Time,
			a_Row.Kind == RowKind::kTrigger ? "TRIG" : "ANNO",
			a_Row.Actor,
			a_Row.Tag,
			a_Row.Detail.empty() ? "-" : a_Row.Detail,
			a_Row.Kind == RowKind::kTrigger ? (a_Row.Handled ? "sent" : "refused") : "-");
	}

	//--------------------------------------------------------------------------------------------
	// Draw, render thread
	//--------------------------------------------------------------------------------------------

	void AnimationDebugger::Draw() {

		if (!AnyEnabled() || !ImUtil::ValidState()) {
			return;
		}

		ImFontManager::Push(ImFontManager::ActiveFontType::kSubText);

		if (VarsEnabled()) {
			DrawVarWindow();
		}

		if (EventsEnabled()) {
			DrawEventWindow();
		}

		ImFontManager::Pop();
	}

	void AnimationDebugger::DrawVarWindow() {

		bool open = true;

		if (!ImGui::Begin("Animation Variables", &open, ImGuiWindowFlags_None) || ImGui::IsWindowCollapsed()) {
			ImGui::End();
			if (!open) {
				SetVarsEnabled(false);
			}
			return;
		}

		if (!open) {
			SetVarsEnabled(false);
		}

		const Scope varScope = m_VarScope.load(std::memory_order_relaxed);

		{
			int scope = static_cast<int>(varScope);
			ImGui::RadioButton("Player", &scope, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Console target", &scope, 1);

			if (static_cast<Scope>(scope) != varScope) {
				m_VarScope.store(static_cast<Scope>(scope), std::memory_order_relaxed);
				m_Rebuild.store(true, std::memory_order_relaxed);
			}
		}

		auto* actor = Resolve(varScope);

		{
			ImGui::SameLine();
			if (ImGuiEx::Button("Rebuild", "Re-read the variable list from the actor's behaviour graphs")) {
				m_Rebuild.store(true, std::memory_order_relaxed);
			}

			ImGui::TextColored(actor ? ImUtil::Colors::OK : ImUtil::Colors::Error, "%s", ShortName(actor).c_str());
		}

		{
			bool gtsOnly = m_GtsOnly.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("GTS only", &gtsOnly, "Only names beginning with GTS. Shared with the event monitor.")) {
				m_GtsOnly.store(gtsOnly, std::memory_order_relaxed);
			}

			ImGui::SameLine();
			ImGuiEx::CheckBox("Changed only", &g_ChangedOnly, "Only variables that have changed since this window opened");

			ImGui::SameLine();
			ImGuiEx::CheckBox("Recent first", &g_RecentFirst, "Sort by last change instead of by name");

			ImGui::SameLine();
			bool paused = m_Paused.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Pause", &paused, "Stop polling and capturing")) {
				m_Paused.store(paused, std::memory_order_relaxed);
			}
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##varfilter", "filter by name", g_VarFilter, sizeof(g_VarFilter));

		{
			std::scoped_lock lock(m_Lock);
			m_View = m_Published;
		}

		if (g_RecentFirst) {
			std::ranges::sort(m_View, [](const GraphVarSlot& a_L, const GraphVarSlot& a_R) {
				return a_L.ChangedAt > a_R.ChangedAt;
			});
		}

		const double now = Time::WorldTimeElapsed();
		const bool gtsOnly = m_GtsOnly.load(std::memory_order_relaxed);
		std::size_t shown = 0;

		if (ImGui::BeginTable("vars", 4, TableFlags)) {

			{
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Was", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();
			}

			for (const auto& slot : m_View) {

				if (gtsOnly && !GraphIntrospect::IsGTSName(slot.Display)) {
					continue;
				}

				if (g_ChangedOnly && slot.ChangedAt < 0.0) {
					continue;
				}

				if (!PassesNameFilter(slot.Display, g_VarFilter)) {
					continue;
				}

				++shown;
				ImGui::TableNextRow();

				const bool fresh = slot.ChangedAt >= 0.0 && (now - slot.ChangedAt) < HighlightFor;

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(slot.Display.c_str());

				ImGui::TableNextColumn();
				ImGui::TextColored(ImUtil::Colors::Subscript, "%s", GraphIntrospect::KindName(slot.Kind).data());

				ImGui::TableNextColumn();
				{
					const std::string value = GraphIntrospect::Describe(slot, slot.Raw);
					if (fresh) {
						ImGui::TextColored(ImUtil::Colors::Warning, "%s", value.c_str());
					}
					else {
						ImGui::TextUnformatted(value.c_str());
					}
				}

				ImGui::TableNextColumn();
				if (slot.ChangedAt >= 0.0) {
					ImGui::TextColored(ImUtil::Colors::Subscript, "%s", GraphIntrospect::Describe(slot, slot.Previous).c_str());
				}
				else {
					ImGui::TextColored(ImUtil::Colors::Subscript, "-");
				}
			}

			ImGui::EndTable();
		}

		ImGui::TextColored(ImUtil::Colors::Subscript, "%zu shown / %zu total", shown, m_View.size());
		ImGui::End();
	}

	void AnimationDebugger::DrawEventWindow() {

		{
			std::scoped_lock lock(m_Lock);

			for (auto& row : m_Incoming) {
				if (m_LogToFile.load(std::memory_order_relaxed)) {
					WriteLog(row);
				}
				m_Rows.push_back(std::move(row));
			}

			m_Incoming.clear();
		}

		while (m_Rows.size() > MaxRows) {
			m_Rows.pop_front();
		}

		bool open = true;

		if (!ImGui::Begin("Animation Events", &open, ImGuiWindowFlags_None) || ImGui::IsWindowCollapsed()) {
			ImGui::End();
			if (!open) {
				SetEventsEnabled(false);
			}
			return;
		}

		if (!open) {
			SetEventsEnabled(false);
		}

		{
			int scope = static_cast<int>(m_EventScope.load(std::memory_order_relaxed));
			ImGui::RadioButton("Player##evt", &scope, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Console target##evt", &scope, 1);
			ImGui::SameLine();
			ImGui::RadioButton("All actors##evt", &scope, 2);
			m_EventScope.store(static_cast<Scope>(scope), std::memory_order_relaxed);
		}

		{
			bool gtsOnly = m_GtsOnly.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("GTS only##evt", &gtsOnly, "Only tags beginning with GTS. Applies to the log as well.")) {
				m_GtsOnly.store(gtsOnly, std::memory_order_relaxed);
			}

			ImGui::SameLine();
			bool triggers = m_WantTriggers.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Triggers", &triggers, "StartAnim calls, including the refused ones")) {
				m_WantTriggers.store(triggers, std::memory_order_relaxed);
			}

			ImGui::SameLine();
			bool annotations = m_WantAnnotations.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Annotations", &annotations, "Every animation event the graph fires, vanilla included")) {
				m_WantAnnotations.store(annotations, std::memory_order_relaxed);
			}
		}

		{
			bool paused = m_Paused.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Pause##evt", &paused)) {
				m_Paused.store(paused, std::memory_order_relaxed);
			}

			ImGui::SameLine();
			ImGuiEx::CheckBox("Auto scroll", &g_AutoScroll);

			ImGui::SameLine();
			bool toFile = m_LogToFile.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Log to file", &toFile, "Write everything captured to GTSPluginEvents.log")) {
				if (toFile && !OpenLog()) {
					toFile = false;
				}
				m_LogToFile.store(toFile, std::memory_order_relaxed);
			}

			ImGui::SameLine();
			if (ImGuiEx::Button("Clear")) {
				m_Rows.clear();
				m_Dropped.store(0, std::memory_order_relaxed);
			}
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##evtfilter", "filter by tag", g_EventFilter, sizeof(g_EventFilter));

		std::size_t shown = 0;

		if (ImGui::BeginTable("events", 5, TableFlags)) {

			{
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 45.0f);
				ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();
			}

			for (const auto& row : m_Rows) {

				if (!PassesNameFilter(row.Tag, g_EventFilter)) {
					continue;
				}

				++shown;
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::TextColored(ImUtil::Colors::Subscript, "%8.2f", row.Time);

				ImGui::TableNextColumn();
				if (row.Kind == RowKind::kTrigger) {
					ImGui::TextColored(row.Handled ? ImUtil::Colors::OK : ImUtil::Colors::Error, "TRIG");
				}
				else {
					ImGui::TextUnformatted("ANNO");
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row.ActorName.c_str());

				ImGui::TableNextColumn();
				if (GraphIntrospect::IsGTSName(row.Tag)) {
					ImGui::TextColored(ImUtil::Colors::Warning, "%s", row.Tag.c_str());
				}
				else {
					ImGui::TextUnformatted(row.Tag.c_str());
				}

				ImGui::TableNextColumn();
				ImGui::TextColored(ImUtil::Colors::Subscript, "%s", row.Detail.empty() ? "-" : row.Detail.c_str());
			}

			if (g_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}

			ImGui::EndTable();
		}

		{
			ImGui::TextColored(ImUtil::Colors::Subscript, "%zu shown / %zu held", shown, m_Rows.size());

			if (const std::size_t dropped = m_Dropped.load(std::memory_order_relaxed); dropped > 0) {
				ImGui::SameLine();
				ImGui::TextColored(ImUtil::Colors::Error, "%zu dropped", dropped);
			}
		}

		ImGui::End();
	}

	//--------------------------------------------------------------------------------------------
	// Lifecycle
	//--------------------------------------------------------------------------------------------

	// Dispatched straight from the Set3D hook, on whichever thread is unloading the cell. The draw
	// code walks m_Polled on the render thread.
	void AnimationDebugger::OnActor3DUnload(RE::Actor* a_Actor) {

		std::scoped_lock lock(m_Lock);

		if (a_Actor && m_Built == a_Actor->formID) {
			m_Polled.clear();
			m_Built = 0;
		}
	}

	void AnimationDebugger::OnPluginReset() {

		m_Polled.clear();
		m_Built = 0;
		m_Dropped.store(0, std::memory_order_relaxed);

		std::scoped_lock lock(m_Lock);
		m_Incoming.clear();
		m_Published.clear();
	}

	void AnimationDebugger::OnSKSEDataLoaded() {

		ConsoleManager::RegisterCommand({
			.Name = "animdebug",
			.Desc = "Toggle the animation variable and event debug windows",
			.Usage = "<vars|events|off>",
			.MinArgs = 1,
			.MaxArgs = 1,
			.Callback = [](const ConsoleArgs& a_Args) {

				const std::string action = a_Args.Lower(0);

				if (action == "vars") {
					SetVarsEnabled(!VarsEnabled());
					Cprint("Animation variables: {}", VarsEnabled() ? "shown" : "hidden");
					return;
				}

				if (action == "events") {
					SetEventsEnabled(!EventsEnabled());
					Cprint("Animation events: {}", EventsEnabled() ? "shown" : "hidden");
					return;
				}

				if (action == "off") {
					SetVarsEnabled(false);
					SetEventsEnabled(false);
					Cprint("Animation debug windows hidden.");
					return;
				}

				ConsoleManager::PrintUsage("animdebug");
			},
		});
	}
}
