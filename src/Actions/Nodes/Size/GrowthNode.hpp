#pragma once

#include "Actions/Core/ActorStateMap.hpp"
#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class GrowthNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kGrowth; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] ExitPolicy Exit() const override;
		[[nodiscard]] std::span<const std::string_view> ExitSignals() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnEnter(const ActionContext& a_Ctx) override;
		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;

		void OnForget(RE::FormID a_Owner) override;
		void OnReset() override;

		[[nodiscard]] std::string_view StateName(RE::FormID a_Owner) const override;

		// Which clip set is playing. Both assert GTS_IsGrowing, so the graph cannot tell them apart
		// and neither can the machine. Only the log and the annotation handlers care.
		enum class Kind : std::uint8_t {
			kManual,
			kRandom,
		};

		struct State {
			Kind Kind = Kind::kManual;
		};

		static State* Get(RE::FormID a_Owner);
		static State& GetOrAdd(RE::FormID a_Owner);

		private:
		static inline ActorStateMap<State> m_State = {};
	};
}
