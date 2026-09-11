#include "Debug/Trace/AnimationTracer.hpp"

#include "Managers/Console/ConsoleManager.hpp"

namespace {

	constexpr std::string_view LogName = "GTSPluginTrace";

	std::string TargetName(RE::Actor* a_Actor) {
		if (!a_Actor) {
			return "none";
		}
		return std::format("{:08X}:{}", a_Actor->formID, a_Actor->GetDisplayFullName());
	}

	// An actor can carry several graphs and a name may live in any of them, so everything here
	// walks the whole list rather than taking the first.
	template <typename Fn>
	std::size_t ForEachGraph(RE::Actor* a_Actor, Fn&& a_Fn) {

		if (!a_Actor || !a_Actor->Is3DLoaded()) {
			return 0;
		}

		RE::BSAnimationGraphManagerPtr manager;
		if (!a_Actor->GetAnimationGraphManager(manager) || !manager) {
			return 0;
		}

		std::size_t visited = 0;
		std::int32_t index = 0;

		for (const auto& graph : manager->graphs) {

			if (graph && graph->behaviorGraph && graph->behaviorGraph->data.get() && graph->behaviorGraph->data->stringData.get()) {
				a_Fn(index, graph->projectName, graph->behaviorGraph->data->stringData.get());
				++visited;
			}

			++index;
		}

		return visited;
	}
}

namespace GTS {

	void AnimationTracer::OnSKSEDataLoaded() {

		std::scoped_lock lock(m_Lock);

		ConsoleManager::RegisterCommand({
			.Name = "trace",
			.Desc = "Record animation triggers, annotations and graph variable changes",
			.Usage = "<start|stop|inventory> [player|target|formid]",
			.MinArgs = 1,
			.MaxArgs = 2,
			.Callback = [](const ConsoleArgs& a_Args) {

				const std::string action = a_Args.Lower(0);

				if (action == "stop") {
					Stop();
					Cprint("Trace stopped.");
					return;
				}

				RE::Actor* target = a_Args.ResolveActor(1);
				if (!target) {
					Cprint("Trace: no actor resolved.");
					return;
				}

				if (action == "start") {
					if (Start(target)) {
						Cprint("Tracing {}. Log: {}.log", TargetName(target), LogName);
					}
					else {
						Cprint("Trace: {} has no readable behaviour graph.", TargetName(target));
					}
					return;
				}

				if (action == "inventory" || action == "inv") {
					if (DumpInventory(target)) {
						Cprint("Inventory written to {}.log", LogName);
					}
					else {
						Cprint("Trace: {} has no readable behaviour graph.", TargetName(target));
					}
					return;
				}

				ConsoleManager::PrintUsage("trace");
			},
		});
	}

	bool AnimationTracer::Capturing() {

		std::scoped_lock lock(m_Lock);
		return m_Target != 0 && m_Log != nullptr;
	}

