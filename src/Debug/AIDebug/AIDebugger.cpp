#include "Debug/AIDebug/AIDebugger.hpp"

#include "Managers/AI/Director.hpp"
#include "UI/Core/ImUtil.hpp"
#include "UI/Controls/CheckBox.hpp"
#include "UI/Controls/Button.hpp"

namespace {

	using namespace GTS;
	using GTS::AI::BrainView;

	char g_BrainFilter[64] = "";
	char g_EventFilter[64] = "";

	bool Matches(std::string_view a_Text, const char* a_Filter) {

		if (!a_Filter || !*a_Filter) {
			return true;
		}

		const auto same = [](char a_A, char a_B) {
			return std::tolower(static_cast<unsigned char>(a_A)) == std::tolower(static_cast<unsigned char>(a_B));
		};

		return std::ranges::search(a_Text, std::string_view(a_Filter), same).begin() != a_Text.end();
	}

	ImVec4 PhaseColour(AI::Phase a_Phase) {
		switch (a_Phase) {
			case AI::Phase::kIdle:     { return ImUtil::Colors::Subscript; }
			case AI::Phase::kApproach: { return ImUtil::Colors::Warning; }
			case AI::Phase::kPosture:  { return ImUtil::Colors::Warning; }
			case AI::Phase::kStart:    { return ImUtil::Colors::OK; }
			case AI::Phase::kEngaged:  { return ImUtil::Colors::OK; }
			default:                   { return ImUtil::Colors::Error; }
		}
	}

	// The Applied bits, spelled out. Nothing applied reads as a dash rather than an empty cell.
	std::string Effects(std::uint8_t a_Applied) {

		std::string text = {};

		const auto add = [&](std::uint8_t a_Bit, std::string_view a_Name) {
			if (a_Applied & a_Bit) {
				text += text.empty() ? "" : " ";
				text += a_Name;
			}
		};

		add(AI::Effect::kSteering, "steering");
		add(AI::Effect::kSuppressed, "suppressed");
		add(AI::Effect::kCombatLever, "out-of-combat");
		add(AI::Effect::kStance, "stance");

		return text.empty() ? "-" : text;
	}

	void Pair(std::string_view a_Label, std::string_view a_Value, const ImVec4* a_Colour = nullptr) {
		ImGui::TextUnformatted(a_Label.data(), a_Label.data() + a_Label.size());
		ImGui::SameLine(160.0f);
		if (a_Colour) {
			ImGui::TextColored(*a_Colour, "%.*s", static_cast<int>(a_Value.size()), a_Value.data());
		}
		else {
			ImGui::TextUnformatted(a_Value.data(), a_Value.data() + a_Value.size());
		}
	}

	// A countdown that has already elapsed is not interesting, so it reads as "now".
	std::string Countdown(float a_Seconds) {
		return a_Seconds <= 0.0f ? "now" : std::format("{:.1f}s", a_Seconds);
	}
}

namespace GTS {

	bool AIDebugger::BrainsEnabled() { return m_ShowBrains.load(std::memory_order_relaxed); }
	bool AIDebugger::EventsEnabled() { return m_ShowEvents.load(std::memory_order_relaxed); }
	bool AIDebugger::AnyEnabled()    { return BrainsEnabled() || EventsEnabled(); }

	void AIDebugger::SetBrainsEnabled(bool a_Enabled) { m_ShowBrains.store(a_Enabled, std::memory_order_relaxed); }
	void AIDebugger::SetEventsEnabled(bool a_Enabled) { m_ShowEvents.store(a_Enabled, std::memory_order_relaxed); }

