#pragma once

#include "Actions/Core/IMovementState.hpp"

namespace GTS::Actions {

	class CrawlMovement final : public IMovementState {

		public:
		[[nodiscard]] MovementId Id() const override { return MovementId::kCrawl; }

		[[nodiscard]] std::span<const GraphExpect> Signature() const override;
		[[nodiscard]] std::span<const EntryDef> Entries() const override;
		[[nodiscard]] std::span<const AnnotationDef> Annotations() const override;
	};
}
