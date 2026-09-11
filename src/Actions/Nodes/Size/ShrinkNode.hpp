#pragma once

#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	class ShrinkNode final : public IActionNode {

		public:
		[[nodiscard]] ActionId Id() const override { return ActionId::kShrink; }
		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] ExitPolicy Exit() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;
	};
}
