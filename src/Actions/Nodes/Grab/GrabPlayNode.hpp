#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// Playing with whoever is in the hand: poking, flicking, kissing, eating them. Reached only by
	// handoff from GrabNode, and handed back the same way, so the actor is already held on entry.
	//
	// Unlike the grab itself this is a real animation state and the graph owns its variable, so the
	// signature is evidence on its own and needs no Confirm.
	class GrabPlayNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kGrabPlay; }

		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const PossessionSlot> OwnedSlots() const override;
		[[nodiscard]] Liveness Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		struct State {
			// Eating them, by mouth or by kiss. The tiny is gone by the end of it, so nothing else in
			// this node should still be reaching for them.
			bool Devouring = false;

			// The grind is running. Its own stop is the only way out of it.
			bool Grinding = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
