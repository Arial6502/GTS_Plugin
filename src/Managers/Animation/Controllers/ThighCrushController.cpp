#include "Managers/Animation/Controllers/ThighCrushController.hpp"
#include "Managers/GTSSizeManager.hpp"
#include "Managers/HighHeel.hpp"
#include "Utils/Actions/TargetShape.hpp"

namespace {

	constexpr float MINIMUM_THIGH_DISTANCE = 58.0f;

}

namespace GTS {

	std::vector<Actor*> ThighCrushController::GetThighTargetsInFront(Actor* pred, std::size_t numberOfPrey) {

		if (!pred || !pred->GetCharController()) {
			return {};
		}

		auto preys = find_actors();
		std::erase_if(preys, [pred, this](Actor* prey) { return !this->CanThighCrush(pred, prey); });
		preys = TargetShape::InFront(pred, std::move(preys), TargetShape::ThighCrush);

		if (preys.size() > numberOfPrey) {
			preys.resize(numberOfPrey);
		}

		return preys;
	}

	float ThighCrushController::Reach(Actor* pred) {
		return (MINIMUM_THIGH_DISTANCE + HighHeelManager::GetBaseHHOffset(pred).Length()) * get_visual_scale(pred);
	}

	bool ThighCrushController::Qualifies(Actor* pred, Actor* prey) {

		if (pred == prey || prey->IsDead()) {
			return false;
		}

		if (AnimationVars::Crawl::IsCrawling(pred) || AnimationVars::General::IsTransitioning(pred) || IsBeingHeld(pred, prey)) {
			return false;
		}

		if (pred->AsActorState()->GetSitSleepState() == SIT_SLEEP_STATE::kIsSitting) { // disallow doing it when using furniture
			return false;
		}

		// 1.5 for the player, whose thigh crush only uses this to freeze tinies.
		const float minimum = pred->IsPlayerRef() ? 1.5f : Action_AI_ThighCrush;

		if (!(get_scale_difference(pred, prey, SizeType::VisualScale, false, true) > minimum)) {
			return false;
		}

		return prey->IsPlayerRef() || CanPerformActionOn(pred, prey, false);
	}

	bool ThighCrushController::CanThighCrush(Actor* pred, Actor* prey) {
		return pred != prey && TargetShape::InReach(pred, prey, Reach(pred)) && Qualifies(pred, prey);
	}
}