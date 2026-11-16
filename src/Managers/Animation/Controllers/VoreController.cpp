#include "Managers/Animation/Controllers/VoreController.hpp"

#include "API/Devourment.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/AttachPoint.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Nodes/Vore/VoreNode.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/AI/AIFunctions.hpp"
#include "Magic/Effects/Common.hpp"
#include "Utils/SurvivalMode.hpp"
#include "Utils/Actions/VoreUtils.hpp"
#include "Utils/Looting.hpp"
#include "Utils/Actions/TargetShape.hpp"

#include "Utils/DeathReport.hpp"

using namespace GTS;

namespace {
	constexpr float MINIMUM_VORE_DISTANCE = 95.0f;

	// The GTS side of consuming one prey. Takes both actors directly so it can also run from a
	// deferred task, after VoreData has already dropped them.
	void Vore_KillTiny(Actor* giant, Actor* tiny) {

		if (!giant || !tiny) {
			return;
		}

		SetBeingHeld(tiny, false);
		AddSMTDuration(giant, 6.0f);

		const auto& MuteVore = Config::Audio.bMuteVoreDeathScreams;

		if (!tiny->IsPlayerRef()) {
			KillActor(giant, tiny, MuteVore);
			PerkHandler::UpdatePerkValues(giant, PerkUpdate::Perk_LifeForceAbsorption);
		}
		else {
			InflictSizeDamage(giant, tiny, 900000);
			KillActor(giant, tiny, MuteVore);
			TriggerScreenBlood(50);
			tiny->SetAlpha(0.0f); // Player can't be disintegrated: simply nothing happens. So we Just make player Invisible instead.
		}

		Vore_AdvanceQuest(giant, tiny, IsDragon(tiny), IsGiant(tiny)); // Progress quest
		DecreaseShoutCooldown(giant);

		std::string taskname = std::format("VoreAbsorb_{}", tiny->formID);
		auto giantref = giant->CreateRefHandle();
		auto tinyref = tiny->CreateRefHandle();

		TaskManager::RunOnce(taskname, [=](auto& update) {
			if (!tinyref) {
				return;
			}
			if (!giantref) {
				return;
			}
			auto g = giantref.get().get();
			auto smoll = tinyref.get().get();

			if (!smoll->IsPlayerRef()) {
				Disintegrate(smoll);
			}
			TransferInventory(smoll, g, 1.0f, false, true, DamageSource::Vored, true);
		});
	}
}

namespace GTS {

	VoreData::VoreData(Actor* giant) : giant(giant? giant->CreateRefHandle() : ActorHandle()) {}

	void VoreData::AddTiny(Actor* tiny) {
		std::unique_lock lock(_lock);
		this->tinies.try_emplace(tiny->formID, tiny->CreateRefHandle());
	}

	void VoreData::Forget(Actor* tiny) {

		if (!tiny) {
			return;
		}

		std::unique_lock lock(_lock);
		this->tinies.erase(tiny->formID);

		if (auto transient = Transient::GetActorData(tiny)) {
			transient->CanBeVored = true;
		}
	}

