#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class KickSwipeNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kKickSwipe; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] ExitPolicy Exit() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;

		// Aims at the chosen actor rather than binding it: this node has no possession slot to put
		// one in, the aim search decides.
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] Liveness Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// Standing swings a leg and asserts GTS_IsKicking. Sneaking and crawling swing an arm from the
		// same trigger and assert GTS_IsHandAttacking instead.
		enum class Variant : std::uint8_t {
			kKick,
			kSneakSwipe,
			kCrawlSwipe,
		};

		struct State {
			Variant Variant = Variant::kKick;
		};

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
