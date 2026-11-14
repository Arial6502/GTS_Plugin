#include "Managers/Animation/Controllers/ButtCrushController.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Animation/Utils/AttachPoint.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/Crush/CrushCommon.hpp"
#include "Managers/GTSSizeManager.hpp"
#include "Managers/Rumble.hpp"
#include "Actions/Core/ActionCleanup.hpp"
#include "Managers/HighHeel.hpp"
#include "Utils/Actions/ButtCrushUtils.hpp"
#include "Utils/Actions/TargetShape.hpp"

using namespace GTS;

namespace {

	constexpr float MINIMUM_BUTTCRUSH_DISTANCE = 95.0f;

	float MinimumScale(Actor* pred) {
		return Action_ButtCrush * (AnimationVars::Crawl::IsCrawling(pred) ? 1.5f : 1.0f);
	}

	void AttachToObjectBTask(Actor* giant, Actor* tiny) {
		std::string name = std::format("ButtCrush_{}", tiny->formID);
		SetBeingEaten(tiny, true);

		// The node owns the binding now, and the slot decides which node picks the tiny up.
		Actions::Possession::Take(giant->formID,
			AnimationVars::Crawl::IsCrawling(giant) ? Actions::PossessionSlot::kBreasts : Actions::PossessionSlot::kButt,
			tiny->GetHandle());

		auto gianthandle = giant->CreateRefHandle();
		auto tinyhandle = tiny->CreateRefHandle();
		
		auto FrameA = Time::FramesElapsed();
		TaskManager::Run(name, [=](auto& progressData) {
			if (!gianthandle) {
				return false;
			}
			if (!tinyhandle) {
				return false;
			}
			auto FrameB = Time::FramesElapsed() - FrameA;
			if (FrameB <= 10.0f) {
				return true;
			}
			auto giantref = gianthandle.get().get();
			auto tinyref = tinyhandle.get().get();

			auto node = find_node(giantref, "AnimObjectB");
			if (!node) {
				return false;
			}

			if (!IsActionOnCooldown(giantref, CooldownSource::Misc_ShrinkParticle_Animation)) {
				auto node_tiny = find_node(tinyref, "NPC Root [Root]");
				if (node_tiny) {
					float rune_scale = get_visual_scale(tinyref) * GetSizeFromBoundingBox(tinyref);
					SpawnParticle(tinyref, 3.00f, "GTS/gts_tinyrune.nif", NiMatrix3(), node_tiny->world.translate, rune_scale, 7, node_tiny); 
				}
				ApplyActionCooldown(giantref, CooldownSource::Misc_ShrinkParticle_Animation);
			}

			ForceRagdoll(tinyref, false);
			float stamina = GetAV(giantref, ActorValue::kStamina);
			float Difference = std::clamp(get_scale_difference(giantref, tinyref, SizeType::VisualScale, true, false), 1.0f, 10.0f);

			
			DamageAV(giantref, ActorValue::kStamina, 0.04f * GetButtCrushCost(giantref, false));
			DamageAV(giantref, ActorValue::kMagicka, 1.4f * (GetButtCrushCost(giantref, true)) / Difference);

			ApplyActionCooldown(giantref, CooldownSource::Action_ButtCrush); // Set butt crush on the cooldown

			if (stamina <= 2.0f && !AnimationVars::Growth::IsChangingSize(giantref)) {
				Actions::ActionRegistry::Perform(giantref, "Crush.Attack");
			}

			if (GetAV(giantref, ActorValue::kHealth) <= 1.0f || giantref->IsDead()) {
				SetButtCrushSize(giantref, 0.0f, true);
				PushActorAway(giantref, giantref, 1.0f);
				PushActorAway(tinyref, tinyref, 1.0f);

				SetBeingEaten(tinyref, false);
				EnableCollisions(tiny);

				SpawnCustomParticle(giantref, ParticleType::Red, NiPoint3(), "NPC Root [Root]", 3.0f);
				SpawnParticle(giantref, 4.60f, "GTS/Effects/TinyCalamity.nif", NiMatrix3(), giantref->GetPosition(), get_visual_scale(giantref) * 4.0f, 7, nullptr);
				Runtime::PlaySoundAtNode_FallOff(Runtime::SNDR.GTSSoundTinyCalamity_Impact, giantref, 1.0f, "NPC COM [COM ]", 0.10f * get_visual_scale(giantref));
				Rumbling::Once("ButtCrushDeath", giantref, 128.0f, 0.25f, "NPC Root [Root]", 0.0f);


				return false;
			}

			auto coords = node->world.translate;
			if (!AnimationVars::Crawl::IsCrawling(giantref)) {
				float HH = HighHeelManager::GetHHOffset(giantref).Length();
				coords.z -= HH;
			} 
			if (!AnimationVars::ButtCrush::IsButtCrushing(giantref)) {
				SetBeingEaten(tinyref, false);
				EnableCollisions(tinyref);
				return false;
			}
			if (!AttachTo_NoForceRagdoll(giantref, tinyref, coords)) {
				SetBeingEaten(tinyref, false);
				EnableCollisions(tinyref);
				return false;
			}
			if (tinyref->IsDead()) {
				SetBeingEaten(tinyref, false);
				EnableCollisions(tinyref);
				return false;
			}
			return true;
		});
	}

