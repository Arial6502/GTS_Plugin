#include "Actions/Nodes/Grab/GrabPlayNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/Grab/GrabCommon.hpp"

#include "Magic/Effects/Common.hpp"

#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Rumble.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Captured end to end, standing and crawling. Times are from one run and are illustrative only.

  This node is only ever reached by handoff from GrabNode. Grab.Play sends GTSBEH_Hand_Enter to the
  giant and GTSBEH_T_Hand_Enter to the tiny, and GrabPlay.Exit hands back the same way. The tiny is
  held in the hand for the whole branch, across both handoffs, so neither node lets go.

  Sends to the tiny are best effort. A creature has no GTS_ variables and refuses all of them, and
  nothing here depends on the answer.

  Like the hug this is a loop, not a sequence. After the intro the giant sits in the idle until a key
  sends one of the actions below. Poke, kiss, sandwich, crush and grind return to the idle on
  GTSBEH_Next. Vore, kiss vore and flick leave, and so does a crush or sandwich that kills.

  The branch borrows two annotations from the cleavage state: GTS_BS_SwitchToObjectB moves the tiny
  onto the mouth for the kiss vore, and GTS_BS_KillAll fires at the end of both vores. Legacy
  registered annotation handlers by tag for the whole DLL so both branches ran them; scoped to a
  node they have to be declared in each, which is why the first appears here as well as in
  CleavageNode.
