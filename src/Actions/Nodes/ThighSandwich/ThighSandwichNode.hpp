#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// The thigh loop: the giant sits on the rune with tinies between the thighs, and attacks by
	// closing the legs. One of two branches in the same animation group, told apart by GTS_IsButtState.
	//
	// This branch owns entry into the group and the way out of it. The butt branch is reached from
	// here and hands control back the same way, and the actors stay possessed across both.
	class ThighSandwichNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kThighSandwich; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;
		void RegisterInput() override;
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		struct State {
			// The thighs are closed on the tinies. Set by the impact, cleared when the legs open.
			bool Closed = false;

			// A heavy attack is running rather than a light one. Only changes which rumble tag is
			// stopped, but the two use different tags and stopping the wrong one leaves it running.
			bool Heavy = false;

			// The unbirth has been entered. Reachable straight from the thigh loop as well as from the
			// butt branch, so this node has to track it too.
			bool Absorbing = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