	void CantButtCrushPlayerMessage(Actor* giant, Actor* tiny, float sizedifference) {
		if (sizedifference < Action_ButtCrush) {
			std::string message = std::format("Player is too big for Butt Crush: x{:.2f}/{:.2f}", sizedifference, Action_ButtCrush);
			NotifyWithSound(tiny, message);
		}
	}
}

namespace GTS {

	void ButtCrushController::ButtCrush_OnCooldownMessage(Actor* giant) {
		double cooldown = GetRemainingCooldown(giant, CooldownSource::Action_ButtCrush);
		std::string message;
		if (giant->IsPlayerRef()) {
			if (!AnimationVars::Crawl::IsCrawling(giant) && !giant->IsSneaking()) {
				message = std::format("Butt Crush is on a cooldown: {:.1f} sec", cooldown);
			} else if (giant->IsSneaking() && !AnimationVars::Crawl::IsCrawling(giant)) {
				message = std::format("Knee Crush is on a cooldown: {:.1f} sec", cooldown);
			} else {
				message = std::format("Breast Crush is on a cooldown: {:.1f} sec", cooldown);
			}
			shake_camera(giant, 0.45f, 0.30f);
			NotifyWithSound(giant, message);
		} else if (IsTeammate(giant) && !AnimationVars::General::IsGTSBusy(giant)) {
			if (!AnimationVars::Crawl::IsCrawling(giant) && !giant->IsSneaking()) {
				message = std::format("Follower's Butt Crush is on a cooldown: {:.1f} sec", cooldown);
			} else if (giant->IsSneaking() && !AnimationVars::Crawl::IsCrawling(giant)) {
				message = std::format("Follower's Knee Crush is on a cooldown: {:.1f} sec", cooldown);
			} else {
				message = std::format("Follower's Breast Crush is on a cooldown: {:.1f} sec", cooldown);
			}
			NotifyWithSound(giant, message);
		}
	}

	std::vector<Actor*> ButtCrushController::GetButtCrushTargets(Actor* pred, std::size_t numberOfPrey) {
		// Get vore target for actor
		auto& sizemanager = SizeManager::GetSingleton();
		if (!CanDoActionBasedOnQuestProgress(pred, QuestAnimationType::kGrabAndSandwich)) {
			return {};
		}

		if (!pred || !pred->GetCharController()) {
			return {};
		}

		auto preys = find_actors();
		std::erase_if(preys, [pred, this](Actor* prey) { return !this->CanButtCrush(pred, prey); });
		preys = TargetShape::InFront(pred, std::move(preys), TargetShape::ButtCrush);

		if (numberOfPrey == 1) {
			return GetMaxActionableTinyCount(pred, preys);
		}

		// Reduce vector size
		if (preys.size() > numberOfPrey) {
			preys.resize(numberOfPrey);
		}

		return preys;
	}

	float ButtCrushController::Reach(Actor* pred) {
		return MINIMUM_BUTTCRUSH_DISTANCE * get_visual_scale(pred);
	}

