#include "Managers/Animation/Controllers/HugController.hpp"

#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Actions/Core/Possession.hpp"

#include "Managers/GTSSizeManager.hpp"
#include "Utils/Actions/TargetShape.hpp"

using namespace GTS;

namespace {

	constexpr float MINIMUM_HUG_DISTANCE = 110.0f;

	bool DisallowHugs(Actor* actor) {
		bool jumping = AnimationVars::Other::IsJumping(actor);
		bool ragdolled = IsRagdolled(actor);
		bool busy = AnimationVars::General::IsGTSBusy(actor);
		return jumping || ragdolled || busy;
	}

	bool Blocked(Actor* pred, Actor* prey) {
		return pred == prey || prey->IsDead() || AnimationVars::General::IsTransitioning(pred) || IsBeingHeld(pred, prey)
			|| DisallowHugs(pred) || DisallowHugs(prey) || pred->AsActorState()->GetSitSleepState() == SIT_SLEEP_STATE::kIsSitting;
	}

	float MinimumScale(Actor* pred) {
		return Action_Hug * (TinyCalamityActive(pred) ? 0.80f : 1.0f);
	}

	void CantHugPlayerMessage(Actor* giant, Actor* tiny, float sizedifference, bool allow) {
		if (allow) {
			if (sizedifference < Action_Hug) {
				std::string message = std::format("Player is too big for hugs: x{:.2f}/{:.2f}", sizedifference, Action_Hug);
				NotifyWithSound(tiny, message);
			} else if (sizedifference > GetHugShrinkThreshold(giant)) {
				std::string message = std::format("Player is too small for hugs: x{:.2f}/{:.2f}", sizedifference, GetHugShrinkThreshold(giant));
				NotifyWithSound(tiny, message);
			}
		}
	}

	bool ShouldAllowWhenTooLarge(Actor* giant, Actor* tiny, float sizedifference, bool allow) {
		if (!giant->IsPlayerRef() && IsTeammate(giant) && sizedifference > GetHugShrinkThreshold(giant)) {
			// Disallow FOLLOWERS to hug someone when size difference is too massive
			if (tiny->IsPlayerRef()) {
				CantHugPlayerMessage(giant, tiny, sizedifference, allow);
			}
			return false;
		}
		return true;
	}

	void RecordSneakState(Actor* giant, Actor* tiny) {
		bool Crawling = AnimationVars::Crawl::IsCrawling(giant);
		bool Sneaking = giant->IsSneaking();

		AnimationVars::Tiny::SetIsBeingCrawlHugged(tiny, Crawling);
		AnimationVars::Tiny::SetIsBeingSneakHugged(tiny, Sneaking);
	}

	void Task_PerformHugs(Actor* giant, Actor* tiny) {
		std::string taskname = std::format("PerformHugs_{}_{}", giant->formID, tiny->formID);
		ActorHandle giantHandle = giant->CreateRefHandle();
		ActorHandle tinyHandle = tiny->CreateRefHandle();
		TaskManager::RunOnce(taskname, [=](auto& update){

			auto predPtr = giantHandle.get();
			auto preyPtr = tinyHandle.get();

			if (!predPtr || !preyPtr) {
				return;
			}

			auto pred = predPtr.get();
			auto prey = preyPtr.get();

			// The node drives both halves from here: the slot is what tells it who its partner is.
			Actions::Possession::Take(pred->formID, Actions::PossessionSlot::kArms, prey->GetHandle());

			if (Actions::ActionRegistry::Perform(pred, "Hug.Enter") != Actions::RequestResult::kAccepted) {
				Actions::Possession::Release(pred->formID, Actions::PossessionSlot::kArms);
			}
		});
	}
}

namespace GTS {

	void HugAnimationController::Hugs_OnCooldownMessage(Actor* giant) {
		double cooldown = GetRemainingCooldown(giant, CooldownSource::Action_Hugs);
		if (giant->IsPlayerRef()) {
			std::string message = std::format("Hugs are on a cooldown: {:.1f} sec", cooldown);
			shake_camera(giant, 0.75f, 0.35f);
			NotifyWithSound(giant, message);
		} else if (IsTeammate(giant) && !AnimationVars::General::IsGTSBusy(giant)) {
			std::string message = std::format("Follower's Hugs are on a cooldown: {:.1f} sec", cooldown);
			NotifyWithSound(giant, message);
		}
	}
	


