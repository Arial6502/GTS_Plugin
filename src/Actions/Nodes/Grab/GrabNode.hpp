#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	// Holding someone in the hand. Unlike every other node this one is mostly idle: the giant walks,
	// fights and turns while it runs, because the graph's carry loop blends locomotion rather than
	// replacing it.
	//
	// The graph splits every one of its eight actions three ways by what the giant is holding -
	// unarmed, weapon drawn, or magic - and picks between them itself. Nothing here has to know.
	//
	// Its signature is GTS_GrabbedTiny, which the DLL writes rather than the graph. That is normally
	// forbidden, and the entry's Confirm annotation is what makes up for it: the variable says the
	// hold is still on, and GTSGrab_Catch_Actor says the animation really took hold in the first place.
	class GrabNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kGrab; }

		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] Liveness Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const override;
		[[nodiscard]] std::span<const PossessionSlot> OwnedSlots() const override;
		[[nodiscard]] bool Permits(ActionId a_Id, RE::FormID a_Owner) const override;
		[[nodiscard]] bool HoldsCarried() const override { return true; }
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
			// The tiny has been moved from the hand to the breasts. Both are carried, so the hold does
			// not end, but nothing that reaches into the hand applies while they are stored.
			bool Stored = false;

			// The throw has let go. Everything after that is the giant's follow through, with nobody
			// left in the hand.
			bool Thrown = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
