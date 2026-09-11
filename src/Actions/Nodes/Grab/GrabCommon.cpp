#include "Actions/Nodes/Grab/GrabCommon.hpp"
#include "Utils/DifficultyUtils.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/AI/AIFunctions.hpp"

#include "API/Devourment.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"

#include "Config/Config.hpp"
#include "Magic/Effects/Common.hpp"

#include "Actions/Nodes/Grab/GrabAttach.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Utils/Actions/VoreUtils.hpp"
#include "Managers/Damage/TinyCalamity.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Damage/SizeHitEffects.hpp"
#include "Managers/CrushManager.hpp"
#include "Managers/Damage/Utils/SizeDamageUtils.hpp"
#include "Managers/GtsSizeManager.hpp"
#include "Managers/Rumble.hpp"

#include "Utils/DeathReport.hpp"

namespace {

	using namespace GTS;

	constexpr std::string_view LHAND = "NPC L Hand [LHnd]";

	constexpr std::string_view RHAND_NODES[] = {
		"NPC R UpperarmTwist1 [RUt1]",
		"NPC R UpperarmTwist2 [RUt2]",
		"NPC R Forearm [RLar]",
		"NPC R ForearmTwist2 [RLt2]",
		"NPC R ForearmTwist1 [RLt1]",
		"NPC R Hand [RHnd]",
	};

	constexpr std::string_view LHAND_NODES[] = {
		"NPC L UpperarmTwist1 [LUt1]",
		"NPC L UpperarmTwist2 [LUt2]",
		"NPC L Forearm [LLar]",
		"NPC L ForearmTwist2 [LLt2]",
		"NPC L ForearmTwist1 [LLt1]",
		"NPC L Hand [LHnd]",
	};

	// One task per carried actor. The hand and the breasts are held at the same time, and a single
	// task per giant could only ever attach one of them.
	std::string CarryTask(RE::FormID a_Giant, RE::FormID a_Tiny) {
		return std::format("GrabCarry_{}_{}", a_Giant, a_Tiny);
	}

	// How long after the tiny's 3d comes back before the carry loop trusts what it reads off them.
	constexpr double CarrySettle = 0.5;

	// How long a carried tiny may stay without 3d before the hold is given up.
	constexpr double CarryGiveUp = 10.0;

	// Shared with the carry task, which cannot hold state of its own: std::function calls it const.
	struct CarryWait {
		double AwaySince = -1.0;
		double SettleUntil = -1.0;
		RE::FormID Cell = 0;
	};

	RE::FormID CellOf(RE::Actor* a_Actor) {
		auto* cell = a_Actor->GetParentCell();
		return cell ? cell->GetFormID() : 0;
	}

	NiMatrix3 PitchMatrix(float a_Degrees) {

		const float a = a_Degrees * (std::numbers::pi_v<float> / 180.0f);
		const float c = std::cos(a);
		const float s = std::sin(a);

		NiMatrix3 m;
		m.entry[0][0] = 1.f; m.entry[0][1] = 0.f; m.entry[0][2] = 0.f;
		m.entry[1][0] = 0.f; m.entry[1][1] = c;   m.entry[1][2] = s;
		m.entry[2][0] = 0.f; m.entry[2][1] = -s;  m.entry[2][2] = c;
		return m;
	}

	NiMatrix3 YawMatrix(float a_Degrees) {

		const float a = a_Degrees * (std::numbers::pi_v<float> / 180.0f);
		const float c = std::cos(a);
		const float s = std::sin(a);

		NiMatrix3 m;
		m.entry[0][0] = c;   m.entry[0][1] = 0.f; m.entry[0][2] = s;
		m.entry[1][0] = 0.f; m.entry[1][1] = 1.f; m.entry[1][2] = 0.f;
		m.entry[2][0] = -s;  m.entry[2][1] = 0.f; m.entry[2][2] = c;
		return m;
	}
}

namespace GTS::Actions::Grabbing {

	void StartHandRumble(std::string_view a_Tag, RE::Actor* a_Giant, float a_Power, float a_HalfLife, bool a_Left) {
		for (const std::string_view node : a_Left ? std::span<const std::string_view>(LHAND_NODES) : std::span<const std::string_view>(RHAND_NODES)) {
			Rumbling::Start(std::format("{}{}", a_Tag, node), a_Giant, a_Power, a_HalfLife, node);
		}
	}

	void StopHandRumble(std::string_view a_Tag, RE::Actor* a_Giant, bool a_Left) {
		for (const std::string_view node : a_Left ? std::span<const std::string_view>(LHAND_NODES) : std::span<const std::string_view>(RHAND_NODES)) {
			Rumbling::Stop(std::format("{}{}", a_Tag, node), a_Giant);
		}
	}

	void StartCarry(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Settle) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		const std::string name = CarryTask(a_Giant->formID, a_Tiny->formID);
		const RE::ActorHandle giantHandle = a_Giant->GetHandle();
		const RE::ActorHandle tinyHandle = a_Tiny->GetHandle();
		const auto wait = std::make_shared<CarryWait>();
		wait->Cell = CellOf(a_Giant);

		if (a_Settle > 0.0f) {
			wait->SettleUntil = Time::WorldTimeElapsed() + a_Settle;
		}

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = giantHandle.get();
			auto tinyPtr = tinyHandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			auto* tiny = tinyPtr.get();

			// The store decides who is being carried. Once it has let go - the tiny died and was
			// dropped, or the branch handed them on - there is nothing left to carry and the task is
			// done. Without this the lambda falls through to FailSafeAbort, which aborts the node
			// while it is still playing the ending for that same death.
			if (Possession::HolderOf(tiny->formID) != giant->formID) {
				return false;
			}

