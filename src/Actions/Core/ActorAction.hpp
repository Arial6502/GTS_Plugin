#pragma once

#include "Actions/Core/ActionTypes.hpp"
#include "Actions/Core/GraphSnapshot.hpp"

namespace GTS::Actions {

	struct ActorAction {

		RE::ActorHandle Owner = {};

		ActionId Current = ActionId::kNone;
		ActionId Pending = ActionId::kNone;
		ActionId HandOffTo = ActionId::kNone;

		double HandOffAt = -1.0;
		ActionId JustLeft = ActionId::kNone;

		// The node that stepped aside for this one, brought back when this one ends. A grab suspended
		// for a stomp is still holding its actor throughout; only the machine's idea of what is
		// running changed.
		ActionId Resumes = ActionId::kNone;
		bool PartnersReleased = false;
		double PartnersReleasedAt = -1.0;
		bool PartnerDied = false;
		bool DeathIntended = false;
		std::string_view AwaitingConfirm;

		// Separate from EnteredAt, since an action can arm a Confirm part way through a node.
		double AwaitingSince = -1.0;

		std::uint8_t PendingEntry = 0;
		GraphSignature Signature = {};

		double EnteredAt = 0.0;

		// When the actor last went idle. See ActionRegistry::SettleGrace.
		double IdleSince = -1.0;

		double PendingSince = -1.0;
		double ExitRequestedAt = -1.0;
		double MismatchSince = -1.0;
		float AnimSpeed = 1.0f;
		float HHSpeed = 1.0f;
		bool CanEditAnimSpeed = false;
		bool HHDisabled = false;
		float SpeedBase = 1.0f;
		std::string_view SpeedInput;
		std::string EnteredVia;
		absl::InlinedVector<std::pair<std::string, double>, 4> Recent;

		[[nodiscard]] bool Active() const { return Current != ActionId::kNone; }
		[[nodiscard]] bool Waiting() const { return Pending != ActionId::kNone; }
		[[nodiscard]] bool Idle() const { return !Active() && !Waiting(); }
	};
}
