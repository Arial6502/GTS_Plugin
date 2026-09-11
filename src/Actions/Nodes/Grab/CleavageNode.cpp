#include "Actions/Nodes/Grab/CleavageNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/Grab/GrabCommon.hpp"
#include "Utils/KillDataUtils.hpp"
#include "Managers/Size_Killmoves/SizeKillMove_BreastSuffocate.hpp"
#include "Managers/Size_Killmoves/SizeKillMove_BreastAbsorb.hpp"
#include "Managers/AI/AIFunctions.hpp"

#include "API/Devourment.hpp"
#include "Config/Config.hpp"
#include "Magic/Effects/Common.hpp"

#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/Rumble.hpp"
#include "Managers/Morphs/MorphManager.hpp"

#include "Utils/DeathReport.hpp"
#include "Utils/Looting.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Captured end to end. Times are from one run and are illustrative only.

  Reached only by handoff from GrabNode, and only with someone already stored in the breasts. The
  store happens in the grab, not here: this branch cannot put anyone there. Cleavage.Enter sends
  GTSBEH_Boobs_Enter to the giant and GTSBEH_T_Boobs_Enter to the tiny, and Cleavage.Exit hands back
  the same way. The tiny stays in the breasts across both handoffs, so neither node lets go.

  Sends to the tiny are best effort. A creature has no GTS_ variables and refuses all of them, and
  nothing here depends on the answer.

  Like the hug this is a loop, not a sequence. After the intro the giant sits in the idle until a key
  sends one of the actions below. The light crush, the heavy crush and the DOT return to the idle on
  GTSBEH_Next. Suffocate, absorb and vore leave, and so does a crush that kills.

  There is no crawling variant. The same clips play in every stance. Pressing sneak during the branch
  only toggles IsSneaking; GTS_IsCrawling stays false throughout.