	void AIDebugger::Record(RE::FormID a_Actor, std::string_view a_Name, std::string_view a_Line) {

		if (!EventsEnabled() || m_Paused.load(std::memory_order_relaxed)) {
			return;
		}

		// The Director's lines are "KIND detail", so the first word is what the filter and the colour
		// key off.
		const std::size_t split = a_Line.find(' ');

		EventRow row = {};
		row.Time = Time::WorldTimeElapsed();
		row.Actor = a_Actor;
		row.Name = a_Name;
		row.Kind = std::string(a_Line.substr(0, split));
		row.Detail = split == std::string_view::npos ? "" : std::string(a_Line.substr(split + 1));

		std::scoped_lock lock(m_Lock);

		if (m_Incoming.size() < MaxRows) {
			m_Incoming.push_back(std::move(row));
		}
	}

	void AIDebugger::OnMainUpdate() {

		if (!BrainsEnabled() || m_Paused.load(std::memory_order_relaxed)) {
			return;
		}

		std::vector<BrainView> views = AI::Director::Inspect();

		std::scoped_lock lock(m_Lock);
		m_Published = std::move(views);
	}

	void AIDebugger::OnPluginReset() {
		std::scoped_lock lock(m_Lock);
		m_Published.clear();
		m_Incoming.clear();
	}

	void AIDebugger::Draw() {

		if (BrainsEnabled()) {
			DrawBrainWindow();
		}

		if (EventsEnabled()) {
			DrawEventWindow();
		}
	}

	void AIDebugger::DrawBrain(const BrainView& a_View) {

		const ImVec4 phase = PhaseColour(a_View.Stage);

		Pair("Phase", std::format("{} for {:.1f}s", AI::PhaseName(a_View.Stage), a_View.StageFor), &phase);
		Pair("Applied", Effects(a_View.Applied));

		{
			const std::string plan = a_View.Adapter.empty()
				? "none"
				: std::format("{}{}{}", a_View.Adapter, a_View.Forced ? " (forced)" : "", a_View.Adopted ? " (adopted)" : "");

			Pair("Plan", plan);
		}

		if (a_View.Target) {
			Pair("Target", std::format("{} [{:08X}]", a_View.TargetName, a_View.Target));
			Pair("Distance", std::format("{:.0f}, reach {:.0f}, {:.0f} to go", a_View.Distance, a_View.Reach, a_View.Remaining));

			if (a_View.ApproachFrom > 0.0f) {
				const float covered = 1.0f - (a_View.Remaining / a_View.ApproachFrom);
				Pair("Approach", std::format("{:.0f}% of {:.0f} covered", covered * 100.0f, a_View.ApproachFrom));
			}
		}
		else {
			Pair("Target", a_View.Adapter.empty() ? "-" : "self");
		}

		Pair("Stance", a_View.Rolled ? std::string(Actions::StanceName(a_View.Want)) : std::string("not rolled yet"));

		{
			const ImVec4 stalled = a_View.Stalled > 2.0f ? ImUtil::Colors::Error : ImUtil::Colors::Subscript;
			Pair("No progress for", std::format("{:.1f}s", a_View.Stalled), &stalled);
		}

		Pair("Action node", a_View.Current);

		{
			const std::string flags = std::format("{} {} {} {}",
				a_View.Eligible ? "eligible" : "INELIGIBLE",
				a_View.Busy ? "busy" : "idle",
				a_View.Gated ? "owns-movement" : "free",
				a_View.Suppressed ? "attacks-held" : "attacks-free");

			const ImVec4 colour = a_View.Eligible ? ImUtil::Colors::Subscript : ImUtil::Colors::Error;
			Pair("State", flags, &colour);
		}

		Pair("Next think", Countdown(a_View.NextThink));

		if (a_View.Stage == AI::Phase::kEngaged) {
			Pair("Next sub-action", std::format("{} ({} empty in a row)", Countdown(a_View.NextSub), a_View.Empty));
		}

		if (a_View.Stage == AI::Phase::kApproach) {
			Pair("Next retarget", Countdown(a_View.NextRetarget));
		}

		if (!a_View.Refused.empty() && ImGui::TreeNode("Refused targets")) {
			for (const auto& [what, left] : a_View.Refused) {
				ImGui::Text("%s, retry in %.1fs", what.c_str(), left);
			}
			ImGui::TreePop();
		}

		if (!a_View.SubRefused.empty() && ImGui::TreeNode("Refused sub-actions")) {
			for (const auto& [what, left] : a_View.SubRefused) {
				ImGui::Text("%s, retry in %.1fs", what.c_str(), left);
			}
			ImGui::TreePop();
		}
	}

