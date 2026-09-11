#include "Actions/Nodes/Grab/GrabNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/PlayerTarget.hpp"
#include "Config/Keybinds.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/Grab/GrabCommon.hpp"
#include "Utils/Actions/VoreUtils.hpp"
#include "Actions/Core/ActionCleanup.hpp"
#include "API/Devourment.hpp"

#include "Config/Config.hpp"
#include "Magic/Effects/Common.hpp"

#include "Managers/Animation/Controllers/GrabAnimationController.hpp"
#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Actions/Nodes/Grab/GrabAttach.hpp"
#include "Managers/Damage/SizeHitEffects.hpp"
#include "Managers/Damage/TinyCalamity.hpp"
#include "Managers/Damage/Utils/SizeDamageUtils.hpp"
#include "Managers/GtsSizeManager.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Managers/Rumble.hpp"


/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  The graph's grab machine is root state 854, Giantess_Grab_Behave, with 24 states: eight actions in
  three variants each. The variants are what the giant is holding - unarmed, weapon drawn, or magic -
  and the graph picks between them on its own, so nothing here chooses.

  The Loop states blend locomotion, which is why the giant walks and turns while this node runs. It
  is the one node that is idle the longest.

  [PICK UP ("GTSBEH_GrabStart")]
    GTSGrab_Catch_Start   -> collision off, held, staggered
    GTSGrab_Catch_Actor   -> the hold is real from here, and this is the entry's Confirm
    GTSGrab_Catch_End
    GTSBEH_Grab_Next      -> back to the carry loop, fires after every action below

  [ATTACK ("GTSBEH_GrabAttack")]
    GTSGrab_Attack_MoveStart
    GTSGrab_Attack_Damage    -> the hand squeezes; this is where the tiny can die
    GTSGrab_Attack_MoveStop

  [THROW ("GTSBEH_GrabThrow")]
    GTSGrab_Throw_MoveStart
    GTSGrab_Throw_FS_R / _FS_L   -> footsteps, any number of them, the giant is walking into it
    GTSGrab_Throw_Throw_Pre      -> the tiny leaves the hand here, the launch is measured from it
    GTSGrab_Throw_ThrowActor     -> exit signal
    GTSGrab_Throw_Throw_Post and GTSGrab_Throw_MoveStop exist but land after the node has gone

  [VORE, STANDING ("GTSBEH_GrabVore")]
    GTSGrab_Eat_Start        -> the tiny is put in the vore data and made to cower
    GTSGrab_Eat_OpenMouth
    GTSGrab_Eat_Eat          -> Devourment takes them here if it is running, otherwise GTS swallows
    GTSGrab_Eat_CloseMouth
    GTSGrab_Eat_Swallow      -> exit signal

  [VORE, CROUCHING ("GTSBEH_GrabVore" while sneaking)]
    A different set, and two of them interleaved. GTS_GrabSneak_* is the grab's own half and
    GTS_Sneak_Vore_* is shared with the standalone sneak vore, which owns them in VoreNode.
    Both halves are declared here because a node only sees the tags it declares.

    GTS_GrabSneak_Start          -> adds the tiny to the vore data
    GTS_Sneak_Vore_SmileOn
    GTS_Sneak_Vore_OpenMouth
    GTS_GrabSneak_Eat            -> switches the attachment
    GTS_Sneak_Vore_CloseMouth
    GTS_Sneak_Vore_Swallow
    GTS_GrabSneak_CamOff
    GTS_Sneak_Vore_SmileOff
    GTS_GrabSneak_KillAll        -> releases the partner
    GTSBEH_ExitGrab              -> exit signal, and the only path that reaches it

    Not reached in the last capture. Crouching with a tiny in hand and pressing vore went to
    VoreNode instead, so the order above is still from the registrations.

  [TO AND FROM THE BREASTS ("GTSBEH_BreastsAdd" / "GTSBEH_BreastsRemove")]
    GTSGrab_Breast_MoveStart
    GTSGrab_Breast_PutActor      -> the tiny leaves the hand and enters the breasts here
    GTSGrab_Breast_MoveEnd
    GTSBEH_BoobExit              -> only on the way in, the take back does not send it

    GTSGrab_Breast_MoveStart
    GTSGrab_Breast_TakeActor     -> and back into the hand
    GTSGrab_Breast_MoveEnd

  [RELEASE ("GTSBEH_GrabRelease")]
    GTSGrab_Release_FreeActor    -> exit signal

  [HANDOFF OUT]
    Grab.Play sends GTSBEH_Hand_Enter and hands to GrabPlayNode, Cleavage.Enter sends
    GTSBEH_Boobs_Enter and hands to CleavageNode. Both keep the tiny held across the change, so this
    node's own signature never drains; the handoff completes when the target asserts instead.

  [EXIT]
    Each action above ends on its own annotation, except the two that do not announce themselves:

    The tiny dying in the hand. The graph runs GTSBEH_Grab_TinyDeath_Exit and then GTSBEH_Grab_Next,
    which is the carry loop, so it goes back to holding nobody. Only GTSBEH_AbortGrab leaves the
    grab root, and a clean exit sends no abort of its own, so OnAttackStop sends it. That is also
    the one window the graph accepts it in: sent any later it is refused.

    GTSBEH_Grab_TinyDeath_Exit is not an exit signal despite the name. The capture has it firing
    after an attack the tiny survived, with a second attack after it, so it says nothing about
    whether anyone died. It is declared and does nothing.

    A crouching vore, which reaches GTSBEH_ExitGrab on its own.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/