*/
/*
  ----- Cleavage [handoff from Grab, "GTSBEH_Boobs_Enter" + "GTSBEH_T_Boobs_Enter"]
  [INTRO]
    No annotations. The giant is already in the idle when the handoff completes.
    GTS_BS_CamOn and GTS_BS_Shake are declared and the graph never fires either.
  [IDLE, annotations the graph fires on its own between actions]
    GTS_BS_OverrideZ_ON       -> the giant leans down, breast tracking takes over the camera
    GTS_BS_Poke               -> hearts and a small damage tick on the stored tiny
    GTS_BS_OverrideZ_OFF
    GTSBEH_Next               -> back to the idle loop
    GTS_BS_Pat runs the same three annotations and fires twice per pat. It only spawns hearts.
  [LIGHT CRUSH ("GTSBEH_Boobs_Crush_Light")]
    GTS_BS_HandsOpen
    GTS_BS_HandsExtended
    GTS_BS_HandsClose
    GTS_BS_DamageTiny_L       -> damage lands here, the tiny can die
    GTS_BS_Impact               same frame as the damage
    GTS_BS_BoobsRelease       -> if the tiny died the branch ends itself here, see the note below
    GTS_BS_HandsOpen
    GTS_BS_HandsExtended
    GTSBEH_BoobExit
    GTSBEH_Next               -> back to the idle loop
  [HEAVY CRUSH ("GTSBEH_Boobs_Crush_Heavy")]
    GTS_BS_SpringUp
    GTS_BS_SwitchToObjectB    -> the tiny moves onto ObjectB for the wind up
    GTS_BS_HandsOpen
    GTS_BS_HandsExtended
    GTS_BS_SwitchToCleavage   -> and back between the breasts for the hit
    GTS_BS_HandsClose
    GTS_BS_Impact
    GTS_BS_DamageTiny_H         same frame as the impact, damage lands here
    GTS_BS_BoobsRelease       -> same ending as the light crush
    GTSBEH_Next               -> back to the idle loop
  [SUFFOCATE ("GTSBEH_Boobs_SufoStart", always lethal)]
    GTS_BS_SwitchToObjectB
    GTS_BS_Poke
    GTS_BS_SufoStart          -> starts the kill move and the suffocation damage
    GTS_BS_HandsClose
    GTS_BS_HandsLand
    GTS_BS_HandsOpen
    GTS_BS_HandsClose
    GTS_BS_HandsLand            the hands land twice, around four seconds apart
    GTS_BS_SufoPress          -> the press, heavier damage
    GTS_BS_BoobsRelease
    GTS_BS_HandsExtended
    GTS_BS_SufoStop
    GTS_BS_PullTiny           -> the body is pulled back out, so it is still needed after this
    GTS_BS_SufoKill           -> the tiny dies here, and this is where the partners are released
    GTS_BS_CamOff             -> exit signal
  [DAMAGE OVER TIME ("GTSBEH_Boobs_Crush_Dot" / "_Stop")]
    GTS_BS_SwitchToObjectB
    GTSBEH_Next
    GTS_BS_StartDOT             same frame as GTSBEH_Next, starts the damage task
    GTS_BS_Smile                on some runs only
    GTS_BS_StopDOT            -> on the stop key, or on its own once the loop ends
    GTS_BS_SwitchToObjectB
    GTS_BS_StopDOT              fires a second time, so the handler has to be safe to run twice
    GTS_BS_CloseMouth
    GTS_BS_SwitchToCleavage   -> back between the breasts, then the idle loop
  [ABSORB ("GTSBEH_Boobs_Absorb")]
    GTS_BS_SwitchToObjectB
    GTS_BS_SpringUp
    GTS_BS_AbsorbStart          starts the kill move, same frame as the first pulse
    GTS_BS_AbsorbPulse          eight of them, roughly one a second
    GTS_BS_FinishAbsorb       -> the tiny dies here and the partners are released
    GTS_BS_GrowBoobs            on some runs only, and only if the node is still up to receive it
    GTS_BS_CamOff             -> exit signal
  [VORE ("GTSBEH_Boobs_Vore")]
    GTS_BS_PrepareEat
    GTS_BS_SpringUp
    GTS_BS_SwitchToObjectB    -> the tiny moves off the breasts and onto the mouth
    GTS_BS_OpenMouth
    GTS_BS_CloseMouth
    GTS_BS_Swallow
    GTS_BS_Smile
    GTS_BS_KillAll            -> the tiny is eaten and released here, exit signal
  [EXIT ("GTSBEH_Boobs_Exit", handoff back to Grab)]
    GTS_BS_CamOff
    GTS_BS_ResetTiny            same frame, and it is a no op, see below
    GTSBEH_Next

  **Info: releasing the last partner ends the branch on its own, because Alive() is "someone is in
  **the breasts". The absorb exits on that a frame after GTS_BS_FinishAbsorb on some runs and only
  **reaches GTS_BS_CamOff on others, which is why GTS_BS_GrowBoobs is not always delivered. Nothing
  **may depend on an annotation that comes after the release.

  **Info: GTS_BS_ResetTiny must send nothing to the tiny. GTSBEH_T_Storage_* is a wildcard on the
  **tiny's graph, so sending one during the branch pulls the tiny out of Tiny_Boob_Behave and into
  **the storage machine. The storage pose is sent once, from OnExit, on the handoff back to the grab.

  **Info: a kill during a crush does not announce itself in time to be useful. The graph reaches the
  **outro over a second later and by then the giant has dropped back towards the idle holding nobody.
  **GTS_BS_BoobsRelease is where the branch has to end itself.

  **Info: the timing between any two of these is not fixed. Animation speed is modified per actor, so
  **nothing here may assume how long a step takes. The absorb pulses are counted, not timed.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/
/*
   ----- Cleavage, all stances
   [SET ON THE HANDOFF IN, THE SAME FRAME GRAB LEAVES]
     GTS_Busy                -> TRUE
     GTS_IsBoobing           -> TRUE   (graph owned, and this node's signature)
   (NON GTS)
     GTSTDM_Dodge            -> TRUE
     bAnimationDriven        -> TRUE
     bNoStagger              -> TRUE
     tdmHeadtrackingBehavior -> FALSE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
   (NON GTS)
     Direction               -> FALSE
     iSyncForwardState       -> FALSE
     tdmHeadtrackingBoth     -> FALSE
   [SHORTLY AFTER]
   (NON GTS)
     bIdleBeforeConversation -> FALSE
     bIsInMT                 -> FALSE
     bVoiceReady             -> FALSE
   [WHILE POKING OR PATTING]
     GTS_OverrideZ           -> TRUE, back to FALSE on GTS_BS_OverrideZ_OFF
   [WHILE CRUSHING, LIGHT OR HEAVY]
     GTSBeh_DontExit         -> TRUE
     GTS_IsGrabAttacking     -> TRUE
   [WHILE SUFFOCATING]
     GTSBeh_DontExit         -> TRUE
     GTS_IsGrabAttacking     -> TRUE
     GTS_IsSuffocating       -> TRUE
     GTS_DisableHH           -> TRUE
   (NON GTS)
     bHumanoidFootIKEnable   -> FALSE
   [WHILE DOTING]
     GTSBeh_DontExit         -> TRUE
     GTS_IsGrabAttacking     -> TRUE
     GTS_Isboobs_doting      -> TRUE
     GTS_Isboobs_leaving     -> TRUE on the stop, and both clear together at the end
   [WHILE ABSORBING]
     GTS_IsAbsorbing         -> TRUE
     GTS_IsGrabAttacking     -> TRUE
   [WHILE VORING]
     GTS_IsGrabAttacking     -> TRUE
     GTS_IsVoring            -> TRUE
   [WHEN THE TINY DIES OR IS EATEN]
     GTS_Storing_Tiny        -> FALSE  (written by the DLL: Possession drops the actor and the graph
                                        variables follow the store)
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_IsBoobing is graph owned, so it is real evidence and the node's signature rests on it.
   **Nothing in the DLL writes it.

   **Info: GTSBeh_DontExit is graph owned as well, and it gates every exit. Read it, never write it.
   **The abort waits for it to clear before sending GTSBEH_Boobs_Abort, or the graph drops the send.

   **Info: the handoff out is not clean in the variables. Cleavage.Exit hands back to Grab in the
   **same frame, but GTS_IsBoobing stays TRUE for a few seconds afterwards while the giant plays the
   **outro. Nothing may read that variable as meaning this node is current.
*/

using namespace GTS;

namespace {

	using namespace GTS::Actions;
	using State = CleavageNode::State;

	constexpr std::string_view LBREAST = "L Breast02";
	constexpr std::string_view RBREAST = "R Breast02";
	constexpr std::string_view HEAD = "NPC Head [Head]";

	// Graph owned, so it is evidence on its own and needs no Confirm.
	constexpr GraphExpect Signature[] = {
		{ "GTS_IsBoobing", true },
	};

