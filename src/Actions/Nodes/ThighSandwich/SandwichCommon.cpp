#include "Actions/Nodes/ThighSandwich/SandwichCommon.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"

#include "API/Devourment.hpp"
#include "Config/Config.hpp"
#include "Magic/Effects/Common.hpp"

#include "Managers/AI/AIFunctions.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Controllers/ThighSandwichController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Audio/GoreAudio.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/CrushManager.hpp"
#include "Managers/Damage/Utils/SizeDamageUtils.hpp"
#include "Managers/GtsSizeManager.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/Rumble.hpp"

#include "Utils/Actions/ButtCrushUtils.hpp"
#include "Utils/Actions/VoreUtils.hpp"
#include "Utils/DeathReport.hpp"
#include "Utils/DifficultyUtils.hpp"
#include "Utils/Looting.hpp"

namespace {

	using namespace GTS;

	constexpr std::string_view LEG_NODES[] = {
		"NPC L Foot [Lft ]",
		"NPC L Toe0 [LToe]",
		"NPC L Calf [LClf]",
		"NPC L PreRearCalf",
		"NPC L FrontThigh",
		"NPC L RearCalf [RrClf]",
	};

	SandwichingData& Data(Actor* a_Giant) {
		return ThighSandwichController::GetSingleton().GetSandwichingData(a_Giant);
	}
}

namespace GTS::Actions::Sandwich {

	std::vector<RE::Actor*> Tinies(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return {};
		}

		std::vector<RE::Actor*> out;

		// Every loop in the legacy pair guarded this except the absorb one, which is where a null
		// slipped through into SetBeingHeld. Filtered once here so no caller has to remember.
		for (auto* tiny : Data(a_Giant).GetActors()) {
			if (tiny) {
				out.push_back(tiny);
			}
		}

