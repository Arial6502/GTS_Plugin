#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// Standing, sneaking and crawling hugs. One node: one trigger pair starts all three and the graph
	// picks the animation set from the stance. Sneak differs only in which behaviour the tiny is sent.
	//
	// This is the first paired node. The tiny runs its own half of the animation and fires its own
	// annotations, which reach PartnerAnnotations because the tiny has no node of its own.
	//
	// Whether the tiny is treated as a friend or a victim is decided once, at entry, and written into
	// the graph by the DLL as GTS_IsFollower on the tiny and GTS_HuggingTeammate on the giant. Neither
	// belongs in the signature: a node must not verify a variable it writes itself.
	class HugNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kHug; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const PossessionSlot> RequiredSlots() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;
		[[nodiscard]] std::span<const AnnotationDef> PartnerAnnotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnUpdate(const ActionContext& a_Ctx, float a_Delta) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;
		void RegisterInput() override;
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		enum class Variant : std::uint8_t {
			kStanding,
			kSneak,
			kCrawl,
		};

		struct State {
			Variant Variant = Variant::kStanding;

			// The friendly branch. Decided at entry and never re-decided, so a follower does not turn
			// into a victim halfway through because their faction standing shifted.
			bool Friendly = false;

			bool Healing = false;

			// The crush is running. The tiny is still alive and still held.
			bool Crushing = false;

			// GTS_Hug_CrushTiny has fired, so the tiny is dead and often has its 3D unloaded with it.
			// The giant keeps animating for several seconds after this with nothing left to hold.
			bool TinyCrushed = false;

			// The friendly release plays once. Re-sending it every frame would restart it.
			bool Released = false;

			// GTS_Hug_Release has fired, so the tiny is on the ground. The giant is still finishing its
			// half of the animation for several seconds after this.
			bool PutDown = false;

			// The hug crush suppresses the drain moans that would otherwise keep firing over it.
			bool MoansBlocked = false;
		};

		// A hit knocked the tiny loose, or something else outside the hug ended it. Says so and then
		// asks the node to cancel.
		static void Cancel(RE::Actor* a_Giant);

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		// The hugged actor, read from the possession slot rather than a second map.
		[[nodiscard]] static RE::Actor* Hugged(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
