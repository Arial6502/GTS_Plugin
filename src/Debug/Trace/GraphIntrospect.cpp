#include "Debug/Trace/GraphIntrospect.hpp"

namespace {

	// hkbNodeInfo, as mapped in the Fenix and Alan CommonLib forks. This fork leaves activeNodes untyped.
	struct NodeInfo {
		std::byte Sync[0x58];
		RE::hkbNode* Clone;
		std::byte Rest[0x30];
	};
	static_assert(sizeof(NodeInfo) == 0x90);
	static_assert(offsetof(NodeInfo, Clone) == 0x58);

	constexpr std::ptrdiff_t ActiveNodes = 0x98;

	// hkbStateMachine::StateInfo, unmapped here as well.
	constexpr std::ptrdiff_t StateName = 0x60;
	constexpr std::ptrdiff_t StateId = 0x68;

	std::uintptr_t VTableOf(const void* a_Object) {
		return *static_cast<const std::uintptr_t*>(a_Object);
	}

	const char* Text(const RE::hkStringPtr& a_String) {
		const char* text = a_String.c_str();
		return text ? text : "";
	}

	bool Listed(std::string_view a_List, std::string_view a_Name) {
		return std::ranges::any_of(std::views::split(a_List, '|'), [&](auto&& a_Part) {
			return std::string_view(a_Part.begin(), a_Part.end()) == a_Name;
		});
	}

	// Calls a_Fn(machine, stateName) for every active state machine. a_Fn returns false to stop.
	// Clips go to a_Clip when it is given.
	template <typename Fn, typename ClipFn>
	void WalkActive(RE::Actor* a_Actor, Fn&& a_Fn, ClipFn&& a_Clip, bool a_Machines = true) {

		if (!a_Actor || !a_Actor->Is3DLoaded()) {
			return;
		}

		RE::BSAnimationGraphManagerPtr manager;
		if (!a_Actor->GetAnimationGraphManager(manager) || !manager) {
			return;
		}

		static const std::uintptr_t machineVtbl = REL::Relocation<std::uintptr_t>{ RE::VTABLE_hkbStateMachine[0] }.address();
		static const std::uintptr_t clipVtbl = REL::Relocation<std::uintptr_t>{ RE::VTABLE_hkbClipGenerator[0] }.address();

		// The animation update rebuilds the active list, so it is only read under the manager lock.
		const RE::BSSpinLockGuard lock(manager->GetRuntimeData().updateLock);

		for (const auto& graph : manager->graphs) {

			if (!graph || !graph->behaviorGraph) {
				continue;
			}

			const auto* nodes = *SKSE::stl::adjust_pointer<RE::hkArray<NodeInfo>*>(graph->behaviorGraph, ActiveNodes);

			if (!nodes) {
				continue;
			}

			for (const NodeInfo& info : *nodes) {

				const RE::hkbNode* node = info.Clone;

				if (!node) {
					continue;
				}

				const std::uintptr_t vtbl = VTableOf(node);

				if (vtbl == clipVtbl) {
					a_Clip(static_cast<const RE::hkbClipGenerator*>(node));
					continue;
				}

				if (vtbl != machineVtbl || !a_Machines) {
					continue;
				}

				const auto* machine = static_cast<const RE::hkbStateMachine*>(node);
				const char* state = "?";

				for (const auto* entry : machine->states) {
					if (entry && *SKSE::stl::adjust_pointer<std::int32_t>(entry, StateId) == machine->currentStateID) {
						state = Text(*SKSE::stl::adjust_pointer<RE::hkStringPtr>(entry, StateName));
						break;
					}
				}

				if (!a_Fn(machine, state)) {
					return;
				}
			}
		}
	}
}

namespace GTS {

	bool GraphIntrospect::BuildSlots(RE::Actor* a_Actor, std::vector<GraphVarSlot>& a_Out) {

		a_Out.clear();

		absl::flat_hash_set<std::string> seen = {};

		ForEachGraph(a_Actor, [&](std::int32_t, const RE::BSFixedString&, const RE::hkbBehaviorGraphStringData* a_Strings) {

			for (const auto& entry : a_Strings->variableNames) {

				if (entry.empty() || !seen.emplace(entry.c_str()).second) {
					continue;
				}

				GraphVarSlot slot{};
				slot.Name = RE::BSFixedString(entry.c_str());
				slot.Display = entry.c_str();

				// The getters type-check against the graph's own variable info, so probing is how the
				// kind is discovered without hkbVariableInfo being mapped. Narrowest wins.
				{
					bool b = false;
					std::int32_t i = 0;
					float f = 0.0f;

					if (a_Actor->GetGraphVariableBool(slot.Name, b)) {
						slot.Kind = GraphVarKind::kBool;
						slot.Raw = b ? 1u : 0u;
					}
					else if (a_Actor->GetGraphVariableInt(slot.Name, i)) {
						slot.Kind = GraphVarKind::kInt;
						slot.Raw = std::bit_cast<std::uint32_t>(i);
					}
					else if (a_Actor->GetGraphVariableFloat(slot.Name, f)) {
						slot.Kind = GraphVarKind::kFloat;
						slot.Raw = std::bit_cast<std::uint32_t>(f);
					}
				}

				if (slot.Kind != GraphVarKind::kNone) {
					slot.Previous = slot.Raw;
					a_Out.push_back(std::move(slot));
				}
			}
		});

		return !a_Out.empty();
	}