/*
   ----- Grab, all stances
   [SET ON THE PICK UP, THE SAME FRAME Grab.Enter IS TAKEN]
     GTS_Busy                -> TRUE
     GTS_Def_State           -> TRUE
     GTS_Grab_State          -> TRUE   (graph owned, and this node's signature)
     GTS_GrabbedTiny         -> TRUE   (written by the DLL, it follows the Possession store)
   (NON GTS)
     GTSTDM_Dodge            -> TRUE
     GTSTDM_LockRotation     -> TRUE
     bAnimationDriven        -> TRUE
     bNoStagger              -> TRUE
     currentDefaultState     -> TRUE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
   (NON GTS)
     bHeadTrackSpine         -> TRUE
   [SHORTLY AFTER]
   (NON GTS)
     bIdleBeforeConversation -> FALSE
     bIsInMT                 -> FALSE
     bVoiceReady             -> FALSE
   [ON GTSBEH_Grab_Next, SETTLING INTO THE CARRY LOOP]
     GTS_Def_State           -> FALSE
     GTS_Grab_State          -> FALSE
     GTS_Busy                -> FALSE
     GTS_Ready               -> TRUE
   (NON GTS)
     GTSTDM_Dodge            -> FALSE
     GTSTDM_LockRotation     -> FALSE
     bAnimationDriven        -> FALSE
     bNoStagger              -> FALSE
   [WHILE ATTACKING]
     GTS_Busy                -> TRUE
     GTS_Def_State           -> TRUE
     GTS_Grab_State          -> TRUE
     GTS_IsGrabAttacking     -> TRUE
   [WHILE THROWING]
     GTS_Busy                -> TRUE
     GTS_Def_State           -> TRUE
     GTS_Grab_State          -> TRUE
   (NON GTS)
     GTSTDM_LockRotation     -> TRUE
   [WHILE VORING, STANDING]
     GTS_Busy                -> TRUE
     GTS_Def_State           -> TRUE
     GTS_Grab_State          -> TRUE
     GTS_IsGrabAttacking     -> TRUE
   [WHILE MOVING THE TINY TO OR FROM THE BREASTS]
     GTS_Busy                -> TRUE
     GTS_Def_State           -> TRUE
     GTS_Grab_State          -> TRUE
     GTS_GrabbedTiny         -> FALSE on the put, TRUE again on the take
     GTS_Storing_Tiny        -> TRUE on the put, FALSE again on the take
   [WHEN THE TINY DIES, IS THROWN, OR IS EATEN]
     GTS_GrabbedTiny         -> FALSE  (the DLL drops the actor and the graph variables follow)
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_Grab_State is graph owned and is this node's signature. It goes FALSE for a moment on
   **every action and comes back on the next frame, which is inside MismatchGrace, so it does not
   **read as a desync. Do not shorten that grace without checking this.

   **Info: GTS_GrabbedTiny and GTS_Storing_Tiny are the two the DLL writes. Possession::Restore sets
   **them from the slots on every change to the store, so they are a mirror and never an input.
   **Nothing may write either of them directly.

   **Info: the crawl grab shares all of the above. Only GTS_IsCrawling and the non GTS locomotion
   **variables differ, and neither this node nor Core reads them.
*/

using namespace GTS;

namespace {

	using namespace GTS::Actions;
	using State = GrabNode::State;

	bool GrabCondition_Start() {
		auto target = GetPlayerOrControlled();
		if (!target) {
			return false;
		}
		// The hand, not Carried, which also answers for the breasts.
		if (Actions::Possession::Occupied(target->formID, Actions::PossessionSlot::kHand)) {
			return false;
		}
		if (!CanDoActionBasedOnQuestProgress(target, QuestAnimationType::kStompsAndKicks)) {
			return false;
		}
		if (AnimationVars::General::IsGTSBusy(target) || IsEquipBusy(target) || AnimationVars::General::IsTransitioning(target)) {
			return false; // Disallow Grabbing if Behavior is busy doing other stuff.
		}
		return true;
	}

	constexpr std::string_view LHAND = "NPC L Hand [LHnd]";

	constexpr std::string_view TASKID_ATTACK = "GrabAttack";
	constexpr std::string_view TASKID_THROW = "GrabThrow";

	constexpr std::string_view RUMBLE_CATCH = "GrabL";
	constexpr std::string_view RUMBLE_ATTACK = "GrabMoveL";
	constexpr std::string_view RUMBLE_THROW = "GrabThrowL";
	constexpr std::string_view RUMBLE_VORE = "GrabVoreL";

	// Written by the DLL rather than the graph, which is why the entry carries a Confirm. See the
	// class comment.
	constexpr GraphExpect Signature[] = {
		{ "GTS_GrabbedTiny", true },
	};

	constexpr std::string_view ExitSignals[] = {
		"GTSGrab_Release_FreeActor",
		"GTSGrab_Throw_ThrowActor",
		"GTSGrab_Eat_Swallow",
		"GTSBEH_ExitGrab",
	};

	// The hand only. Possession::Carried falls back to the breasts.
	RE::Actor* Held(const ActionContext& a_Ctx) {
		return Possession::FirstActor(a_Ctx.Owner(), Grabbing::Hand);
	}

