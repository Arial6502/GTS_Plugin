#include "Managers/AI/Eligibility.hpp"
#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Actions/Core/Possession.hpp"
#include "Config/Config.hpp"

namespace GTS::AI {

	bool Eligibility::Performer(RE::Actor* a_Actor) {

		if (!a_Actor || !a_Actor->Is3DLoaded() || !a_Actor->Get3D()) {
			return false;
		}

		if (a_Actor->IsPlayerRef() && !Config::Advanced.bPlayerAI) {
			return false;
		}

		if (a_Actor->IsDead() || GetAV(a_Actor, ActorValue::kHealth) <= 0.0f) {
			return false;
		}

		if (!IsFemale(a_Actor, true) || IsinRagdollState(a_Actor)) {
			return false;
		}

		if (a_Actor->AsActorState()->GetSitSleepState() != SIT_SLEEP_STATE::kNormal) {
			return false;
		}

		if (a_Actor->IsInKillMove() || a_Actor->IsOnMount()) {
			return false;
		}

		if (!IsVisible(a_Actor)) {
			return false;
		}

		const bool InCombat = a_Actor->IsInCombat() || a_Actor->GetActorRuntimeData().currentCombatTarget.get().get() != nullptr;

		if (Config::AI.bCombatOnly && !InCombat) {
			return false;
		}

		if (IsTeammate(a_Actor) || (a_Actor->IsPlayerRef() && Config::Advanced.bPlayerAI)) {
			return true;
		}

		return EffectsForEveryone(a_Actor);
	}

	bool Eligibility::Prey(RE::Actor* a_Performer, RE::Actor* a_Prey) {

		if (!a_Prey || !a_Performer || a_Prey == a_Performer) {
			return false;
		}

		if (!a_Prey->Is3DLoaded() || !a_Prey->Get3D() || IsFlying(a_Prey)) {
			return false;
		}

		if (AnimationVars::General::IsGTSBusy(a_Prey) || !IsVisible(a_Prey)) {
			return false;
		}

		if (Config::AI.bHostileOnly && !IsHostile(a_Prey, a_Performer) && !IsHostile(a_Performer, a_Prey)) {
			return false;
		}

		if (a_Prey->IsPlayerRef()) {
			return Config::AI.bAllowPlayer;
		}

		if (VoreController::GetSingleton().IsTinyInDataList(a_Prey)) {
			return false;
		}

		if (Actions::Possession::IsHeldIn(a_Prey->formID, Actions::PossessionSlot::kArms)) {
			return false;
		}

		if (IsTeammate(a_Prey)) {
			return Config::AI.bAllowFollowers;
		}

		if (a_Prey->IsEssential()) {
			return !Config::General.bProtectEssentials;
		}

		return true;
	}

	std::vector<RE::Actor*> Eligibility::Candidates(RE::Actor* a_Performer) {

		std::vector<RE::Actor*> found = {};

		if (!a_Performer) {
			return found;
		}

		// The combat target goes first so an equal-weight pick prefers who she is already fighting.
		if (RE::Actor* combat = a_Performer->GetActorRuntimeData().currentCombatTarget.get().get()) {
			if (Prey(a_Performer, combat)) {
				found.push_back(combat);
			}
		}

		for (RE::Actor* actor : find_actors()) {
			if (actor != a_Performer && !std::ranges::contains(found, actor) && Prey(a_Performer, actor)) {
				found.push_back(actor);
			}
		}

		return found;
	}
}
