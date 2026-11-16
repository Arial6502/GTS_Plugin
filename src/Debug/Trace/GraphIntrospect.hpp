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

	// One playing clip and how far into it the graph was, as read on the previous frame. The pointer is
	// only compared, never followed.
	using ClipSamples = absl::InlinedVector<std::pair<const void*, float>, 8>;

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

		// What the behaviour graph is running: "SM machine=state" for each active state machine, with a
		// trailing * while it has a transition in progress, and "CLIP file" for each playing clip. Sorted.
		static void ReadActive(RE::Actor* a_Actor, std::vector<std::string>& a_Out);

		// True when any active state machine is in one of the named states. a_State may list several,
		// separated by |. Does not allocate.
		[[nodiscard]] static bool InState(RE::Actor* a_Actor, std::string_view a_State);

		// True when a state machine with one of these names is active, whatever state it is in.
		[[nodiscard]] static bool InMachine(RE::Actor* a_Actor, std::string_view a_Machine);

		// Seconds of animation the actor played since the last call with the same a_Previous, read from
		// the clips' own time. This is what a wait on an animation is measured in: it runs slow when the
		// animation is slowed and stops when it is paused.
		static float Advance(RE::Actor* a_Actor, ClipSamples& a_Previous, float a_Delta);

		[[nodiscard]] static std::string Describe(const GraphVarSlot& a_Slot, std::uint32_t a_Raw);
		[[nodiscard]] static std::string_view KindName(GraphVarKind a_Kind);

		// Covers GTS_, GTSBEH_, GTSTDM_ and anything else the project spells with the prefix.
		[[nodiscard]] static bool IsGTSName(std::string_view a_Name);
	};

	// Seconds of animation an actor has played since the first Tick. For a task that waits on one.
	struct AnimationClock {

		double Played = 0.0;
		ClipSamples Clips = {};

		double Tick(RE::Actor* a_Actor, double a_Delta) {
			Played += GraphIntrospect::Advance(a_Actor, Clips, static_cast<float>(a_Delta));
			return Played;
		}
	};
}