	bool ButtCrushController::Qualifies(Actor* pred, Actor* prey) {

		if (pred == prey || prey->IsDead()) {
			return false;
		}

		if (get_scale_difference(pred, prey, SizeType::VisualScale, true, false) < MinimumScale(pred) || IsFlying(prey)) {
			return false;
		}

		return prey->IsPlayerRef() || CanPerformActionOn(pred, prey, false);
	}

	bool ButtCrushController::CanButtCrush(Actor* pred, Actor* prey) const {

		if (pred == prey || prey->IsDead() || !TargetShape::InReach(pred, prey, Reach(pred))) {
			return false;
		}

		const float sizedifference = get_scale_difference(pred, prey, SizeType::VisualScale, true, false);
		const float MINIMUM_BUTTCRUSH_SCALE = MinimumScale(pred);

		if (sizedifference < MINIMUM_BUTTCRUSH_SCALE) {
			if (pred->IsPlayerRef()) {
				std::string message = fmt::format("{} is too big for Butt Crush: x{:.2f}/{:.2f}", prey->GetDisplayFullName(), sizedifference, MINIMUM_BUTTCRUSH_SCALE);
				if (!AnimationVars::Crawl::IsCrawling(pred) && pred->IsSneaking()) {
					message = fmt::format("{} is too big for Knee Crush: x{:.2f}/{:.2f}", prey->GetDisplayFullName(), sizedifference, MINIMUM_BUTTCRUSH_SCALE);
				} else if (AnimationVars::Crawl::IsCrawling(pred)) {
					message = fmt::format("{} is too big for Breast Crush: x{:.2f}/{:.2f}", prey->GetDisplayFullName(), sizedifference, MINIMUM_BUTTCRUSH_SCALE);
				} 
				shake_camera(pred, 0.45f, 0.30f);
				NotifyWithSound(pred, message);
			} else if (this->allow_message && prey->IsPlayerRef() && IsTeammate(pred)) {
				CantButtCrushPlayerMessage(pred, prey, sizedifference);
			}
			return false;
		}

		if (IsFlying(prey)) {
			return false; // Disallow to butt crush flying dragons
		}

		if (!prey->IsPlayerRef() && !CanPerformActionOn(pred, prey, false)) {
			std::string message = std::format("{} is Essential", prey->GetDisplayFullName());
			NotifyWithSound(pred, message);
			return false;
		}

		return true;
	}

	bool ButtCrushController::StartButtCrush(Actor* pred, Actor* prey, const bool dochecks, std::string_view a_Suffix) {
		auto& buttcrush = ButtCrushController::GetSingleton();

		if (dochecks) {
			if (!buttcrush.CanButtCrush(pred, prey)) {
				return false;
			}
		}

		if (CanDoButtCrush(pred, false) && !IsBeingHeld(pred, prey)) {
			if (TinyCalamity_ShouldShrinkFirst(pred, prey, Action_ButtCrush, 3.4f, 0.25f, 0.25f)) {
				return false;
			}

			// The entry's Verify claims the cooldown, so claiming it here first made the entry refuse.
			if (Actions::ActionRegistry::Perform(pred, Actions::Crush::EntryAction(pred, a_Suffix)) != Actions::RequestResult::kAccepted) {
				return false;
			}

			// Before the grab. Once the tiny is held, collision free and on AnimObjectB, its graph has
			// no state left that takes the fear transition, so this is the moment that works.
			Actions::Cleanup::Scare(prey);
			DisableCollisions(prey, pred);

			float WasteStamina = 60.0f * GetButtCrushCost(pred, false);

			AttachToObjectBTask(pred, prey);

			ActorHandle giantHandle = pred->CreateRefHandle();
			std::string taskname = std::format("ButtCrushMagicka_{}", pred->formID);
			TaskManager::RunOnce(taskname, [=](auto& update){

				if (!giantHandle) {
					return;
				}

				auto giantref = giantHandle.get().get();
				DamageAV(giantref, ActorValue::kMagicka, 180 * GetButtCrushCost(giantref, true));
				DamageAV(giantref, ActorValue::kStamina, WasteStamina);
			});

			return true;
		}

		ButtCrush_OnCooldownMessage(pred);
		return false;
	}

	void ButtCrushController::AllowMessage(bool allow) {
		this->allow_message = allow;
	}
}