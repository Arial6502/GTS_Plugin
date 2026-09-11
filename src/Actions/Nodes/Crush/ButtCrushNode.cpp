#include "Actions/Nodes/Crush/ButtCrushNode.hpp"

#include "Actions/Core/ActionCleanup.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/PlayerTarget.hpp"
#include "Config/Keybinds.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Controllers/ButtCrushController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Audio/Footstep.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/Damage/LaunchActor.hpp"
#include "Managers/GTSSizeManager.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Managers/Rumble.hpp"

#include "Config/Config.hpp"
#include "Utils/Actions/ButtCrushUtils.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
*/

/*
  ----- ButtCrush Fast (Quick) (Standing) [("GTSBEH_ButtCrush_StartFast")]
  [INTRO]
    IdleOffsetStop [NON GTS]
    GTSButtCrush_Enter
    GTSButtCrush_MoveBody_Start
    GTSButtCrush_FootstepR
    GTSButtCrush_FootstepL
    GTSButtCrush_FootstepR
    GTSButtCrush_MoveBody_Stop
  [FALL DOWN]
    GTSButtCrush_FallDownStart
    GTSButtCrush_FallDownImpact
  [EXIT]
    GTSButtCrush_HandImpactR
    GTSButtCrush_FootstepL
    GTSButtCrush_FootstepR
    GTSBEH_Exit
    GTSButtCrush_FootstepL
  [CLEAN UP]
    GTS_ResetVars
    GTSBEH_Camera_Reset
	HeadTrackingOn [NON GTS]
    IdleStop       [NON GTS]
    tailMTIdle     [NON GTS]
    GTSBeh_Dummy
*/

/*
  ----- KneeCrush Fast (Quick) (Crouching) [("GTSBEH_ButtCrush_StartFast")]
  [INTRO]
	IdleOffsetStop [NON GTS]
	GTS_SneakCrush_FootStep_SilentR
	GTS_SneakCrush_FootStep_SilentL
	GTS_DisableHH
  [FALL DOWN]
    GTS_SneakCrush_Knee_FallDownImpact
    GTS_Sneak_ButtCrush_Landed
  [EXIT]
	GTS_SneakCrush_FootStepR
    GTS_EnableHH
    GTSBEH_Exit
    GTS_SneakCrush_FootStepL
  [CLEAN UP]
    GTS_ResetVars
    GTSBEH_Camera_Reset
    HeadTrackingOn [NON GTS]
    IdleStop       [NON GTS]
    tailSneakIdle  [NON GTS]
    GTSBeh_Dummy
*/

/*
  ----- ButtCrush (Normal) (Standing) [("GTSBEH_ButtCrush_Start")]
  ----- Whole loop, Start, Idle, Grow, Attack (In that Order)
 [INTRO]
   IdleOffsetStop [NON GTS]
   GTSButtCrush_Enter
   GTSButtCrush_MoveBody_Start
   GTSButtCrush_FootstepR
   GTSButtCrush_FootstepL
   GTSButtCrush_FootstepR
   GTSBEH_Next
   GTSButtCrush_MoveBody_MixFrameToLoop
 [LOOP/IDLE]
   GTSButtCrush_ButtSwayLoop_Start
   GTSButtCrush_ButtSwayLoop_MixFrameToAttack
   GTSButtCrush_ButtSwayLoop_Stop
 [GROW]
   GTSButtCrush_GrowthStart
   GTSBEH_ButtCrush_GrowthFinish
   GTSBEH_Next
   GTSGrowth_SpurtStop
 [ATTACK]
   GTSButtCrush_FallDownStart
   GTSButtCrush_FallDownImpact
   GTSButtCrush_HandImpactR
 [STAND UP/EXIT]
   GTSButtCrush_FootstepR
   GTSButtCrush_FootstepL
   GTSButtCrush_FootstepL
   GTSButtCrush_FootstepR
   GTSBEH_Exit
   GTSButtCrush_FootstepL
 [CLEANUP]
   GTS_ResetVars
   GTSBEH_Camera_Reset
   HeadTrackingOn  [NON GTS]
   IdleStop        [NON GTS]
   tailMTIdle      [NON GTS]
   GTSBeh_Dummy
*/

/*
  ----- KneeCrush (Normal) (Crouching) [("GTSBEH_ButtCrush_Start")]
  ----- Whole loop, Start, Idle, Grow, Attack (In that Order)
 [INTRO]
   IdleOffsetStop [NON GTS]
   GTSButtCrush_Enter
   GTS_SneakCrush_Knee_CamOn
   GTS_SneakCrush_FootStep_SilentR
   GTS_SneakCrush_FootStep_SilentL
   GTSBEH_Next
 [LOOP/IDLE]
   ** NOTHING**
 [GROW]
   GTSButtCrush_GrowthStart
   GTSBEH_Next
   GTSBEH_ButtCrush_GrowthFinish
 [ATTACK]
   GTS_DisableHH
   GTS_SneakCrush_Knee_FallDownImpact
   GTS_Sneak_ButtCrush_Landed
 [STAND UP/EXIT]
   GTS_SneakCrush_FootStepL
   GTS_SneakCrush_Knee_CamOff
   GTS_EnableHH
   GTS_SneakCrush_FootStepR
   GTSBEH_Exit
 [CLEANUP]
   GTS_ResetVars
   GTS_Sneak_ButtCrush_Landed (A bit late, possible bug?)
   GTSBEH_Camera_Reset
   HeadTrackingOn [NON GTS]
   IdleStop       [NON GTS]
   tailSneakIdle  [NON GTS]
   GTSBeh_Dummy
*/