			// The action killed them and the store kept the body so the rest of the animation still has
			// it. There is nothing left to carry, and running on would reach FailSafeAbort, which tears
			// down the node while it is playing the ending for that same kill.
			if (Possession::Intended(tiny->formID) && (tiny->IsDead() || GetAV(tiny, ActorValue::kHealth) <= 0.0f)) {
				return false;
			}

			// Taken out of the world rather than moved between cells. The store drops them on its own
			// sweep; this is what stops the wait below treating it as a transition and reattaching an
			// actor somebody has just disabled.
			if (IsActorLost(giant) || IsActorLost(tiny)) {
				return false;
			}

			ReattachTiny(giant, tiny);

			const double now = Time::WorldTimeElapsed();

			// Fast travel moves the tiny along with the giant, so their 3d never goes away and neither
			// of the checks below notices anything happened. The cell changing is what says a transition
			// took place, and everything read off both actors is stale until it has bedded in.
			if (const RE::FormID cell = CellOf(giant); cell != wait->Cell) {
				wait->Cell = cell;
				wait->SettleUntil = now + CarrySettle;
			}

			// A tiny whose cell has unloaded is still carried, and reattaching is what puts them back
			// in the hand once it returns. Nothing about them can be measured while that is happening:
			// scale comes off a bounding box that is not there and the graph variables read as their
			// defaults, which HandleGrabLogic takes for the tiny being too small or having escaped and
			// ends the grab on.
			if (!tiny->Is3DLoaded() || IsCurrentlyReattaching(giant)) {

				if (wait->AwaySince < 0.0) {
					wait->AwaySince = now;
				}

				wait->SettleUntil = now + CarrySettle;

				if ((now - wait->AwaySince) > CarryGiveUp) {
					return FailSafeAbort(giant, tiny);
				}

				return true;
			}

			wait->AwaySince = -1.0;

			// Back, but only just. The 3d is rebuilt a frame before the data read off it is.
			if (now < wait->SettleUntil) {
				return true;
			}

