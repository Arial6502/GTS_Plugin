#pragma once

#include "Actions/Core/IMovementState.hpp"

namespace GTS::Actions {

	class ProneMovement final : public IMovementState {

		public:
		[[nodiscard]] MovementId Id() const override { return MovementId::kProne; }

		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;
		[[nodiscard]] std::span<const VanillaBlock> Blocks() const override;

		[[nodiscard]] bool CanEnter(const EntryContext& a_Ctx) const override;

		void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) override;
	};
}
