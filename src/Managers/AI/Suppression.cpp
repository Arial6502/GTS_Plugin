#include "Managers/AI/Suppression.hpp"
#include "Config/Config.hpp"

namespace {

	// Nothing below this size difference is suppressed, and the ramp is certain by 10.5x. Never below
	// 1.0: the chance curve below goes negative when the threshold does.
	float SizeThreshold() {
		return std::max(GTS::Config::AI.fSuppressSizeThreshold, 1.0f);
	}

	std::shared_mutex g_Lock;
	absl::flat_hash_set<RE::FormID> g_Planned;

	bool CanSheathe(RE::Actor* a_Actor, const RE::ActorState* a_State) {
		return a_State->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone &&
		       a_State->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
		       !a_Actor->IsInKillMove() &&
		       !a_Actor->IsOnMount();
	}
}

namespace GTS::AI {

	RE::Actor* Suppression::Target(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return nullptr;
		}

		const auto& runtime = a_Actor->GetActorRuntimeData();

		if (runtime.currentCombatTarget) {
			if (RE::Actor* direct = runtime.currentCombatTarget.get().get()) {
				return direct;
			}
		}

		if (RE::CombatController* combat = runtime.combatController; combat && combat->targetHandle) {
			return combat->targetHandle.get().get();
		}

		return nullptr;
	}

	bool Suppression::SizePolicy(RE::Actor* a_Actor) {

		if (!Config::AI.bEnableActionAI || !Config::AI.bDisableAttacks) {
			return false;
		}

		if (!a_Actor || a_Actor->IsPlayerRef() || !IsHumanoid(a_Actor)) {
			return false;
		}

		RE::Actor* target = Target(a_Actor);

		if (Config::AI.bAlwaysDisableAttacks) {
			const float scale = target
				? get_scale_difference(a_Actor, target, SizeType::VisualScale, false, false)
				: get_visual_scale(a_Actor);
			return scale >= SizeThreshold();
		}

		if (!target) {
			return false;
		}

		const float difference = get_scale_difference(a_Actor, target, SizeType::VisualScale, true, false);
		const float threshold = SizeThreshold();

		if (difference < threshold) {
			return false;
		}

		const float chance = (threshold * (difference - threshold)) / 20.0f;
		return StableRoll(a_Actor, target) < chance;
	}

	bool Suppression::Holds(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return false;
		}

		{
			std::shared_lock lock(g_Lock);
			if (g_Planned.contains(a_Actor->formID)) {
				return true;
			}
		}

		return SizePolicy(a_Actor);
	}

	void Suppression::Take(RE::FormID a_Actor) {
		std::unique_lock lock(g_Lock);
		g_Planned.insert(a_Actor);
	}

	void Suppression::Give(RE::FormID a_Actor) {
		std::unique_lock lock(g_Lock);
		g_Planned.erase(a_Actor);
	}

	void Suppression::Clear() {
		std::unique_lock lock(g_Lock);
		g_Planned.clear();
	}

	void Suppression::Enforce(RE::Actor* a_Actor) {

		if (!a_Actor || !a_Actor->Is3DLoaded() || a_Actor->IsDead()) {
			return;
		}

		RE::ActorState* state = a_Actor->AsActorState();

		if (!state || state->GetWeaponState() != RE::WEAPON_STATE::kDrawn) {
			return;
		}

		if (!CanSheathe(a_Actor, state)) {
			return;
		}

		a_Actor->DrawWeaponMagicHands(false);
	}
}
