#pragma once

#include "Actions/Core/ActionContext.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"

namespace GTS::Actions::Grabbing {

	// The hand is where a grabbed actor lives, and the breasts are where they go when stored. Both
	// are carried slots, so the hold survives a cell change rather than ending with it.
	constexpr PossessionSlot Hand = PossessionSlot::kHand;
	constexpr PossessionSlot Breasts = PossessionSlot::kBreasts;

	// Leaves the grab root from any point inside it, including the sub branches.
	constexpr std::string_view BEH_ABORT = "GTSBEH_AbortGrab";


	void StartHandRumble(std::string_view a_Tag, RE::Actor* a_Giant, float a_Power, float a_HalfLife, bool a_Left = true);
	void StopHandRumble(std::string_view a_Tag, RE::Actor* a_Giant, bool a_Left = true);

	// Keeps the actor in the hand every frame. Runs on PostPhysics, because the hand's world
	// transform is only final once physics has written it.
	// a_Settle holds off the abort checks for that many seconds. A carry started right after a save
	// load reads a bounding box the game has not filled in yet, and the grab reads that as the tiny
	// being too small to hold.
	void StartCarry(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Settle = 0.0f);

	// Everyone the giant carries, one slot's worth, or one actor. A carried actor keeps their own
	// task, so stopping the wrong one drops somebody the caller never meant to let go of.
	void StopCarry(RE::FormID a_Giant);
	void StopCarry(RE::FormID a_Giant, PossessionSlot a_Slot);
	void StopCarry(RE::FormID a_Giant, RE::FormID a_Tiny);

	// Everything that has to be true of a tiny the giant is carrying in its hand. Called from the
	// catch annotation when a grab starts, and from the restore path after a save load, where no
	// animation is going to fire that annotation.
	void BeginHold(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Settle = 0.0f);

	// The same for a tiny stored in the breasts: the pose, the flag and the pitch task.
	void BeginBreastHold(RE::Actor* a_Giant, RE::Actor* a_Tiny);

	// Every way the hand empties - released, thrown, eaten, killed. The graph keeps the holding pose
	// until it is told, so this is what leaves it.
	void HandEmptied(RE::Actor* a_Giant);

	// Everything that has to be undone on the tiny when the hold ends, whichever way it ends.
	void LetGo(RE::Actor* a_Giant, RE::Actor* a_Tiny, bool a_Push);

	// Finishes a tiny the hand has just squeezed, if that squeeze was enough to kill them.
	void CrushTask(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Bonus, bool a_Sound, bool a_Stagger, DamageSource a_Source, QuestStage a_Stage);

	void ThrowActor(const RE::ActorHandle& a_Giant, const RE::ActorHandle& a_Tiny, NiPoint3 a_Start, NiPoint3 a_End, std::string_view a_Task, float a_Speed, float a_Pitch, float a_Yaw = 0.0f);

	// The grab play branch's helpers, moved out of Grab_Play_Events.
	namespace Play {

	void FixKissVoreTinyOffset(Actor* giant, bool Fix);
	void ResetTinyAnimSpeed(Actor* giant);
	void Task_CopyAnimationSpeed(Actor* giant, Actor* tiny);
	void GTSGrab_KickTiny(Actor* a_giant);
	void GTSGrab_SpawnHeartsAtHead(Actor* giant, float Z_Offset, float Heart_Scale);
	void GTSGrab_AddToVoreData(Actor* giant);
	void GTSGrab_SetBeingEaten(Actor* giant);
	void GTSGrab_SwallowTiny(Actor* giant);
	void GTSGrab_FullyEatTiny(Actor* giant);
	void GTSGrab_Do_Damage(Actor* giant, float base_damage);
	void GTSGrab_DelayedSmile(Actor* a_giant, float delay);
	void GTSGrab_DelayedVore(Actor* a_giant, float delay);
	}

	// The cleavage branch's helpers, moved out of CleavageEvents.
	namespace Cleave {

	void Task_FixTinyAnimation(Actor* aGiant);
	void Absorb_GrowInSize(Actor* giant, Actor* tiny, float multiplier);
	void CancelAnimation(Actor* giant);
	void RecoverAttributes(Actor* giant, ActorValue Attribute, float percentage);
	void ShrinkTinyWithCleavage(Actor* giant, float scale_limit, float shrink_for, float stamina_damage, bool hearts, bool damage_stamina);
	void SuffocateTinyFor(Actor* giant, Actor* tiny, float DamageMult, float Shrink, float StaminaDrain);
	void Task_RunSuffocateTask(Actor* giant, Actor* tiny);
	void Deal_breast_damage(Actor* giant, float damage_mult);
	}

	// The rest of the DLL's way into the grab, for code that is not part of the branch: hooks, the
	// AI, and the hand slam animations. Everything here goes through the action layer.
	[[nodiscard]] bool Grab(RE::Actor* a_Giant, RE::Actor* a_Tiny);
	void Abort(RE::Actor* a_Giant);

	// Ends one actor's hold. The node is only aborted when nobody else is still carried.
	void DropOne(RE::Actor* a_Giant, RE::Actor* a_Tiny);
	void DamageActorInHand(RE::Actor* a_Giant, float a_Damage);

	namespace Cleave {
		void LaunchCooldownFor(Actor* giant, CooldownSource Source);
		void PassToTiny(Actor* giant, std::string_view a_Behaviour);

		// The damage over time branch. StartDot runs both tasks, StopDot cancels them.
		void StartDot(Actor* giant, Actor* tiny);
		void StopDot(Actor* giant, Actor* tiny);
	}
}