/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/

/*
   ----- ButtCrush Fast (Quick) (Standing) [("GTSBEH_ButtCrush_StartFast")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER ("GTSBeh_TriggerSitdown")]
	 GTS_Busy                -> TRUE
	 GTS_IsButtCrushing      -> TRUE
	 GTS_Ready               -> FALSE
   (NON GTS)
	 TDM_Dodge               -> TRUE
	 TDM_LockRotation11      -> TRUE
	 bAnimationDriven        -> TRUE
	 bIdleBeforeConversation -> FALSE
	 bNoStagger              -> TRUE
	 bVoiceReady             -> FALSE
   [ON BUTT IMPACT WITH GROUND]
	 GTS_IsButtCrushing      -> FALSE (Bool Resets on impact earlier)
   [EXIT]
	 **ALL PREVIOUS STATED VARS INVERT**
*/

/*
   ----- KneeCrush Fast (Quick) (Crouching) [("GTSBEH_ButtCrush_StartFast")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
	 GTS_Busy                -> TRUE
	 GTS_Ready               -> FALSE
   (NON GTS)
	 TDM_Dodge               -> TRUE
	 TDM_LockRotation11      -> TRUE
	 bAnimationDriven        -> TRUE
	 bIdleBeforeConversation -> FALSE
	 bHumanoidFootIKEnable   -> FALSE
	 bNoStagger              -> TRUE
	 bVoiceReady             -> FALSE
   [FALL DOWN ACTION COMMITED]
	 IsSneaking              -> FALSE 
   [EXIT]
	 **ALL PREVIOUS STATED VARS INVERT BACK**
	 
   **Info: KneeCrush does not set the ButtCrush AnimationVar. The animgraph gates its entry behind IsSneaking (and possbily iIsInSneak?)
   (POSSIBLE BUG) The statefull variant does set the animvar. As does the normal standing quick one, so this one not setting it could be a bug.
*/
  
/*
 ----- ButtCrush Normal (Standing) [("GTSBEH_ButtCrush_Start")]
 [INTRO]
     GTS_Busy                -> TRUE
	 GTS_IsButtCrushing      -> TRUE
	 GTS_Ready               -> FALSE
 (NON GTS)
     TDM_Dodge               -> TRUE
	 TDM_LockRotation11      -> TRUE
	 bAnimationDriven        -> TRUE
	 bIdleBeforeConversation -> FALSE
	 bNoStagger              -> TRUE
	 bVoiceReady             -> FALSE
 [LOOP/IDLE]
     **NO CHANGES**
 [GROW]
    GTS_IsGrowing            -> TRUE
 [GROW COMPLETE]
	GTS_IsGrowing            -> FALSE
 [ATTACK]
     BUGGED BEHAVIOR: GTS_IsButtCrushing Flips to FALSE then TRUE again for a single frame.
 [STAND UP/EXIT]
	**NO CHANGES**
 [CLEANUP]
	 GTS_Busy                -> FALSE
	 GTS_IsButtCrushing      -> FALSE
	 GTS_Ready               -> TRUE
 (NON GTS)
	 TDM_Dodge               -> FALSE
	 TDM_LockRotation11      -> FALSE
	 bAnimationDriven        -> FALSE
	 bIdleBeforeConversation -> TRUE
	 bNoStagger              -> FALSE
	 bVoiceReady             -> TRUE
 **Info: The attack behavior is possibly bugged as it briefly sets GTS_IsButtCrushing to false and then true again all within the span of a frame.
 (Or atleast what appears to be a frame, the Graph debugger may not sample fast enough idk).
*/

