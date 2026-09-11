#pragma once

namespace GTS {

	enum class GraphVarKind : std::uint8_t {
		kNone,
		kBool,
		kInt,
		kFloat,
	};

	struct GraphVarSlot {
		RE::BSFixedString Name;

		// BSFixedString interning is case insensitive and hands back whatever spelling was interned
		// first, so the graph's own name is kept separately for display.
		std::string Display;

		GraphVarKind Kind = GraphVarKind::kNone;
		std::uint32_t Raw = 0;
		std::uint32_t Previous = 0;
		double ChangedAt = -1.0;
	};

	class GraphIntrospect {

		public:
		// An actor can carry several behaviour graphs and a name may live in any of them, so this
		// walks the whole list rather than taking the first.
		template <typename Fn>
		static std::size_t ForEachGraph(RE::Actor* a_Actor, Fn&& a_Fn) {

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

		static bool BuildSlots(RE::Actor* a_Actor, std::vector<GraphVarSlot>& a_Out);

		// Refreshes every slot and reports each change through a_OnChange(slot, oldRaw, newRaw).
		template <typename Fn>
		static std::size_t Poll(RE::Actor* a_Actor, std::vector<GraphVarSlot>& a_Slots, double a_Now, Fn&& a_OnChange) {

			if (!a_Actor || !a_Actor->Is3DLoaded()) {
				return 0;
			}

			std::size_t changed = 0;

			for (auto& slot : a_Slots) {

				std::uint32_t raw = slot.Raw;
				if (!Read(a_Actor, slot, raw)) {
					continue;
				}

				if (raw == slot.Raw) {
					continue;
				}

				a_OnChange(slot, slot.Raw, raw);

				slot.Previous = slot.Raw;
				slot.Raw = raw;
				slot.ChangedAt = a_Now;
				++changed;
			}

			return changed;
		}

		static bool Read(RE::Actor* a_Actor, const GraphVarSlot& a_Slot, std::uint32_t& a_Out);

		[[nodiscard]] static std::string Describe(const GraphVarSlot& a_Slot, std::uint32_t a_Raw);
		[[nodiscard]] static std::string_view KindName(GraphVarKind a_Kind);

		// Covers GTS_, GTSBEH_, GTSTDM_ and anything else the project spells with the prefix.
		[[nodiscard]] static bool IsGTSName(std::string_view a_Name);
	};
}
