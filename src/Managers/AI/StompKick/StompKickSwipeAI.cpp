#include "Actions/Core/ActionRegistry.hpp"
#include "Managers/AI/StompKick/StompKickSwipeAI.hpp"
#include "Utils/Actions/AutoAim/AimAssist.hpp"
#include "Config/Config.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Utils/Actions/AutoAim/AutoAimUtils.hpp"


using namespace GTS;

namespace {

	constexpr float MINIMUM_STOMP_DISTANCE = 50.0f;
	constexpr float MINIMUM_STOMP_SCALE_RATIO = 1.5f;
	constexpr float STOMP_ANGLE = 50;
	constexpr float PI = std::numbers::pi_v<float>;

	// The heavy roll can land on the low sweep, which only exists standing. Which side each plays is
	// the entry resolver's business now, so the AI only says what kind of attack it wants.
	constexpr std::array HEAVY_KICKS = { "Kick.Heavy"sv, "Kick.LowSweep"sv };

	bool ProtectFollowers(Actor* a_Pred, Actor* a_Prey) {
		bool NPC = Config::General.bProtectFollowers;
		bool Hostile = IsHostile(a_Pred, a_Prey);

		if (!a_Prey->IsPlayerRef() && !Hostile && NPC && (IsTeammate(a_Pred)) && (IsTeammate(a_Prey))) {
			return true; // Disallow NPC's to perform stomps on followers
		}

		return false;
	}

	bool CanStompDead(Actor* a_Prey, float a_SizeDifference) {
		if (a_Prey->IsDead() && a_SizeDifference < Action_Crush) {
			return false;
		}
		return true;
	}

	bool CanStomp(Actor* a_Pred, Actor* a_Prey) {

		if (a_Pred == a_Prey) {
			return false;
		}

		if (ProtectFollowers(a_Pred, a_Prey)) {
			return false;
		}

		if (!CanPerformActionOn(a_Pred, a_Prey, false)) {
			return false;
		}

		const float PredScale = get_visual_scale(a_Pred);
		const float SizeDiff = get_scale_difference(a_Pred, a_Prey, SizeType::VisualScale, true, false);

		float bonus = 1.0f;
		if (AnimationVars::Crawl::IsCrawling(a_Pred)) {
			bonus = 2.0f; // +100% stomp distance
		}

		if (!CanStompDead(a_Prey, SizeDiff)) { // We don't want the follower to be stuck stomping corpses that can't be crushed.
			return false;
		}

		float prey_distance = (a_Pred->GetPosition() - a_Prey->GetPosition()).Length();
		if (a_Pred->IsPlayerRef() && prey_distance <= (MINIMUM_STOMP_DISTANCE * PredScale * bonus) && SizeDiff < MINIMUM_STOMP_SCALE_RATIO) {
			return false;
		}

		if (prey_distance <= (MINIMUM_STOMP_DISTANCE * PredScale * bonus) && SizeDiff > MINIMUM_STOMP_SCALE_RATIO) { // We don't want the Stomp to be too close
			return true;
		}

		return false;
	}

	// The aim, the side and whether the understomp plays are all resolved by the entry, so every one
	// of these is now the name of what the AI wants and nothing else.
	void Do_LightKick(Actor* pred) {
		Actions::ActionRegistry::Perform(pred, "Kick.Light");
	}

	void Do_HeavyKick(Actor* a_Performer) {
		Actions::ActionRegistry::Perform(a_Performer, HEAVY_KICKS.at(RandomIntWeighted(10, 10)));
	}

	void Do_LightSwipe(Actor* a_Performer) {
		Actions::ActionRegistry::Perform(a_Performer, "Swipe.Light");
	}

	void Do_HeavySwipe(Actor* a_Performer) {
		Actions::ActionRegistry::Perform(a_Performer, "Swipe.Heavy");
	}

	void Do_StrongStomp(Actor* a_Performer, Actor* a_Prey) {
		Actions::ActionRegistry::Perform(a_Performer, "Stomp.Strong");
	}

	void Do_LightStomp(Actor* a_Performer, Actor* a_Prey) {
		Utils_UpdateHighHeelBlend(a_Performer, false);
		Actions::ActionRegistry::Perform(a_Performer, "Stomp.Light");
	}

	void Do_Tramples(Actor* a_Performer, Actor* a_Prey) {
		Utils_UpdateHighHeelBlend(a_Performer, false);
		Actions::ActionRegistry::Perform(a_Performer, "Stomp.Trample");
	}
}