*/
/*
  ----- Grab Play Standing [handoff from Grab, "GTSBEH_Hand_Enter" + "GTSBEH_T_Hand_Enter"]
  [INTRO]
    Collision_RecoilCancelable  [NON GTS]
    IdleOffsetStop              [NON GTS]
    GTS_ToLeft                  [NOT REGISTERED] the tiny is moved to the left hand
    GTS_HS_CamOn
    GTS_HS_TossTiny
    FootScuffRight              [NON GTS]
    GTS_HS_TinyLand
    FootScuffLeft               [NON GTS]
    GTSBEH_Next               -> settles into the idle loop
  [POKE ("GTSBEH_Hand_Poke")]
    GTS_HS_Poke_TinyLand      -> rumble and hearts, this is the only poke annotation that fires
    GTSBEH_Next               -> back to the idle loop
    GTS_HS_Poke_WindUp and GTS_HS_Poke_Contact are declared with no handler. The graph never fires
    them, so what legacy hung on Contact is on TinyLand instead.
  [SANDWICH, the light crush ("GTSBEH_Hand_Sandwich")]
    GTSBeh_NoTinyExit_WinStart  -> the window in which the graph will take the no tiny exit
    GTS_HS_Sand_WindUp
    GTS_HS_Sand_Lower
    GTS_HS_Sand_Hit           -> damage lands here, the tiny can die
    GTS_HS_Sand_Release       -> if the hand is empty the branch ends itself, see EXIT
  [CRUSH, the heavy one ("GTSBEH_Hand_Crush_Heavy")]
    GTSBeh_NoTinyExit_WinStart
    GTS_HS_Fist_WindUp
    GTS_HS_Fist_Lower
    GTS_HS_Fist_Hit           -> damage lands here, the tiny can die
    GTS_HS_Fist_Release       -> same ending as the sandwich
  [FLICK ("GTSBEH_Hand_Flick_Heavy")]
    GTS_HS_Flick_Launch       -> the tiny is launched and released here
    GTS_HS_Flick_Ragdoll
    GTS_HS_CamOff             -> exit signal
  [KISS ("GTSBEH_Hand_Kiss")]
    GTS_HS_Kiss_Start
    GTS_HS_KissSound_Play     -> heals the tiny 10% of max health
    GTSBeh_Interupt_WinStart  -> the kiss vore can only be started between here and WinEnd
    GTS_HS_KissSound_Stop
    GTS_HS_Kiss_Stop
    GTSBeh_Interupt_WinEnd
    GTSBeh_Interupt_TransEnd
    GTSBEH_Next               -> back to the idle loop
  [KISS VORE ("GTSBEH_Hand_Kiss_Vore", during the kiss interrupt window)]
    GTS_HS_KissSound_Stop
    GTS_HS_Kiss_Stop
    GTSBeh_Interupt_WinEnd
    GTS_HS_K_Vore_OpenMouth   -> the tiny is added to the vore data and starts shrinking
    GTS_BS_SwitchToObjectB    [SHARED WITH CLEAVAGE] moves the tiny off the hand and onto the mouth
    GTS_HS_K_Vore_TinyMuffle
    GTS_HS_K_Vore_SlurpTiny
    GTS_HS_K_Vore_SlurpTiny_End
    GTS_HS_K_Vore_TinyInMouth
    GTS_HS_K_Vore_CloseMouth
    GTS_HS_K_Vore_SwallowTiny -> the tiny is eaten and released here
    GTS_BS_KillAll            [SHARED WITH CLEAVAGE] declared with no handler, the swallow above
                                 has already released the tiny
    GTSBeh_ExitEvents         [NON GTS]
    GTS_HS_CamOff             -> exit signal
  [VORE ("GTSBEH_Hand_Vore")]
    GTS_HS_SmileOn
    GTS_HS_SmileOff
    GTS_HS_Vore_OpenMouth
    GTS_HS_Vore_CloseMouth
    GTS_ToAnimB               [NOT REGISTERED]
    GTS_HS_Vore_TinyInMouth
    GTS_HS_Vore_SwallowTiny
    GTS_BS_KillAll            [SHARED WITH CLEAVAGE] declared with no handler, the swallow above
                                 has already released the tiny
    GTSBeh_ExitEvents         [NON GTS]
    GTS_HS_CamOff             -> exit signal
  [GRIND ("GTSBEH_Hand_Grind_Start" / "_Stop")]
    GTSBEH_NextAlt            -> the grind loop, it holds here until the stop
    GTSBEH_Next               -> after the stop, back to the idle loop
    The grind has no annotations of its own and no effect on the tiny. Legacy was the same.
  [EXIT ("GTSBEH_Hand_Exit", handoff back to Grab)]
    GTS_HS_SmileOn
    GTS_HS_CamOff
    GTS_HS_SmileOff
  [CLEAN UP, on the paths that leave rather than hand back]
    GTSBeh_NoTinyExit_TransEnd  -> only after a kill
    GTS_HS_Exit_NoTiny          -> only after a kill, and it arrives after the node has gone
    GTSBEH_Exit         [NON GTS]
    GTS_HS_CamOff
    MTState             [NON GTS]
    arrowDetach         [NON GTS]
    GTSBeh_MT           [NON GTS]
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailMTIdle          [NON GTS]

  **Info: a kill does not announce itself in time to be useful. The graph reaches GTS_HS_Exit_NoTiny
  **more than a second after the crush ends, and by then it has already dropped back towards the play
  **idle holding nobody. The release annotation is where the branch has to end itself: it sends
  **GTSBEH_AbortGrab, which is the only event that leaves the grab root, then GTSBEH_TinyDied.
*/
/*
  ----- Grab Play Crawling [same entry events]
  ----- The GTSCrawl_* impacts belong to the crawl locomotion, not to this branch. They fire
  ----- throughout and are handled by Crawling.cpp.
  [INTRO]
    Same as standing, except GTSCrawl_KneeImpact_R / _L replace the foot scuffs.
  [EVERY ACTION]
    Same annotations in the same order as standing, with two exceptions below.
  [KISS VORE]
    GTS_HS_K_Vore_CloseMouth fires early here, right after TinyMuffle, instead of just before
    SwallowTiny. Nothing keys off the order, but do not assume the standing sequence.
    The branch ends on GTSBEH_NextAlt then GTS_HS_CamOff rather than GTSBeh_ExitEvents.
  [CRUSH]
    Plays GTS_Hand_FistCrush_Sneak.hkx rather than the standing clip. Its GTSBEH_Next clip trigger
    was set to fire from the start of the clip instead of the end, so the crush was entered and left
    in the same frame and read in game as the giant twitching. Fixed in the behaviour, not here.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/
/*
   ----- Grab Play, all stances
   [SET ON THE HANDOFF IN, THE SAME FRAME GRAB LEAVES]
     GTS_Busy                -> TRUE
     GTS_IsInGrabPlayState   -> TRUE   (graph owned, and this node's signature)
   (NON GTS)
     GTSTDM_Dodge            -> TRUE
     bAnimationDriven        -> TRUE
     bNoStagger              -> TRUE
     tdmHeadtrackingBehavior -> FALSE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
   (NON GTS)
     tdmHeadtrackingBoth     -> FALSE
   [SHORTLY AFTER]
   (NON GTS)
     bIdleBeforeConversation -> FALSE
     bVoiceReady             -> FALSE
   [WHILE POKING, FLICKING OR GRINDING]
     GTS_IsPlaying           -> TRUE, back to FALSE when the action ends
   [WHILE KISSING]
     GTS_AllowInterupt       -> TRUE   this is what lets the kiss vore start
     GTS_IsKissing           -> TRUE
   [WHILE SANDWICHING OR CRUSHING]
     GTSBeh_DontExit         -> TRUE
     GTS_IsHandCrushing      -> TRUE
     GTS_IsPlaying           -> FALSE  the crush clears what a poke before it had set
   [WHILE VORING, EITHER KIND]
     GTSBeh_DontExit         -> TRUE
     GTS_IsGrabAttacking     -> TRUE
     GTS_IsVoring            -> TRUE
   (NON GTS)
     currentDefaultState     -> FALSE
   [WHEN THE TINY DIES OR IS EATEN]
     GTS_GrabbedTiny         -> FALSE  (written by the DLL: Possession drops the actor and the graph
                                        variables follow the store)
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **UNVERIFIED: this capture has GTS_IsInGrabPlayState going TRUE, and the comment on Alive() says it
   **never asserts at all. Both cannot be right and it has not been settled. Nothing rests on the
   **answer today: the signature is never what enters this node, the handoff in is, and Alive() reads
   **the hand instead. Confirm it with "gts action watch" before relying on the variable for anything.

   **Info: the handoff out is not clean in the variables. GrabPlay.Exit hands back to Grab in the
   **same frame, but GTS_IsInGrabPlayState stays TRUE for around three seconds afterwards while the
   **giant plays the outro. Nothing may read that variable as meaning this node is current.
*/

