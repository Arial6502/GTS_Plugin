#pragma once

#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class TrampleNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kTrample; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] ExitPolicy Exit() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;
	};
}