	constexpr std::string_view ExitSignals[] = {
		"GTS_BS_CamOff",
		"GTS_BS_KillAll",
		"GTSBEH_Boobs_Exit",
	};

	// Whoever is stored, dead or alive. The suffocation kills at GTS_BS_SufoKill and GTS_BS_PullTiny
	// pulls that body back out, so the handlers after a kill still need it.
	RE::Actor* Held(const ActionContext& a_Ctx) {
		return Possession::FirstActor(a_Ctx.Owner(), Grabbing::Breasts);
	}

	RE::Actor* Living(const ActionContext& a_Ctx) {
		return Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Breasts);
	}

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	// Nothing else is offered once the tiny is being consumed or suffocated: both own the state until
	// they finish, and the graph offers no way out of either.
	bool CanAct(const ActionContext& a_Ctx) {
		const State* state = CleavageNode::Get(a_Ctx.Owner());
		return Living(a_Ctx) && !(state && (state->Consuming || state->Suffocating));
	}

	bool CanStopSuffocate(const ActionContext& a_Ctx) {
		const State* state = CleavageNode::Get(a_Ctx.Owner());
		return state && state->Suffocating;
	}

	bool CanLeave(const ActionContext& a_Ctx) {
		const State* state = CleavageNode::Get(a_Ctx.Owner());
		return !(state && state->Consuming);
	}

	//----------------------------------------------------------------------------------------------
	// Takes. Every action here is paired: the giant's half and the tiny's.
	//----------------------------------------------------------------------------------------------

	void Pair(const ActionContext& a_Ctx, std::string_view a_Tiny) {
		a_Ctx.SendToHeld(Grabbing::Breasts, a_Tiny);
	}

	void TakeLight(const ActionContext& a_Ctx) { Pair(a_Ctx, "GTSBEH_T_Boobs_Crush_Light"); }
	void TakeHeavy(const ActionContext& a_Ctx) { Pair(a_Ctx, "GTSBEH_T_Boobs_Crush_Heavy"); }
	// Nothing is sent to the tiny. Tiny_Boob_Outro_State is terminal, so GTSBEH_T_Boobs_Exit drops it
	// out of the cleavage machine into the default idle, standing upright while still stored. Left
	// alone it stays in Tiny_Boob_Idle, which is the upside down stored pose and what is wanted.
	void TakeExit(const ActionContext& a_Ctx)  {}

	void TakeSuffocate(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Boobs_SufoStart");
		CleavageNode::GetOrAdd(a_Ctx.Owner()).Suffocating = true;
	}

	// GTSBEH_T_Boobs_SufoStop is the only way out of Tiny_Boob_Sufocate_State. Without it a suffocation
	// stopped early leaves the tiny in the struggle for good. Legacy only ever registered the giant's
	// half of this, so it had the same gap.
	void TakeSuffocateStop(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Boobs_SufoStop");
		CleavageNode::GetOrAdd(a_Ctx.Owner()).Suffocating = false;
	}

	void TakeAbsorb(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Boobs_Absorb");
		CleavageNode::GetOrAdd(a_Ctx.Owner()).Consuming = true;
	}

	void TakeVore(const ActionContext& a_Ctx) {
		Pair(a_Ctx, "GTSBEH_T_Boobs_Vore");
		CleavageNode::GetOrAdd(a_Ctx.Owner()).Consuming = true;
	}

	void TakeDotStart(const ActionContext& a_Ctx) { Pair(a_Ctx, "GTSBEH_T_Boobs_Crush_Dot"); }
	// Tiny_Boob_Dot_State only leaves on GTSBEH_T_BS_DOT_Leave. Crush_Dot_Stop is not a transition
	// from it, which is why the tiny stayed in the DOT struggle after a stop.
	void TakeDotStop(const ActionContext& a_Ctx)  { Pair(a_Ctx, "GTSBEH_T_BS_DOT_Leave"); }

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnCamOn(const ActionContext& a_Ctx)  { ManageCamera(a_Ctx.Actor(), true, CameraTracking::Spine_02); }
	void OnCamOff(const ActionContext& a_Ctx) { ManageCamera(a_Ctx.Actor(), false, CameraTracking::Spine_02); }
	void OnSpringUp(const ActionContext& a_Ctx) { ManageCamera(a_Ctx.Actor(), true, CameraTracking::ObjectB); }

	void OnDamageLight(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DamageAV(giant, ActorValue::kStamina, 30 * GetWasteMult(giant));
		Rumbling::Once("HandImpact_L", giant, 0.3f, 0.0f, LBREAST, 0.0f);
		Rumbling::Once("HandImpact_R", giant, 0.3f, 0.0f, RBREAST, 0.0f);
		Grabbing::Cleave::Deal_breast_damage(giant, 1.0f);
	}

	void OnDamageHeavy(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DamageAV(giant, ActorValue::kStamina, 45 * GetWasteMult(giant));
		Rumbling::Once("HandImpactH_L", giant, 0.55f, 0.0f, LBREAST, 0.0f);
		Rumbling::Once("HandImpactH_R", giant, 0.55f, 0.0f, RBREAST, 0.0f);
		Grabbing::Cleave::Deal_breast_damage(giant, 2.25f);
	}

	void OnShake(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Grabbing::Cleave::ShrinkTinyWithCleavage(giant, 0.035f, 0.980f, 25.0f, true, true);
		Rumbling::Once("BreastShake_L", giant, 0.3f, 0.0f, LBREAST, 0.0f);
		Rumbling::Once("BreastShake_R", giant, 0.3f, 0.0f, RBREAST, 0.0f);

		if (!IsActionOnCooldown(giant, CooldownSource::Emotion_Laugh)) {
			Task_FacialEmotionTask_Smile(giant, 6.0f, "ShakeSmile", 0.35f);
			ApplyActionCooldown(giant, CooldownSource::Emotion_Laugh);
		}
	}

	void OnHandsLand(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (!tiny) {
			return;
		}

		Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kMagicka, 0.045f);
		Grabbing::Cleave::SuffocateTinyFor(giant, tiny, 0.20f, 0.75f, 20.0f);

		Rumbling::Once("HandLand_R", giant, 0.45f, 0.0f, LBREAST, 0.0f);
		Rumbling::Once("HandLand_L", giant, 0.45f, 0.0f, RBREAST, 0.0f);

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundThighSandwichImpact, tiny, std::clamp(0.12f * get_visual_scale(giant), 0.12f, 1.0f), "NPC Root [Root]");
	}

	void OnPoke(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (auto* tiny = Held(a_Ctx)) {
			Task_FacialEmotionTask_Smile(giant, 0.7f, "SufoPoke", RandomFloat(0.0f, 0.90f));
			SpawnHearts(giant, tiny, 35, 0.4f, false);
		}
	}

	// The tiny is in the cleavage state machine from GTSBEH_T_Boobs_Enter onwards. Nothing here may
	// send it GTSBEH_T_Storage_Enemy or _Ally: those belong to the storage machine, and sending one
	// pulls the tiny out of the cleavage one. It is then stuck outside it, every later cleavage event
	// aimed at it is refused, and it plays the storage struggle instead of the branch's animations.
	void OnPat(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (tiny) {
			SpawnHearts(giant, tiny, 35, 0.4f, false);
		}
	}

	void OnSettled(const ActionContext& a_Ctx) {

		// Back in the cleavage idle, so whatever action was running is over. A living tiny means it did
		// not kill, and the intent that take set has to go with it. With nobody left there is nothing
		// to do here: OnUpdate leaves the branch once the graph stops holding GTSBeh_DontExit.
		if (Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Breasts)) {
			Possession::ClearIntent(a_Ctx.Owner(), Grabbing::Breasts);
		}
	}

	// The graph fires this from one clip only, and not from the intro. There is nothing to reset it to
	// either: the tiny's cleavage branch has a single idle with no willing or unwilling variant.
	void OnResetTiny(const ActionContext& a_Ctx) {}


	void OnStartDOT(const ActionContext& a_Ctx) {

		if (auto* tiny = Held(a_Ctx)) {
			Grabbing::Cleave::StartDot(a_Ctx.Actor(), tiny);
		}

		// Legacy allowed the player to drive the strangle's pace. This runs on the narrower
		// 0.50 to 1.75 clamp rather than the usual one.
		a_Ctx.SetCanEditAnimSpeed(true);
	}

	// The graph's own fail-safe out of the DOT. Cancels the tasks and returns the tiny to the idle.
	void OnStopDOT(const ActionContext& a_Ctx) {
		Grabbing::Cleave::StopDot(a_Ctx.Actor(), Held(a_Ctx));
		a_Ctx.SetCanEditAnimSpeed(false);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: suffocation
	//----------------------------------------------------------------------------------------------

	void OnSufoStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		CleavageNode::GetOrAdd(a_Ctx.Owner()).Suffocating = true;

		if (auto* tiny = Held(a_Ctx)) {
			Grabbing::Cleave::SuffocateTinyFor(giant, tiny, 0.10f, 0.85f, 25.0f);
			Grabbing::Cleave::Task_RunSuffocateTask(giant, tiny);
			SpawnHearts(giant, tiny, 35, 0.35f, false);
		}

		Task_FacialEmotionTask_Smile(giant, 1.8f, "SufoStart", RandomFloat(0.0f, 0.75f), 0.2f);

		if (RandomInt(0, 3) >= 2) {
			Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Struggle);
		}
	}

	void OnSufoPress(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (auto* tiny = Held(a_Ctx)) {
			Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kMagicka, 0.035f);
			Grabbing::Cleave::SuffocateTinyFor(giant, tiny, 0.35f, 0.85f, 25.0f);
			SpawnHearts(giant, tiny, 35, 0.50f, false);
		}

		Task_FacialEmotionTask_Smile(giant, 1.2f, "SufoPress");
		Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Struggle);
	}

	void OnSufoKill(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Grabbing::Cleave::LaunchCooldownFor(giant, CooldownSource::Action_Breasts_Suffocate);
		ModSizeExperience(giant, 0.235f);

		CleavageNode::GetOrAdd(a_Ctx.Owner()).Suffocating = false;
		Grabbing::Cleave::CancelAnimation(giant);
	}

	void OnPullTiny(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (!tiny) {
			return;
		}

		BreastSuffocateKillMove::_settings.PulledOut = true;

		Task_FacialEmotionTask_Smile(giant, 1.6f, "SufoPullOut", 0.25f);
		Rumbling::Once("PullOut_R", giant, 0.75f, 0.0f, LBREAST, 0.0f);
		Rumbling::Once("PullOut_L", giant, 0.75f, 0.0f, RBREAST, 0.0f);

		Attachment_SetTargetNode(giant, AttachToNode::ObjectL);
		RecordKill(giant, tiny, DeathType::kBreastSuffocated);
		ManageCamera(giant, true, CameraTracking::ObjectB);

		SpawnHearts(giant, tiny, 35, 0.50f, false);
		Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Superiority);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: absorbing
	//----------------------------------------------------------------------------------------------

	void OnAbsorbStart(const ActionContext& a_Ctx) {

		State& state = CleavageNode::GetOrAdd(a_Ctx.Owner());
		state.Consuming = true;
		state.Pulses = 0;

		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 3.2f, "AbsorbStart", 0.3f);

		if (auto* tiny = Held(a_Ctx)) {
			VoreController::RecordOriginalScale(tiny);
		}
	}

	void OnAbsorbPulse(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		State& state = CleavageNode::GetOrAdd(a_Ctx.Owner());
		++state.Pulses;

		if (!tiny) {
			return;
		}

		Rumbling::Once("AbsorbPulse_R", giant, 0.8f, 0.05f, LBREAST, 0.0f, true);
		Rumbling::Once("AbsorbPulse_L", giant, 0.8f, 0.05f, RBREAST, 0.0f, true);

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundThighSandwichImpact, tiny, std::clamp(0.12f * get_visual_scale(giant), 0.12f, 1.0f), "NPC Root [Root]");

		// Counted rather than timed, and stopped before the last few so the kill is not laughed at.
		if (!IsActionOnCooldown(giant, CooldownSource::Emotion_Laugh) && state.Pulses < 5) {
			ApplyActionCooldown(giant, CooldownSource::Emotion_Laugh);
			Task_FacialEmotionTask_Smile(giant, 0.9f, "AbsorbSmile", 0.12f, 0.25f);
			Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Struggle);
		}

		Grabbing::Cleave::ShrinkTinyWithCleavage(giant, 0.010f, 0.66f, 45.0f, true, true);
		Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kHealth, 0.025f);
		Grabbing::Cleave::Absorb_GrowInSize(giant, tiny, 0.0650f);
	}

	void OnFinishAbsorb(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		if (RandomBool(15.0f)) {
			Task_FacialEmotionTask_SlightSmile(giant, 1.75f, "AbsorbSmile", 0.05f);
			Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Superiority);
		}
		else {
			Task_FacialEmotionTask_Moan(giant, 1.1f, "AbsorbMoan");
			Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Absorption);
		}

		if (!tiny) {
			return;
		}

		SpawnHearts(giant, tiny, 35.0f, 1.15f, false);
		tiny->Attacked(giant);

		Rumbling::Once("AbsorbTiny_R", giant, 0.8f, 0.05f, LBREAST, 0.0f);
		Rumbling::Once("AbsorbTiny_L", giant, 0.8f, 0.05f, RBREAST, 0.0f);

		DamageAV(giant, ActorValue::kHealth, -30);
		Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kHealth, 0.05f);

		const RE::ActorHandle giantHandle = giant->GetHandle();
		const RE::ActorHandle tinyHandle = tiny->GetHandle();

		auto absorb = [giantHandle, tinyHandle] {

			auto giantPtr = giantHandle.get();
			auto tinyPtr = tinyHandle.get();

			if (!giantPtr || !tinyPtr) {
				return;
			}

			auto* g = giantPtr.get();
			auto* t = tinyPtr.get();

			AdvanceQuestProgression(g, t, QuestStage::HugSteal, 1.0f, false);
			ReportDeath(g, t, DamageSource::BreastAbsorb);
			Grabbing::Cleave::Absorb_GrowInSize(g, t, 1.05f);

			AdjustSizeReserve(g, 0.0285f);
			AdjustMassLimit(0.0095f, g);
			DecreaseShoutCooldown(g);

			KillActor(g, t, Config::Audio.bMuteBreastAbsorptionDeathScreams);

			if (!t->IsPlayerRef()) {
				Disintegrate(t);
				SendDeathEvent(g, t);
			}
			else {
				DamageAV(t, ActorValue::kHealth, 999999);
				t->KillImpl(g, 1, true, true);
			}

			TaskManager::RunOnce(std::format("CleavageMerge_{}_{}", g->formID, t->formID), [=](const OneshotUpdate&) {

				auto gp = giantHandle.get();
				auto tp = tinyHandle.get();

				if (!gp || !tp) {
					return;
				}

				PerkHandler::UpdatePerkValues(gp.get(), PerkUpdate::Perk_LifeForceAbsorption);
				TransferInventory(tp.get(), gp.get(), get_visual_scale(tp.get()) * GetSizeFromBoundingBox(tp.get()), false, true, DamageSource::Vored, true);
			});

			ModSizeExperience(g, 0.235f);
		};

		if (Devourment::Enabled() && Devourment::Swallow(giant, tiny, DevourmentLocus::kBreastLeft)) {
			SetBetweenBreasts(tiny, false);
			SetBeingHeld(tiny, false);
			Devourment::Resolve(tiny, absorb);
			return;
		}

		absorb();
	}

	void OnGrowBoobs(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Grabbing::Cleave::LaunchCooldownFor(giant, CooldownSource::Action_Breasts_Absorb);

		if (!Config::Gameplay.ActionSettings.bEnlargeBreastsOnAbsorption) {
			return;
		}

		MorphManager::AlterMorph(giant, MorphManager::kBreasts, MorphManager::Action::kModify, Config::Gameplay.ActionSettings.fBreastsAbsorbIncrementBy, MorphManager::UpdateKind::kGradual, 0.75f);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: eating out of the cleavage
	//----------------------------------------------------------------------------------------------

	void OnPrepareEat(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		CleavageNode::GetOrAdd(a_Ctx.Owner()).Consuming = true;
		ManageCamera(giant, true, CameraTracking::ObjectB);

		if (auto* tiny = Held(a_Ctx)) {
			VoreController::GetSingleton().GetVoreData(giant).AddTiny(tiny);
		}
	}

	void OnOpenMouth(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (auto* tiny = Held(a_Ctx)) {
			SetBeingEaten(tiny, true);
			VoreController::GetSingleton().ShrinkOverTime(giant, tiny);
		}
	}

	void OnSwallow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto& data = VoreController::GetSingleton().GetVoreData(giant);

		if (Held(a_Ctx)) {

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
				Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundSwallow, giant, 1.0f, HEAD);
				data.Swallow();
			}
		}

		Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kHealth, 0.07f);
		Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kMagicka, 0.07f);
		Grabbing::Cleave::RecoverAttributes(giant, ActorValue::kStamina, 0.07f);
	}

	void OnKillAll(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (auto* tiny = Held(a_Ctx)) {

			SetBeingEaten(tiny, false);
			VoreController::GetSingleton().GetVoreData(giant).KillAll();
			SetBeingHeld(tiny, false);

			Grabbing::StopCarry(a_Ctx.Owner(), Grabbing::Breasts);
			Possession::Release(a_Ctx.Owner(), Grabbing::Breasts);
		}

		Grabbing::Cleave::LaunchCooldownFor(giant, CooldownSource::Action_Breasts_Vore);
		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: attachment and the kill moves
	//----------------------------------------------------------------------------------------------

	void OnOverrideOn(const ActionContext& a_Ctx)  { AnimationVars::Action::SetIsCleavageZOverrideEnabled(a_Ctx.Actor(), true); }
	void OnOverrideOff(const ActionContext& a_Ctx) { AnimationVars::Action::SetIsCleavageZOverrideEnabled(a_Ctx.Actor(), false); }

	void OnSwitchToObjectB(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Held(a_Ctx);

		Attachment_SetTargetNode(giant, AttachToNode::ObjectB);

		if (!tiny) {
			return;
		}

		auto* node = find_node(giant, HEAD);

		if (AnimationVars::Cleavage::isBreastAbsorbing(giant)) {
			StartBreastAbsorbKillmove(giant, tiny, node, DamageSource::BreastAbsorb, 9000.0f, 0.1f, false, true);
		}
		else if (AnimationVars::Cleavage::IsSuffocating(giant)) {
			StartBreastSuffocateKillmove(giant, tiny, node, DamageSource::BreastAbsorb, 9000.0f, 0.1f, false, true);
		}
	}

	void OnSwitchToCleavage(const ActionContext& a_Ctx) {
		Attachment_SetTargetNode(a_Ctx.Actor(), AttachToNode::None);
	}

	void OnSmile(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 1.2f, "CleavageSmile", 0.15f);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	// Reached from idle with somebody already in the breasts. The same name is an action on GrabNode,
	// which is what the key resolves to while the giant is holding someone in the hand; that one hands
	// off instead of entering.
	bool HasStored(const EntryContext& a_Ctx) {
		return Possession::FirstAlive(a_Ctx.Owner(), PossessionSlot::kBreasts) != nullptr;
	}

	void TakeEnter(const EntryContext& a_Ctx) {
		if (auto* tiny = Possession::FirstActor(a_Ctx.Owner(), PossessionSlot::kBreasts)) {
			ActionRegistry::Notify(tiny, "GTSBEH_T_Boobs_Enter");
		}
	}

	// Leave releases nothing, so the stored tiny goes back to the carry. The kills still release them.
	constexpr PossessionSlot Partners[] = { PossessionSlot::kBreasts };

	constexpr EntryDef Entries[] = {
		{
			.Action = "Cleavage.Enter",
			.Behaviour = "GTSBEH_Boobs_Enter",
			.Guard = HasStored,
			.Input = "Action.Cleavage.Enter",
			.OnTaken = TakeEnter,
		},
	};

	constexpr ActionDef ActionTable[] = {
		{ .Action = "Cleavage.Light",     .Behaviour = "GTSBEH_Boobs_Crush_Light",    .Guard = CanAct,           .OnTaken = TakeLight,         .Kills = true, .Cooldown = 0.30f, .Input = "Action.Cleavage.AttackLight" },
		{ .Action = "Cleavage.Heavy",     .Behaviour = "GTSBEH_Boobs_Crush_Heavy",    .Guard = CanAct,           .OnTaken = TakeHeavy,         .Kills = true, .Cooldown = 0.30f, .Input = "Action.Cleavage.AttackHeavy" },

		{ .Action = "Cleavage.Suffocate", .Behaviour = "GTSBEH_Boobs_SufoStart",      .Guard = CanAct,           .OnTaken = TakeSuffocate,     .Kills = true, .Cooldown = 0.35f, .Input = "Action.Cleavage.Suffocate" },
		{ .Action = "Cleavage.SufoStop",  .Behaviour = "GTSBEH_Boobs_SufoStop",       .Guard = CanStopSuffocate, .OnTaken = TakeSuffocateStop, .Cooldown = 0.30f },

		{ .Action = "Cleavage.DotStart",  .Behaviour = "GTSBEH_Boobs_Crush_Dot",      .Guard = CanAct,           .OnTaken = TakeDotStart,      .Kills = true, .Cooldown = 0.30f, .Input = "Action.Cleavage.Strangle" },
		{ .Action = "Cleavage.DotStop",   .Behaviour = "GTSBEH_Boobs_Crush_Dot_Stop", .OnTaken = TakeDotStop,    .Cooldown = 0.30f },

		{ .Action = "Cleavage.Absorb",    .Behaviour = "GTSBEH_Boobs_Absorb",         .Guard = CanAct,           .OnTaken = TakeAbsorb,        .Kills = true, .Cooldown = 0.50f, .Input = "Action.Cleavage.Absorb" },
		{ .Action = "Cleavage.Vore",      .Behaviour = "GTSBEH_Boobs_Vore",           .Guard = CanAct,           .OnTaken = TakeVore,          .Kills = true, .Cooldown = 0.50f, .Input = "Action.Cleavage.Vore" },

		// Back to the carry, with the tiny still stored in the breasts.
		{ .Action = "Cleavage.Exit",      .Behaviour = "GTSBEH_Boobs_Exit",           .Guard = CanLeave,         .OnTaken = TakeExit, .HandOff = ActionId::kGrab, .Cooldown = 0.30f, .Input = "Action.Cleavage.Exit" },

		{ .Action = "Cleavage.Cancel",    .Behaviour = "",                            .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_BS_CamOn",            .Handler = OnCamOn },
		{ .Tag = "GTS_BS_CamOff",           .Handler = OnCamOff },
		{ .Tag = "GTS_BS_SpringUp",         .Handler = OnSpringUp },

		{ .Tag = "GTS_BS_DamageTiny_L",     .Handler = OnDamageLight },
		{ .Tag = "GTS_BS_DamageTiny_H",     .Handler = OnDamageHeavy },
		{ .Tag = "GTS_BS_Shake",            .Handler = OnShake },
		{ .Tag = "GTS_BS_HandsLand",        .Handler = OnHandsLand },
		{ .Tag = "GTS_BS_Poke",             .Handler = OnPoke },
		{ .Tag = "GTS_BS_Pat",              .Handler = OnPat },
		{ .Tag = "GTS_BS_Smile",            .Handler = OnSmile },
		{ .Tag = "GTS_BS_ResetTiny",        .Handler = OnResetTiny },
		{ .Tag = "GTSBEH_Next",             .Handler = OnSettled, .Shared = true },

		{ .Tag = "GTS_BS_SufoStart",        .Handler = OnSufoStart },
		{ .Tag = "GTS_BS_SufoPress",        .Handler = OnSufoPress },
		{ .Tag = "GTS_BS_SufoKill",         .Handler = OnSufoKill, .ReleasesPartners = true },
		{ .Tag = "GTS_BS_PullTiny",         .Handler = OnPullTiny },

		{ .Tag = "GTS_BS_AbsorbStart",      .Handler = OnAbsorbStart },
		{ .Tag = "GTS_BS_AbsorbPulse",      .Handler = OnAbsorbPulse },
		{ .Tag = "GTS_BS_FinishAbsorb",     .Handler = OnFinishAbsorb, .ReleasesPartners = true },
		{ .Tag = "GTS_BS_GrowBoobs",        .Handler = OnGrowBoobs },

		{ .Tag = "GTS_BS_PrepareEat",       .Handler = OnPrepareEat },
		{ .Tag = "GTS_BS_OpenMouth",        .Handler = OnOpenMouth },
		{ .Tag = "GTS_BS_Swallow",          .Handler = OnSwallow },
		{ .Tag = "GTS_BS_KillAll",          .Handler = OnKillAll, .ReleasesPartners = true },

		{ .Tag = "GTS_BS_OverrideZ_ON",     .Handler = OnOverrideOn },
		{ .Tag = "GTS_BS_OverrideZ_OFF",    .Handler = OnOverrideOff },
		{ .Tag = "GTS_BS_SwitchToObjectB",  .Handler = OnSwitchToObjectB },
		{ .Tag = "GTS_BS_SwitchToCleavage", .Handler = OnSwitchToCleavage },

		// Fire, carry no work of their own, and are listed so the trace does not call them orphans.
		{ .Tag = "GTS_BS_BoobsRelease" },

		// The crush's own steps. Declared so they stop showing as unhandled in a capture.
		{ .Tag = "GTS_BS_HandsOpen" },
		{ .Tag = "GTS_BS_HandsExtended" },
		{ .Tag = "GTS_BS_HandsClose" },
		{ .Tag = "GTS_BS_Impact" },
		{ .Tag = "GTSBEH_BoobExit" },
		{ .Tag = "GTS_BS_CloseMouth" },
		{ .Tag = "GTS_BS_SufoStop" },

		{ .Tag = "GTS_BS_StartDOT",         .Handler = OnStartDOT },
		{ .Tag = "GTS_BS_StopDOT",          .Handler = OnStopDOT },
		// Deliberately left without a handler. Legacy used it to tear the grab down: it cleared the
		// tiny's BetweenBreasts, BeingEaten and BeingHeld, drained stamina and released the hold. The
		// handoff between this node and the grab does all of that now, so a handler here would be
		// doing it twice. Kept declared so it does not read as an orphan.
		{ .Tag = "GTSBEH_Boobs_StartTransition" },
	};
}

namespace GTS::Actions {

	std::span<const EntryDef> CleavageNode::Entries() const { return ::Entries; }
	std::span<const PossessionSlot> CleavageNode::OwnedSlots() const { return {}; }
	std::span<const PossessionSlot> CleavageNode::PartnerSlots() const { return ::Partners; }
	std::span<const GraphExpect> CleavageNode::Signature() const { return ::Signature; }
	std::string_view CleavageNode::AbortSignal() const { return "GTSBEH_Boobs_Abort"; }
	std::span<const std::string_view> CleavageNode::ExitSignals() const { return ::ExitSignals; }
	Liveness CleavageNode::Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const {
		return Possession::Occupied(a_Owner, PossessionSlot::kBreasts) ? Liveness::kAlive : Liveness::kOver;
	}

	std::span<const ActionDef> CleavageNode::Actions() const { return ::ActionTable; }
	std::span<const AnnotationDef> CleavageNode::Annotations() const { return ::Annotations; }

	CleavageNode::State* CleavageNode::Get(RE::FormID a_Owner) { return m_State.Find(a_Owner); }
	CleavageNode::State& CleavageNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	void CleavageNode::OnEnter(const ActionContext& a_Ctx) {
		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};
	}

	void CleavageNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		// The cameras and the Z override belong to this branch either way, handoff included.
		if (giant) {
			ManageCamera(giant, false, CameraTracking::Spine_02);
			ManageCamera(giant, false, CameraTracking::ObjectB);
			AnimationVars::Action::SetIsCleavageZOverrideEnabled(giant, false);

			// Same as grab play: the attach target is where the tiny is pinned, and the kill moves move
			// it to ObjectL and ObjectB. It has to be cleared on a handoff too or the grab keeps it.
			Attachment_SetTargetNode(giant, AttachToNode::None);
		}

		// Back to the carry with the tiny still stored, so it is put back in the storage pose that
		// matches how it is treated. GTSBEH_T_Storage_Ally and _Enemy are a wildcard on
		// GTS_Tiny_Anims_Behave, so there is no state to be in first and no way for this to be
		// refused. It is only ever sent here, on the way out: sent while the cleavage is running it
		// pulls the tiny into the storage machine and out of the cleavage one, which is what broke the
		// suffocation. Sending nothing leaves the tiny in whatever pose the outro ended on.
		if (KeepsState(a_Reason)) {

			if (giant) {
				if (auto* tiny = Possession::FirstAlive(owner, Grabbing::Breasts)) {
					a_Ctx.SendTo(tiny, IsHostile(giant, tiny) ? "GTSBEH_T_Storage_Enemy" : "GTSBEH_T_Storage_Ally");
				}
			}

			m_State.Forget(owner);
			return;
		}

		if (giant) {

			Attachment_SetTargetNode(giant, AttachToNode::None);

			const State* state = m_State.Find(owner);

			// Still alive and still stored, so leaving this state is just going back to carrying them
			// there. The store owns that slot, not this node, and the same pose the handoff sends is
			// what puts them back into the plain carry.
			if (auto* tiny = Possession::FirstAlive(owner, Grabbing::Breasts); tiny && !(state && state->Consuming)) {
				a_Ctx.SendTo(tiny, IsHostile(giant, tiny) ? "GTSBEH_T_Storage_Enemy" : "GTSBEH_T_Storage_Ally");
				m_State.Forget(owner);
				return;
			}

			// Absorbed, eaten, or dead. Nothing is left to carry there.
			if (auto* tiny = Possession::FirstActor(owner, Grabbing::Breasts); tiny && !(state && state->Consuming)) {
				Grabbing::LetGo(giant, tiny, false);
			}

			Grabbing::StopCarry(owner, Grabbing::Breasts);
			Possession::Release(owner, Grabbing::Breasts);

			// This branch sits inside the grab when the giant is also holding someone in the hand. Its
			// own abort only unwinds one level and leaves that grab holding nobody, so when there is
			// nobody left the grab root is left as well. GrabPlayNode does not need this because its
			// abort signal is the grab's already.
			if (!Possession::CarriedAlive(owner)) {
				ActionRegistry::Notify(giant, Grabbing::BEH_ABORT, true);
				Grabbing::HandEmptied(giant);
			}
		}

		m_State.Forget(owner);
	}

	// The branch is over once the tiny it was holding is dead, but the graph will not let go while it
	// is still playing that kill. GTSBeh_DontExit is how it says so, written by the BSIsActiveModifier
	// of whatever action is running, so this waits for it to clear and leaves on the first frame it
	// can. Aborting rather than exiting, because the exit plays the outro of handing back a tiny that
	// is not there.
	void CleavageNode::OnUpdate(const ActionContext& a_Ctx, float a_Delta) {

		auto* giant = a_Ctx.Actor();

		if (!giant || Possession::FirstAlive(a_Ctx.Owner(), Grabbing::Breasts) || AnimationVars::Action::DontExit(giant)) {
			return;
		}

		ActionRegistry::Notify(giant, "GTSBEH_Boobs_Abort", true);

		if (!Possession::Occupied(a_Ctx.Owner(), Grabbing::Hand)) {
			Grabbing::HandEmptied(giant);
		}

		a_Ctx.RequestExit();
	}

	void CleavageNode::OnForget(RE::FormID a_Owner) { m_State.Forget(a_Owner); }
	void CleavageNode::OnReset() { m_State.Clear(); }

	std::string_view CleavageNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);

		if (!state) {
			return "";
		}

		if (state->Consuming) {
			return "Consuming";
		}

		return state->Suffocating ? "Suffocating" : "Cleavage";
	}
}