	void AnimationTracer::OpenLog() {

		std::scoped_lock lock(m_Lock);

		if (m_Log) {
			return;
		}

		auto path = SKSE::log::log_directory();
		if (!path) {
			logger::error("Trace: no log directory");
			return;
		}

		*path /= LogName;
		*path += L".log";

		try {
			m_Log = std::make_shared<spdlog::logger>("Trace", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
			m_Log->set_pattern("%v");
			m_Log->set_level(spdlog::level::trace);
			m_Log->flush_on(spdlog::level::trace);
		}
		catch (const std::exception& e) {
			logger::error("Trace: could not open {}.log: {}", LogName, e.what());
			m_Log = nullptr;
		}
	}

	void AnimationTracer::Write(std::string_view a_Line) {

		std::scoped_lock lock(m_Lock);
		if (m_Log) {
			m_Log->trace("{:>8}|{:.3f}|{}", m_Frame, Time::WorldTimeElapsed(), a_Line);
		}
	}

	bool AnimationTracer::BuildSlots(RE::Actor* a_Actor) {

		std::scoped_lock lock(m_Lock);

		m_Slots.clear();

		absl::flat_hash_set<std::string> seen = {};

		ForEachGraph(a_Actor, [&](std::int32_t, const RE::BSFixedString&, const RE::hkbBehaviorGraphStringData* a_Strings) {

			for (const auto& entry : a_Strings->variableNames) {

				if (entry.empty()) {
					continue;
				}

				if (!seen.emplace(entry.c_str()).second) {
					continue;
				}

				VarSlot slot{};
				slot.Name = RE::BSFixedString(entry.c_str());
				slot.Display = entry.c_str();

				// The getters type-check against the graph's own variable info, so probing is how the
				// kind is discovered without hkbVariableInfo being mapped. Narrowest wins.
				{
					bool b = false;
					std::int32_t i = 0;
					float f = 0.0f;

					if (a_Actor->GetGraphVariableBool(slot.Name, b)) {
						slot.Kind = VarKind::kBool;
						slot.Raw = b ? 1u : 0u;
					}
					else if (a_Actor->GetGraphVariableInt(slot.Name, i)) {
						slot.Kind = VarKind::kInt;
						slot.Raw = std::bit_cast<std::uint32_t>(i);
					}
					else if (a_Actor->GetGraphVariableFloat(slot.Name, f)) {
						slot.Kind = VarKind::kFloat;
						slot.Raw = std::bit_cast<std::uint32_t>(f);
					}
				}

				if (slot.Kind != VarKind::kNone) {
					m_Slots.push_back(std::move(slot));
				}
			}
		});

		return !m_Slots.empty();
	}

	std::string AnimationTracer::Describe(const VarSlot& a_Slot, std::uint32_t a_Raw) {

		std::scoped_lock lock(m_Lock);

		switch (a_Slot.Kind) {
			case VarKind::kBool:
			{
				return a_Raw ? "true" : "false";
			}
			case VarKind::kInt:
			{
				return std::format("{}", std::bit_cast<std::int32_t>(a_Raw));
			}
			case VarKind::kFloat:
			{
				return std::format("{:.4f}", std::bit_cast<float>(a_Raw));
			}
			default:
			{
				return "?";
			}
		}
	}

	bool AnimationTracer::Start(RE::Actor* a_Actor) {

		std::scoped_lock lock(m_Lock);

		Stop();
		OpenLog();

		if (!m_Log || !BuildSlots(a_Actor)) {
			return false;
		}

		m_Target = a_Actor->formID;
		m_Frame = 0;

		Write(std::format("START|{}|vars={}", TargetName(a_Actor), m_Slots.size()));
		return true;
	}

	void AnimationTracer::Stop() {

		std::scoped_lock lock(m_Lock);

		if (m_Target != 0) {
			Write("STOP");
		}

		m_Target = 0;
		m_Slots.clear();
	}

	bool AnimationTracer::DumpInventory(RE::Actor* a_Actor) {

		std::scoped_lock lock(m_Lock);

		OpenLog();
		if (!m_Log) {
			return false;
		}

		const std::size_t visited = ForEachGraph(a_Actor, [&](std::int32_t a_Index, const RE::BSFixedString& a_Project, const RE::hkbBehaviorGraphStringData* a_Strings) {

			const std::string project = a_Project.empty() ? "unnamed" : a_Project.c_str();

			Write(std::format("INV|BEGIN|{}|graph={}|{}", TargetName(a_Actor), a_Index, project));

			for (const auto& entry : a_Strings->variableNames) {
				if (!entry.empty()) {
					Write(std::format("INV|VAR|{}|{}", entry.c_str(), a_Index));
				}
			}

			for (const auto& entry : a_Strings->eventNames) {
				if (!entry.empty()) {
					Write(std::format("INV|EVT|{}|{}", entry.c_str(), a_Index));
				}
			}

			for (const auto& entry : a_Strings->attributeNames) {
				if (!entry.empty()) {
					Write(std::format("INV|ATTR|{}|{}", entry.c_str(), a_Index));
				}
			}

			Write(std::format("INV|END|graph={}|{}|vars={}|events={}|attrs={}", a_Index, project,
				a_Strings->variableNames.size(), a_Strings->eventNames.size(), a_Strings->attributeNames.size()));
		});

		Write(std::format("INV|GRAPHS|{}", visited));

		return visited > 0;
	}

	void AnimationTracer::PollVariables(RE::Actor* a_Actor) {

		std::scoped_lock lock(m_Lock);

		for (auto& slot : m_Slots) {

			std::uint32_t raw = slot.Raw;

			switch (slot.Kind) {
				case VarKind::kBool:
				{
					bool value = false;
					if (!a_Actor->GetGraphVariableBool(slot.Name, value)) {
						continue;
					}
					raw = value ? 1u : 0u;
					break;
				}
				case VarKind::kInt:
				{
					std::int32_t value = 0;
					if (!a_Actor->GetGraphVariableInt(slot.Name, value)) {
						continue;
					}
					raw = std::bit_cast<std::uint32_t>(value);
					break;
				}
				case VarKind::kFloat:
				{
					float value = 0.0f;
					if (!a_Actor->GetGraphVariableFloat(slot.Name, value)) {
						continue;
					}
					raw = std::bit_cast<std::uint32_t>(value);
					break;
				}
				default:
				{
					continue;
				}
			}

			if (raw != slot.Raw) {
				Write(std::format("VAR|{}|{}|{}", slot.Display, Describe(slot, slot.Raw), Describe(slot, raw)));
				slot.Raw = raw;
			}
		}
	}

	void AnimationTracer::OnMainUpdate() {

		std::scoped_lock lock(m_Lock);

		if (!Capturing()) {
			return;
		}

		++m_Frame;

		auto* target = RE::TESForm::LookupByID<RE::Actor>(m_Target);
		if (!target || !target->Is3DLoaded()) {
			Write("LOST|target unloaded");
			Stop();
			return;
		}

		PollVariables(target);
	}

	void AnimationTracer::Trigger(RE::Actor* a_Actor, std::string_view a_Trigger, std::string_view a_Behaviour, bool a_Accepted) {

		std::scoped_lock lock(m_Lock);

		if (!Capturing() || !a_Actor || a_Actor->formID != m_Target) {
			return;
		}

		Write(std::format("TRIG|{}|{}|{}", a_Trigger, a_Behaviour, a_Accepted ? "sent" : "refused"));
	}

	void AnimationTracer::Annotation(RE::Actor* a_Actor, std::string_view a_Tag, bool a_Handled) {

		std::scoped_lock lock(m_Lock);

		if (!Capturing() || !a_Actor || a_Actor->formID != m_Target) {
			return;
		}

		Write(std::format("ANNO|{}|{}", a_Tag, a_Handled ? "handled" : "unhandled"));
	}

	void AnimationTracer::OnActor3DUnload(RE::Actor* a_Actor) {

		std::scoped_lock lock(m_Lock);

		if (Capturing() && a_Actor && a_Actor->formID == m_Target) {
			Write("LOST|3d unloaded");
			Stop();
		}
	}

	void AnimationTracer::OnPluginReset() {

		std::scoped_lock lock(m_Lock);
		Stop();
	}
}
