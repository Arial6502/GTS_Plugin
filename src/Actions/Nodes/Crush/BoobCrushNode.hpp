#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Nodes/Crush/CrushCommon.hpp"

namespace GTS::Actions {

	// The crawling variant of butt crush. A node of its own rather than a variant of ButtCrushNode,
	// because the graph gives it a discriminator the machine can verify: GTS_IsCrawlButtCrush is set
	// for the intro, the loop, the quick version and the outro. It also owns state the other one has
	// nothing like - a bound victim and two long running damage tasks.
	class BoobCrushNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kBoobCrush; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::string_view AbortSignal() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const ActionDef> Actions() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		struct State {
			bool Quick = false;
			bool DOTing = false;
			bool Impacted = false;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		// The victim under the breasts. Reads the possession slot, so it stays correct across the
		// whole action instead of living in a second map.
		[[nodiscard]] static RE::Actor* Victim(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