	void AIDebugger::DrawBrainWindow() {

		bool open = true;

		if (!ImGui::Begin("AI Brains", &open, ImGuiWindowFlags_None) || ImGui::IsWindowCollapsed()) {
			ImGui::End();
			if (!open) {
				SetBrainsEnabled(false);
			}
			return;
		}

		if (!open) {
			SetBrainsEnabled(false);
		}

		{
			bool paused = m_Paused.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Pause", &paused, "Stop snapshotting and capturing. Shared with the event monitor.")) {
				m_Paused.store(paused, std::memory_order_relaxed);
			}
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##brainfilter", "filter by actor name", g_BrainFilter, sizeof(g_BrainFilter));

		{
			std::scoped_lock lock(m_Lock);
			m_View = m_Published;
		}

		if (m_View.empty()) {
			ImGui::TextColored(ImUtil::Colors::Warning, "No brains running. Nobody is eligible, or the AI is off.");
			ImGui::End();
			return;
		}

		ImGui::Text("%zu brain(s)", m_View.size());
		ImGui::Separator();

		for (const BrainView& view : m_View) {

			if (!Matches(view.Name, g_BrainFilter)) {
				continue;
			}

			const std::string header = std::format("{} [{:08X}] - {}###{:08X}",
				view.Name, view.Actor, AI::PhaseName(view.Stage), view.Actor);

			if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Indent();
				DrawBrain(view);
				ImGui::Unindent();
			}
		}

		ImGui::End();
	}

	void AIDebugger::DrawEventWindow() {

		bool open = true;

		if (!ImGui::Begin("AI Events", &open, ImGuiWindowFlags_None) || ImGui::IsWindowCollapsed()) {
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
			bool paused = m_Paused.load(std::memory_order_relaxed);
			if (ImGuiEx::CheckBox("Pause", &paused, "Stop snapshotting and capturing. Shared with the brain view.")) {
				m_Paused.store(paused, std::memory_order_relaxed);
			}

			ImGui::SameLine();
			if (ImGuiEx::Button("Clear", "Drop every captured line")) {
				m_Rows.clear();
			}
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##eventfilter", "filter by actor, kind or detail", g_EventFilter, sizeof(g_EventFilter));

		{
			std::scoped_lock lock(m_Lock);

			for (EventRow& row : m_Incoming) {
				m_Rows.push_back(std::move(row));
			}

			m_Incoming.clear();
		}

		while (m_Rows.size() > MaxRows) {
			m_Rows.pop_front();
		}

		if (ImGui::BeginTable("aievents", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {

			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableSetupColumn("Actor", ImGuiTableColumnFlags_WidthFixed, 140.0f);
				ImGui::TableSetupColumn("What", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Detail");
				ImGui::TableHeadersRow();
			}

			for (const EventRow& row : m_Rows) {

				if (!Matches(row.Name, g_EventFilter) && !Matches(row.Kind, g_EventFilter) && !Matches(row.Detail, g_EventFilter)) {
					continue;
				}

				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("%.1f", row.Time);

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row.Name.c_str());

				ImGui::TableNextColumn();
				{
					const ImVec4 colour = row.Kind == "BAIL" ? ImUtil::Colors::Error
						: row.Kind == "START" || row.Kind == "PLAN" ? ImUtil::Colors::OK
						: row.Kind == "SUB" ? ImUtil::Colors::OK
						: ImUtil::Colors::Subscript;

					ImGui::TextColored(colour, "%s", row.Kind.c_str());
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row.Detail.c_str());
			}

			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
				ImGui::SetScrollHereY(1.0f);
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}
}
