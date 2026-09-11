#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class ThighCrushNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kThighCrush; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;

		// Aims at the chosen actor rather than binding it: this node has no possession slot to put
		// one in, the aim search decides.
		[[nodiscard]] bool StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool a_Explain) const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// Where the animation is inside itself. The graph cannot tell these apart, so the machine
		// never sees this: it gates the attack, and it names the node in the log.
		enum class Stage : std::uint8_t {
			kSittingDown,
			kIdle,
			kLight,
			kHeavy,
			kStandingUp,
		};

		struct State {
			Stage Stage = Stage::kSittingDown;
			bool DrainingLight = false;
			bool DrainingHeavy = false;

			// The stand-up fires GTSstandR twice. Only the first carries damage.
			std::uint8_t RightSteps = 0;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