			return HandleGrabLogic(giant, tiny, giantHandle, tinyHandle);
		});

		TaskManager::ChangeUpdate(name, UpdateKind::PostPhysics);
	}

	void StopCarry(RE::FormID a_Giant, RE::FormID a_Tiny) {
		TaskManager::Cancel(CarryTask(a_Giant, a_Tiny));
	}

	void StopCarry(RE::FormID a_Giant, PossessionSlot a_Slot) {
		for (const auto& handle : Possession::All(a_Giant, a_Slot)) {
			if (auto ptr = handle.get()) {
				StopCarry(a_Giant, ptr->formID);
			}
		}
	}

	void StopCarry(RE::FormID a_Giant) {
		StopCarry(a_Giant, PossessionSlot::kHand);
		StopCarry(a_Giant, PossessionSlot::kBreasts);
	}

	void BeginHold(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Settle) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		SetBeingHeld(a_Tiny, true);
		DisableCollisions(a_Tiny, a_Giant);
		StartCarry(a_Giant, a_Tiny, a_Settle);
	}

	void BeginBreastHold(RE::Actor* a_Giant, RE::Actor* a_Tiny) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		SetBetweenBreasts(a_Tiny, true);
		Task_RotateActorToBreastX(a_Giant, a_Tiny);
		ActionRegistry::Notify(a_Tiny, IsHostile(a_Giant, a_Tiny) ? "GTSBEH_T_Storage_Enemy" : "GTSBEH_T_Storage_Ally");
	}

	void HandEmptied(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		// The hand's carry only. A tiny in the breasts is held by the store rather than by whatever
		// just let go of the hand, and cancelling their task drops them on the floor.
		StopCarry(a_Giant->formID, PossessionSlot::kHand);
		ManageCamera(a_Giant, false, CameraTracking::Grab_Left);
	}

	void LetGo(RE::Actor* a_Giant, RE::Actor* a_Tiny, bool a_Push) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		SetBetweenBreasts(a_Tiny, false);
		SetBeingHeld(a_Tiny, false);
		EnableCollisions(a_Tiny);
		Anims_FixAnimationDesync(a_Giant, a_Tiny, true);

		if (a_Push && (IsHostile(a_Tiny, a_Giant) || IsHostile(a_Giant, a_Tiny))) {
			PushActorAway(a_Giant, a_Tiny, 1.0f);
		}
	}

	// The breast flag is cleared a beat late on purpose: clearing it the same frame the actor dies
	// leaves them snapping out of the cleavage before the death has been seen.
	void DelayedBreastRelease(RE::Actor* a_Tiny) {

		if (!a_Tiny) {
			return;
		}

		const RE::ActorHandle handle = a_Tiny->GetHandle();
		const double start = Time::WorldTimeElapsed();

		TaskManager::RunFor(std::format("GrabBreastRelease_{}", a_Tiny->formID), 1.0f, [=](auto&) {

			auto ptr = handle.get();

			if (!ptr) {
				return false;
			}

			if ((Time::WorldTimeElapsed() - start) > 0.50) {
				SetBetweenBreasts(ptr.get(), false);
				return false;
			}

			return true;
		});
	}

	void CrushTask(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Bonus, bool a_Sound, bool a_Stagger, DamageSource a_Source, QuestStage a_Stage) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		const RE::ActorHandle giantHandle = a_Giant->GetHandle();
		const RE::ActorHandle tinyHandle = a_Tiny->GetHandle();

		TaskManager::RunOnce(std::format("GrabCrush_{}", a_Tiny->formID), [=](const OneshotUpdate&) {

			auto giantPtr = giantHandle.get();
			auto tinyPtr = tinyHandle.get();

			if (!giantPtr || !tinyPtr) {
				return;
			}

			auto* giant = giantPtr.get();
			auto* tiny = tinyPtr.get();

			const bool killed = tiny->Is3DLoaded() && (GetAV(tiny, ActorValue::kHealth) <= 1.0f || tiny->IsDead());

			if (killed) {

				ModSizeExperience_Crush(giant, tiny, false);
				CrushManager::Crush(giant, tiny);
				DelayedBreastRelease(tiny);
				SetBeingHeld(tiny, false);

				Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundCrushDefault, giant, 1.0f, LHAND);

				if (a_Sound) {
					if (!Config::General.bLessGore) {
						Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundCrunchImpact, giant, 1.0f, LHAND);
						Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundCrunchImpact, giant, 1.0f, LHAND);
					}
					else {
						Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundSoftHandAttack, giant, 1.0f, LHAND);
					}
				}

				AdjustSizeReserve(giant, get_visual_scale(tiny) / 10);
				SpawnHurtParticles(giant, tiny, 3.0f, 1.6f);
				SpawnHurtParticles(giant, tiny, 3.0f, 1.6f);

				tiny->StartCombat(giant);
				AdvanceQuestProgression(giant, tiny, a_Stage, 1.0f, false);
				ReportDeath(giant, tiny, a_Source);
				return;
			}

			if (a_Sound) {
				if (!Config::General.bLessGore) {
					Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundCrunchImpact, giant, 1.0f, LHAND);
					SpawnHurtParticles(giant, tiny, 1.0f, 1.0f);
				}
				else {
					Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundSoftHandAttack, giant, 1.0f, LHAND);
				}
			}

			if (a_Stagger) {
				StaggerActor(giant, tiny, 0.75f);
			}
		});
	}

	void ThrowActor(const RE::ActorHandle& a_Giant, const RE::ActorHandle& a_Tiny, NiPoint3 a_Start, NiPoint3 a_End, std::string_view a_Task, float a_Speed, float a_Pitch, float a_Yaw) {

		const double start = Time::WorldTimeElapsed();

		TaskManager::Run(a_Task, [=](auto&) {

			auto giantPtr = a_Giant.get();
			auto tinyPtr = a_Tiny.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			auto* tiny = tinyPtr.get();

			if (!giant->Is3DLoaded() || !giant->GetCurrent3D() || !tiny->Is3DLoaded() || !tiny->GetCurrent3D()) {
				return true;
			}

			// The throw's power comes from how far the hand travelled while the animation played, so
			// it needs a moment of that movement before it can be measured.
			const double now = Time::WorldTimeElapsed();

			if ((now - start) <= 0.05) {
				return true;
			}

			const NiPoint3 travel = a_End - a_Start;
			const float speed = static_cast<float>((travel.Length() / (now - start)) * 10);

			const NiPoint3 forward = NiPoint3(0.0f, 0.0f, 1.0f);
			NiPoint3 direction = giant->GetCurrent3D()->world.rotate * (YawMatrix(a_Yaw) * (PitchMatrix(-a_Pitch + 90.0f) * forward));

			if (const float length = direction.Length(); length > 0.0f) {
				direction.x /= length;
				direction.y /= length;
				direction.z /= length;
			}

			ApplyManualHavokImpulse(tiny, direction.x, direction.y, direction.z, speed * a_Speed * (1.0f / Time::GGTM()));
			return false;
		});
	}
}

// Moved wholesale from Grab_Play_Events, which is going out of the build. These are the grab play
// branch's own helpers and nothing else calls them.
namespace GTS::Actions::Grabbing::Play {

	void FixKissVoreTinyOffset(Actor* giant, bool Fix) {
		auto Transient = Transient::GetActorData(giant);
		if (Transient) {
			Transient->KissVoring = Fix; // Fix slight .Z offset when kiss voring, else Tiny will be below mouth
		}
	}

	void ResetTinyAnimSpeed(Actor* giant) {
		auto Tiny = Possession::FirstActor(giant->formID, Hand);
		if (Tiny) {
			TaskManager::Cancel(std::format("CopyAnimSpeed_{}", Tiny->formID));
			Anims_FixAnimationDesync(giant, Tiny, true);
		}
	}

	void Task_CopyAnimationSpeed(Actor* giant, Actor* tiny) {
		auto gianthandle = giant->CreateRefHandle();
		auto tinyhandle = tiny->CreateRefHandle();
		if (tiny) {
			std::string name = std::format("CopyAnimSpeed_{}", tiny->formID);

			TaskManager::Run(name, [=](auto& update){
				auto gianthandleRef = gianthandle.get();
				auto tinyhandleRef = tinyhandle.get();

				if (!gianthandleRef || !tinyhandleRef) {
					return false;
				}
				Actor* giantref = gianthandleRef.get();
				Actor* tinyref = tinyhandleRef.get();
				if (!AnimationVars::Action::IsInGrabPlayState(giantref)) { // Exit Grab Play state as soon as attack bool is false
					Anims_FixAnimationDesync(giantref, tinyref, true);
					return false;
				} else {
					Anims_FixAnimationDesync(giantref, tinyref, false);
					return true;
				}
				return true; // Else continue to wait for it
			});
		}
	}