/*
 ----- KneeCrush Normal (Crouching) [("GTSBEH_ButtCrush_Start")]
 [INTRO]
	 GTS_Busy                -> TRUE
	 GTS_IsButtCrushing      -> TRUE
	 GTS_Ready               -> FALSE
 (NON GTS)
     IsSneaking              -> FALSE
	 TDM_Dodge               -> TRUE
	 TDM_LockRotation11      -> TRUE
	 bAnimationDriven        -> TRUE
	 bIdleBeforeConversation -> FALSE
	 bNoStagger              -> TRUE
	 bVoiceReady             -> FALSE
 [LOOP/IDLE]
	 **NO CHANGES**
 [GROW]
	GTS_IsGrowing            -> TRUE
 [GROW COMPLETE]
	GTS_IsGrowing            -> FALSE
 [ATTACK]
	 BUGGED BEHAVIOR: GTS_IsButtCrushing Flips to FALSE then TRUE again for a single frame.
 [STAND UP/EXIT]
	**NO CHANGES**
 [CLEANUP]
	 GTS_Busy                -> FALSE
	 GTS_IsButtCrushing      -> FALSE
	 GTS_Ready               -> TRUE
 (NON GTS)
     IsSneaking              -> TRUE
	 TDM_Dodge               -> FALSE
	 TDM_LockRotation11      -> FALSE
	 bAnimationDriven        -> FALSE
	 bIdleBeforeConversation -> TRUE
	 bNoStagger              -> FALSE
	 bVoiceReady             -> TRUE
 **Info: The attack behavior is possibly bugged as it briefly sets GTS_IsButtCrushing to false and then true again all within the span of a frame.
 (Or atleast what appears to be a frame, the Graph debugger may not sample fast enough idk).
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Variant = ButtCrushNode::Variant;
	using State = ButtCrushNode::State;

	constexpr std::string_view NODE_FOOT_R = "NPC R Foot [Rft ]";
	constexpr std::string_view NODE_FOOT_L = "NPC L Foot [Lft ]";

	constexpr std::string_view RUMBLE_TAG_GROWTH = "ButtCrush_Growth_Rumble";

	constexpr std::string_view TASKID_FMT_CAMERA   = "DisableCameraTask_{}";
	constexpr std::string_view TASKID_FMT_ATTACK   = "ButtCrushAttack_{}";
	constexpr std::string_view TASKID_FMT_MAGICKA  = "ButtCrushMagicka_{}";

	// The body set without the breast bones. Boob crush uses the same table plus those four.
	std::span<const std::string_view> RumbleNodes() {
		return Crush::BodyRumbleNodes();
	}

	void StartRumble(std::string_view a_Tag, Actor& a_Actor, float a_Power, float a_Halflife) {
		Crush::StartRumble(a_Tag, a_Actor, a_Power / RumbleNodes().size(), a_Halflife, RumbleNodes());
	}

	void StopRumble(std::string_view a_Tag, Actor& a_Actor) {
		Crush::StopRumble(a_Tag, a_Actor, RumbleNodes());
	}

	void TrackKnee(Actor* a_Giant, bool a_Enable) {
		if (Config::General.bTrackBonesDuringAnim) {
			SizeManager::GetSingleton().SetTrackedBone(a_Giant, a_Enable, CameraTracking::ObjectB);
		}
	}

	void TrackBooty(Actor* a_Giant, bool a_Enable) {
		if (Config::General.bTrackBonesDuringAnim) {
			SizeManager::GetSingleton().SetTrackedBone(a_Giant, a_Enable, CameraTracking::Mid_Butt_Legs);
		}
	}

	void DisableButtTrackTask(Actor* a_Giant) {

		const std::string name = std::format(TASKID_FMT_CAMERA, a_Giant->formID);
		auto handle = a_Giant->CreateRefHandle();
		const double start = Time::WorldTimeElapsed();

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			const double finish = Time::WorldTimeElapsed();

			if ((finish - start) / AnimationManager::GetAnimSpeed(giant) > 12.0f
				|| (!AnimationVars::ButtCrush::IsButtCrushing(giant) && !AnimationVars::General::IsGTSBusy(giant))) {
				ManageCamera(giant, false, CameraTracking::Butt);
				return false;
			}

			return true;
		});
	}

	void DoFootImpact(Actor* a_Giant, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		float smt = 1.0f;
		float dust = 1.0f;

		if (TinyCalamityActive(a_Giant)) {
			dust = 1.25f;
			smt = 1.5f;
		}

		const float shake = Rumble_ButtCrush_FeetImpact * smt * GetHighHeelsBonusDamage(a_Giant, true);

		Rumbling::Once(std::string(a_Rumble), a_Giant, shake, 0.0f, a_Node, 0.33f);
		DoDamageEffect(a_Giant, Damage_ButtCrush_FootImpact, Radius_ButtCrush_FootImpact, 10, 0.25f, a_Event, 1.0f, a_Source);
		DoFootstepSound(a_Giant, 1.0f, a_Event, a_Node);
		DoDustExplosion(a_Giant, dust, a_Event, a_Node);
		DoLaunch(a_Giant, 0.75f * perk, 1.6f, a_Event);

		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	// The knee crush footsteps are a different formula from the standing ones, so they stay separate.
	void DoSneakFootsteps(Actor* a_Giant, float a_Power, bool a_Right) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		float dust = 1.0f;
		float smt = 1.0f;

		if (TinyCalamityActive(a_Giant)) {
			dust = 1.25f;
			smt = 2.0f;
		}

		const float shake = Rumble_KneeCrush_FootImpact * a_Power;
		smt *= GetHighHeelsBonusDamage(a_Giant, true);

		const FootEvent event = a_Right ? FootEvent::Right : FootEvent::Left;
		const DamageSource source = a_Right ? DamageSource::CrushedRight : DamageSource::CrushedLeft;
		const std::string_view node = a_Right ? NODE_FOOT_R : NODE_FOOT_L;

		Rumbling::Once(a_Right ? "FST_R" : "FST_L", a_Giant, shake * smt, 0.0f, node, 0.0f);
		DoDamageEffect(a_Giant, Damage_Walk_Defaut * a_Power, Radius_Walk_Default, 10, 0.25f, event, 1.0f, source);
		DoFootstepSound(a_Giant, 1.10f * a_Power, event, node);
		DoDustExplosion(a_Giant, dust * a_Power, event, node);
		DoLaunch(a_Giant, 0.65f * perk * a_Power, 1.35f * a_Power, event);

		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	void DoButtDamage(Actor* a_Giant) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const float dust = TinyCalamityActive(a_Giant) ? 1.25f : 1.0f;

		Crush::RestoreSize(a_Giant);

		const float damage = GetButtCrushDamage(a_Giant);
		auto* thighL = find_node(a_Giant, "NPC L Thigh [LThg]");
		auto* thighR = find_node(a_Giant, "NPC R Thigh [RThg]");
		auto* buttR = find_node(a_Giant, "NPC R Butt");
		auto* buttL = find_node(a_Giant, "NPC L Butt");

		if (buttR && buttL && thighL && thighR) {
			DoDamageAtPoint(a_Giant, Radius_ButtCrush_Impact, Damage_ButtCrush_ButtImpact * damage, thighL, 4, 0.70f, 0.85f, DamageSource::Booty);
			DoDamageAtPoint(a_Giant, Radius_ButtCrush_Impact, Damage_ButtCrush_ButtImpact * damage, thighR, 4, 0.70f, 0.85f, DamageSource::Booty);
			DoDustExplosion(a_Giant, 1.45f * dust * damage, FootEvent::Butt, "NPC R Butt");
			DoDustExplosion(a_Giant, 1.45f * dust * damage, FootEvent::Butt, "NPC L Butt");
			DoFootstepSound(a_Giant, 1.25f, FootEvent::Right, NODE_FOOT_R);
			DoLaunch(a_Giant, 1.30f * perk, 5.0f, FootEvent::Butt);
			Rumbling::Once("Butt_L", a_Giant, 3.60f * damage, 0.05f, "NPC R Butt", 0.0f);
			Rumbling::Once("Butt_R", a_Giant, 3.60f * damage, 0.05f, "NPC L Butt", 0.0f);
		}
		else {
			Notify("Error: Missing Butt or Thigh Nodes");
			Notify("Error: effects not inflicted");
			Notify("install 3BBB/XP32 Skeleton");
		}

	}

	void DoKneeDamage(Actor* a_Giant) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const float dust = TinyCalamityActive(a_Giant) ? 1.25f : 1.0f;

		Crush::RestoreSize(a_Giant);

		const float damage = GetButtCrushDamage(a_Giant);
		auto* kneeL = find_node(a_Giant, "NPC L Calf [LClf]");
		auto* kneeR = find_node(a_Giant, "NPC R Calf [RClf]");

		if (kneeL && kneeR) {
			DoDamageAtPoint(a_Giant, Radius_Sneak_KneeCrush, Damage_KneeCrush * damage, kneeL, 4, 0.70f, 0.85f, DamageSource::KneeLeft);
			DoDamageAtPoint(a_Giant, Radius_Sneak_KneeCrush, Damage_KneeCrush * damage, kneeR, 4, 0.70f, 0.85f, DamageSource::KneeRight);
			DoDustExplosion(a_Giant, 1.45f * dust * damage, FootEvent::Left, "NPC L Calf [LClf]");
			DoDustExplosion(a_Giant, 1.45f * dust * damage, FootEvent::Right, "NPC R Calf [RClf]");
			DoFootstepSound(a_Giant, 1.25f, FootEvent::Left, "NPC L Calf [LClf]");
			DoFootstepSound(a_Giant, 1.25f, FootEvent::Right, "NPC R Calf [RClf]");
			LaunchActor::LaunchAtNode(a_Giant, 1.30f * perk, 4.20f, "NPC L Calf [LClf]");
			LaunchActor::LaunchAtNode(a_Giant, 1.30f * perk, 4.20f, "NPC R Calf [RClf]");
			Rumbling::Once("Knee_L", a_Giant, 3.60f * damage, 0.05f, "NPC L Calf [LClf]", 0.0f);
			Rumbling::Once("Knee_R", a_Giant, 3.60f * damage, 0.05f, "NPC R Calf [RClf]", 0.0f);
		}
		else {
			Notify("Error: Missing Knee Nodes");
			Notify("Error: effects not inflicted");
			Notify("install 3BBB/XP32 Skeleton");
		}

		ModGrowthCount(a_Giant, 0, true);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks - standing
	//----------------------------------------------------------------------------------------------

	void OnMoveBodyStart(const ActionContext& a_Ctx) {
		ApplyButtCrushCooldownTask(a_Ctx.Actor());
		RecordStartButtCrushSize(a_Ctx.Actor());
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Butt);
	}

	void OnMoveBodyMixFrameToLoop(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::ObjectB);
	}

	void OnMoveBodyStop(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Butt);
	}

	void OnGrowthStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Crush::ApplyGrowth(a_Ctx, "ButtCrushGrowth", 0.25f);

		if (AnimationVars::Action::IsInSecondSandwichBranch(giant)) {
			// The second sandwich branch fires the wrong events, so the moan is issued here instead.
			Task_FacialEmotionTask_Moan(giant, 1.25f, "GrowthMoan", 0.15f);
			Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Growth, CooldownSource::Emotion_Voice_Long);
		}

		StartRumble(RUMBLE_TAG_GROWTH, *giant, Rumble_ButtCrush_Growth, 0.30f);
	}

	void OnGrowthFinish(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (auto* tiny : find_actors()) {

			if (!tiny || tiny == giant) {
				continue;
			}

			if (!IsHostile(giant, tiny) && !IsHostile(tiny, giant)) {
				continue;
			}

			if ((giant->GetPosition() - tiny->GetPosition()).Length() <= 212.0f * get_visual_scale(giant)) {
				ChanceToScare(giant, tiny, 1, 30, true);
			}
		}

		if (!IsActionOnCooldown(giant, CooldownSource::Emotion_Moan)) {
			Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Growth, CooldownSource::Emotion_Voice_Long);
			ApplyActionCooldown(giant, CooldownSource::Emotion_Moan);
			Task_FacialEmotionTask_Moan(giant, 1.2f, "ButtCrush_Growth");
		}

		StopRumble(RUMBLE_TAG_GROWTH, *giant);
	}

	void OnFootstepR(const ActionContext& a_Ctx) {
		DoFootImpact(a_Ctx.Actor(), true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "FS_R");
	}

	void OnFootstepL(const ActionContext& a_Ctx) {
		DoFootImpact(a_Ctx.Actor(), false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "FS_L");
	}

	void OnHandImpactR(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.0f, Damage_ButtCrush_HandImpact, CrawlEvent::RightHand, "RightHand", 0.8f, Radius_ButtCrush_HandImpact, 1.0f, DamageSource::HandCrawlRight);
		a_Ctx.SetHHDisabled(false, 1.5f);
	}

	void OnFallDownStart(const ActionContext& a_Ctx) {
		ButtCrushNode::GetOrAdd(a_Ctx.Owner()).Dropping = true;
		a_Ctx.SetHHDisabled(true, 0.5f);
	}

	void OnFallDownImpact(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		float dust = 1.0f;
		float smt = 1.0f;

		if (TinyCalamityActive(giant)) {
			dust = 1.25f;
			smt = 1.5f;
		}

		ManageCamera(giant, true, CameraTracking::Butt);

		const std::string name = std::format(TASKID_FMT_ATTACK, giant->formID);
		auto handle = giant->CreateRefHandle();
		const double start = Time::WorldTimeElapsed();

		// The animation's own timing is wrong, so the impact is delayed by a frame or two here.
		TaskManager::RunFor(name, 1.0f, [=](auto&) {

			if (!handle) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start <= 0.04) {
				return true;
			}

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giantref = giantPtr.get();

			Crush::RestoreSize(giantref);

			const float damage = GetButtCrushDamage(giantref);
			auto* thighL = find_node(giantref, "NPC L Thigh [LThg]");
			auto* thighR = find_node(giantref, "NPC R Thigh [RThg]");
			auto* buttR = find_node(giantref, "NPC R Butt");
			auto* buttL = find_node(giantref, "NPC L Butt");

			ApplyThighDamage(giantref, true, false, Radius_ThighCrush_ButtCrush_Drop, Damage_ButtCrush_LegDrop * damage, 0.35f, 1.0f, 14, DamageSource::Booty);
			ApplyThighDamage(giantref, false, false, Radius_ThighCrush_ButtCrush_Drop, Damage_ButtCrush_LegDrop * damage, 0.35f, 1.0f, 14, DamageSource::Booty);

			if (buttR && buttL && thighL && thighR) {

				const float shake = Rumble_ButtCrush_ButtImpact * dust * damage;

				DoDamageAtPoint(giantref, Radius_ButtCrush_Impact, Damage_ButtCrush_ButtImpact * damage, thighL, 4, 0.70f, 0.8f, DamageSource::Booty);
				DoDamageAtPoint(giantref, Radius_ButtCrush_Impact, Damage_ButtCrush_ButtImpact * damage, thighR, 4, 0.70f, 0.8f, DamageSource::Booty);
				DoDustExplosion(giantref, 1.45f * dust * damage, FootEvent::Butt, "NPC R Butt");
				DoDustExplosion(giantref, 1.45f * dust * damage, FootEvent::Butt, "NPC L Butt");
				DoLaunch(giantref, 2.25f * perk, 5.0f, FootEvent::Butt);
				DoFootstepSound(giantref, 1.25f, FootEvent::Right, NODE_FOOT_R);
				Rumbling::Once("Butt_L", giantref, shake * smt, 0.075f, "NPC R Butt", 0.0f);
				Rumbling::Once("Butt_R", giantref, shake * smt, 0.075f, "NPC L Butt", 0.0f);
			}
			else {
				Notify("Error: Missing Butt or Thigh Nodes");
				Notify("Error: effects not inflicted");
				Notify("install 3BBB/XP32 Skeleton");
			}

			DisableButtTrackTask(giantref);

			return false;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks - knee crush
	//----------------------------------------------------------------------------------------------

	void OnKneeCamOn(const ActionContext& a_Ctx) {
		ButtCrushNode::GetOrAdd(a_Ctx.Owner()).Variant = Variant::kKnee;
		TrackKnee(a_Ctx.Actor(), true);
	}

	void OnKneeCamOff(const ActionContext& a_Ctx) {
		TrackKnee(a_Ctx.Actor(), false);
	}

	void OnBootyCamOn(const ActionContext& a_Ctx) {
		ButtCrushNode::GetOrAdd(a_Ctx.Owner()).Variant = Variant::kKnee;
		TrackBooty(a_Ctx.Actor(), true);
	}

	void OnBootyCamOff(const ActionContext& a_Ctx) {
		TrackBooty(a_Ctx.Actor(), false);
	}

	void OnKneeImpact(const ActionContext& a_Ctx) {
		ButtCrushNode::GetOrAdd(a_Ctx.Owner()).Dropping = true;
		DoKneeDamage(a_Ctx.Actor());
	}

	void OnSneakButtImpact(const ActionContext& a_Ctx) {
		ButtCrushNode::GetOrAdd(a_Ctx.Owner()).Dropping = true;
		DoButtDamage(a_Ctx.Actor());
	}

	void OnSneakFootstepL(const ActionContext& a_Ctx) {
		DoSneakFootsteps(a_Ctx.Actor(), 1.0f, false);
		a_Ctx.SetHHDisabled(false, 4.0f);
	}

	void OnSneakFootstepR(const ActionContext& a_Ctx) {
		DoSneakFootsteps(a_Ctx.Actor(), 1.0f, true);
		ApplyButtCrushCooldownTask(a_Ctx.Actor());
	}

	void OnSneakFootstepSilentL(const ActionContext& a_Ctx) {
		DoSneakFootsteps(a_Ctx.Actor(), 1.0f, false);
		TrackKnee(a_Ctx.Actor(), true);
		SetButtCrushSize(a_Ctx.Actor(), 0.0f, false);
	}

	void OnSneakFootstepSilentR(const ActionContext& a_Ctx) {
		DoSneakFootsteps(a_Ctx.Actor(), 1.0f, true);
	}

	void OnDisableHH(const ActionContext& a_Ctx) {
		a_Ctx.SetHHDisabled(true, 2.25f);
	}

	void OnEnableHH(const ActionContext& a_Ctx) {
		a_Ctx.SetHHDisabled(false, 1.0f);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	// GTS_IsCrawlButtCrush false is load bearing: it is the only thing separating this node's
	// signature from BoobCrushNode's.
	constexpr GraphExpect Signature[] = {
		{ "GTS_IsButtCrushing",   true  },
		{ "GTS_IsCrawlButtCrush", false },
		{ "GTS_Busy",             true  },
	};

	// Only the deliberate drop. The graph's own exit tag is claimed below but not treated as a
	// signal, so a reset that skips to it still reads as a desync.
	// Leaves the root Giantess_State from any point inside it. Vore, both crushes and thigh crush
	// all run in that state, and nothing else in the DLL sends this.
	constexpr std::string_view BEH_ABORT = "GTSBeh_ExitEvents";

	constexpr std::string_view ExitSignals[] = {
		"GTSButtCrush_FallDownStart",
		"GTS_SneakCrush_Knee_FallDownImpact",
		"GTS_SneakCrush_Butt_FallDownImpact",
	};

	bool CanGrow(const ActionContext& a_Ctx) {
		const State* state = ButtCrushNode::Get(a_Ctx.Owner());
		return Crush::CanGrow(a_Ctx, !state || state->Quick || state->Dropping);
	}

	bool CanAttack(const ActionContext& a_Ctx) {
		const State* state = ButtCrushNode::Get(a_Ctx.Owner());
		return state && !state->Quick && !state->Dropping;
	}

	// Measured: the quick knee variant sets GTS_Busy and nothing else, and the quick standing one
	// clears GTS_IsButtCrushing again on impact. Neither can be confirmed by the node's own
	// signature, so the quick entry carries its own.
	constexpr GraphExpect QuickSignature[] = {
		{ "GTS_Busy", true },
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "ButtCrush.Enter",      .Behaviour = "GTSBEH_ButtCrush_Start",     .Guard = Crush::CanStart, .Verify = Crush::VerifyStart, .BlockedBy = kBlockedByHand },
		{ .Action = "ButtCrush.EnterQuick", .Behaviour = "GTSBEH_ButtCrush_StartFast", .Guard = Crush::CanStart, .Verify = Crush::VerifyStart, .BlockedBy = kBlockedByHand, .Signature = QuickSignature },
	};

	// One key drives the standing, knee and crawl variants, so the two in-action names are shared
	// with BoobCrushNode. Action names only ever resolve against the node that is already active, so
	// two nodes claiming one name is well defined - unlike entry names, which must be unique.
	constexpr ActionDef Actions[] = {
		{ .Action = "Crush.Grow",   .Behaviour = "GTSBEH_ButtCrush_Grow",   .Guard = CanGrow,   .Verify = Crush::VerifyGrow, .Cooldown = 0.35f, .Input = "Action.Crush.Grow" },
		{ .Action = "Crush.Attack", .Behaviour = "GTSBEH_ButtCrush_Attack", .Guard = CanAttack, .Exits = true, .Cooldown = 0.35f, .Input = "Action.Crush.Attack" },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSButtCrush_MoveBody_Start",           .Handler = OnMoveBodyStart },
		{ .Tag = "GTSButtCrush_MoveBody_MixFrameToLoop",  .Handler = OnMoveBodyMixFrameToLoop },
		{ .Tag = "GTSButtCrush_MoveBody_Stop",            .Handler = OnMoveBodyStop },
		{ .Tag = "GTSButtCrush_GrowthStart",              .Handler = OnGrowthStart },
		{ .Tag = "GTSBEH_ButtCrush_GrowthFinish",         .Handler = OnGrowthFinish },
		{ .Tag = "GTSButtCrush_FootstepR",                .Handler = OnFootstepR },
		{ .Tag = "GTSButtCrush_FootstepL",                .Handler = OnFootstepL },
		{ .Tag = "GTSButtCrush_HandImpactR",              .Handler = OnHandImpactR },
		{ .Tag = "GTSButtCrush_FallDownStart",            .Handler = OnFallDownStart },
		{ .Tag = "GTSButtCrush_FallDownImpact",           .Handler = OnFallDownImpact },

		{ .Tag = "GTS_SneakCrush_Knee_CamOn",             .Handler = OnKneeCamOn },
		{ .Tag = "GTS_SneakCrush_Knee_CamOff",            .Handler = OnKneeCamOff },
		{ .Tag = "GTS_SneakCrush_Butt_CamOn",             .Handler = OnBootyCamOn },
		{ .Tag = "GTS_SneakCrush_Butt_CamOff",            .Handler = OnBootyCamOff },
		{ .Tag = "GTS_SneakCrush_Knee_FallDownImpact",    .Handler = OnKneeImpact },
		{ .Tag = "GTS_SneakCrush_Butt_FallDownImpact",    .Handler = OnSneakButtImpact },
		{ .Tag = "GTS_SneakCrush_FootStepL",              .Handler = OnSneakFootstepL },
		{ .Tag = "GTS_SneakCrush_FootStepR",              .Handler = OnSneakFootstepR },
		{ .Tag = "GTS_SneakCrush_FootStep_SilentL",       .Handler = OnSneakFootstepSilentL },
		{ .Tag = "GTS_SneakCrush_FootStep_SilentR",       .Handler = OnSneakFootstepSilentR },
		{ .Tag = "GTS_Sneak_ButtCrush_Landed" },

		// Claimed so they are not orphans. Nothing to do for them yet.
		{ .Tag = "GTSButtCrush_Enter" },
		{ .Tag = "GTSButtCrush_ButtSwayLoop_Start" },
		{ .Tag = "GTSButtCrush_ButtSwayLoop_MixFrameToAttack" },
		{ .Tag = "GTSButtCrush_ButtSwayLoop_Stop" },
		{ .Tag = "GTSGrowth_SpurtStop" },

		// Generic names the behaviour project reuses elsewhere.
		{ .Tag = "GTS_DisableHH",                         .Handler = OnDisableHH, .Shared = true },
		{ .Tag = "GTS_EnableHH",                          .Handler = OnEnableHH,  .Shared = true },
		{ .Tag = "GTSBEH_Next", .Shared = true },
		{ .Tag = "GTSBEH_Exit", .Shared = true },
	};

	constexpr std::string_view VariantNames[] = {
		"Standing",
		"Knee",
	};

	//----------------------------------------------------------------------------------------------
	// Input
	//----------------------------------------------------------------------------------------------

	//Determine which version should be started based on if the actor has the perk.
	std::string_view StartSuffix(Actor* a_Actor) {
		return Runtime::HasPerk(a_Actor, Runtime::PERK.GTSPerkButtCrushAug1) ? "Enter" : "EnterQuick";
	}

	bool CanStartEvent() {

		// The follower's own stance decides between butt and breast crush, so the name asked about
		// here is only the one that finds the node.
		if (PlayerTarget::Engaged("Action.Crush.Start")) {
			return PlayerTarget::CanPerform("ButtCrush.Enter");
		}

		Actor* player = GetPlayerOrControlled();
		return player && ActionRegistry::CanPerform(player, Crush::EntryAction(player, StartSuffix(player)));
	}

	bool CanQuickStartEvent() {
		Actor* player = RE::PlayerCharacter::GetSingleton();
		return player && ActionRegistry::CanPerform(player, Crush::EntryAction(player, "EnterQuick"));
	}

	void StartEvent(const ManagedInputEvent&) {

		if (PlayerTarget::Engaged("Action.Crush.Start")) {
			PlayerTarget::Perform("ButtCrush.Enter");
			return;
		}

		Actor* player = GetPlayerOrControlled();
		if (!player) {
			return;
		}

		if (Runtime::HasPerk(player, Runtime::PERK.GTSPerkButtCrushAug1)) {
			for (auto* prey : ButtCrushController::GetSingleton().GetButtCrushTargets(player, 1)) {
				ButtCrushController::StartButtCrush(player, prey);
			}
			return;
		}

		if (ActionRegistry::Perform(player, Crush::EntryAction(player, "EnterQuick")) == RequestResult::kAccepted) {
			DamageAV(player, ActorValue::kStamina, 100.0f * GetButtCrushCost(player, false));
		}
	}

	void QuickStartEvent(const ManagedInputEvent&) {
		Actor* player = RE::PlayerCharacter::GetSingleton();
		ActionRegistry::Perform(player, Crush::EntryAction(player, "EnterQuick"));
	}

}

namespace GTS::Actions {

	std::span<const GraphExpect> ButtCrushNode::Signature() const { return ::Signature; }
	std::span<const std::string_view> ButtCrushNode::ExitSignals() const { return ::ExitSignals; }
	std::string_view ButtCrushNode::AbortSignal() const { return BEH_ABORT; }
	std::span<const EntryDef> ButtCrushNode::Entries() const { return ::Entries; }
	std::span<const ActionDef> ButtCrushNode::Actions() const { return ::Actions; }
	std::span<const AnnotationDef> ButtCrushNode::Annotations() const { return ::Annotations; }

	ButtCrushNode::State* ButtCrushNode::Get(RE::FormID a_Owner) {
		return m_State.Find(a_Owner);
	}

	ButtCrushNode::State& ButtCrushNode::GetOrAdd(RE::FormID a_Owner) {
		return m_State.GetOrAdd(a_Owner);
	}

	void ButtCrushNode::RegisterInput() {
		Keybinds::NoteBindKind("Action.Crush.Start", true);
		InputManager::RegisterInputEvent("Action.Crush.Start", StartEvent, CanStartEvent);
		Keybinds::NoteBindKind("Action.Crush.StartQuick", true);
		InputManager::RegisterInputEvent("Action.Crush.StartQuick", QuickStartEvent, CanQuickStartEvent);
	}

	// Reached through ButtCrush.Enter whatever the performer's stance is. StartButtCrush derives the
	// real entry from the follower, so a crawling follower still gets the breast crush.
	bool ButtCrushNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view, bool a_Explain) const {

		if (!CanDoActionBasedOnQuestProgress(a_Target, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}

		auto& controller = ButtCrushController::GetSingleton();

		controller.AllowMessage(a_Explain);
		const bool started = ButtCrushController::StartButtCrush(a_Actor, a_Target);
		controller.AllowMessage(false);

		return started;
	}

	// Crawling is BoobCrushNode's stance, not this one's.
	bool ButtCrushNode::CanEnter(const EntryContext& a_Ctx) const {
		return Crush::CanEnter(a_Ctx, false);
	}

	void ButtCrushNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		auto* giant = a_Ctx.Actor();
		// Knee crush is the crouched set. Crawling is BoobCrushNode's, so it never reaches here.
		state.Variant = a_Ctx.Stance() == Stance::kStanding ? Variant::kStanding : Variant::kKnee;

		// The quick entry never fires GTSButtCrush_MoveBody_Start, so recording here is the only way
		// every variant has a scale to come back to.
		Crush::RecordStart(giant);
		state.Quick = a_Ctx.EnteredVia() == "ButtCrush.EnterQuick";
	}

	// Everything started anywhere in this node stops here, on every path out. The legacy files never
	// unwound the attach task, the camera task or the recorded growth, so an abnormal exit left the
	// giant grown, the tiny stuck to AnimObjectB and the camera locked to the butt.
	void ButtCrushNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		Crush::CancelHeldTasks(owner, PossessionSlot::kButt);

		TaskManager::Cancel(std::format(TASKID_FMT_CAMERA, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_ATTACK, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_MAGICKA, owner));

		if (giant) {
			StopRumble(RUMBLE_TAG_GROWTH, *giant);

			// Only if the animation did not already restore it. The giant keeps whatever size a
			// desync interrupted otherwise.
			Crush::RestoreSize(giant);
		}

		if (a_Reason != ExitReason::kCompleted) {
			logger::debug("ButtCrush: {:08X} left on {}", owner, ExitReasonName(a_Reason));
		}

		m_State.Forget(owner);
	}

	void ButtCrushNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void ButtCrushNode::OnReset() {
		m_State.Clear();
	}

	std::string_view ButtCrushNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);
		if (!state) {
			return "";
		}

		if (state->Dropping) {
			return "Dropping";
		}

		return state->Quick ? "Quick" : VariantNames[std::to_underlying(state->Variant)];
	}
}