	RE::Actor* Stored(const ActionContext& a_Ctx) {
		return Possession::FirstActor(a_Ctx.Owner(), Grabbing::Breasts);
	}

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	// The hand, alive or not. There is no exit while only the breasts are occupied.
	bool HasTiny(const ActionContext& a_Ctx) {
		return Possession::Occupied(a_Ctx.Owner(), Grabbing::Hand);
	}

	// Alive, not merely present. An attack that kills leaves the body in the hand for the rest of the
	// animation, and nothing here may be started on it again.
	bool InHand(const ActionContext& a_Ctx) {
		const State* state = GrabNode::Get(a_Ctx.Owner());
		return Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kHand) && !(state && state->Thrown);
	}

	bool VerifyAttack(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float cost = Runtime::HasPerk(giant, Runtime::PERK.GTSPerkDestructionBasics) ? 20.0f * 0.65f : 20.0f;

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, "You're too tired to perform a hand attack");
			return false;
		}

		return true;
	}

	bool VerifyThrow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float cost = Runtime::HasPerk(giant, Runtime::PERK.GTSPerkDestructionBasics) ? 40.0f * 0.65f : 40.0f;

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, "You're too tired to throw");
			return false;
		}

		return true;
	}

	// The release animation uses the free hand, which a drawn weapon is occupying.
	bool CanRelease(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant || !Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kHand)) {
			return false;
		}

		if (AnimationVars::General::IsTransitioning(giant) || giant->AsActorState()->IsWeaponDrawn()) {
			return false;
		}

		return true;
	}

	// The two slots are independent, so each move needs a free destination as well as an occupied
	// source. Without that, storing a second tiny would stack them in the breasts and taking one back
	// would drop them into an occupied hand.
	bool CanStore(const ActionContext& a_Ctx) {
		return Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kHand) != nullptr
			&& !Possession::Occupied(a_Ctx.Owner(), PossessionSlot::kBreasts);
	}

	bool CanTakeBack(const ActionContext& a_Ctx) {
		return Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kBreasts) != nullptr
			&& !Possession::Occupied(a_Ctx.Owner(), PossessionSlot::kHand);
	}

	// The second catch. Grabbing::Grab fills the hand before asking, the same as for the entry.
	bool CatchSecond(const ActionContext& a_Ctx) {
		return a_Ctx.Deferred(Possession::Occupied(a_Ctx.Owner(), PossessionSlot::kHand));
	}

	// Entering the cleavage state only needs someone in the breasts. The hand is free to be holding
	// somebody else.
	bool CanCleave(const ActionContext& a_Ctx) {
		return Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kBreasts) != nullptr;
	}

	//----------------------------------------------------------------------------------------------
	// Takes
	//----------------------------------------------------------------------------------------------

	// The hold begins the moment the request is accepted, not when the animation says so. That is
	// what makes the signature match, and the Confirm annotation is what proves it was real.
	void TakeGrab(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return;
		}

		AnimationVars::Grab::SetHasGrabbedTiny(giant, true);
	}

	void TakePlay(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Grabbing::Hand, "GTSBEH_T_Hand_Enter");
	}

	// The cleavage state is only reachable with someone already stored in the breasts.
	void TakeCleavage(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Grabbing::Breasts, "GTSBEH_T_Boobs_Enter");
	}

	void TakeRelease(const ActionContext& a_Ctx) {
		Utils_UpdateHighHeelBlend(a_Ctx.Actor(), false);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: the pick up
	//----------------------------------------------------------------------------------------------

	void OnCatchStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, true, CameraTracking::Grab_Left);
		Grabbing::StartHandRumble(RUMBLE_CATCH, giant, 0.5f, 0.10f);

		if (auto* tiny = Held(a_Ctx)) {
			DisableCollisions(tiny, giant);
			SetBeingHeld(tiny, true);
			StaggerActor(giant, tiny, 100.0f);
		}
	}

	void OnCatchActor(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (tiny) {

			if (Config::Gameplay.ActionSettings.bGrabStartIsHostile && !IsTeammate(tiny)) {
				tiny->Attacked(giant);
			}

			Grabbing::BeginHold(giant, tiny);
		}

		Rumbling::Once("GrabCatch", giant, 1.0f, 0.05f);
	}

	void OnCatchEnd(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Grab_Left);
		Grabbing::StopHandRumble(RUMBLE_CATCH, a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: the hand attack
	//----------------------------------------------------------------------------------------------

	void OnAttackStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DrainStamina(giant, TASKID_ATTACK, Runtime::PERK.GTSPerkDestructionBasics, true, 0.75f);
		ManageCamera(giant, true, CameraTracking::Grab_Left);
		Grabbing::StartHandRumble(RUMBLE_ATTACK, giant, 0.5f, 0.10f);
	}

	void OnAttackDamage(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (!tiny) {
			return;
		}

		auto& sizemanager = SizeManager::GetSingleton();

		tiny->Attacked(giant);

		const float tinyScale = get_visual_scale(tiny) * GetSizeFromBoundingBox(tiny);
		const float giantScale = get_visual_scale(giant) * GetSizeFromBoundingBox(giant);
		const float power = std::clamp(sizemanager.GetSizeAttribute(giant, SizeAttribute::Normal), 1.0f, 1000000.0f);
		const float vulnerability = 1.0f + sizemanager.GetSizeVulnerability(tiny);

		const float damage = (Damage_Grab_Attack * BalanceSizeDamage(giantScale / tinyScale)) * power * vulnerability * vulnerability;
		const float bonus = TinyCalamityActive(giant) ? 1.65f : 1.0f;

		if (CanDoDamage(giant, tiny, false)) {

			if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkGrowingPressure)) {
				sizemanager.ModSizeVulnerability(tiny, damage * 0.0010f);
			}

			TinyCalamity_ShrinkActor(giant, tiny, damage * 0.10f);
			SizeHitEffects::PerformInjuryDebuff(giant, tiny, damage * 0.15f, 6);
			InflictSizeDamage(giant, tiny, damage);
		}

		Rumbling::Once("GrabAttack", giant, Rumble_Grab_Hand_Attack * bonus, 0.05f, LHAND, 0.0f);
		ModSizeExperience(giant, std::clamp(damage / 1600.0f, 0.0f, 0.06f));
		AddSMTDuration(giant, 1.0f);

		Grabbing::CrushTask(giant, tiny, bonus, true, true, DamageSource::HandCrushed, QuestStage::HandCrush);
	}

	void OnAttackStop(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, false, CameraTracking::Grab_Left);
		DrainStamina(giant, TASKID_ATTACK, Runtime::PERK.GTSPerkDestructionBasics, false, 0.75f);
		Grabbing::StopHandRumble(RUMBLE_ATTACK, giant);

		// The attack killed whoever was in the hand. This is the one window the graph takes these:
		// later it has moved on through GTSBEH_Grab_TinyDeath_Exit into GTSBEH_Grab_Next and refuses
		// them, looping back into holding nobody. AbortGrab is what leaves the grab root, and a clean
		// exit sends no abort of its own, so the branch sends it itself.
		if (Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Hand)) {
			return;
		}

		// Someone is still stored. The graph goes back to the carry loop, which is where they belong,
		// and AbortGrab would take the storage pose with it. The node exits clean and the carry comes back.
		if (Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Breasts)) {

			if (auto* body = Held(a_Ctx)) {
				Grabbing::LetGo(giant, body, false);
			}

			Grabbing::HandEmptied(giant);
			Possession::Release(a_Ctx.Owner(), Grabbing::Hand);
			a_Ctx.RequestExit();
			return;
		}

		ActionRegistry::Notify(giant, Grabbing::BEH_ABORT);
		Grabbing::HandEmptied(giant);
		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: the throw
	//----------------------------------------------------------------------------------------------

	void OnThrowStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DrainStamina(giant, TASKID_THROW, Runtime::PERK.GTSPerkDestructionBasics, true, 1.25f);
		ManageCamera(giant, true, CameraTracking::Grab_Left);
		Grabbing::StartHandRumble(RUMBLE_THROW, giant, 0.5f, 0.10f);
	}

	// The hand lets go here, and how far the hand travels from this frame is what decides how hard
	// the throw lands. Measured rather than fixed, so it survives the animation running at any speed.
	void OnThrowPre(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Possession::FirstActor(a_Ctx.Owner(), PossessionSlot::kHand);

		GrabNode::GetOrAdd(a_Ctx.Owner()).Thrown = true;
		Grabbing::StopCarry(a_Ctx.Owner(), PossessionSlot::kHand);

		if (!tiny) {
			return;
		}

		if (auto* controller = tiny->GetCharController()) {
			controller->SetLinearVelocityImpl({ 0.0f, 0.0f, 0.0f, 0.0f });
		}

		auto* bone = find_node(giant, LHAND);

		if (!bone) {
			return;
		}

		const NiPoint3 start = bone->world.translate;
		const RE::ActorHandle giantHandle = giant->GetHandle();
		const RE::ActorHandle tinyHandle = tiny->GetHandle();
		const std::string launch = std::format("GrabThrowLaunch_{}_{}", giant->formID, tiny->formID);

		TaskManager::Run(std::format("GrabThrowMeasure_{}_{}", giant->formID, tiny->formID), [=](auto&) {

			auto giantPtr = giantHandle.get();
			auto tinyPtr = tinyHandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* g = giantPtr.get();
			auto* t = tinyPtr.get();

			if (!g->Is3DLoaded() || !g->GetCurrent3D() || !t->Is3DLoaded() || !t->GetCurrent3D()) {
				return true;
			}

			Grabbing::LetGo(g, t, true);

			if (auto* controller = t->GetCharController()) {
				controller->SetLinearVelocityImpl({ 0.0f, 0.0f, 0.0f, 0.0f });
			}

			float speed = TinyCalamityActive(g) ? 5.0f : 2.0f;
			float pitch = 35.0f;

			// Crouched throws are cut right down, or a normal sized giant launches someone most of a
			// hundred metres.
			if (g->IsSneaking()) {
				speed *= 0.2f;
				if (AnimationVars::Crawl::IsCrawling(g)) {
					pitch = 25.0f;
				}
			}

			Grabbing::ThrowActor(giantHandle, tinyHandle, start, bone->world.translate, launch, speed, pitch);
			return false;
		});
	}

	// The giant walks into the throw, and each of those steps is a full footstep: damage, launch,
	// sound and dust, the same as a stomp. Skipped while sitting or crawling, where the animation is
	// blended and the feet are not really landing.
	void ThrowFootstep(const ActionContext& a_Ctx, FootEvent a_Foot, DamageSource a_Source, std::string_view a_Node) {

		auto* giant = a_Ctx.Actor();

		if (AnimationVars::Action::IsSitting(giant) || AnimationVars::Crawl::IsCrawling(giant)) {
			return;
		}

		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float launch = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.25f : 0.9f;
		const float perk = GetPerkBonus_Basics(giant);
		const float speed = a_Ctx.AnimSpeed();

		Rumbling::Once("Stomp", giant, Rumble_Grab_Throw_Footstep * smt * GetHighHeelsBonusDamage(giant, true), 0.05f, a_Node, 0.0f);

		DoDamageEffect(giant, 1.1f * launch * speed * perk, 1.0f * launch * speed, 10, 0.20f, a_Foot, 1.0f, a_Source);
		DoFootstepSound(giant, 1.0f, a_Foot, a_Node);
		DoDustExplosion(giant, dust, a_Foot, a_Node);
		DoLaunch(giant, 0.75f * perk, 1.55f, a_Foot);
	}

	void OnThrowFootL(const ActionContext& a_Ctx) { ThrowFootstep(a_Ctx, FootEvent::Left, DamageSource::CrushedLeft, "NPC L Foot [Lft ]"); }
	void OnThrowFootR(const ActionContext& a_Ctx) { ThrowFootstep(a_Ctx, FootEvent::Right, DamageSource::CrushedRight, "NPC R Foot [Rft ]"); }

	void OnThrowActor(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Grabbing::HandEmptied(giant);
		Rumbling::Once("ThrowFoe", giant, 2.50f, 0.10f, LHAND, 0.0f);

		Possession::Release(a_Ctx.Owner(), Grabbing::Hand);
		a_Ctx.RequestExit();
	}

	void OnThrowStop(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), TASKID_THROW, Runtime::PERK.GTSPerkDestructionBasics, false, 1.25f);
		Grabbing::StopHandRumble(RUMBLE_THROW, a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: eating out of the hand
	//----------------------------------------------------------------------------------------------

	void OnSneakVoreStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, true, CameraTracking::Grab_Left);

		auto& data = VoreController::GetSingleton().GetVoreData(giant);

		if (auto* tiny = Held(a_Ctx)) {
			data.AddTiny(tiny);
		}

		data.AllowToBeVored(false);

		for (auto* prey : data.GetVories()) {
			AllowToBeCrushed(prey, false);
			DisableCollisions(prey, giant);
			SetBeingHeld(prey, true);
		}

		Task_HighHeel_SyncVoreAnim(giant);
	}

	void OnSneakVoreEat(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto& data = VoreController::GetSingleton().GetVoreData(giant);

		for (auto* prey : data.GetVories()) {
			prey->NotifyAnimationGraph("JumpFall");
			prey->Attacked(giant);
		}

		data.GrabAll();
	}

	void OnSneakVoreOpenMouth(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Task_FacialEmotionTask_OpenMouth(giant, 0.6f, "SneakVoreOpenMouth");

		for (auto* prey : VoreController::GetSingleton().GetVoreData(giant).GetVories()) {
			VoreController::GetSingleton().ShrinkOverTime(giant, prey);
		}
	}

	void OnSneakVoreSwallow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, false, CameraTracking::ObjectA);
		ManageCamera(giant, false, CameraTracking::Hand_Right);
		ApplySwallowAndDevourment(giant, true);
	}

	void OnSneakVoreCamOff(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::ObjectA);
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Grab_Left);
	}

	void OnSneakVoreKillAll(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto& data = VoreController::GetSingleton().GetVoreData(giant);

		for (auto* prey : data.GetVories()) {
			AllowToBeCrushed(prey, true);
			EnableCollisions(prey);
		}

		data.AllowToBeVored(true);
		data.KillAll();
		data.ReleaseAll();
	}

	void OnSneakVoreSmileOn(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 2, 1.0f, CharEmotionType::Expression, 0.32f, 0.72f);
		AdjustFacialExpression(a_Ctx.Actor(), 3, 0.8f, CharEmotionType::Phenome, 0.32f, 0.72f);
	}

	void OnSneakVoreSmileOff(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 2, 0.0f, CharEmotionType::Expression, 0.32f, 0.72f);
		AdjustFacialExpression(a_Ctx.Actor(), 3, 0.0f, CharEmotionType::Phenome, 0.32f, 0.72f);
	}

	void OnEatStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, true, CameraTracking::Grab_Left);
		Grabbing::StartHandRumble(RUMBLE_VORE, giant, 0.5f, 0.10f);

		if (auto* tiny = Held(a_Ctx)) {
			VoreController::GetSingleton().GetVoreData(giant).AddTiny(tiny);
			Cleanup::Scare(tiny);
		}
	}

	void OnEatOpenMouth(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (auto* tiny = Held(a_Ctx)) {
			SetBeingEaten(tiny, true);
			VoreController::GetSingleton().ShrinkOverTime(giant, tiny);
		}

		Task_FacialEmotionTask_OpenMouth(giant, 1.1f, "GrabVoreOpenMouth", 0.3f);
		Grabbing::StopHandRumble(RUMBLE_VORE, giant);
	}

	void OnEatSwallowStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!Held(a_Ctx)) {
			return;
		}

		auto& data = VoreController::GetSingleton().GetVoreData(giant);
		bool swallow = false;

		for (auto* prey : data.GetVories()) {

			if (Devourment::Enabled() && Devourment::Swallow(giant, prey, DevourmentLocus::kStomach)) {
				continue;
			}

			swallow = true;

			if (AnimationVars::Crawl::IsCrawling(giant)) {
				prey->SetAlpha(0.0f);
			}
		}

		if (swallow) {
			data.Swallow();
		}
	}

	void OnEatSwallowEnd(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (!tiny) {
			return;
		}

		SetBeingEaten(tiny, false);
		VoreController::GetSingleton().GetVoreData(giant).KillAll();

		ManageCamera(giant, false, CameraTracking::Grab_Left);
		SetBeingHeld(tiny, false);

		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: to and from the breasts
	//----------------------------------------------------------------------------------------------

	void OnBreastMoveStart(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Grab_Left);
	}

	void OnBreastPut(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		auto* moved = Possession::FirstActor(a_Ctx.Owner(), Grabbing::Hand);

		if (!moved || !Possession::Transfer(a_Ctx.Owner(), Grabbing::Hand, Grabbing::Breasts, moved->GetHandle())) {
			return;
		}

		GrabNode::GetOrAdd(a_Ctx.Owner()).Stored = true;

		AnimationVars::Action::SetIsStoringTiny(giant, true);
		AnimationVars::Grab::SetHasGrabbedTiny(giant, false);

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundBreastImpact, giant, 1.0f, LHAND);

		if (auto* tiny = Possession::FirstActor(a_Ctx.Owner(), PossessionSlot::kBreasts)) {
			Grabbing::BeginBreastHold(giant, tiny);
		}
	}

	void OnBreastTake(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		auto* moved = Possession::FirstActor(a_Ctx.Owner(), Grabbing::Breasts);

		if (!moved || !Possession::Transfer(a_Ctx.Owner(), Grabbing::Breasts, Grabbing::Hand, moved->GetHandle())) {
			return;
		}

		GrabNode::GetOrAdd(a_Ctx.Owner()).Stored = false;

		AnimationVars::Action::SetIsStoringTiny(giant, false);
		AnimationVars::Grab::SetHasGrabbedTiny(giant, true);

		if (auto* tiny = Possession::FirstActor(a_Ctx.Owner(), PossessionSlot::kHand)) {
			SetBetweenBreasts(tiny, false);
			a_Ctx.SendTo(tiny, "GTSBEH_T_Remove");
			Anims_FixAnimationDesync(giant, tiny, true);
		}
	}

	void OnBreastMoveEnd(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Grab_Left);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: letting go
	//----------------------------------------------------------------------------------------------

	void OnReleaseFreeActor(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Grabbing::HandEmptied(giant);

		if (auto* tiny = Held(a_Ctx)) {
			Grabbing::LetGo(giant, tiny, true);
		}

		a_Ctx.RequestExit();
	}

	// Back in the idle, so whatever action was running is over. If the actor it was aimed at is still
	// alive it did not kill, and the kill intent that take set has to go: left standing, a death from
	// anything else while the giant idles would read as this branch's doing and the body would be kept
	// instead of ending the branch.
	void OnIdleReturn(const ActionContext& a_Ctx) {

		if (Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Hand)) {
			Possession::ClearIntent(a_Ctx.Owner(), Grabbing::Hand);
		}
	}

	void OnGraphExit(const ActionContext& a_Ctx) {
		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	// The hand only. Whoever is in the breasts is kept there by the store and outlives this node.
	constexpr PossessionSlot Owned[] = { PossessionSlot::kHand };

	constexpr EntryDef Entries[] = {
		{
			.Action = "Grab.Enter",
			.Behaviour = "GTSBEH_GrabStart",
			.Confirm = "GTSGrab_Catch_Actor",
			.OnTaken = TakeGrab,
		},
	};

	constexpr ActionDef ActionTable[] = {
		// Same name as the entry. A stored tiny keeps this node running, so a second grab lands here.
		{ .Action = "Grab.Enter",    .Behaviour = "GTSBEH_GrabStart",    .Guard = CatchSecond, .Cooldown = 0.30f, .Confirm = "GTSGrab_Catch_Actor" },

		{ .Action = "Grab.Attack",   .Behaviour = "GTSBEH_GrabAttack",   .Guard = InHand,      .Verify = VerifyAttack, .Kills = true, .Cooldown = 0.30f, .Input = "Action.Grab.Attack" },
		{ .Action = "Grab.Throw",    .Behaviour = "GTSBEH_GrabThrow",    .Guard = InHand,      .Verify = VerifyThrow,  .Cooldown = 0.30f, .Input = "Action.Grab.Throw" },
		{ .Action = "Grab.Vore",     .Behaviour = "GTSBEH_GrabVore",     .Guard = InHand,      .Kills = true, .Cooldown = 0.50f, .Input = "Action.Grab.Vore" },
		{ .Action = "Grab.Release",  .Behaviour = "GTSBEH_GrabRelease",  .Guard = CanRelease,  .OnTaken = TakeRelease, .Cooldown = 0.30f, .Input = "Action.Grab.Release" },

		{ .Action = "Grab.Store",    .Behaviour = "GTSBEH_BreastsAdd",   .Guard = CanStore,    .Cooldown = 0.30f, .Input = "Action.Grab.Breasts.Place" },
		{ .Action = "Grab.Unstore",  .Behaviour = "GTSBEH_BreastsRemove",.Guard = CanTakeBack, .Cooldown = 0.30f, .Input = "Action.Grab.Breasts.Remove" },

		// Into the two nested branches. Both are handoffs: the hand keeps its actor across them.
		{ .Action = "Grab.Play",     .Behaviour = "GTSBEH_Hand_Enter",   .Guard = InHand,      .OnTaken = TakePlay,  .HandOff = ActionId::kGrabPlay,  .Cooldown = 0.30f, .Input = "Action.Grab.GrabPlay.Enter" },
		{ .Action = "Cleavage.Enter", .Behaviour = "GTSBEH_Boobs_Enter",  .Guard = CanCleave, .OnTaken = TakeCleavage, .HandOff = ActionId::kCleavage, .Cooldown = 0.30f, .Input = "Action.Cleavage.Enter" },

		{ .Action = "Grab.Exit",     .Behaviour = "GTSBEH_ExitGrab",     .Guard = HasTiny,     .Exits = true, .Cooldown = 0.30f },

		// No key and no guard. A hit, the AI giving up, or the tiny being lost.
		{ .Action = "Grab.Cancel",   .Behaviour = "",                    .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSGrab_Catch_Start",       .Handler = OnCatchStart },
		{ .Tag = "GTSGrab_Catch_Actor",       .Handler = OnCatchActor },
		{ .Tag = "GTSGrab_Catch_End",         .Handler = OnCatchEnd },

		{ .Tag = "GTSGrab_Attack_MoveStart",  .Handler = OnAttackStart },
		{ .Tag = "GTSGrab_Attack_Damage",     .Handler = OnAttackDamage },
		{ .Tag = "GTSGrab_Attack_MoveStop",   .Handler = OnAttackStop },

		{ .Tag = "GTSGrab_Throw_MoveStart",   .Handler = OnThrowStart },
		{ .Tag = "GTSGrab_Throw_Throw_Pre",   .Handler = OnThrowPre, .ReleasesPartners = true },
		{ .Tag = "GTSGrab_Throw_ThrowActor",  .Handler = OnThrowActor },
		{ .Tag = "GTSGrab_Throw_MoveStop",    .Handler = OnThrowStop },
		{ .Tag = "GTSGrab_Throw_Throw_Post" },
		{ .Tag = "GTSGrab_Throw_FS_L",        .Handler = OnThrowFootL },
		{ .Tag = "GTSGrab_Throw_FS_R",        .Handler = OnThrowFootR },

		{ .Tag = "GTS_GrabSneak_Start",       .Handler = OnSneakVoreStart },
		{ .Tag = "GTS_GrabSneak_Eat",         .Handler = OnSneakVoreEat },
		{ .Tag = "GTS_GrabSneak_CamOff",      .Handler = OnSneakVoreCamOff },
		{ .Tag = "GTS_GrabSneak_KillAll",     .Handler = OnSneakVoreKillAll, .ReleasesPartners = true },
		{ .Tag = "GTS_Sneak_Vore_SmileOn",    .Handler = OnSneakVoreSmileOn },
		{ .Tag = "GTS_Sneak_Vore_OpenMouth",  .Handler = OnSneakVoreOpenMouth },
		{ .Tag = "GTS_Sneak_Vore_Swallow",    .Handler = OnSneakVoreSwallow },
		{ .Tag = "GTS_Sneak_Vore_SmileOff",   .Handler = OnSneakVoreSmileOff },
		{ .Tag = "GTS_Sneak_Vore_CloseMouth" },

		{ .Tag = "GTSGrab_Eat_Start",         .Handler = OnEatStart },
		{ .Tag = "GTSGrab_Eat_OpenMouth",     .Handler = OnEatOpenMouth },
		{ .Tag = "GTSGrab_Eat_Eat",           .Handler = OnEatSwallowStart },
		{ .Tag = "GTSGrab_Eat_CloseMouth" },
		{ .Tag = "GTSGrab_Eat_Swallow",       .Handler = OnEatSwallowEnd, .ReleasesPartners = true },

		{ .Tag = "GTSGrab_Breast_MoveStart",  .Handler = OnBreastMoveStart },
		{ .Tag = "GTSGrab_Breast_PutActor",   .Handler = OnBreastPut },
		{ .Tag = "GTSGrab_Breast_TakeActor",  .Handler = OnBreastTake },
		{ .Tag = "GTSGrab_Breast_MoveEnd",    .Handler = OnBreastMoveEnd },

		{ .Tag = "GTSGrab_Release_FreeActor", .Handler = OnReleaseFreeActor, .ReleasesPartners = true },
		// Not an exit signal, despite the name. It fires after an ordinary attack too, and arming the
		// drain there leaves it armed: the next action runs with the branch already counted as leaving,
		// so a kill during it ends the node early instead of letting the attack finish.
		{ .Tag = "GTSBEH_Grab_TinyDeath_Exit" },
		{ .Tag = "GTSBEH_Grab_Next",          .Handler = OnIdleReturn, .Shared = true },
		{ .Tag = "GTSBEH_ExitGrab",           .Handler = OnGraphExit, .Shared = true },

		// Fires when the graph leaves the breast storage pose. CleavageNode declares it too, so
		// without this it reads as an orphan every time a tiny is stored.
		{ .Tag = "GTSBEH_BoobExit" },
	};

	//----------------------------------------------------------------------------------------------
	// Input
	//----------------------------------------------------------------------------------------------

	void GrabEvent(const ManagedInputEvent&) {

		if (PlayerTarget::Engaged("Action.Grab.Grab")) {
			PlayerTarget::Perform("Grab.Enter");
			return;
		}

		auto* player = PlayerCharacter::GetSingleton();
		auto& controller = GrabAnimationController::GetSingleton();

		for (auto* prey : controller.GetGrabTargetsInFront(player, 1)) {
			GrabAnimationController::StartGrab(player, prey);
		}
	}

}

namespace GTS::Actions {

	std::span<const GraphExpect> GrabNode::Signature() const { return ::Signature; }
	std::string_view GrabNode::AbortSignal() const { return Grabbing::BEH_ABORT; }
	std::span<const std::string_view> GrabNode::ExitSignals() const { return ::ExitSignals; }
	std::span<const EntryDef> GrabNode::Entries() const { return ::Entries; }
	// Alive while the giant carries anyone, in the hand or the breasts. Storing, taking back and
	// entering the cleavage are actions of this node, so it has to still be running to reach them.
	Liveness GrabNode::Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const {
		const bool held = Possession::Occupied(a_Owner, PossessionSlot::kHand) || Possession::Occupied(a_Owner, PossessionSlot::kBreasts);
		return held ? Liveness::kAlive : Liveness::kOver;
	}

	std::span<const PossessionSlot> GrabNode::OwnedSlots() const { return ::Owned; }

	bool GrabNode::Permits(ActionId a_Id, RE::FormID a_Owner) const {

		// Nothing in the hand, so this node is only here because someone is stored in the breasts.
		// That gets in the way of nothing: the entries that do care about a full hand refuse for
		// themselves through BlockedBy.
		if (!Possession::Occupied(a_Owner, PossessionSlot::kHand)) {
			return true;
		}

		// Holding someone. The carry loop blends with locomotion, so the feet still work, and anything
		// needing the hands, the lap or the whole body is refused.
		return a_Id == ActionId::kStomp || a_Id == ActionId::kTrample || a_Id == ActionId::kKickSwipe;
	}

	std::span<const ActionDef> GrabNode::Actions() const { return ::ActionTable; }
	std::span<const AnnotationDef> GrabNode::Annotations() const { return ::Annotations; }

	GrabNode::State* GrabNode::Get(RE::FormID a_Owner) { return m_State.Find(a_Owner); }
	GrabNode::State& GrabNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	// The controller binds the actor immediately before it asks, so a real request always has one.
	bool GrabNode::CanEnter(const EntryContext& a_Ctx) const {
		return a_Ctx.Deferred(a_Ctx.Occupied(Grabbing::Hand));
	}

	void GrabNode::RegisterInput() {
		Keybinds::NoteBindKind("Action.Grab.Grab", true);
		InputManager::RegisterInputEvent("Action.Grab.Grab", GrabEvent, GrabCondition_Start);
	}

	bool GrabNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view, bool a_Explain) const {

		if (!CanDoActionBasedOnQuestProgress(a_Target, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}

		auto& controller = GrabAnimationController::GetSingleton();

		controller.AllowMessage(a_Explain);
		const bool started = GrabAnimationController::StartGrab(a_Actor, a_Target);
		controller.AllowMessage(false);

		return started;
	}

	void GrabNode::OnEnter(const ActionContext& a_Ctx) {
		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		// Coming back after a stomp the tiny may already be stored, and the flag only ever tracked
		// what the two breast annotations did.
		state.Stored = Possession::Occupied(a_Ctx.Owner(), PossessionSlot::kBreasts);
	}

	void GrabNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		// A handoff is the grab passing to grab play or to the cleavage state. The actor stays held
		// and the hand keeps carrying, so nothing here is undone.
		if (KeepsState(a_Reason)) {
			return;
		}

		// Tells the graph the hand is empty, which is what takes it out of the hold. Without it the
		// grab runs GTSBEH_Grab_Next and loops back into holding nobody. A completed exit sends no
		// abort signal, so this is the only thing saying the branch is over.
		Grabbing::HandEmptied(giant);

		if (giant) {

			AnimationVars::Grab::SetHasGrabbedTiny(giant, false);
			AnimationVars::Grab::SetGrabState(giant, false);
			AnimationVars::Action::SetIsCleavageZOverrideEnabled(giant, false);

			Attachment_SetTargetNode(giant, AttachToNode::None);
			ManageCamera(giant, false, CameraTracking::Grab_Left);

			DrainStamina(giant, TASKID_ATTACK, Runtime::PERK.GTSPerkDestructionBasics, false, 0.75f);
			DrainStamina(giant, TASKID_THROW, Runtime::PERK.GTSPerkDestructionBasics, false, 1.25f);

			Grabbing::StopHandRumble(RUMBLE_CATCH, giant);
			Grabbing::StopHandRumble(RUMBLE_ATTACK, giant);
			Grabbing::StopHandRumble(RUMBLE_THROW, giant);
			Grabbing::StopHandRumble(RUMBLE_VORE, giant);

			// The throw already let go and launched them. Anyone still in the hand is being put down;
			// the breasts are not this node's to empty.
			const State* state = m_State.Find(owner);

			if (auto* tiny = Possession::FirstActor(owner, PossessionSlot::kHand); tiny && !(state && state->Thrown)) {
				Grabbing::LetGo(giant, tiny, false);
			}
		}

		m_State.Forget(owner);
	}

	void GrabNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void GrabNode::OnReset() {
		m_State.Clear();
	}

	std::string_view GrabNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);

		if (!state) {
			return "";
		}

		if (state->Thrown) {
			return "Thrown";
		}

		return state->Stored ? "Stored" : "Hand";
	}
}