	void GTSGrab_KickTiny(Actor* a_giant) {
		auto otherActor = Possession::FirstActor(a_giant->formID, Hand);

		StopCarry(a_giant->formID, Hand);
		Possession::Release(a_giant->formID, Hand);

		AnimationVars::Grab::SetHasGrabbedTiny(a_giant, false);
		AnimationVars::Grab::SetGrabState(a_giant, false);

		if (otherActor) {

			if (auto charcont = otherActor->GetCharController()) {
				charcont->SetLinearVelocityImpl(0.0f); // Needed so Actors won't fall down.
			}

			if (const auto bone = find_node(a_giant, "NPC R Finger22 [RF22]")) {

				NiPoint3 startCoords = bone->world.translate;

				ActorHandle gianthandle = a_giant->CreateRefHandle();
				ActorHandle tinyhandle = otherActor->CreateRefHandle();

				const std::string name = std::format("Throw_{}_{}", a_giant->formID, otherActor->formID);
				const std::string pass_name = std::format("ThrowOther_{}_{}", a_giant->formID, otherActor->formID);

				// Run task that will actually launch the Tiny
				TaskManager::Run(name, [=](auto&) {

					if (!gianthandle || !tinyhandle) {
						return false;
					}

					auto giantPtr = gianthandle.get();
					auto tinyPtr = tinyhandle.get();

					if (!giantPtr || !tinyPtr) {
						return false;
					}

					Actor* giant = giantPtr.get();
					Actor* tiny = tinyPtr.get();
					
					// Wait for 3D to be ready
					if (!giant->Is3DLoaded()   || 
						!giant->GetCurrent3D() || 
						!tiny->Is3DLoaded()    || 
						!tiny->GetCurrent3D()) {
						return true;
					}

					NiPoint3 endCoords = bone->world.translate;

					Anims_FixAnimationDesync(giant, tiny, true);

					SetBeingHeld(tiny, false);
					EnableCollisions(tiny);

					PushActorAway(giant, tiny, 1.0f);

					const float Flick_Power = TinyCalamityActive(giant) ? 9.0f : 4.0f;

					ThrowActor(gianthandle, tinyhandle, startCoords, endCoords, pass_name, Flick_Power, 20.0f);
					
					return false;
				});
			}
		}
	}

	void GTSGrab_SpawnHeartsAtHead(Actor* giant, float Z_Offset, float Heart_Scale) {
		auto tiny = Possession::FirstActor(giant->formID, Hand);
		if (tiny) {
			logger::info("Tiny True");
			auto node = find_node(tiny, "NPC Head [Head]");
			if (node) {
				logger::info("Trying to spawn hearts");
				SpawnHearts(giant, tiny, Z_Offset, Heart_Scale, false, node->world.translate);
			}
		}
	}

	void GTSGrab_AddToVoreData(Actor* giant) {
		auto& VoreData = VoreController::GetSingleton().GetVoreData(giant);
		auto otherActor = Possession::FirstActor(giant->formID, Hand);
		if (otherActor) {
			VoreData.AddTiny(otherActor);
		}
	}

	void GTSGrab_SetBeingEaten(Actor* giant) {
		auto otherActor = Possession::FirstActor(giant->formID, Hand);
		if (otherActor) {
			SetBeingEaten(otherActor, true);
		}
	}

	void GTSGrab_SwallowTiny(Actor* giant) {
		auto& VoreData = VoreController::GetSingleton().GetVoreData(giant);
		auto otherActor = Possession::FirstActor(giant->formID, Hand);
		if (otherActor) {
			for (auto& tiny: VoreData.GetVories()) {
				if (Devourment::Enabled() && Devourment::Swallow(giant, tiny, DevourmentLocus::kStomach)) {
					continue;
				}
				if (AnimationVars::Crawl::IsCrawling(giant)) {
					tiny->SetAlpha(0.0f); // Hide Actor
				}
			}
			VoreData.Swallow(); // Skips whatever Devourment just took
		}
	}

	void GTSGrab_FullyEatTiny(Actor* giant) {
		auto otherActor = Possession::FirstActor(giant->formID, Hand);
		if (otherActor) {
			SetBeingEaten(otherActor, false);
			auto& VoreData = VoreController::GetSingleton().GetVoreData(giant);
			for (auto& tiny: VoreData.GetVories()) {
				VoreData.KillAll();
			}
			AnimationVars::Grab::SetHasGrabbedTiny(giant, false);
			AnimationVars::Grab::SetGrabState(giant, false);
			
			HandEmptied(giant);

			SetBeingEaten(otherActor, false);
			LetGo(giant, otherActor, false);

			Possession::Release(giant->formID, Hand);
		}
	}

	void GTSGrab_Do_Damage(Actor* giant, float base_damage) {
		auto& sizemanager = SizeManager::GetSingleton();
		float bonus = TinyCalamityActive(giant) ? 1.65f : 1.0f;
		auto grabbedActor = Possession::FirstActor(giant->formID, Hand);

		if (grabbedActor) {
			grabbedActor->Attacked(giant); // force combat

			float tiny_scale = get_visual_scale(grabbedActor) * GetSizeFromBoundingBox(grabbedActor);
			float gts_scale = get_visual_scale(giant) * GetSizeFromBoundingBox(giant);

			float sizeDiff = gts_scale/tiny_scale;
			float power = std::clamp(sizemanager.GetSizeAttribute(giant, SizeAttribute::Normal), 1.0f, 1000000.0f);
			float additionaldamage = 1.0f + sizemanager.GetSizeVulnerability(grabbedActor);
			float damage = (base_damage * BalanceSizeDamage(sizeDiff)) * power * additionaldamage * additionaldamage;
			float experience = std::clamp(damage/1600, 0.0f, 0.06f);

            if (CanDoDamage(giant, grabbedActor, false)) {
                if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkGrowingPressure)) {
                    auto& mgr = SizeManager::GetSingleton();
                    mgr.ModSizeVulnerability(grabbedActor, damage * 0.0010f);
                }

                TinyCalamity_ShrinkActor(giant, grabbedActor, damage * 0.10f);

                SizeHitEffects::PerformInjuryDebuff(giant, grabbedActor, damage*0.15f, 6);
                InflictSizeDamage(giant, grabbedActor, damage);
            }
			