	std::vector<Actor*> HugAnimationController::GetHugTargetsInFront(Actor* pred, std::size_t numberOfPrey) {

		if (!pred || AnimationVars::General::IsGTSBusy(pred) || !pred->GetCharController()) {
			return {};
		}

		auto preys = find_actors();
		std::erase_if(preys, [pred, this](Actor* prey) { return !this->CanHug(pred, prey); });
		preys = TargetShape::InFront(pred, std::move(preys), TargetShape::Hug);

		if (preys.size() > numberOfPrey) {
			preys.resize(numberOfPrey);
		}

		return preys;
	}

	float HugAnimationController::Reach(Actor* pred) {

		float distance = MINIMUM_HUG_DISTANCE;

		if (pred->IsSneaking()) {
			distance *= AnimationVars::Crawl::IsCrawling(pred) ? 2.35f : 1.6f;
		}

		return distance * get_visual_scale(pred);
	}

	bool HugAnimationController::Qualifies(Actor* pred, Actor* prey) {

		if (Blocked(pred, prey)) {
			return false;
		}

		const float sizedifference = get_scale_difference(pred, prey, SizeType::VisualScale, false, true);

		if (!(sizedifference > MinimumScale(pred))) {
			return false;
		}

		if (!prey->IsPlayerRef() && !CanPerformActionOn(pred, prey, true)) {
			return false;
		}

		if (!IsHuman(prey)) {
			return false;
		}

		return !(!pred->IsPlayerRef() && IsTeammate(pred) && sizedifference > GetHugShrinkThreshold(pred));
	}

	bool HugAnimationController::CanHug(Actor* pred, Actor* prey) {

		if (Blocked(pred, prey) || !TargetShape::InReach(pred, prey, Reach(pred))) {
			return false;
		}

		const float sizedifference = get_scale_difference(pred, prey, SizeType::VisualScale, false, true);
		const float minimum = MinimumScale(pred);

		if (!(sizedifference > minimum)) {
			if (pred->IsPlayerRef()) {
				std::string message = std::format("{} is too big to be hugged: x{:.2f}/{:.2f}", prey->GetDisplayFullName(), sizedifference, minimum);
				shake_camera(pred, 0.45f, 0.30f);
				NotifyWithSound(pred, message);
			} else if (prey->IsPlayerRef() && IsTeammate(pred)) {
				CantHugPlayerMessage(pred, prey, sizedifference, this->allow_message);
			}
			return false;
		}

		if (!prey->IsPlayerRef() && !CanPerformActionOn(pred, prey, true)) {
			return false;
		}

		if (!IsHuman(prey)) { // Allow hugs with humanoids only
			if (pred->IsPlayerRef()) {
				std::string message = std::format("You have no desire to hug {}", prey->GetDisplayFullName());
				NotifyWithSound(pred, message); // Just no. We don't have Creature Anims.
				shake_camera(pred, 0.45f, 0.30f);
			}
			return false;
		}

		return ShouldAllowWhenTooLarge(pred, prey, sizedifference, this->allow_message);
	}

	bool HugAnimationController::StartHug(Actor* pred, Actor* prey) {
		auto& hugging = HugAnimationController::GetSingleton();
		if (!hugging.CanHug(pred, prey)) {
			return false;
		}

		if (IsActionOnCooldown(pred, CooldownSource::Action_Hugs)) {
			HugAnimationController::Hugs_OnCooldownMessage(pred);
			return false;
		}

		if (AnimationVars::Crawl::IsCrawling(pred)) {
			if (!CanDoActionBasedOnQuestProgress(PlayerCharacter::GetSingleton(), QuestAnimationType::kOthers)) {
				// Can Crawl Hug only after quest is done
				if (pred->IsPlayerRef()) {
					NotifyWithSound(pred, "You're not experienced enough for Crawl Hugs");
				}
				return false;
			}
			DamageAV(pred, ActorValue::kMagicka, 225 * Perk_GetCostReduction(pred));
		}

		UpdateFriendlyHugs(pred, prey, false);
		RecordSneakState(pred, prey); // Helps to determine which hugs to play: normal or crawl ones
		RecordSneaking(pred); // store previous sneak value, used to fix missing footstep sounds in sneak later
		DisarmActor(pred);
		DisarmActor(prey);

		Task_PerformHugs(pred, prey); // Start hugs
		return true;
	}

	void HugAnimationController::AllowMessage(bool allow) {
		this->allow_message = allow;
	}
}