using namespace GTS;

namespace {

	using namespace GTS::Actions;
	using State = GrabPlayNode::State;

	constexpr std::string_view LHAND = "NPC L Hand [LHnd]";
	constexpr std::string_view HEAD = "NPC Head [Head]";

	// Graph owned, unlike the grab's own, so it is evidence on its own and needs no Confirm. The
	// cleavage state is excluded because it runs inside the same group.
	constexpr GraphExpect Signature[] = {
		{ "GTS_IsInGrabPlayState", true },
		{ "GTS_IsBoobing", false },
	};

	constexpr std::string_view ExitSignals[] = {
		"GTS_HS_CamOff",
		"GTSBEH_Hand_Exit",
	};

	// Whoever is in the hand, dead or alive. A crush that kills leaves the body there for the rest of
	// the animation, and the handlers after it still work on that body.
	RE::Actor* Held(const ActionContext& a_Ctx) {
		return Possession::FirstActor(a_Ctx.Owner(), PossessionSlot::kHand);
	}

	RE::Actor* Living(const ActionContext& a_Ctx) {
		return Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kHand);
	}

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	// Nothing is offered once the tiny is being eaten or a grind is running: both own the state until
	// they finish, and the graph has no transition out of either.
	bool CanAct(const ActionContext& a_Ctx) {
		const State* state = GrabPlayNode::Get(a_Ctx.Owner());
		return Living(a_Ctx) && !(state && (state->Devouring || state->Grinding));
	}

	bool CanGrindStop(const ActionContext& a_Ctx) {
		const State* state = GrabPlayNode::Get(a_Ctx.Owner());
		return state && state->Grinding;
	}

	bool CanLeave(const ActionContext& a_Ctx) {
		const State* state = GrabPlayNode::Get(a_Ctx.Owner());
		return !(state && state->Devouring);
	}

	//----------------------------------------------------------------------------------------------
	// Takes. Every action here is paired: the giant's half and the tiny's.
	//----------------------------------------------------------------------------------------------

	void Pair(const ActionContext& a_Ctx, std::string_view a_Tiny) {
		a_Ctx.SendToHeld(Grabbing::Hand, a_Tiny);
	}

	// The tiny has no transition for this yet, so it refuses it and plays nothing. Sent anyway: the
	// giant's half does not depend on it, and the name stays honest for when the graph gains one.
	void TakeCrush(const ActionContext& a_Ctx)    { Pair(a_Ctx, "GTSBEH_T_Hand_Crush_Heavy"); }
	void TakePoke(const ActionContext& a_Ctx)     { Pair(a_Ctx, "GTSBEH_T_Hand_Poke"); }
	void TakeFlick(const ActionContext& a_Ctx)    { Pair(a_Ctx, "GTSBEH_T_Hand_Flick_Heavy"); }
	void TakeSandwich(const ActionContext& a_Ctx) { Pair(a_Ctx, "GTSBEH_T_Hand_Sandwich"); }
	void TakeKiss(const ActionContext& a_Ctx)     { Pair(a_Ctx, "GTSBEH_T_Hand_Kiss"); }
	void TakeExit(const ActionContext& a_Ctx)     { Pair(a_Ctx, "GTSBEH_T_Hand_Exit"); }

	void TakeVore(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Hand_Vore");
		GrabPlayNode::GetOrAdd(a_Ctx.Owner()).Devouring = true;
	}

	void TakeKissVore(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Hand_Kiss_Vore");
		GrabPlayNode::GetOrAdd(a_Ctx.Owner()).Devouring = true;
	}

	void TakeGrindStart(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Hand_Grind_Start");
		GrabPlayNode::GetOrAdd(a_Ctx.Owner()).Grinding = true;
	}

	void TakeGrindStop(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Hand_Grind_Stop");
		GrabPlayNode::GetOrAdd(a_Ctx.Owner()).Grinding = false;
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnSmileOn(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_SlightSmile(a_Ctx.Actor(), RandomFloat(0.6f, 0.8f), "Grab_Smile", 0.125f);
	}

	void OnCamOn(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		ManageCamera(giant, true, CameraTracking::ObjectL);

		if (auto* tiny = Held(a_Ctx)) {
			Grabbing::Play::Task_CopyAnimationSpeed(giant, tiny);
		}
	}

	void OnCamOff(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::ObjectL);
		Attachment_SetTargetNode(a_Ctx.Actor(), AttachToNode::None);
	}

	void OnTossTiny(const ActionContext& a_Ctx) {
		Rumbling::Once("TossTiny", a_Ctx.Actor(), 1.25f, 0.05f, LHAND, true);
	}

	void OnTinyLand(const ActionContext& a_Ctx) {
		Rumbling::Once("TossTiny", a_Ctx.Actor(), 1.65f, 0.05f, LHAND, true);
		Grabbing::Play::GTSGrab_SpawnHeartsAtHead(a_Ctx.Actor(), 0.0f, 0.45f);
	}

	void OnPokeWindUp(const ActionContext& a_Ctx) {
		Rumbling::Once("PokeTiny", a_Ctx.Actor(), 1.0f, 0.05f, LHAND, true);
	}

	void OnPokeContact(const ActionContext& a_Ctx) {
		Rumbling::Once("PokeTiny_C", a_Ctx.Actor(), 1.75f, 0.05f, LHAND, true);
		Grabbing::Play::GTSGrab_SpawnHeartsAtHead(a_Ctx.Actor(), 0.0f, 0.45f);
	}

	void OnPokeTinyLand(const ActionContext& a_Ctx) {
		Rumbling::Once("PokeTiny_L", a_Ctx.Actor(), 1.25f, 0.05f, LHAND, true);
	}

	void OnFlickLaunch(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Rumbling::Once("FlichTiny_Launch", giant, 0.75f, 0.05f, LHAND, true);
		Grabbing::Play::GTSGrab_SpawnHeartsAtHead(giant, 0.0f, 0.45f);
		Grabbing::Play::GTSGrab_KickTiny(giant);
		Grabbing::Play::ResetTinyAnimSpeed(giant);

		// The flick throws them out of the hand, so the whole group is over rather than just this
		// branch. There is nobody left to hand back.
		a_Ctx.RequestExit();
	}

	void OnFlickRagdoll(const ActionContext& a_Ctx) {
		Rumbling::Once("FlichTiny_Land", a_Ctx.Actor(), 2.1f, 0.05f, LHAND, true);
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundSwingImpact, a_Ctx.Actor(), 0.75f, LHAND);
	}

	void OnSandHit(const ActionContext& a_Ctx) {
		Grabbing::Play::GTSGrab_Do_Damage(a_Ctx.Actor(), Damage_Grab_Play_Light);
		Rumbling::Once("SmashTiny_L", a_Ctx.Actor(), 1.65f, 0.05f, LHAND, true);
	}

	void OnFistHit(const ActionContext& a_Ctx) {
		Grabbing::Play::GTSGrab_Do_Damage(a_Ctx.Actor(), Damage_Grab_Play_Heavy);
		Rumbling::Once("SmashTiny_H", a_Ctx.Actor(), 2.25f, 0.05f, LHAND, true);
	}

	void OnKissStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Rumbling::Once("KissTiny", giant, 2.25f, 0.05f, HEAD, true);
		Task_FacialEmotionTask_Kiss(giant, 1.25f, "KissTiny", 0.325f);
		Grabbing::Play::GTSGrab_SpawnHeartsAtHead(giant, 0.0f, 0.45f);

		if (auto* tiny = Held(a_Ctx)) {
			Task_FacialEmotionTask_Kiss(tiny, 1.25f, "KissedByGts", 0.325f);
		}
	}

	void OnKissSound(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Runtime::PlaySoundAtNode_FallOff(Runtime::SNDR.GTSSoundKissing, giant, 1.0f, HEAD, 0.11f * get_visual_scale(giant));

		if (auto* tiny = Held(a_Ctx)) {
			tiny->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, GetMaxAV(tiny, ActorValue::kHealth) * 0.1f);
		}
	}

	void OnKissVoreOpenMouth(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Task_FacialEmotionTask_OpenMouth(giant, 2.2f, "GrabVoreOpenMouth", 0.65f);
		Grabbing::Play::GTSGrab_AddToVoreData(giant);
		ManageCamera(giant, true, CameraTracking::ObjectB);
		Grabbing::Play::FixKissVoreTinyOffset(giant, true);

		if (auto* tiny = Held(a_Ctx)) {
			VoreController::GetSingleton().ShrinkOverTime(giant, tiny, 0.05f, 6.0f);
		}
	}

	// The kiss vore reuses the cleavage branch's attachment tags. Legacy registered annotation
	// handlers by tag for the whole DLL, so these fired here too; scoped to a node they have to be
	// declared in both. Without them the tiny is never moved off the hand.
	void OnSwitchToObjectB(const ActionContext& a_Ctx) {
		Attachment_SetTargetNode(a_Ctx.Actor(), AttachToNode::ObjectB);
	}

	void OnSwitchBack(const ActionContext& a_Ctx) {
		Attachment_SetTargetNode(a_Ctx.Actor(), AttachToNode::None);
	}

	void OnKissVoreCloseMouth(const ActionContext& a_Ctx) {
		Grabbing::Play::GTSGrab_SetBeingEaten(a_Ctx.Actor());
	}

	void OnSlurpEnd(const ActionContext& a_Ctx) {
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundSwallow, a_Ctx.Actor(), 1.0f, HEAD);
	}

	void OnTinyInMouth(const ActionContext& a_Ctx) {
		Grabbing::Play::GTSGrab_SwallowTiny(a_Ctx.Actor());
	}

	void OnKissVoreSwallow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Grabbing::Play::FixKissVoreTinyOffset(giant, false);
		Grabbing::Play::GTSGrab_DelayedSmile(giant, 1.2f);
		Grabbing::Play::GTSGrab_FullyEatTiny(giant);
	}

	void OnVoreOpenMouth(const ActionContext& a_Ctx) {
		Grabbing::Play::GTSGrab_DelayedVore(a_Ctx.Actor(), 0.2f);
	}

	void OnVoreSwallow(const ActionContext& a_Ctx) {
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundSwallow, a_Ctx.Actor(), 1.0f, HEAD);
		Grabbing::Play::GTSGrab_FullyEatTiny(a_Ctx.Actor());
		a_Ctx.RequestExit();
	}

	// The tiny died in here. There is nobody to hand back, so the group ends.
	// The crush can kill, and when it does the graph runs its own death exit and then drops back into
	// the play idle holding nobody. Only GTSBEH_AbortGrab leaves the grab root, and a clean exit sends
	// no abort of its own, so the branch sends it here at the end of the crush. Later than this the
	// graph has moved on and refuses it. HandEmptied is part of it: it stops the carry, drops the
	// grab's camera and sends GTSBEH_TinyDied.
	//
	// Asks for a living actor rather than an empty hand. A crush the action declared is left holding
	// the body it killed, so the hand is not empty and the release annotation is still where the
	// branch has to end.
	void EndIfTinyGone(const ActionContext& a_Ctx) {

		if (Living(a_Ctx)) {
			return;
		}

		// Someone is still stored, so leave through the hand exit and keep the grab root. AbortGrab is
		// the fallback if the graph refuses the exit here.
		if (Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Breasts)) {

			if (auto* body = Held(a_Ctx)) {
				Grabbing::LetGo(a_Ctx.Actor(), body, false);
			}

			Grabbing::HandEmptied(a_Ctx.Actor());
			Possession::Release(a_Ctx.Owner(), PossessionSlot::kHand);

			if (a_Ctx.Send("GTSBEH_Hand_Exit", true)) {
				a_Ctx.HandOff(ActionId::kGrab);
				return;
			}
		}

		ActionRegistry::Notify(a_Ctx.Actor(), Grabbing::BEH_ABORT);
		Grabbing::HandEmptied(a_Ctx.Actor());
		a_Ctx.RequestExit();
	}

	void OnSandRelease(const ActionContext& a_Ctx) { EndIfTinyGone(a_Ctx); }
	void OnFistRelease(const ActionContext& a_Ctx) { EndIfTinyGone(a_Ctx); }

	// Back in the idle, so whatever action was running is over. If the actor it was aimed at is still
	// alive it did not kill, and the kill intent that take set has to go: left standing, a death from
	// anything else while the giant idles would read as this branch's doing and the body would be kept
	// instead of ending the branch.
	void OnIdleReturn(const ActionContext& a_Ctx) {

		if (Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kHand)) {
			Possession::ClearIntent(a_Ctx.Owner(), PossessionSlot::kHand);
		}
	}

	// The graph fires this with a tiny still in the hand: a sandwich that did not kill runs the whole
	// no-tiny window and takes this exit anyway, with GTS_GrabbedTiny asserted throughout. Taking it
	// at its word ended the branch and dropped a live actor, so it is only acted on when the hand is
	// really empty. It is not an exit signal either, for the same reason.
	void OnExitNoTiny(const ActionContext& a_Ctx) {

		if (Held(a_Ctx)) {
			return;
		}

		EndIfTinyGone(a_Ctx);
		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr PossessionSlot Owned[] = { PossessionSlot::kHand };

	constexpr ActionDef ActionTable[] = {
		{ .Action = "GrabPlay.Crush",      .Behaviour = "GTSBEH_Hand_Crush_Heavy", .Guard = CanAct,       .OnTaken = TakeCrush,      .Kills = true, .Cooldown = 0.30f, .Input = "Action.Grab.GrabPlay.Crush" },
		{ .Action = "GrabPlay.Poke",       .Behaviour = "GTSBEH_Hand_Poke",        .Guard = CanAct,       .OnTaken = TakePoke,       .Cooldown = 0.30f,                .Input = "Action.Grab.GrabPlay.Poke" },
		{ .Action = "GrabPlay.Flick",      .Behaviour = "GTSBEH_Hand_Flick_Heavy", .Guard = CanAct,       .OnTaken = TakeFlick,      .Cooldown = 0.30f,                .Input = "Action.Grab.GrabPlay.Flick" },
		{ .Action = "GrabPlay.Sandwich",   .Behaviour = "GTSBEH_Hand_Sandwich",    .Guard = CanAct,       .OnTaken = TakeSandwich,   .Kills = true, .Cooldown = 0.30f, .Input = "Action.Grab.GrabPlay.Sandwich" },
		{ .Action = "GrabPlay.Kiss",       .Behaviour = "GTSBEH_Hand_Kiss",        .Guard = CanAct,       .OnTaken = TakeKiss,       .Cooldown = 0.30f,                .Input = "Action.Grab.GrabPlay.Kiss" },
		{ .Action = "GrabPlay.Vore",       .Behaviour = "GTSBEH_Hand_Vore",        .Guard = CanAct,       .OnTaken = TakeVore,       .Kills = true, .Cooldown = 0.50f, .Input = "Action.Grab.GrabPlay.Vore" },
		{ .Action = "GrabPlay.KissVore",   .Behaviour = "GTSBEH_Hand_Kiss_Vore",   .Guard = CanAct,       .OnTaken = TakeKissVore,   .Kills = true, .Cooldown = 0.50f, .Input = "Action.Grab.GrabPlay.KissVore" },

		{ .Action = "GrabPlay.GrindStart", .Behaviour = "GTSBEH_Hand_Grind_Start", .Guard = CanAct,       .OnTaken = TakeGrindStart, .Cooldown = 0.30f, .Input = "Action.Grab.GrabPlay.GrindStart" },
		{ .Action = "GrabPlay.GrindStop",  .Behaviour = "GTSBEH_Hand_Grind_Stop",  .Guard = CanGrindStop, .OnTaken = TakeGrindStop,  .Cooldown = 0.30f, .Input = "Action.Grab.GrabPlay.GrindStop" },

		// Back to the carry. A handoff, so the tiny stays in the hand across it.
		{ .Action = "GrabPlay.Exit",       .Behaviour = "GTSBEH_Hand_Exit",        .Guard = CanLeave,     .OnTaken = TakeExit, .HandOff = ActionId::kGrab, .Cooldown = 0.30f, .Input = "Action.Grab.GrabPlay.Exit" },

		{ .Action = "GrabPlay.Cancel",     .Behaviour = "",                        .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_HS_SmileOn",              .Handler = OnSmileOn },
		{ .Tag = "GTS_HS_CamOn",                .Handler = OnCamOn },
		{ .Tag = "GTS_HS_CamOff",               .Handler = OnCamOff },
		{ .Tag = "GTS_HS_TossTiny",             .Handler = OnTossTiny },
		{ .Tag = "GTS_HS_TinyLand",             .Handler = OnTinyLand },

		{ .Tag = "GTS_HS_Poke_WindUp" },
		{ .Tag = "GTS_HS_Poke_Contact" },
		{ .Tag = "GTS_HS_Poke_TinyLand",        .Handler = OnPokeTinyLand },

		{ .Tag = "GTS_HS_Flick_Launch",         .Handler = OnFlickLaunch, .ReleasesPartners = true },
		{ .Tag = "GTS_HS_Flick_Ragdoll",        .Handler = OnFlickRagdoll },

		{ .Tag = "GTS_HS_Sand_Hit",             .Handler = OnSandHit },
		{ .Tag = "GTS_HS_Fist_Hit",             .Handler = OnFistHit },

		{ .Tag = "GTS_HS_Kiss_Start",           .Handler = OnKissStart },
		{ .Tag = "GTS_HS_KissSound_Play",       .Handler = OnKissSound },

		{ .Tag = "GTS_BS_SwitchToObjectB",      .Handler = OnSwitchToObjectB },
		{ .Tag = "GTS_BS_SwitchToCleavage",     .Handler = OnSwitchBack },
		{ .Tag = "GTS_HS_K_Vore_OpenMouth",     .Handler = OnKissVoreOpenMouth },
		{ .Tag = "GTS_HS_K_Vore_CloseMouth",    .Handler = OnKissVoreCloseMouth },
		{ .Tag = "GTS_HS_K_Vore_SlurpTiny_End", .Handler = OnSlurpEnd },
		{ .Tag = "GTS_HS_K_Vore_TinyInMouth",   .Handler = OnTinyInMouth },
		{ .Tag = "GTS_HS_K_Vore_SwallowTiny",   .Handler = OnKissVoreSwallow, .ReleasesPartners = true },

		{ .Tag = "GTS_HS_Vore_OpenMouth",       .Handler = OnVoreOpenMouth },
		{ .Tag = "GTS_HS_Vore_TinyInMouth",     .Handler = OnTinyInMouth },
		{ .Tag = "GTS_HS_Vore_SwallowTiny",     .Handler = OnVoreSwallow, .ReleasesPartners = true },

		{ .Tag = "GTSBEH_Next",                 .Handler = OnIdleReturn, .Shared = true },
		// No ReleasesPartners. Core acts on that flag whatever the handler decides, and this annotation
		// fires with a live tiny still in the hand, so the release dropped them.
		{ .Tag = "GTS_HS_Exit_NoTiny",          .Handler = OnExitNoTiny },

		// Fire, carry no work of their own, and are listed so the trace does not call them orphans.
		{ .Tag = "GTS_HS_SmileOff" },
		{ .Tag = "GTS_HS_Sand_WindUp" },
		{ .Tag = "GTS_HS_Sand_Lower" },
		{ .Tag = "GTS_HS_Sand_Release",         .Handler = OnSandRelease },
		{ .Tag = "GTS_HS_Fist_WindUp" },
		{ .Tag = "GTS_HS_Fist_Lower" },
		{ .Tag = "GTS_HS_Fist_Release",         .Handler = OnFistRelease },
		{ .Tag = "GTS_HS_Kiss_Stop" },
		{ .Tag = "GTS_HS_KissSound_Stop" },
		{ .Tag = "GTS_BS_KillAll" },
		{ .Tag = "GTS_HS_K_Vore_TinyYell" },
		{ .Tag = "GTS_HS_K_Vore_TinyMuffle" },
		{ .Tag = "GTS_HS_K_Vore_SlurpTiny" },
		{ .Tag = "GTS_HS_Vore_CloseMouth" },
	};
}

namespace GTS::Actions {

	// Shadow until the cleavage branch is ported. The legacy grab files still register their own
	// handlers, and both would run: doubled damage, doubled camera, doubled sounds. Shadow tracks and
	// logs the node so a capture can prove its signature and annotation order, and runs none of its
	// callbacks. Flip to kLive in the same change that makes those files inert.
	// The hand, like the grab it sits inside. A tiny in the breasts is not part of this branch.
	std::span<const PossessionSlot> GrabPlayNode::OwnedSlots() const { return ::Owned; }
	std::span<const GraphExpect> GrabPlayNode::Signature() const { return ::Signature; }
	std::string_view GrabPlayNode::AbortSignal() const { return Grabbing::BEH_ABORT; }
	std::span<const std::string_view> GrabPlayNode::ExitSignals() const { return ::ExitSignals; }
	// GTS_IsInGrabPlayState is read by the graph and written by nobody, so it never asserts and the
	// signature can never match. The hand is what this state actually depends on.
	Liveness GrabPlayNode::Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const {

		return Possession::Occupied(a_Owner, PossessionSlot::kHand) ? Liveness::kAlive : Liveness::kOver;
	}

	std::span<const ActionDef> GrabPlayNode::Actions() const { return ::ActionTable; }
	std::span<const AnnotationDef> GrabPlayNode::Annotations() const { return ::Annotations; }

	GrabPlayNode::State* GrabPlayNode::Get(RE::FormID a_Owner) { return m_State.Find(a_Owner); }
	GrabPlayNode::State& GrabPlayNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	void GrabPlayNode::OnEnter(const ActionContext& a_Ctx) {
		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};
	}

	void GrabPlayNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		// The cameras and the attach target belong to this branch either way, handoff included. The
		// target is where the tiny is pinned right now, and the kiss vore moves it to ObjectB. Left
		// set, the branch taking over keeps attaching there: a stored tiny stays stuck by the hand.
		if (giant) {
			ManageCamera(giant, false, CameraTracking::ObjectL);
			ManageCamera(giant, false, CameraTracking::ObjectB);
			Attachment_SetTargetNode(giant, AttachToNode::None);
		}

		// Back to the carry. The hand keeps its actor, so nothing is let go.
		if (KeepsState(a_Reason)) {
			m_State.Forget(owner);
			return;
		}

		if (giant) {

			Grabbing::Play::ResetTinyAnimSpeed(giant);

			// A tiny eaten in here is already gone. Anyone still held is being put down.
			const State* state = m_State.Find(owner);

			if (auto* tiny = Possession::FirstActor(owner, PossessionSlot::kHand); tiny && !(state && state->Devouring)) {
				Grabbing::LetGo(giant, tiny, false);
			}

			Grabbing::StopCarry(owner, PossessionSlot::kHand);
			Possession::Release(owner, PossessionSlot::kHand);
		}

		m_State.Forget(owner);
	}

	void GrabPlayNode::OnForget(RE::FormID a_Owner) { m_State.Forget(a_Owner); }
	void GrabPlayNode::OnReset() { m_State.Clear(); }

	std::string_view GrabPlayNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);

		if (!state) {
			return "";
		}

		if (state->Devouring) {
			return "Devouring";
		}

		return state->Grinding ? "Grinding" : "Play";
	}
}