			Rumbling::Once("GrabAttack", giant, Rumble_Grab_Hand_Attack * bonus, 0.05f, "NPC L Hand [LHnd]", 0.0f);

			ModSizeExperience(giant, experience);
			AddSMTDuration(giant, 1.0f);

            CrushTask(giant, grabbedActor, bonus, true, true, DamageSource::HandCrushed, QuestStage::HandCrush);
		}
	}

	void GTSGrab_DelayedSmile(Actor* a_giant, float delay) {

		if (!a_giant) {
			return;
		}

		std::string name = std::format("DelayedSmile_{}", a_giant->formID);

		ActorHandle giantHandle = a_giant->CreateRefHandle();
		double start = Time::WorldTimeElapsed();

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = giantHandle.get();

			if (!giantPtr) {
				return false;
			}

			double finish = Time::WorldTimeElapsed();
			auto* giantref = giantPtr.get();

			float timepassed = static_cast<float>(finish - start) * AnimationManager::GetAnimSpeed(giantref);

			bool ShouldRevert = timepassed >= (delay * GetAnimationSlowdown(giantref));
			
			if (ShouldRevert) {
				Task_FacialEmotionTask_Smile(giantref, 1.25f, "GrabSmile", 0.125f);
				return false;
			}
			return true;
		});
	}

	void GTSGrab_DelayedVore(Actor* a_giant, float delay) {

		if (!a_giant) {
			return;
		}

		auto* a_tiny = Possession::FirstActor(a_giant->formID, Hand);

		if (!a_tiny) {
			return;
		}

		std::string name = std::format("DelayedVore_{}", a_giant->formID);

		ActorHandle giantHandle = a_giant->CreateRefHandle();
		ActorHandle tinyHandle = a_tiny->CreateRefHandle();
		double start = Time::WorldTimeElapsed();

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = giantHandle.get();
			auto tinyPtr = tinyHandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			double finish = Time::WorldTimeElapsed();
			auto* giantref = giantPtr.get();
			auto* tinyref = tinyPtr.get();

			float timepassed = static_cast<float>(finish - start) * AnimationManager::GetAnimSpeed(giantref);

			bool ShouldRevert = timepassed >= (delay * GetAnimationSlowdown(giantref));
			
			if (ShouldRevert) {
				VoreController::GetSingleton().ShrinkOverTime(giantref, tinyref, 0.05f, 6.0f);
				Task_FacialEmotionTask_OpenMouth(giantref, 0.65f, "GrabVoreOpenMouth", 0.1f, 2.15f);
				GTSGrab_AddToVoreData(giantref);
				return false;
			}
			return true;
		});
	}
}

// Moved wholesale from CleavageEvents, which is going out of the build. These are the cleavage
// branch's own helpers and nothing else calls them.
namespace GTS::Actions::Grabbing::Cleave {


    void Task_FixTinyAnimation(Actor* aGiant) {

        const std::string TaskName = std::format("FixTinyCleavageTask_{}", aGiant->formID);
        auto Tiny = Possession::FirstActor(aGiant->formID, Breasts);

        if (!Tiny) {
            return;
        }

        const ActorHandle TinyHandle = Tiny->CreateRefHandle();
        const ActorHandle GiantHandle = aGiant->CreateRefHandle();

		TaskManager::Run(TaskName, [=](auto&) {

	        const auto TinyActor = TinyHandle.get().get();
	        const auto GiantActor = GiantHandle.get().get();

	        if (!TinyActor || !GiantActor) {
	            return false;
	        }

	        const bool InCleavage = AnimationVars::Action::IsInCleavageState(GiantActor);
	        if (InCleavage) {
	            return true;
	        }

	        ActionRegistry::Notify(TinyActor, IsHostile(GiantActor, TinyActor) ? "GTSBEH_T_Storage_Enemy" : "GTSBEH_T_Storage_Ally");

	        return false;

        });
    }

