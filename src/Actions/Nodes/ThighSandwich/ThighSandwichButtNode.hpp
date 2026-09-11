#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// The butt branch of the thigh sandwich: the giant stands up off the rune and sits back down on
	// whoever is still there. Second of the two branches in one animation group, told apart from the
	// thigh loop by GTS_IsButtState.
	//
	// It has no entry from idle. The only way in is a handoff from ThighSandwichNode, and the only
	// ordinary way out is a handoff back, so the actors it works on are already held when it starts.
	class ThighSandwichButtNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kThighSandwichButt; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		struct State {
			// The grind is running. Its damage lives in a task that ends itself when the graph stops
			// reporting the grind, so this is only what the trace and the guards read.
			bool Grinding = false;

			// The unbirth branch has been entered. It kills whoever is left, so nothing else in this
			// node should still be reaching for them.
			bool Absorbing = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