	bool GraphIntrospect::Read(RE::Actor* a_Actor, const GraphVarSlot& a_Slot, std::uint32_t& a_Out) {

		switch (a_Slot.Kind) {
			case GraphVarKind::kBool:
			{
				bool value = false;
				if (!a_Actor->GetGraphVariableBool(a_Slot.Name, value)) {
					return false;
				}
				a_Out = value ? 1u : 0u;
				return true;
			}
			case GraphVarKind::kInt:
			{
				std::int32_t value = 0;
				if (!a_Actor->GetGraphVariableInt(a_Slot.Name, value)) {
					return false;
				}
				a_Out = std::bit_cast<std::uint32_t>(value);
				return true;
			}
			case GraphVarKind::kFloat:
			{
				float value = 0.0f;
				if (!a_Actor->GetGraphVariableFloat(a_Slot.Name, value)) {
					return false;
				}
				a_Out = std::bit_cast<std::uint32_t>(value);
				return true;
			}
			default:
			{
				return false;
			}
		}
	}

	std::string GraphIntrospect::Describe(const GraphVarSlot& a_Slot, std::uint32_t a_Raw) {

		switch (a_Slot.Kind) {
			case GraphVarKind::kBool:
			{
				return a_Raw ? "true" : "false";
			}
			case GraphVarKind::kInt:
			{
				return std::format("{}", std::bit_cast<std::int32_t>(a_Raw));
			}
			case GraphVarKind::kFloat:
			{
				return std::format("{:.4f}", std::bit_cast<float>(a_Raw));
			}
			default:
			{
				return "?";
			}
		}
	}

	std::string_view GraphIntrospect::KindName(GraphVarKind a_Kind) {

		switch (a_Kind) {
			case GraphVarKind::kBool:  { return "bool"; }
			case GraphVarKind::kInt:   { return "int"; }
			case GraphVarKind::kFloat: { return "float"; }
			default:                   { return "?"; }
		}
	}

	void GraphIntrospect::ReadActive(RE::Actor* a_Actor, std::vector<std::string>& a_Out) {

		a_Out.clear();

		WalkActive(a_Actor,
			[&](const RE::hkbStateMachine* a_Machine, const char* a_State) {
				a_Out.emplace_back(std::format("SM {}={}{}", Text(a_Machine->name), a_State, a_Machine->activeTransitions.empty() ? "" : "*"));
				return true;
			},
			[&](const RE::hkbClipGenerator* a_Clip) {
				a_Out.emplace_back(std::format("CLIP {}", Text(a_Clip->animationName)));
			});

		std::ranges::sort(a_Out);
		a_Out.erase(std::ranges::unique(a_Out).begin(), a_Out.end());
	}

	bool GraphIntrospect::InState(RE::Actor* a_Actor, std::string_view a_State) {

		bool found = false;

		WalkActive(a_Actor,
			[&](const RE::hkbStateMachine*, const char* a_Name) {
				found = Listed(a_State, a_Name);
				return !found;
			},
			[](const RE::hkbClipGenerator*) {});

		return found;
	}

	bool GraphIntrospect::InMachine(RE::Actor* a_Actor, std::string_view a_Machine) {

		bool found = false;

		WalkActive(a_Actor,
			[&](const RE::hkbStateMachine* a_Node, const char*) {
				found = Listed(a_Machine, Text(a_Node->name));
				return !found;
			},
			[](const RE::hkbClipGenerator*) {});

		return found;
	}

	float GraphIntrospect::Advance(RE::Actor* a_Actor, ClipSamples& a_Previous, float a_Delta) {

		ClipSamples current = {};
		float best = 0.0f;
		bool compared = false;

		WalkActive(a_Actor,
			[](const RE::hkbStateMachine*, const char*) { return true; },
			[&](const RE::hkbClipGenerator* a_Clip) {

				current.emplace_back(a_Clip, a_Clip->localTime);

				for (const auto& [clip, time] : a_Previous) {

					// A looping clip wraps to a smaller time. That frame is skipped for that clip.
					if (clip == a_Clip && a_Clip->localTime >= time) {
						best = std::max(best, a_Clip->localTime - time);
						compared = true;
					}
				}
			},
			false);

		a_Previous = std::move(current);

		// Nothing played in both frames: no graph, or every clip is new. The frame time stands in so a
		// wait still ends for an actor that has no animation to measure.
		return compared ? std::min(best, 1.0f) : a_Delta;
	}

	bool GraphIntrospect::IsGTSName(std::string_view a_Name) {

		if (a_Name.size() < 3) {
			return false;
		}

		return (a_Name[0] == 'G' || a_Name[0] == 'g')
			&& (a_Name[1] == 'T' || a_Name[1] == 't')
			&& (a_Name[2] == 'S' || a_Name[2] == 's');
	}
}