    void Absorb_GrowInSize(Actor* giant, Actor* tiny, float multiplier) {
        if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkHugsGreed)) {
			multiplier *= 1.15f;
		}
        float grow_value = 0.08f * multiplier * 0.845f;
        float original = VoreController::ReadOriginalScale(tiny) + 0.875f; // Compensate 
        update_target_scale(giant, grow_value * original, SizeEffectType::kGrow);
    }

    void CancelAnimation(Actor* giant) {
        auto tiny = Possession::FirstActor(giant->formID, Breasts);
        AnimationVars::Action::SetIsCleavageZOverrideEnabled(giant, false);

        if (tiny) {
            KillActor(giant, tiny);
            PerkHandler::UpdatePerkValues(giant, PerkUpdate::Perk_LifeForceAbsorption);
            DrainStamina(giant, "GrabAttack", Runtime::PERK.GTSPerkDestructionBasics, false, 0.75f);
            SetBetweenBreasts(tiny, false);
            SetBeingEaten(tiny, false);
            SetBeingHeld(tiny, false);
        }

        std::string name = std::format("GrabAttach_{}", giant->formID);
        TaskManager::Cancel(name);

            }

    void RecoverAttributes(Actor* giant, ActorValue Attribute, float percentage) {
        float Percent = GetMaxAV(giant, Attribute);
        float value = Percent * percentage;

        if (Runtime::HasPerk(giant, Runtime::PERK.GTSPerkBreastsMastery2)) {
            value *= 1.5f;
        }

        DamageAV(giant, Attribute, -value);
    }

    void ShrinkTinyWithCleavage(Actor* giant, float scale_limit, float shrink_for, float stamina_damage, bool hearts, bool damage_stamina) {
        Actor* tiny = Possession::FirstActor(giant->formID, Breasts);
        if (tiny) {
            if (get_target_scale(tiny) > scale_limit && shrink_for < 1.0f) {
                DamageAV(giant, ActorValue::kHealth, -get_target_scale(tiny) * 10); // Heal GTS
                set_target_scale(tiny, get_target_scale(tiny) * shrink_for);
            } else {
                set_target_scale(tiny, scale_limit);
            }
            
            if (damage_stamina) {
                DamageAV(tiny, ActorValue::kStamina, stamina_damage);
            }
            if (hearts) {
                SpawnHearts(giant, tiny, 35, 0.4f, false);
            }
        }
    }

    void SuffocateTinyFor(Actor* giant, Actor* tiny, float DamageMult, float Shrink, float StaminaDrain) {
        float TotalHP = GetMaxAV(tiny, ActorValue::kHealth);
        float CurrentHP = GetAV(tiny, ActorValue::kHealth);
        float Threshold = 1.0f;
        
        float damage = TotalHP * DamageMult; // 100% hp by default
        
        if (CurrentHP > Threshold) {
            if (DamageMult > 0 && CurrentHP - damage > Threshold) {
                DamageAV(tiny, ActorValue::kHealth, damage);
            } else {
                if (DamageMult > 0) {
                    SetAV(tiny, ActorValue::kHealth, 1.0f);
                }
            }
            DamageAV(tiny, ActorValue::kStamina, StaminaDrain);
        }
    }

    void Task_RunSuffocateTask(Actor* giant, Actor* tiny) {
        std::string name = std::format("SuffoTask_{}", giant->formID);
		ActorHandle gianthandle = giant->CreateRefHandle();
		ActorHandle tinyhandle = tiny->CreateRefHandle();

		TaskManager::Run(name, [=](auto& progressData) {
			auto gianthandleRef = gianthandle.get();

			if (!gianthandleRef) {
				return false;
			}
			if (!tinyhandle) {
				return false;
			}
			auto giantref = gianthandleRef.get();
			auto tinyref = tinyhandle.get().get();

			if (!tinyref) {
				return false; // end task in that case
			}
            float damage = 0.0f;//0.0015f * starting_hppercentage * TimeScale();
            SuffocateTinyFor(giantref, tinyref, damage, 0.998f, 0.0035f * TimeScale());

            if (tinyref->IsDead() || GetAV(tinyref, ActorValue::kHealth) <= 0) {
                return false;
            }
			// All good try another frame
			return true;
		});
    }

    ///=================================================================== Functions
    void Deal_breast_damage(Actor* giant, float damage_mult) {
        Actor* tiny = Possession::FirstActor(giant->formID, Breasts);
        if (tiny) {
            if (!IsTeammate(tiny)) {
                tiny->Attacked(giant); // force combat
            }

            float bonus = 1.0f;
            auto& sizemanager = SizeManager::GetSingleton();

			float tiny_scale = get_visual_scale(tiny) * GetSizeFromBoundingBox(tiny);
			float gts_scale = get_visual_scale(giant) * GetSizeFromBoundingBox(giant);

			float sizeDiff = gts_scale/tiny_scale;
			float power = std::clamp(SizeManager::GetSizeAttribute(giant, SizeAttribute::Normal), 1.0f, 1000000.0f);
			float additionaldamage = 1.0f + sizemanager.GetSizeVulnerability(tiny);
			float damage = (Damage_Breast_Squish * damage_mult) * power * additionaldamage * additionaldamage * BalanceSizeDamage(sizeDiff);
			float experience = std::clamp(damage/1600, 0.0f, 0.06f);

			if (TinyCalamityActive(giant)) {
				bonus = 1.65f;
			}

            if (CanDoDamage(giant, tiny, false)) {
                if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkGrowingPressure)) {
                    auto& mgr = SizeManager::GetSingleton();
                    mgr.ModSizeVulnerability(tiny, damage * 0.0010f);
                }

                TinyCalamity_ShrinkActor(giant, tiny, damage * 0.20f);

                SizeHitEffects::PerformInjuryDebuff(giant, tiny, damage * 0.15f, 6);
                if (!IsTeammate(tiny) || IsHostile(giant, tiny)) {
                    InflictSizeDamage(giant, tiny, damage);
                    DamageAV(tiny, ActorValue::kStamina, damage * 0.25f);
                } else {
                    tiny->AsActorValueOwner()->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, damage * 5);
                }

                DamageAV(giant, ActorValue::kHealth, -damage * 0.33f);
            }
			
			Rumbling::Once("GrabAttack", tiny, Rumble_Grab_Hand_Attack * bonus * damage_mult, 0.05f, "NPC Root [Root]", 0.0f);
            Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundThighSandwichImpact, tiny, 1.0f, "NPC Root [Root]");

            CrushTask(giant, tiny, bonus, false, false, DamageSource::BreastImpact, QuestStage::Crushing);
            ModSizeExperience(giant, experience);
        }
    }

    ///===================================================================

    ///=================================================================== Camera
}

namespace GTS::Actions::Grabbing {