		return out;
	}

	bool Empty(RE::Actor* a_Giant) {
		return Tinies(a_Giant).empty();
	}

	void Claim(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return;
		}

		for (auto* tiny : Tinies(giant)) {
			a_Ctx.Take(Slot, tiny);
		}
	}

	void Suffocate(RE::Actor* a_Giant, bool a_Enable, float a_Mult) {

		if (!a_Giant) {
			return;
		}

		Data(a_Giant).EnableSuffocate(a_Enable);
		Data(a_Giant).SetSuffocateMult(a_Mult);
	}

	void ResetData(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		Suffocate(a_Giant, false, 1.0f);
		Data(a_Giant).ReleaseAll();

		if (GetGrowthCount(a_Giant) > 0) {
			ModGrowthCount(a_Giant, 0, true);
			SetButtCrushSize(a_Giant, 0, true);
		}
	}

	float StaminaCost(RE::Actor* a_Giant, float a_Base) {

		float cost = a_Base;

		if (Runtime::HasPerk(a_Giant, Runtime::PERK.GTSPerkThighAbilities)) {
			cost *= 0.65f;
		}

		return cost * Perk_GetCostReduction(a_Giant);
	}

	void ThighDamage(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Damage) {

		if (!a_Giant || !a_Tiny || !a_Tiny->Is3DLoaded()) {
			return;
		}

		auto& sizemanager = SizeManager::GetSingleton();

		const float difference = get_scale_difference(a_Giant, a_Tiny, SizeType::VisualScale, false, false);
		const float vulnerability = 1.0f + sizemanager.GetSizeVulnerability(a_Tiny);
		const float attribute = std::clamp(SizeManager::GetSizeAttribute(a_Giant, SizeAttribute::Normal), 1.0f, 999.0f);

		float damage = a_Damage * BalanceSizeDamage(difference) * vulnerability * attribute * GetPerkBonus_Thighs(a_Giant);

		if (TinyCalamityActive(a_Giant)) {
			damage *= 1.5f;
		}

		if (CanDoDamage(a_Giant, a_Tiny, false)) {
			InflictSizeDamage(a_Giant, a_Tiny, damage);
		}

		ModSizeExperience(a_Giant, std::clamp(damage / 200.0f, 0.0f, 0.20f));

		const float hp = GetAV(a_Tiny, ActorValue::kHealth);

		if (damage > hp || hp <= 0.0f || a_Tiny->IsDead()) {

			ModSizeExperience_Crush(a_Giant, a_Tiny, true);
			CrushManager::Crush(a_Giant, a_Tiny);
			ReportDeath(a_Giant, a_Tiny, DamageSource::ThighSandwiched);
			AdvanceQuestProgression(a_Giant, a_Tiny, QuestStage::HandCrush, 1.0f, false);

			PlayCrushSound(a_Giant, find_node(a_Giant, "NPC R FrontThigh"), false, get_corrected_scale(a_Tiny));

			Data(a_Giant).Remove(a_Tiny);
			Possession::ReleaseOne(a_Giant->formID, Slot, a_Tiny->GetHandle());
		}
	}

	bool ButtDamage(RE::Actor* a_Giant, float a_Damage, bool a_AllowFinisher, float a_Mult) {

		if (!a_Giant) {
			return true;
		}

		auto& sizemanager = SizeManager::GetSingleton();

		float mult = a_Mult;
		if (TinyCalamityActive(a_Giant)) {
			mult *= 1.5f;
		}

		constexpr float Threshold = 0.15f;
		bool dealDamage = true;

		for (auto* tiny : Tinies(a_Giant)) {

			if (!tiny->Is3DLoaded()) {
				continue;
			}

			const float difference = get_scale_difference(a_Giant, tiny, SizeType::VisualScale, false, false);
			const float vulnerability = 1.0f + sizemanager.GetSizeVulnerability(tiny);
			const float attribute = std::clamp(SizeManager::GetSizeAttribute(a_Giant, SizeAttribute::Normal), 1.0f, 999.0f);

			// Computed from the argument every time round. The legacy version multiplied the same
			// variable in place, so the second tiny in a sandwich took the first tiny's damage again.
			const float damage = a_Damage * mult * BalanceSizeDamage(difference) * attribute * vulnerability * GetPerkBonus_Thighs(a_Giant);

			const float maxHealth = GetMaxAV(tiny, ActorValue::kHealth) * Threshold;
			const float health = GetAV(tiny, ActorValue::kHealth);

			if (a_AllowFinisher && (GetHealthPercentage(tiny) <= Threshold || health - (damage * GetDifficultyMultiplier(a_Giant, tiny)) <= maxHealth)) {
				dealDamage = false;
			}

			if (CanDoDamage(a_Giant, tiny, false)) {
				InflictSizeDamage(a_Giant, tiny, damage);
			}

			ModSizeExperience(a_Giant, std::clamp(damage / 200.0f, 0.0f, 0.20f));

			const float after = GetAV(tiny, ActorValue::kHealth);

			if (damage > after || after <= 0.0f || tiny->IsDead()) {

				ModSizeExperience_Crush(a_Giant, tiny, true);
				CrushManager::Crush(a_Giant, tiny);
				ReportDeath(a_Giant, tiny, DamageSource::Booty);
				AdvanceQuestProgression(a_Giant, tiny, QuestStage::Crushing, 1.0f, false);

				PlayCrushSound(a_Giant, find_node(a_Giant, "AnimObjectA"), false, get_corrected_scale(tiny));

				Data(a_Giant).Remove(tiny);
				Possession::ReleaseOne(a_Giant->formID, Slot, tiny->GetHandle());

				if (Empty(a_Giant)) {
					Sound_PlayLaughs(a_Giant, 1.0f, 0.14f, EmotionTriggerSource::Superiority, CooldownSource::Emotion_Voice_Long);
					Task_FacialEmotionTask_Smile(a_Giant, 1.15f, "Kill_Smile", 0.15f);
				}
			}
		}

		return dealDamage;
	}

	void ButtDamageOverTime(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		const std::string name = std::format("SandwichButtDOT_{}", a_Giant->formID);
		const RE::ActorHandle handle = a_Giant->GetHandle();
		const double start = Time::WorldTimeElapsed();

		TaskManager::Run(name, [=](auto&) {

			auto ptr = handle.get();
			if (!ptr) {
				return false;
			}

			auto* giant = ptr.get();

			// The grind variables are set by the animation a frame or two after the task starts, so
			// the first moments are given to them before they are believed.
			if ((Time::WorldTimeElapsed() - start) < 0.1) {
				return true;
			}

			if (!AnimationVars::Action::IsInSecondSandwichBranch(giant) || !AnimationVars::Action::IsThighGrinding(giant)) {
				return false;
			}

			// The grind stops once a tiny is low enough that the finisher should take over. Handing it
			// the kill is the whole point of the threshold, so the task ends by asking for it.
			if (!ButtDamage(giant, Damage_ThighSandwich_Butt_Grind, true)) {

				Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Superiority, CooldownSource::Emotion_Voice_Long);
				Task_FacialEmotionTask_Smile(giant, 1.15f, "Kill_Smile", 0.15f);

				ActionRegistry::Perform(giant, "Sandwich.Butt.Finisher");
				return false;
			}

			return true;
		});
	}

	namespace {

		void AbsorbOne(RE::Actor* a_Giant, RE::Actor* a_Tiny) {

			if (!a_Giant || !a_Tiny) {
				return;
			}

			const bool mute = Config::Audio.bMuteVoreDeathScreams;

			ModSizeExperience(a_Giant, 0.08f + (get_natural_scale(a_Tiny) * 0.025f));
			Vore_AdvanceQuest(a_Giant, a_Tiny, IsDragon(a_Tiny), IsGiant(a_Tiny));
			ReportDeath(a_Giant, a_Tiny, DamageSource::Vored, true);

			if (!a_Tiny->IsPlayerRef()) {
				KillActor(a_Giant, a_Tiny, mute);
				PerkHandler::UpdatePerkValues(a_Giant, PerkUpdate::Perk_LifeForceAbsorption);
			}
			else {
				InflictSizeDamage(a_Giant, a_Tiny, 900000);
				KillActor(a_Giant, a_Tiny, mute);
				TriggerScreenBlood(50);
				// The player cannot be disintegrated, so they are made invisible instead.
				a_Tiny->SetAlpha(0.0f);
			}

			DecreaseShoutCooldown(a_Giant);

			const RE::ActorHandle giantHandle = a_Giant->GetHandle();
			const RE::ActorHandle tinyHandle = a_Tiny->GetHandle();

			TaskManager::RunOnce(std::format("SandwichAbsorb_{}", a_Tiny->formID), [=](auto&) {

				auto giantPtr = giantHandle.get();
				auto tinyPtr = tinyHandle.get();

				if (!giantPtr || !tinyPtr) {
					return;
				}

				if (!tinyPtr->IsPlayerRef()) {
					Disintegrate(tinyPtr.get());
				}

				TransferInventory(tinyPtr.get(), giantPtr.get(), 1.0f, false, true, DamageSource::Vored, true);
			});
		}
	}

	void AbsorbTinies(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		for (auto* tiny : Tinies(a_Giant)) {

			SetBeingHeld(tiny, false);
			Data(a_Giant).Remove(tiny);
			Possession::ReleaseOne(a_Giant->formID, Slot, tiny->GetHandle());

			if (Devourment::Enabled() && Devourment::Swallow(a_Giant, tiny, DevourmentLocus::kUnbirth)) {

				const RE::ActorHandle giantHandle = a_Giant->GetHandle();
				const RE::ActorHandle tinyHandle = tiny->GetHandle();

				Devourment::Resolve(tiny, [giantHandle, tinyHandle] {
					auto giantPtr = giantHandle.get();
					auto tinyPtr = tinyHandle.get();
					if (giantPtr && tinyPtr) {
						AbsorbOne(giantPtr.get(), tinyPtr.get());
					}
				});

				continue;
			}

			AbsorbOne(a_Giant, tiny);
		}
	}

	void StartLegRumble(std::string_view a_Tag, RE::Actor* a_Giant, float a_Power, float a_HalfLife) {
		for (const std::string_view node : LEG_NODES) {
			Rumbling::Start(std::format("{}{}", a_Tag, node), a_Giant, a_Power, a_HalfLife, node);
		}
	}

	void StopLegRumble(std::string_view a_Tag, RE::Actor* a_Giant) {
		for (const std::string_view node : LEG_NODES) {
			Rumbling::Stop(std::format("{}{}", a_Tag, node), a_Giant);
		}
	}

	void TakeUnbirth(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Slot, BEH_UNBIRTH_T);
	}

	void OnUnbirthInserted(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 0.75f, "UB_Smile_Slight", 0.15f);
	}

	void OnUnbirthKill(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Absorption, CooldownSource::Emotion_Voice_Long);
		Task_FacialEmotionTask_Moan(giant, 1.75f, "UB_Moan", 0.15f);

		AbsorbTinies(giant);
	}

	void OnResetData(const ActionContext& a_Ctx) {
		ResetData(a_Ctx.Actor());
	}

	void DisableRune(RE::Actor* a_Giant) {

		if (!a_Giant || !Runtime::HasMagicEffect(a_Giant, Runtime::MGEF.GTSEffectThighRune)) {
			return;
		}

		if (auto* spell = Runtime::GetSpell(Runtime::SPEL.GTSSpellThighRune)) {
			RE::ActorHandle handle = a_Giant->GetHandle();
			a_Giant->AsMagicTarget()->DispelEffect(spell, handle);
		}
	}
}
