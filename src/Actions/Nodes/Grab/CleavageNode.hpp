#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// The cleavage state: someone stored in the breasts, and everything the giant does to them there.
	// Entered from idle whenever the breasts are occupied, or by handoff from GrabNode when the giant
	// is already holding somebody in the hand.
	//
	// It owns no slot. The tiny is kept in the breasts by the store, so they stay there when this ends,
	// and the giant can leave the state and go back into it.
	//
	// Like the grab play branch its variable is graph owned, so the signature is evidence on its own.
	class CleavageNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kCleavage; }

		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const PossessionSlot> OwnedSlots() const override;
		[[nodiscard]] std::span<const PossessionSlot> PartnerSlots() const override;
		[[nodiscard]] Liveness Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;
		void OnUpdate(const ActionContext& a_Ctx, float a_Delta) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		struct State {
			// Absorbing or eating them. Either way the tiny is gone by the end, so nothing else in the
			// node should still be reaching for them.
			bool Consuming = false;

			// The suffocation is running, which owns the state until its own kill or stop.
			bool Suffocating = false;

			// How many absorb pulses have landed. The last one stops laughing, so the kill is not
			// played for a joke.
			std::uint8_t Pulses = 0;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