	void VoreData::Swallow() {
		std::unique_lock lock(_lock);
		for (auto& tinyref : this->tinies | std::views::values) {
			auto tiny = tinyref.get().get();
			auto giant = this->giant.get().get();

			if (Devourment::Owns(tiny)) {
				continue;
			}

			if (giant->IsPlayerRef()) {
				if (IsLiving(tiny) && IsHuman(tiny)) {
					CallVampire();
				}
				bool Living = IsLiving(tiny);
				
				float DefaultScale = get_natural_scale(tiny);
				ModSizeExperience(giant, 0.08f + (DefaultScale*0.025f));

				SurvivalMode_AdjustHunger(this->giant.get().get(), VoreController::ReadOriginalScale(tiny) * GetSizeFromBoundingBox(tiny), Living, false);
			}

			Task_Vore_StartVoreBuff(giant, tiny, static_cast<int>(this->tinies.size()));
			ReportDeath(giant, tiny, DamageSource::Vored, false);
		}
	}
	void VoreData::KillAll() {
		std::unique_lock lock(_lock);
		for (auto& tinyref : this->tinies | std::views::values) {
			auto giantref = this->giant;
			auto tiny = tinyref.get().get();
			auto giant = giantref.get().get();

			if (!tiny || !giant) {
				continue;
			}

			if (Devourment::Enabled()) { // Hand the actor over and let Devourment digest it
				Anims_FixAnimationDesync(giant, tiny, true); // Reset anim speed override
				PushActorAway(giant, tiny, 1.0f);
				SetBetweenBreasts(tiny, false);
				SetBeingEaten(tiny, false);
				SetBeingHeld(tiny, false);

				Devourment::Resolve(tiny, [giantref, tinyref] {
					if (giantref && tinyref) {
						// The GTS digestion buff was skipped while Devourment was expected to
						// take this one, so it has to start here.
						Task_Vore_StartVoreBuff(giantref.get().get(), tinyref.get().get(), 1);
						Vore_KillTiny(giantref.get().get(), tinyref.get().get());
					}
				});

				continue;
			}

			Vore_KillTiny(giant, tiny);
		}
		this->tinies.clear();
	}

	void VoreData::AllowToBeVored(bool allow) {
		std::unique_lock lock(_lock);
		for (auto& tinyref : this->tinies | std::views::values) {
			auto tiny = tinyref.get().get();
			auto transient = Transient::GetActorData(tiny);
			if (transient) {
				transient->CanBeVored = allow;
			}
		}
	}

	bool VoreData::GetTimer() {
		return this->moantimer.ShouldRun();
	}

	void VoreData::GrabAll() {
		this->allGrabbed = true;
	}

	void VoreData::ReleaseAll() {
		this->allGrabbed = false;
	}

	std::vector<Actor*> VoreData::GetVories() {
		std::unique_lock lock(_lock);
		std::vector<Actor*> result;
		for (auto& actorref : this->tinies | std::views::values) {
			auto actor = actorref.get().get();
			result.push_back(actor);
		}
		return result;
	}

	void VoreData::Update() {
		GTS_PROFILE_SCOPE("VoreData: Update");
		if (this->giant) {
			auto giant = this->giant.get().get();
			float giantScale = get_visual_scale(giant);
			// Stick them to the AnimObjectA
			for (auto& tinyref : this->tinies | std::views::values) {
				auto tiny = tinyref.get().get();
				if (!tiny) {
					continue;
				}
				if (!giant) {
					continue;
				}

				if (this->allGrabbed && !giant->IsDead()) {
					AttachToObjectA(giant, tiny);
				}
			}
		}
	}

	void VoreController::OnMainUpdate() {
		std::unique_lock lock(_lock);
		for (auto& voreData : this->data | std::views::values) {
			voreData.Update();
		}
	}

	Actor* VoreController::GetVoreTargetInFront(Actor* pred) {
		auto preys = this->GetVoreTargetsInFront(pred, 1);
		if (!preys.empty()) {
			return preys[0];
		} else {
			return nullptr;
		}
	}

	std::vector<Actor*> VoreController::GetVoreTargetsInFront(Actor* pred, std::size_t numberOfPrey, bool WrathfulCalamity) {
		// Get vore target for actor
		if (!pred) {
			return {};
		}

		if (!pred->GetCharController()) {
			return {};
		}

		auto preys = find_actors();
		std::erase_if(preys, [pred, this](Actor* prey) { return !this->CanVore(pred, prey); });
		preys = TargetShape::InFront(pred, std::move(preys), TargetShape::Vore);

		if (numberOfPrey == 1 && !WrathfulCalamity) {
			return GetMaxActionableTinyCount(pred, preys);
		}

		// Reduce vector size
		if (preys.size() > numberOfPrey) {
			preys.resize(numberOfPrey);
		}

		return preys;
	}

	bool VoreController::CanVore(Actor* pred, Actor* prey) const {

		if (pred == prey) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(pred, QuestAnimationType::kVore)) {
			return false;
		}