	// Fills the hand before asking, since the signature and the catch annotations both read the store.
	bool Grab(RE::Actor* a_Giant, RE::Actor* a_Tiny) {

		if (!a_Giant || !a_Tiny) {
			return false;
		}

		if (Possession::Occupied(a_Giant->formID, Hand)) {
			return false;
		}

		if (!Possession::Take(a_Giant->formID, Hand, a_Tiny->GetHandle())) {
			return false;
		}

		if (ActionRegistry::Perform(a_Giant, "Grab.Enter") == RequestResult::kAccepted) {
			return true;
		}

		Possession::Release(a_Giant->formID, Hand);
		return false;
	}

	void DropOne(RE::Actor* a_Giant, RE::Actor* a_Tiny) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		const RE::FormID owner = a_Giant->formID;

		if (Possession::HolderOf(a_Tiny->formID) != owner) {
			Abort(a_Giant);
			return;
		}

		const PossessionSlot slot = Possession::IsHeldIn(a_Tiny->formID, Hand) ? Hand : Breasts;

		Possession::ReleaseOne(owner, slot, a_Tiny->GetHandle());

		// With someone still carried the running node sees its own slot empty through Alive and ends
		// itself, and the carry takes the rest back. GTSBEH_AbortGrab would leave the storage pose too.
		if (!Possession::CarriedAlive(owner)) {
			Abort(a_Giant);
		}
	}

	void Abort(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		DrainStamina(a_Giant, "GrabAttack", Runtime::PERK.GTSPerkDestructionBasics, false, 0.75f);
		ActionRegistry::Abort(a_Giant);
	}

	void DamageActorInHand(RE::Actor* a_Giant, float a_Damage) {

		if (!a_Giant) {
			return;
		}

		auto* grabbed = Possession::FirstActor(a_Giant->formID, Hand);

		if (!grabbed) {
			return;
		}

		auto& sizemanager = SizeManager::GetSingleton();

		grabbed->Attacked(a_Giant);

		const float tiny_scale = get_visual_scale(grabbed) * GetSizeFromBoundingBox(grabbed);
		const float gts_scale = get_visual_scale(a_Giant) * GetSizeFromBoundingBox(a_Giant);
		const float sizeDiff = gts_scale / tiny_scale;

		const float power = std::clamp(SizeManager::GetSizeAttribute(a_Giant, SizeAttribute::Normal), 1.0f, 999999.0f);
		const float additionaldamage = 1.0f + sizemanager.GetSizeVulnerability(grabbed);
		const float damage = (a_Damage * BalanceSizeDamage(sizeDiff)) * power * additionaldamage * additionaldamage;
		const float experience = std::clamp(damage / 1600, 0.0f, 0.06f);
		const float bonus = TinyCalamityActive(a_Giant) ? 1.65f : 1.0f;

		if (CanDoDamage(a_Giant, grabbed, false)) {

			if (Runtime::HasPerkTeam(a_Giant, Runtime::PERK.GTSPerkGrowingPressure)) {
				sizemanager.ModSizeVulnerability(grabbed, damage * 0.0010f);
			}

			TinyCalamity_ShrinkActor(a_Giant, grabbed, damage * 0.10f);
			SizeHitEffects::PerformInjuryDebuff(a_Giant, grabbed, damage * 0.15f, 6);
			InflictSizeDamage(a_Giant, grabbed, damage);
		}

		Rumbling::Once("GrabAttack", a_Giant, Rumble_Grab_Hand_Attack * bonus, 0.05f, "NPC L Hand [LHnd]", 0.0f);

		ModSizeExperience(a_Giant, experience);
		AddSMTDuration(a_Giant, 1.0f);

		CrushTask(a_Giant, grabbed, bonus, false, true, DamageSource::HandCrushed, QuestStage::HandCrush);
	}

	namespace Cleave {

		void PassToTiny(Actor* giant, std::string_view a_Behaviour) {

			if (!giant || !AnimationVars::Action::IsInCleavageState(giant)) {
				return;
			}

			if (auto* tiny = Possession::FirstActor(giant->formID, Hand)) {
				AnimationManager::StartAnim(a_Behaviour, tiny);
			}
		}

		// Keeps the cooldown stamped for as long as the state lasts, so it cannot be re-triggered the
		// moment the animation ends.
		void LaunchCooldownFor(Actor* giant, CooldownSource Source) {

			const RE::ActorHandle handle = giant->GetHandle();

			TaskManager::Run(std::format("CDWatcher_{}_{}", giant->formID, Time::WorldTimeElapsed()), [=](const TaskUpdate&) {

				auto ptr = handle.get();

				if (!ptr || !AnimationVars::Action::IsInCleavageState(ptr.get())) {
					return false;
				}

				ApplyActionCooldown(ptr.get(), Source);
				return true;
			});
		}
	}
}

namespace GTS::Actions::Grabbing::Cleave {

	namespace {

		const std::vector<std::string_view> Nodes_3BBB = { "L Breast03", "R Breast03" };
		const std::vector<std::string_view> Nodes_1BBB = { "NPC L Breast", "NPC R Breast" };

		void ThresholdReachedRumbling(Actor* giant) {

			if (find_node(giant, "L Breast03")) {
				for (auto node : Nodes_3BBB) {
					Rumbling::Once("ReadyToAbsorb1", giant, 4.2f, 0.075f, node, true);
				}
			}
			else {
				for (auto node : Nodes_1BBB) {
					Rumbling::Once("ReadyToAbsorb2", giant, 4.2f, 0.075f, node, true);
				}
			}
		}

		std::string DotDamageTask(RE::FormID a_Giant, RE::FormID a_Tiny) {
			return std::format("Suffo_DOT_{}_{}", a_Giant, a_Tiny);
		}

