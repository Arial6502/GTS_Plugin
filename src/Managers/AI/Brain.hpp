#pragma once

#include "Actions/Core/ActionTypes.hpp"

namespace GTS::AI {

	enum class Phase : std::uint8_t {
		kIdle,
		kApproach,
		kPosture,
		kStart,
		kEngaged,
		kRecover,
	};

	constexpr std::string_view PhaseName(Phase a_Phase) {
		switch (a_Phase) {
			case Phase::kIdle:     { return "idle"; }
			case Phase::kApproach: { return "approach"; }
			case Phase::kPosture:  { return "posture"; }
			case Phase::kStart:    { return "start"; }
			case Phase::kEngaged:  { return "engaged"; }
			case Phase::kRecover:  { return "recover"; }
			default:               { return "?"; }
		}
	}

	// What the brain has switched on. Recover undoes exactly these, so nothing is left applied when a
	// plan ends on an unusual path.
	enum Effect : std::uint8_t {
		kSteering    = 1 << 0,
		kSuppressed  = 1 << 1,
		kCombatLever = 1 << 2,
		kStance      = 1 << 3,
		kMovement    = 1 << 4,
	};

	struct Plan {
		std::uint8_t Adapter = 0xFF;
		RE::ActorHandle Target = {};
		Actions::Stance Want = Actions::Stance::kStanding;

		// The stance is rolled once per plan. Standing is a real outcome, so this says whether the roll
		// has happened rather than reading Want.
		bool Rolled = false;

		// Set once the branch has been told to wrap up, so the exit is sent once rather than every tick.
		bool Ending = false;
		bool Forced = false;
		bool Adopted = false;

		[[nodiscard]] bool Valid() const {
			return Adapter != 0xFF;
		}
	};

	struct Brain {
		Phase Stage = Phase::kIdle;
		double StageSince = 0.0;

		Plan Intent = {};
		std::uint8_t Applied = 0;

		double NextThink = 0.0;
		double NextSub = 0.0;
		double NextRetarget = 0.0;

		// Consecutive sub-action picks that found nothing to send. A branch mid animation has nothing
		// available for a moment, which is not the same as having nothing left to do.
		std::uint8_t Empty = 0;

		// How far the actor still had to walk to be in range when the approach began. The stance is
		// taken once most of that is behind her, so the change plays on the way in.
		float ApproachFrom = 0.0f;

		// (target FormID << 8 | adapter) -> retry after. Keeps a brain from hammering an option that
		// just refused it.
		absl::InlinedVector<std::pair<std::uint64_t, double>, 4> Refused = {};

		// Action literal -> retry after, for a sub-action the node just turned down.
		absl::InlinedVector<std::pair<const char*, double>, 6> SubRefused = {};
	};

	// A copy of one brain, taken on the game thread for the debug UI to draw from. Everything the
	// window shows is in here, so the render thread never touches a live actor or the brain map.
	struct BrainView {

		RE::FormID Actor = 0;
		std::string Name;

		Phase Stage = Phase::kIdle;
		float StageFor = 0.0f;

		std::string Adapter;
		RE::FormID Target = 0;
		std::string TargetName;
		bool Forced = false;
		bool Adopted = false;

		bool Rolled = false;

		// Set once the branch has been told to wrap up, so the exit is sent once rather than every tick.
		bool Ending = false;
		Actions::Stance Want = Actions::Stance::kStanding;

		std::uint8_t Applied = 0;

		float Distance = 0.0f;
		float Reach = 0.0f;
		float Remaining = 0.0f;
		float ApproachFrom = 0.0f;
		float Stalled = 0.0f;

		float NextThink = 0.0f;
		float NextSub = 0.0f;
		float NextRetarget = 0.0f;
		std::uint8_t Empty = 0;

		bool Eligible = false;
		bool Busy = false;
		bool Gated = false;
		bool Suppressed = false;
		std::string Current;

		std::vector<std::pair<std::string, float>> Refused;
		std::vector<std::pair<std::string, float>> SubRefused;
	};
}