		auto transient = Transient::GetActorData(prey);
		if (prey->IsDead()) {
			return false;
		}

		if (IsBeingHeld(pred, prey)) {
			return false;
		}

		if (transient) {
			if (transient->CanBeVored == false) {
				Notify("{} is already being eaten by someone else", prey->GetDisplayFullName());
				Cprint("{} is already being eaten by someone else", prey->GetDisplayFullName());
				return false;
			}
		}

		if (!TargetShape::InReach(pred, prey, Reach(pred))) {
			return false;
		}

		const float MINIMUM_VORE_SCALE = Action_Vore;
		const float sizedifference = get_scale_difference(pred, prey, SizeType::VisualScale, true, false);

		if (sizedifference < MINIMUM_VORE_SCALE) {
			if (IsInsect(prey, true) || IsBlacklisted(prey) || IsUndead(prey, true)) {
				std::string message = fmt::format("{} has no desire to eat {}", pred->GetDisplayFullName(), prey->GetDisplayFullName());
				NotifyWithSound(pred, message);
				return false;
			}
			
			if (pred->IsPlayerRef()) {
				std::string message = fmt::format("{} is too big to be eaten: x{:.2f}/{:.2f}", prey->GetDisplayFullName(), sizedifference, MINIMUM_VORE_SCALE);
				shake_camera(pred, 0.45f, 0.30f);
				NotifyWithSound(pred, message);
			} else if (this->allow_message && prey->IsPlayerRef() && IsTeammate(pred)) {
				CantVorePlayerMessage(pred, prey, sizedifference);
			}
			return false;
		}

		if (!(sizedifference > MINIMUM_VORE_SCALE) || IsFlying(prey)) {
			return false; // Disallow to vore flying dragons
		}

		if (!prey->IsPlayerRef() && !CanPerformActionOn(pred, prey, false)) {
			Notify("{} is important and shouldn't be eaten.", prey->GetDisplayFullName());
			return false;
		}

