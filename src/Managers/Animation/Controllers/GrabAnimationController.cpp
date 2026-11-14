#include "Actions/Nodes/Grab/GrabCommon.hpp"
#include "Managers/Animation/Controllers/GrabAnimationController.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"

#include "Managers/Animation/AnimationManager.hpp"

#include "Managers/GTSSizeManager.hpp"
#include "Utils/Actions/TargetShape.hpp"

using namespace GTS;

namespace {

	constexpr float MINIMUM_GRAB_DISTANCE = 85.0f;

	void CantGrabPlayerMessage(Actor* giant, Actor* tiny, float sizedifference) {
		if (sizedifference < Action_Grab) {
			std::string message = std::format("Player is too big to be grabbed x{:.2f}/{:.2f}", sizedifference, Action_Grab);
			NotifyWithSound(tiny, message);
		}
	}

	bool DelayedGrabTask(Actor* pred, Actor* prey) { // Needed to fix tinies becoming immune to size stuff if animation wasnt started
		if (!Actions::Grabbing::Grab(pred, prey)) {
			return false;
		}

		std::string taskname = std::format("GrabCheck_{}_{}", pred->formID, prey->formID);
		ActorHandle giantHandle = pred->CreateRefHandle();

		TaskManager::RunOnce(taskname, [=](auto& update){

			auto giantPtr = giantHandle.get();

			if (!giantPtr) {
				return;
			}

			auto* giant = giantPtr.get();

			if (!AnimationVars::General::IsGTSBusy(giant)) { // Means anim isn't applied so we cancel everything
				Actions::Grabbing::Abort(giant);
			}
		});

		return true;
	}
}

namespace GTS {

	std::vector<Actor*> GrabAnimationController::GetGrabTargetsInFront(Actor* pred, std::size_t numberOfPrey) {

		if (!pred || AnimationVars::General::IsGTSBusy(pred) || !pred->GetCharController()) {
			return {};
		}

		auto preys = find_actors();
		std::erase_if(preys, [pred, this](Actor* prey) { return !this->CanGrab(pred, prey); });
		preys = TargetShape::InFront(pred, std::move(preys), TargetShape::Grab);

		if (numberOfPrey == 1) {
			return GetMaxActionableTinyCount(pred, preys);
		}

		if (preys.size() > numberOfPrey) {
			preys.resize(numberOfPrey);
		}

		return preys;
	}

	float GrabAnimationController::Reach(Actor* pred) {
		const float mult = TinyCalamityActive(pred) || AnimationVars::Crawl::IsCrawling(pred) ? 1.6f : 1.0f;
		return MINIMUM_GRAB_DISTANCE * mult * get_visual_scale(pred);
	}

	bool GrabAnimationController::Qualifies(Actor* pred, Actor* prey) {

		if (pred == prey || prey->IsDead() || GetAV(prey, ActorValue::kHealth) < 0) {
			return false;
		}

		if (!(get_scale_difference(pred, prey, SizeType::VisualScale, true, false) > Action_Grab)) {
			return false;
		}

		if (IsFlying(prey)) {
			return false;
		}

		return prey->IsPlayerRef() || CanPerformActionOn(pred, prey, false);
	}

	bool GrabAnimationController::CanGrab(Actor* pred, Actor* prey) const {

		if (pred == prey || prey->IsDead() || GetAV(prey, ActorValue::kHealth) < 0) {
			return false;
		}

		if (!TargetShape::InReach(pred, prey, Reach(pred))) {
			return false;
		}

		const float sizedifference = get_scale_difference(pred, prey, SizeType::VisualScale, true, false);

		if (sizedifference < Action_Grab) {
			if (pred->IsPlayerRef()) {
				std::string message = std::format("{} is too big to be grabbed: x{:.2f}/{:.2f}.", prey->GetDisplayFullName(), sizedifference, Action_Grab);
				shake_camera(pred, 0.45f, 0.30f);
				NotifyWithSound(pred, message);
			} else if (this->allow_message && prey->IsPlayerRef() && IsTeammate(pred)) {
				CantGrabPlayerMessage(pred, prey, sizedifference);
			}
			return false;
		}

		return Qualifies(pred, prey);
	}

	bool GrabAnimationController::StartGrab(Actor* pred, Actor* prey) {
		auto& grabbing = GrabAnimationController::GetSingleton();
		if (!grabbing.CanGrab(pred, prey)) {
			return false;
		}

		float shrinkrate = 0.18f;

		if (pred->IsSneaking()) {
			shrinkrate = 0.13f;
		}

		if (TinyCalamity_ShouldShrinkFirst(pred, prey, Action_Grab, 10.2f, 0.18f, 0.13f)) {
			return false;
		}
	
		if (!DelayedGrabTask(pred, prey)) {
			return false;
		}

		Utils_UpdateHighHeelBlend(pred, false);
		return true;
	}

	void GrabAnimationController::AllowMessage(bool allow) {
		this->allow_message = allow;
	}
}