#pragma once

#include "Actions/Core/IActionNode.hpp"

namespace GTS::Actions {

	#define GTS_MOVEMENT_LIST(X) \
		X(Crawl)                 \
		X(Prone)

	enum class MovementId : std::uint8_t {
		kNone = 0,
		#define GTS_MOVEMENT_ENUM(a_Name) k##a_Name,
		GTS_MOVEMENT_LIST(GTS_MOVEMENT_ENUM)
		#undef GTS_MOVEMENT_ENUM
		kTotal,
	};

	constexpr std::size_t kMovementCount = std::to_underlying(MovementId::kTotal);

	constexpr std::string_view MovementName(MovementId a_Id) {
		switch (a_Id) {
			case MovementId::kNone:
			{
				return "upright";
			}
			#define GTS_MOVEMENT_CASE(a_Name) case MovementId::k##a_Name: { return #a_Name; }
			GTS_MOVEMENT_LIST(GTS_MOVEMENT_CASE)
			#undef GTS_MOVEMENT_CASE
			default:
			{
				return "?";
			}
		}
	}

	struct ActorMovement {
		RE::ActorHandle Owner = {};

		MovementId Current = MovementId::kNone;
		MovementId Pending = MovementId::kNone;

		GraphSignature Signature = {};

		double EnteredAt = 0.0;
		double PendingSince = -1.0;
		double MismatchSince = -1.0;
		double LeavingAt = -1.0;

		std::string EnteredVia;

		[[nodiscard]] bool Active() const { return Current != MovementId::kNone; }
		[[nodiscard]] bool Waiting() const { return Pending != MovementId::kNone; }
		[[nodiscard]] bool Idle() const { return !Active() && !Waiting(); }
	};

	class IMovementState {

		public:
		virtual ~IMovementState() = default;

		[[nodiscard]] virtual MovementId Id() const = 0;
		[[nodiscard]] virtual std::span<const GraphExpect> Signature() const = 0;
		[[nodiscard]] virtual std::span<const EntryDef> Entries() const = 0;
		[[nodiscard]] virtual std::span<const AnnotationDef> Annotations() const = 0;
		[[nodiscard]] virtual std::span<const VanillaBlock> Blocks() const { return {}; }
		[[nodiscard]] virtual bool CanEnter(const EntryContext& a_Ctx) const { return true; }

		virtual void OnEnter(const ActionContext& a_Ctx) {}
		virtual void OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {}
		virtual void OnUpdate(const ActionContext& a_Ctx, float a_Delta) {}

		virtual void OnForget(RE::FormID a_Owner) {}
		virtual void OnReset() {}
	};
}