		return true;
	}

	float VoreController::Reach(Actor* pred) {
		return MINIMUM_VORE_DISTANCE * (TinyCalamityActive(pred) ? 1.75f : 1.0f) * get_visual_scale(pred);
	}

	bool VoreController::Qualifies(Actor* pred, Actor* prey) {

		if (pred == prey || prey->IsDead() || IsBeingHeld(pred, prey)) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(pred, QuestAnimationType::kVore)) {
			return false;
		}

		if (auto* transient = Transient::GetActorData(prey); transient && !transient->CanBeVored) {
			return false;
		}

		if (!(get_scale_difference(pred, prey, SizeType::VisualScale, true, false) > Action_Vore) || IsFlying(prey)) {
			return false;
		}

		return prey->IsPlayerRef() || CanPerformActionOn(pred, prey, false);
	}

	void VoreController::OnPluginReset() {
		std::unique_lock lock(_lock);
		this->data.clear();
	}

	void VoreController::OnGameActorReset(Actor* actor) {
		std::unique_lock lock(_lock);
		if (!actor) {
			logger::info("VoreController::ResetActor: actor == nullptr");
			return;
		}
		
		this->data.erase(actor->formID);
	}

	bool VoreController::StartVore(Actor* pred, Actor* prey) {
		if (!pred || !prey) return false;
		if (!CanVore(pred, prey)) {
			return false;
		}

		float pred_scale = get_visual_scale(pred);
		float prey_scale = get_visual_scale(prey);

		float sizedifference = pred_scale/prey_scale;

		float wastestamina = 45; // Drain stamina, should be 300 once tests are over
		float staminacheck = pred->AsActorValueOwner()->GetActorValue(ActorValue::kStamina);

		if (!pred->IsPlayerRef()) {
			wastestamina = 30; // Less tamina drain for non Player
		}

		if (!Runtime::HasPerkTeam(pred, Runtime::PERK.GTSPerkVoreAbility)) { // Damage stamina if we don't have perk
			if (staminacheck < wastestamina) {
				Notify("{} is too tired for vore.", pred->GetDisplayFullName());
				DamageAV(prey, ActorValue::kHealth, 3 * sizedifference);
				if (pred->IsPlayerRef()) {
					Runtime::PlaySound(Runtime::SNDR.GTSSoundFail, pred, 0.4f, 1.0f);
				}
				StaggerActor(pred, prey, 0.25f);
				return false;
			}
			DamageAV(pred, ActorValue::kStamina, wastestamina);
		}

		const bool notCrawling = pred->IsSneaking() && !AnimationVars::Crawl::IsCrawling(pred);
		const float shrinkRate = notCrawling ? 0.14f : 0.16f;
		if (TinyCalamity_ShouldShrinkFirst(pred, prey, Action_Vore, 10.2f, shrinkRate, shrinkRate)) {
			if (!notCrawling) {
				StaggerActor(pred, prey, 0.25f);
			}
			return false;
		}
		
		if (pred->IsPlayerRef()) {
			Runtime::PlaySound(Runtime::SNDR.GTSSoundFail, pred, 0.4f, 1.0f);
		}
		auto& voreData = this->GetVoreData(pred);
		voreData.AddTiny(prey);

		Actions::ActionRegistry::Perform(pred, "Vore.Enter");

		DisarmActor(pred, false);
		return true;
	}

	void VoreController::RecordOriginalScale(Actor* tiny) {
		auto Data = Transient::GetActorData(tiny);
		if (Data) {
			Data->VoreRecordedScale = std::clamp(get_visual_scale(tiny), 0.02f, 1000000.0f);
		}
	}

	float VoreController::ReadOriginalScale(Actor* tiny) {
		auto Data = Transient::GetActorData(tiny);
		if (Data) {
			return Data->VoreRecordedScale;
		}
		return 1.0f;
	}

	void VoreController::ShrinkOverTime(Actor* giant, Actor* tiny, float time_mult, float targetscale_mult) {
		if (tiny && !IsDevourmentEnabled()) {
			float Bounding_Box = GetSizeFromBoundingBox(tiny);
			float preyscale = get_visual_scale(tiny) * Bounding_Box;
			float targetScale = std::clamp(preyscale / (12.0f * Bounding_Box), 0.01f, 1000000.0f);
			targetScale *= targetscale_mult;

			float shrink_magnitude = -targetScale * time_mult; 
			shrink_magnitude *= GetAnimationSlowdown(giant) * Perk_ApplyAccelerationPerk(giant);
			shrink_magnitude *= Bounding_Box;

			ActorHandle tinyHandle = tiny->CreateRefHandle();

			std::string name = std::format("ShrinkTo_{}", tiny->formID);

			if (preyscale > targetScale) {
				GetSingleton().RecordOriginalScale(tiny); // We're shrinking the tiny which affects effectiveness of vore bonuses, this fixes it
				TaskManager::Run(name, [=](auto& progressData) {
					Actor* actor = tinyHandle.get().get();
					if (!actor) {
						return false;
					}

					float scale = get_visual_scale(actor);

					if (scale > targetScale) {
						override_actor_scale(actor, shrink_magnitude * 0.225f * TimeScale(), SizeEffectType::kNeutral);
						if (get_target_scale(actor) < targetScale) {
							set_target_scale(actor, targetScale);
							return false;
						}
						return true;
					} else {
						return false;
					}

					return false;
				});
			}
		}
	}

	// Gets the current vore data of a giant
	VoreData& VoreController::GetVoreData(Actor* giant) {
		std::unique_lock lock(_lock);
		// Create it now if not there yet
		this->data.try_emplace(giant->formID, giant);

		return this->data.at(giant->formID);
	}

	void VoreController::AllowMessage(bool allow) {
		this->allow_message = allow;
	}

	bool VoreController::IsTinyInDataList(Actor* aTiny) {

		std::unique_lock lock(_lock);

		if (!aTiny) {
			return false;
		}

		for (auto& val : data | std::views::values) {
			for (const auto& Tiny : val.GetVories()) {
				if (Tiny) {
					if (Tiny->formID == aTiny->formID) {
						return true;
					}
				}
			}
		}
		return false;
	}
}
