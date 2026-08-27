#include "Managers/AttackManager.hpp"
#include "Managers/AI/CombatSteering.hpp"

#include "Config/Config.hpp"

using namespace GTS;

namespace {

	constexpr float kSizeThreshold = 2.5f;

	bool CanTakeSheatheRequest(Actor* a_Actor, const ActorState* a_State) {
		return a_State->GetAttackState() == ATTACK_STATE_ENUM::kNone &&
		       a_State->GetSitSleepState() == SIT_SLEEP_STATE::kNormal &&
		       !a_Actor->IsInKillMove() &&
		       !a_Actor->IsOnMount();
	}
}

namespace GTS {

	Actor* AttackManager::SuppressionTarget(Actor* a_Actor) {

		if (!a_Actor) {
			return nullptr;
		}

		const auto& Runtime = a_Actor->GetActorRuntimeData();

		if (Runtime.currentCombatTarget) {
			if (Actor* Direct = Runtime.currentCombatTarget.get().get()) {
				return Direct;
			}
		}

		if (RE::CombatController* Combat = Runtime.combatController; Combat && Combat->targetHandle) {
			return Combat->targetHandle.get().get();
		}

		return nullptr;
	}

	bool AttackManager::ShouldSuppressAttacks(Actor* a_Actor) {

		if (!Config::AI.bEnableActionAI || !Config::AI.bDisableAttacks) {
			return false;
		}

		if (!a_Actor || a_Actor->IsPlayerRef() || !IsHumanoid(a_Actor)) {
			return false;
		}

		Actor* Target = SuppressionTarget(a_Actor);

		if (Config::AI.bAlwaysDisableAttacks) {
			const float Scale = Target
				? get_scale_difference(a_Actor, Target, SizeType::VisualScale, false, false)
				: get_visual_scale(a_Actor);
			return Scale >= kSizeThreshold;
		}

		if (!Target) {
			return false;
		}

		const float SizeDiff = get_scale_difference(a_Actor, Target, SizeType::VisualScale, true, false);
		if (SizeDiff < kSizeThreshold) {
			return false;
		}

		//Same curve the old roll used: nothing at the threshold, certain by 10.5x.
		const float Chance = (kSizeThreshold * (SizeDiff - kSizeThreshold)) / 20.0f;
		return StableRoll(a_Actor, Target) < Chance;
	}

	void AttackManager::OnActor3DUnload(Actor* a_Actor) {
		CombatSteering::Release(a_Actor);
	}

	void AttackManager::OnSerdeRevert(SKSE::SerializationInterface*) {
		CombatSteering::Clear();
	}

	
	void AttackManager::OnActorUpdate(Actor* a_Actor) {

		if (!a_Actor) {
			return;
		}

		if (!a_Actor->Is3DLoaded() || a_Actor->IsDead()) {
			return;
		}

		ActorState* State = a_Actor->AsActorState();
		if (!State) {
			return;
		}

		if (const bool Suppress = ShouldSuppressAttacks(a_Actor); Suppress) {
			CombatSteering::Apply(a_Actor, SuppressionTarget(a_Actor));
		}
		else {
			CombatSteering::Release(a_Actor);
		}

		if (State->GetWeaponState() != WEAPON_STATE::kDrawn) {
			return;
		}

		if (!CanTakeSheatheRequest(a_Actor, State)) {
			return;
		}

		if (!ShouldSuppressAttacks(a_Actor)) {
			return;
		}

		a_Actor->DrawWeaponMagicHands(false);
	}
}