		std::string DotHeartsTask(RE::FormID a_Giant, RE::FormID a_Tiny) {
			return std::format("Hearts_DOT_{}_{}", a_Giant, a_Tiny);
		}
	}

	// Hearts, and the tiny's own animation put back if it has fallen out of it. That re-send is what
	// stops the struggle freezing: the tiny leaves Tiny_Boob_Dot_State for any number of reasons and
	// nothing else notices.
	void StartDot(Actor* giant, Actor* tiny) {

		const RE::ActorHandle gianthandle = giant->GetHandle();
		const RE::ActorHandle tinyhandle = tiny->GetHandle();

		static Timer HeartTimer = Timer(1.75f);

		// std::function calls the lambda const.
		struct DotSend { bool Sent = false; };
		const auto started = std::make_shared<DotSend>();

		TaskManager::Run(DotHeartsTask(giant->formID, tiny->formID), [=](const TaskUpdate&) {

			auto giantPtr = gianthandle.get();
			auto tinyPtr = tinyhandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* giantref = giantPtr.get();
			auto* tinyref = tinyPtr.get();

			if (!AnimationVars::Cleavage::IsBoobsDoting(giantref)) {
				return false;
			}

			// Sent once. The tiny's GTS_Busy cannot gate it, since creatures do not have it.
			if (!started->Sent) {
				started->Sent = true;
				ActionRegistry::Notify(tinyref, "GTSBEH_T_Boobs_Crush_Dot");
			}

			if (HeartTimer.ShouldRunFrame()) {
				SpawnHearts(giantref, tinyref, 35.0f, 0.425f, false);
			}

			return true;
		});

		const float damage_Setting = GetDifficultyMultiplier(giant, tiny);
		constexpr float threshold = 0.075f;

		if (!IsTeammate(tiny)) {
			tiny->StartCombat(giant);
			tiny->Attacked(giant);
		}

		TaskManager::Run(DotDamageTask(giant->formID, tiny->formID), [=](const TaskUpdate&) {

			auto giantPtr = gianthandle.get();
			auto tinyPtr = tinyhandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* giantref = giantPtr.get();
			auto* tinyref = tinyPtr.get();

			auto& sizemanager = SizeManager::GetSingleton();

			const float max_tiny_hp = GetMaxAV(tinyref, ActorValue::kHealth) * threshold;
			const float tiny_health_perc = GetHealthPercentage(tinyref);
			const float tiny_health = GetAV(tinyref, ActorValue::kHealth);

			const float power = std::clamp(SizeManager::GetSizeAttribute(giantref, SizeAttribute::Normal), 1.0f, 999999.0f);
			const float sizeDiff = get_scale_difference(giantref, tinyref, SizeType::VisualScale, false, false);
			const float additionaldamage = 1.0f + sizemanager.GetSizeVulnerability(tinyref);
			const float speed = AnimationManager::GetBonusAnimationSpeed(giantref);

			float damage = Damage_Breast_Strangle * power * additionaldamage * BalanceSizeDamage(sizeDiff) * TimeScale() * speed;

			if (TinyCalamityActive(giantref)) {
				damage *= 1.5f;
			}

			if (!AnimationVars::Cleavage::IsBoobsDoting(giantref) && !AnimationVars::General::IsGTSBusy(giantref)) {
				AnimationManager::ResetAnimationSpeedData(giantref);
				RestoreBreastAttachmentState(giantref, tinyref);
				return false;
			}

			if (tiny_health_perc <= threshold || tiny_health - (damage * damage_Setting) <= max_tiny_hp) {

				if (auto* data = Transient::GetActorData(tinyref)) {

					// One reprieve each. The first time the threshold is reached the tiny is spared and
					// the branch returns to its idle; the next time it is the finisher.
					if (data->ImmuneToBreastOneShot) {

						ActionRegistry::Notify(giantref, "GTSBEH_Boobs_Crush_Dot_Stop");
						ActionRegistry::Notify(tinyref, "GTSBEH_T_Boobs_Crush_Dot_Stop");

						SpawnHearts(giantref, tinyref, 35.0f, 0.6f, false);
						ThresholdReachedRumbling(giantref);

						data->ImmuneToBreastOneShot = false;
						return false;
					}

					if (tiny_health - (damage * damage_Setting * 2.0f) <= 2.5f || tinyref->IsDead()) {
						ActionRegistry::Notify(giantref, "GTSBEH_Boobs_Crush_Kill");
						ActionRegistry::Notify(tinyref, "GTSBEH_T_Boobs_Crush_Kill");
						return false;
					}
				}
			}

			InflictSizeDamage(giantref, tinyref, damage);

			if (tinyref->IsDead() || giantref->IsDead() || GetAV(tinyref, ActorValue::kHealth) <= 0.0f) {
				AnimationManager::ResetAnimationSpeedData(giantref);
				return false;
			}

			return true;
		});
	}

	// GTSBEH_T_BS_DOT_Leave, not Crush_Dot_Stop: the leave is what Tiny_Boob_Dot_State transitions back
	// to the idle on. Crush_Dot_Stop is the spare, sent from the damage task at the threshold.
	void StopDot(Actor* giant, Actor* tiny) {

		if (!giant) {
			return;
		}

		if (tiny) {
			ActionRegistry::Notify(tiny, "GTSBEH_T_BS_DOT_Leave");
			TaskManager::Cancel(DotDamageTask(giant->formID, tiny->formID));
			TaskManager::Cancel(DotHeartsTask(giant->formID, tiny->formID));
		}
	}
}