namespace GTS {

	std::vector<Actor*> StompKickSwipeAI_FilterList(Actor* a_Pred, const std::vector<Actor*>& a_PotentialPrey) {
		if (!a_Pred) {
			return {};
		}

		auto CharacterController = a_Pred->GetCharController();
		if (!CharacterController) {
			return {};
		}

		NiPoint3 PredPosition = a_Pred->GetPosition();
		auto PreyList = a_PotentialPrey;

		// Sort prey by distance
		std::ranges::sort(PreyList,[PredPosition](const Actor* preyA, const Actor* preyB) -> bool {
			const float DistToA = (preyA->GetPosition() - PredPosition).Length();
			const float DistToB = (preyB->GetPosition() - PredPosition).Length();
			return DistToA < DistToB;
		});

		// Filter out invalid targets
		std::erase_if(PreyList, [a_Pred](auto idxPrey) {
			return !CanStomp(a_Pred, idxPrey);
		});

		// Filter out actors not in front
		auto ActorAngle = a_Pred->data.angle.z;
		constexpr RE::NiPoint3 FWDVector{ 0.f, 1.f, 0.f };
		RE::NiPoint3 ActorFWD = RotateAngleAxis(FWDVector, -ActorAngle, { 0.f, 0.f, 1.f });

		NiPoint3 PredDir = ActorFWD;
		PredDir = PredDir / PredDir.Length();
		PreyList.erase(std::ranges::remove_if(PreyList, [PredPosition, PredDir](auto prey) {
			NiPoint3 PreyDir = prey->GetPosition() - PredPosition;
			if (PreyDir.Length() <= 1e-4) {
				return false;
			}
			PreyDir = PreyDir / PreyDir.Length();
			const float CosineTheta = PredDir.Dot(PreyDir);
			return CosineTheta <= 0; // 180 degress
		}).begin(), PreyList.end());

		// Filter out actors not in a truncated cone
		// \      x   /
		//  \  x     /
		//   \______/  <- Truncated cone
		//   | pred |  <- Based on width of pred
		//   |______|

		const float PredConeWidth = 70 * get_visual_scale(a_Pred);
		float ShiftAmmount = fabs((PredConeWidth / 2.0f) / tan(STOMP_ANGLE / 2.0f));
		const NiPoint3 ConeStart = PredPosition - PredDir * ShiftAmmount;

		PreyList.erase(std::ranges::remove_if(PreyList, [ConeStart, PredConeWidth, PredDir](auto prey) {
			NiPoint3 PreyDirection = prey->GetPosition() - ConeStart;
			if (PreyDirection.Length() <= PredConeWidth * 0.4f) {
				return false;
			}
			PreyDirection = PreyDirection / PreyDirection.Length();
			const float CosineTheta = PredDir.Dot(PreyDirection);
			return CosineTheta <= cos(STOMP_ANGLE * PI / 180.0f);
		}).begin(), PreyList.end());
		// Reduce vector size

		return GetMaxActionableTinyCount(a_Pred, PreyList);
	}



	void StompAI_Start(Actor* a_Performer, Actor* a_Prey) {

		switch (RandomIntWeighted(10,10,10)) {

			case 0: {
				Do_LightStomp(a_Performer, a_Prey);
				return;
			}
			case 1:{
				Do_StrongStomp(a_Performer, a_Prey);
				return;
			}
			case 2: {

				if (!AnimationVars::Crawl::IsCrawling(a_Performer) && !a_Performer->IsSneaking()) {
					Do_Tramples(a_Performer, a_Prey);
				}
				else {
					Do_LightStomp(a_Performer, a_Prey);
				}
				break;
			}
			default: {}
		}
	}

	void KickSwipeAI_Start(Actor* a_Performer) {

		Utils_UpdateHighHeelBlend(a_Performer, false);
		const bool Crawling = AnimationVars::Crawl::IsCrawling(a_Performer);

		switch (RandomIntWeighted(10, 10)) {

			case 0: {

				if (Crawling) {
					Do_LightSwipe(a_Performer);
				}
				else {
					Do_LightKick(a_Performer);
				}
				return;
			}
			case 1:{
				if (Crawling) {
					Do_HeavySwipe(a_Performer);
				}
				else {
					Do_HeavyKick(a_Performer);
				}
				break;
			}
			default: {}
		}
	}
}
