#include "Actions/Nodes/Vore/VoreNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/PlayerTarget.hpp"
#include "Config/Keybinds.hpp"
#include "Actions/Core/Possession.hpp"

#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Managers/Rumble.hpp"

#include "Config/Config.hpp"
#include "Utils/Actions/VoreUtils.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
*/

/*
  ----- Vore Standing [("GTSBEH_StartVore")]
  [INTRO]
    IdleOffsetStop   [NON GTS]
    HeadTrackingOff  [NON GTS]
    GTSvore_sit_start
    GTSvore_impactLS
    GTSvore_sit_end
    GTSvore_hand_extend
  [GRAB]
    GTSvore_hand_grab
    GTSvore_attachactor_AnimObject_A
    GTSvore_bringactor_start
  [SWALLOW]
    GTSvore_open_mouth
    GTSvore_bringactor_end
    GTSvore_swallow
    GTSvore_swallow_sound
    GTSvore_close_mouth
  [RECOVER]
    GTSvore_handR_reposition_S
    GTSvore_handL_reposition_S
    GTSvore_handR_reposition_E
    GTSvore_handL_reposition_E
    GTSvore_eat_actor
    GTSvore_detachactor_AnimObject_A
  [STAND UP/EXIT]
    GTSvore_standup_start
    GTSvore_impactRS
    GTSBEH_Exit
    GTSvore_standup_end
  [CLEAN UP]
    GTS_ResetVars
    HeadTrackingOn      [NON GTS]
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailMTIdle          [NON GTS]
    GTSBeh_Dummy
*/

/*
  ----- Vore Sneaking (Crouching) [("GTSBEH_StartVore")]
  [INTRO]
    IdleOffsetStop   [NON GTS]
    HeadTrackingOff  [NON GTS]
    GTS_Sneak_Vore_Start
    GTS_Sneak_Vore_SmileOn
  [GRAB]
    GTS_Sneak_Vore_Grab
  [SWALLOW]
    GTS_Sneak_Vore_OpenMouth
    GTS_Sneak_Vore_CloseMouth
    GTS_Sneak_Vore_Swallow
    GTS_Sneak_Vore_KillAll
  [EXIT]
    GTS_Sneak_Vore_SmileOff
    GTSBEH_Exit
  [CLEAN UP]
    GTS_ResetVars
    HeadTrackingOn      [NON GTS]
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailSneakIdle       [NON GTS]
    GTSBeh_Dummy

  **Info: The whole sequence is one pass, there is no idle loop to sit in.
*/

/*
  ----- Vore Crawling [("GTSBEH_StartVore")]
  ----- The GTSCrawl_* impacts belong to the crawl locomotion, not to vore. They fire throughout and
  ----- are handled by Crawling.cpp.
  [INTRO]
    IdleOffsetStop   [NON GTS]
    HeadTrackingOff  [NON GTS]
    GTSBeh_CrawlVoring
    GTSCrawl_KneeImpact_L / GTSCrawl_HandImpact_R / GTSCrawl_KneeImpact_R / GTSCrawl_HandImpact_L
    GTSCrawlVore_SmileOn
  [GRAB]
    GTSCrawl_KneeImpact_R / GTSCrawl_KneeImpact_L
    GTSCrawlVore_Grab
    GTSCrawl_ButtImpact
  [SWALLOW]
    GTSCrawlVore_OpenMouth
    GTSCrawlVore_CloseMouth
    GTSCrawlVore_Swallow
    GTSCrawlVore_KillAll
  [EXIT]
    GTSCrawlVore_SmileOff
    GTSBeh_CrawlVoreExit
    GTSBEH_Exit
  [CLEAN UP]
    GTS_ResetVars
    HeadTrackingOn      [NON GTS]
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailSneakIdle       [NON GTS]
    GTSBeh_Dummy
*/

/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/

/*
   ----- Vore Standing [("GTSBEH_StartVore")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
     GTS_Busy                -> TRUE
     GTS_IsVoring            -> TRUE
   (NON GTS)
     TDM_Dodge               -> TRUE
     TDM_LockRotation11      -> TRUE
     bAnimationDriven        -> TRUE
     bNoStagger              -> TRUE
     tdmHeadtrackingBehavior -> FALSE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
   (NON GTS)
     tdmHeadtrackingBoth     -> FALSE
   [AFTER GTSvore_sit_start]
   (NON GTS)
     bIdleBeforeConversation -> FALSE
     bVoiceReady             -> FALSE
   [DURING THE REST OF THE ANIMATION]
     **NO CHANGES**
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**
     GTS_HHoffset            -> FALSE
*/

/*
   ----- Vore Sneaking (Crouching) [("GTSBEH_StartVore")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
     GTS_Busy                -> TRUE
     GTS_IsVoring            -> TRUE
   (NON GTS)
     TDM_Dodge               -> TRUE
     TDM_LockRotation11      -> TRUE
     bAnimationDriven        -> TRUE
     bHumanoidFootIKEnable   -> FALSE
     bNoStagger              -> TRUE
     tdmHeadtrackingBehavior -> FALSE
   [ONE FRAME LATER]
     GTS_HHoffset            -> TRUE
     GTS_Ready               -> FALSE
   (NON GTS)
     tdmHeadtrackingBoth     -> FALSE
   [SHORTLY AFTER]
   (NON GTS)
     IsSneaking              -> FALSE
     bVoiceReady             -> FALSE
   [DURING THE REST OF THE ANIMATION]
     **NO CHANGES**
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: bHumanoidFootIKEnable is the only var unique to this variant. Sneak does not set
   GTS_IsCrawlVoring, so by the graph it looks the same as the standing one.
*/

/*
   ----- Vore Crawling [("GTSBEH_StartVore")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
     GTS_Busy                -> TRUE
     GTS_IsCrawlVoring       -> TRUE
     GTS_IsVoring            -> TRUE
   (NON GTS)
     TDM_Dodge               -> TRUE
     TDM_LockRotation11      -> TRUE
     bAnimationDriven        -> TRUE
     bNoStagger              -> TRUE
     tdmHeadtrackingBehavior -> FALSE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
     GTS_VoreCamera          -> TRUE
   (NON GTS)
     tdmHeadtrackingBoth     -> FALSE
   [SHORTLY AFTER]
   (NON GTS)
     IsSneaking              -> FALSE
     bVoiceReady             -> FALSE
   [ON GTSCrawlVore_Grab]
     GTS_VoreCamera          -> FALSE
   [ON GTSCrawlVore_SmileOff]
     GTS_VoreCamera          -> TRUE
   [ON GTSBeh_CrawlVoreExit]
     GTS_VoreCamera          -> FALSE
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_VoreCamera is driven three times across this variant and nothing in the DLL reads it.
   Crawl is the only variant that touches it.
 */

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Variant = VoreNode::Variant;
	using Stage = VoreNode::Stage;
	using State = VoreNode::State;

	constexpr std::string_view NODE_FOOT_R = "NPC R Foot [Rft ]";
	constexpr std::string_view NODE_FOOT_L = "NPC L Foot [Lft ]";
	constexpr std::string_view NODE_THIGH_R = "NPC R Thigh [RThg]";
	constexpr std::string_view NODE_THIGH_L = "NPC L Thigh [LThg]";
	constexpr std::string_view NODE_BUTT_R = "NPC R Butt";
	constexpr std::string_view NODE_BUTT_L = "NPC L Butt";

	constexpr std::string_view RUMBLE_TAG_BODY  = "Vore_Rumble_Body";
	constexpr std::string_view RUMBLE_TAG_HAND_R = "Vore_Rumble_HandR";
	constexpr std::string_view RUMBLE_TAG_HAND_L = "Vore_Rumble_HandL";

	constexpr std::array RHAND_RUMBLE_NODES = {
		"NPC R UpperarmTwist1 [RUt1]"sv,
		"NPC R UpperarmTwist2 [RUt2]"sv,
		"NPC R Forearm [RLar]"sv,
		"NPC R ForearmTwist2 [RLt2]"sv,
		"NPC R ForearmTwist1 [RLt1]"sv,
		"NPC R Hand [RHnd]"sv,
	};

	constexpr std::array LHAND_RUMBLE_NODES = {
		"NPC L UpperarmTwist1 [LUt1]"sv,
		"NPC L UpperarmTwist2 [LUt2]"sv,
		"NPC L Forearm [LLar]"sv,
		"NPC L ForearmTwist2 [LLt2]"sv,
		"NPC L ForearmTwist1 [LLt1]"sv,
		"NPC L Hand [LHnd]"sv,
	};

	constexpr std::array BODY_RUMBLE_NODES = {
		"NPC COM [COM ]"sv,
		"NPC L Foot [Lft ]"sv,
		"NPC R Foot [Rft ]"sv,
		"NPC L Toe0 [LToe]"sv,
		"NPC R Toe0 [RToe]"sv,
		"NPC L Calf [LClf]"sv,
		"NPC R Calf [RClf]"sv,
		"NPC L PreRearCalf"sv,
		"NPC R PreRearCalf"sv,
		"NPC L FrontThigh"sv,
		"NPC R FrontThigh"sv,
		"NPC R RearCalf [RrClf]"sv,
		"NPC L RearCalf [RrClf]"sv,
	};

	void StartRumble(std::string_view a_Tag, Actor& a_Actor, float a_Power, float a_Halflife, std::span<const std::string_view> a_Nodes) {
		for (const auto& node : a_Nodes) {
			Rumbling::Start(std::format("{}_{}", a_Tag, node), &a_Actor, a_Power, a_Halflife, node);
		}
	}

	void StopRumble(std::string_view a_Tag, Actor& a_Actor, std::span<const std::string_view> a_Nodes) {
		for (const auto& node : a_Nodes) {
			Rumbling::Stop(std::format("{}_{}", a_Tag, node), &a_Actor);
		}
	}

	VoreData& Data(const ActionContext& a_Ctx) {
		return VoreController::GetSingleton().GetVoreData(a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Shared across the variants
	//----------------------------------------------------------------------------------------------

	// Every variant opens by taking the tinies out of the world: no crushing, no collision, held.
	void HoldTinies(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto& data = Data(a_Ctx);

		data.AllowToBeVored(false);

		for (auto* tiny : data.GetVories()) {
			AllowToBeCrushed(tiny, false);
			DisableCollisions(tiny, giant);
			SetBeingHeld(tiny, true);
			a_Ctx.Take(PossessionSlot::kMouth, tiny);
		}
	}

	void GrabTinies(const ActionContext& a_Ctx, bool a_AlwaysGrab) {

		auto* giant = a_Ctx.Actor();
		auto& data = Data(a_Ctx);

		if (a_AlwaysGrab) {
			data.GrabAll();
		}

		for (auto* tiny : data.GetVories()) {

			if (!a_AlwaysGrab && !Vore_ShouldAttachToRHand(giant, tiny)) {
				data.GrabAll();
			}

			tiny->NotifyAnimationGraph("JumpFall");
			tiny->Attacked(giant);
		}

		VoreNode::GetOrAdd(a_Ctx.Owner()).Stage = Stage::kHeld;
	}

	void ShrinkTinies(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		for (auto* tiny : Data(a_Ctx).GetVories()) {
			VoreController::ShrinkOverTime(giant, tiny);
		}
	}

	void Smile(const ActionContext& a_Ctx, bool a_On, float a_In = 0.0f, float a_Out = 0.0f) {

		auto* giant = a_Ctx.Actor();
		const float value = a_On ? 1.0f : 0.0f;
		const float mouth = a_On ? 0.8f : 0.0f;

		if (a_In > 0.0f) {
			AdjustFacialExpression(giant, 2, value, CharEmotionType::Expression, a_In, a_Out);
			AdjustFacialExpression(giant, 3, mouth, CharEmotionType::Phenome, a_In, a_Out);
			return;
		}

		AdjustFacialExpression(giant, 2, value, CharEmotionType::Expression);
		AdjustFacialExpression(giant, 3, mouth, CharEmotionType::Phenome);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks - standing
	//----------------------------------------------------------------------------------------------

	void OnSitStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		HoldTinies(a_Ctx);

		if (Config::Gameplay.ActionSettings.bVoreFreecam && giant->IsPlayerRef()) {
			EnableFreeCamera();
			VoreNode::GetOrAdd(a_Ctx.Owner()).FreeCamera = true;
		}
		else {
			ManageCamera(giant, true, CameraTracking::Hand_Right);
		}

		StartRumble(RUMBLE_TAG_BODY, *giant, 0.15f, 0.10f, BODY_RUMBLE_NODES);
		Task_HighHeel_SyncVoreAnim(giant);
	}

	void OnImpactLS(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto& data = Data(a_Ctx);

		for (auto* tiny : data.GetVories()) {
			tiny->NotifyAnimationGraph("GTS_EnterFear");
		}

		data.AllowToBeVored(false);

		float shake = Rumble_Vore_Stomp_Light * GetHighHeelsBonusDamage(giant, true);
		if (TinyCalamityActive(giant)) {
			shake *= 2.0f;
		}

		Rumbling::Once("StompLS", giant, shake, 0.05f, NODE_FOOT_L, 0.0f);
		DoFootstepSound(giant, 0.90f, FootEvent::Left, NODE_FOOT_L);
		DoDustExplosion(giant, 0.90f, FootEvent::Left, NODE_FOOT_L);
		DoDamageEffect(giant, Damage_Vore_Standing_Footstep, Radius_Vore_Standing_Footstep, 30, 0.25f, FootEvent::Left, 1.0f, DamageSource::CrushedLeft);
		DoLaunch(giant, 0.50f, 1.20f, FootEvent::Left);
	}

	void OnSitEnd(const ActionContext& a_Ctx) {
		StopRumble(RUMBLE_TAG_BODY, *a_Ctx.Actor(), BODY_RUMBLE_NODES);
		AdjustFacialExpression(a_Ctx.Actor(), 2, 1.0f, CharEmotionType::Expression);
	}

	void OnHandExtend(const ActionContext& a_Ctx) {

		StartRumble(RUMBLE_TAG_HAND_R, *a_Ctx.Actor(), 0.25f, 0.15f, RHAND_RUMBLE_NODES);

		for (auto* tiny : Data(a_Ctx).GetVories()) {
			tiny->NotifyAnimationGraph("GTS_ExitFear");
		}
	}

	void OnHandGrab(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Data(a_Ctx).GrabAll();
		GrabTinies(a_Ctx, true);

		ManageCamera(giant, false, CameraTracking::Hand_Right);
		ManageCamera(giant, true, CameraTracking::VoreHand_Right);

		StopRumble(RUMBLE_TAG_HAND_R, *giant, RHAND_RUMBLE_NODES);
	}

	void OnBringActorStart(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 3, 0.8f, CharEmotionType::Phenome);
		StartRumble(RUMBLE_TAG_HAND_R, *a_Ctx.Actor(), 0.2f, 0.175f, RHAND_RUMBLE_NODES);
	}

	void OnOpenMouth(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_OpenMouth(a_Ctx.Actor(), 0.75f, "StandingVoreOpenMouth");
		ShrinkTinies(a_Ctx);
	}

	void OnBringActorEnd(const ActionContext& a_Ctx) {

		auto& data = Data(a_Ctx);

		StopRumble(RUMBLE_TAG_HAND_R, *a_Ctx.Actor(), RHAND_RUMBLE_NODES);
		data.AllowToBeVored(true);

		for (auto* tiny : data.GetVories()) {
			AllowToBeCrushed(tiny, true);
		}
	}

	void OnSwallow(const ActionContext& a_Ctx) {
		VoreNode::GetOrAdd(a_Ctx.Owner()).Stage = Stage::kSwallowed;
		ApplySwallowAndDevourment(a_Ctx.Actor());
	}

	void OnSwallowSound(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 3, 0.0f, CharEmotionType::Phenome);
	}

	void OnCloseMouth(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		TaskManager::Cancel(std::format("Phenome_{}_{}_{}", giant->formID, 0, 1.0f));
		TaskManager::Cancel(std::format("Phenome_{}_{}_{}", giant->formID, 1, 0.5f));

		for (auto* tiny : Data(a_Ctx).GetVories()) {
			if (tiny->IsPlayerRef()) {
				RE::PlayerCamera::GetSingleton()->cameraTarget = giant->CreateRefHandle();
			}
		}
	}

	void OnHandRepositionRStart(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Modifier);
		AdjustFacialExpression(giant, 1, 0.0f, CharEmotionType::Modifier);
		StartRumble(RUMBLE_TAG_HAND_R, *giant, 0.20f, 0.15f, RHAND_RUMBLE_NODES);
	}

	void OnHandRepositionLStart(const ActionContext& a_Ctx) {
		StartRumble(RUMBLE_TAG_HAND_L, *a_Ctx.Actor(), 0.20f, 0.15f, LHAND_RUMBLE_NODES);
	}

	void OnHandRepositionREnd(const ActionContext& a_Ctx) {
		StopRumble(RUMBLE_TAG_HAND_R, *a_Ctx.Actor(), RHAND_RUMBLE_NODES);
	}

	void OnHandRepositionLEnd(const ActionContext& a_Ctx) {
		StopRumble(RUMBLE_TAG_HAND_L, *a_Ctx.Actor(), LHAND_RUMBLE_NODES);
	}

	void OnEatActor(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 2, 0.0f, CharEmotionType::Expression);
		FinishAllTinies(a_Ctx.Actor());
	}

	void OnStandUpStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		StartRumble(RUMBLE_TAG_BODY, *giant, 0.15f, 0.10f, BODY_RUMBLE_NODES);
		ManageCamera(giant, false, CameraTracking::Hand_Right);
		ManageCamera(giant, false, CameraTracking::VoreHand_Right);
	}

	void OnImpactRS(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);

		Rumbling::Once("StompRS", giant, 0.95f, 0.05f, NODE_FOOT_R, 0.0f);
		DoFootstepSound(giant, 0.90f, FootEvent::Right, NODE_FOOT_R);
		DoDustExplosion(giant, 0.90f, FootEvent::Right, NODE_FOOT_R);
		DoDamageEffect(giant, Damage_Vore_Standing_Footstep, Radius_Vore_Standing_Footstep, 30, 0.25f, FootEvent::Right, 1.0f, DamageSource::CrushedRight);
		DoLaunch(giant, 0.70f * perk, 1.20f * a_Ctx.AnimSpeed(), FootEvent::Right);
	}

	void OnStandUpEnd(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Data(a_Ctx).ReleaseAll();

		if (Config::Gameplay.ActionSettings.bVoreFreecam && giant->IsPlayerRef()) {
			EnableFreeCamera();
			VoreNode::GetOrAdd(a_Ctx.Owner()).FreeCamera = false;
		}

		StopRumble(RUMBLE_TAG_BODY, *giant, BODY_RUMBLE_NODES);
		Utils_UpdateHighHeelBlend(giant, true);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks - sneaking
	//----------------------------------------------------------------------------------------------

	void OnSneakStart(const ActionContext& a_Ctx) {
		HoldTinies(a_Ctx);
		Task_HighHeel_SyncVoreAnim(a_Ctx.Actor());
	}

	// Sneak and crawl both attach to the left hand object when a tiny is already grabbed.
	void OnHandGrabToObject(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		GrabTinies(a_Ctx, false);

		ManageCamera(giant, true, AnimationVars::Grab::HasGrabbedTiny(giant) ? CameraTracking::ObjectA : CameraTracking::Hand_Right);
	}

	void OnSneakOpenMouth(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_OpenMouth(a_Ctx.Actor(), 0.6f, "SneakVoreOpenMouth");
		ShrinkTinies(a_Ctx);
	}

	void OnSneakSwallow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		VoreNode::GetOrAdd(a_Ctx.Owner()).Stage = Stage::kSwallowed;

		ManageCamera(giant, false, CameraTracking::ObjectA);
		ManageCamera(giant, false, CameraTracking::Hand_Right);
		ApplySwallowAndDevourment(giant, true);
	}

	void OnSneakSmileOn(const ActionContext& a_Ctx) {
		Smile(a_Ctx, true, 0.32f, 0.72f);
	}

	void OnSneakSmileOff(const ActionContext& a_Ctx) {
		Smile(a_Ctx, false, 0.32f, 0.72f);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks - crawling
	//----------------------------------------------------------------------------------------------

	void OnCrawlStart(const ActionContext& a_Ctx) {
		HoldTinies(a_Ctx);
	}

	void OnCrawlSmileOn(const ActionContext& a_Ctx) {
		Smile(a_Ctx, true);
	}

	void OnCrawlSmileOff(const ActionContext& a_Ctx) {
		Smile(a_Ctx, false);
	}

	void OnCrawlButtImpact(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		float dust = 1.0f;
		float smt = 1.0f;

		if (TinyCalamityActive(giant)) {
			dust = 1.25f;
			smt = 2.0f;
		}

		auto* thighL = find_node(giant, NODE_THIGH_L);
		auto* thighR = find_node(giant, NODE_THIGH_R);
		auto* buttR = find_node(giant, NODE_BUTT_R);
		auto* buttL = find_node(giant, NODE_BUTT_L);

		if (!buttR || !buttL || !thighL || !thighR) {
			return;
		}

		ApplyThighDamage(giant, true, false, Radius_ThighCrush_ButtCrush_Drop, Damage_ButtCrush_LegDrop * perk, 0.35f, 1.0f, 14, DamageSource::ThighCrushed);
		ApplyThighDamage(giant, false, false, Radius_ThighCrush_ButtCrush_Drop, Damage_ButtCrush_LegDrop * perk, 0.35f, 1.0f, 14, DamageSource::ThighCrushed);

		DoDamageAtPoint(giant, Radius_Crawl_Vore_ButtImpact, Damage_Crawl_Vore_Butt_Impact * perk, thighL, 10, 0.70f, 0.95f, DamageSource::Booty);
		DoDamageAtPoint(giant, Radius_Crawl_Vore_ButtImpact, Damage_Crawl_Vore_Butt_Impact * perk, thighR, 10, 0.70f, 0.95f, DamageSource::Booty);
		DoDustExplosion(giant, 1.8f * dust, FootEvent::Right, NODE_BUTT_R);
		DoDustExplosion(giant, 1.8f * dust, FootEvent::Left, NODE_BUTT_L);
		DoFootstepSound(giant, 1.2f, FootEvent::Right, NODE_FOOT_R);
		DoFootstepSound(giant, 1.2f, FootEvent::Left, NODE_FOOT_L);
		DoLaunch(giant, 2.25f, 5.0f, FootEvent::Butt);

		const float shake = Rumble_Crawl_KneeDrop * smt;
		Rumbling::Once("Butt_L", giant, shake, 0.10f, NODE_BUTT_R, 0.0f);
		Rumbling::Once("Butt_R", giant, shake, 0.10f, NODE_BUTT_L, 0.0f);
	}

	void OnCrawlOpenMouth(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_OpenMouth(a_Ctx.Actor(), 0.5f, "CrawlVoreOpenMouth");
		ShrinkTinies(a_Ctx);
	}

	void OnCrawlSwallow(const ActionContext& a_Ctx) {
		VoreNode::GetOrAdd(a_Ctx.Owner()).Stage = Stage::kSwallowed;
		ApplySwallowAndDevourment(a_Ctx.Actor());
	}

	void OnCrawlKillAll(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, false, CameraTracking::ObjectA);
		ManageCamera(giant, false, CameraTracking::Hand_Right);
		FinishAllTinies(giant);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	// Measured: all three variants set GTS_IsVoring, crawl additionally setting GTS_IsCrawlVoring.
	// One signature confirms every entry, so none of them needs an override.
	constexpr GraphExpect Signature[] = {
		{ "GTS_IsVoring", true },
		{ "GTS_Busy",     true },
	};

	// Leaves the root Giantess_State from any point inside it. Vore, both crushes and thigh crush
	// all run in that state, and nothing else in the DLL sends this.
	constexpr std::string_view BEH_ABORT = "GTSBeh_ExitEvents";

	constexpr std::string_view ExitSignals[] = {
		"GTSvore_standup_start",
		"GTSvore_standup_end",
		"GTSCrawlVore_SmileOff",
		"GTS_Sneak_Vore_SmileOff",
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "Vore.Enter", .Behaviour = "GTSBEH_StartVore", .BlockedBy = kBlockedByHand },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSvore_sit_start",                .Handler = OnSitStart },
		{ .Tag = "GTSvore_impactLS",                 .Handler = OnImpactLS },
		{ .Tag = "GTSvore_sit_end",                  .Handler = OnSitEnd },
		{ .Tag = "GTSvore_hand_extend",              .Handler = OnHandExtend },
		{ .Tag = "GTSvore_hand_grab",                .Handler = OnHandGrab },
		{ .Tag = "GTSvore_attachactor_AnimObject_A" },
		{ .Tag = "GTSvore_bringactor_start",         .Handler = OnBringActorStart },
		{ .Tag = "GTSvore_open_mouth",               .Handler = OnOpenMouth },
		{ .Tag = "GTSvore_bringactor_end",           .Handler = OnBringActorEnd },
		{ .Tag = "GTSvore_swallow",                  .Handler = OnSwallow },
		{ .Tag = "GTSvore_swallow_sound",            .Handler = OnSwallowSound },
		{ .Tag = "GTSvore_close_mouth",              .Handler = OnCloseMouth },
		{ .Tag = "GTSvore_handR_reposition_S",       .Handler = OnHandRepositionRStart },
		{ .Tag = "GTSvore_handL_reposition_S",       .Handler = OnHandRepositionLStart },
		{ .Tag = "GTSvore_handR_reposition_E",       .Handler = OnHandRepositionREnd },
		{ .Tag = "GTSvore_handL_reposition_E",       .Handler = OnHandRepositionLEnd },
		{ .Tag = "GTSvore_eat_actor",                .Handler = OnEatActor },
		{ .Tag = "GTSvore_detachactor_AnimObject_A" },
		{ .Tag = "GTSvore_standup_start",            .Handler = OnStandUpStart },
		{ .Tag = "GTSvore_impactRS",                 .Handler = OnImpactRS },
		{ .Tag = "GTSvore_standup_end",              .Handler = OnStandUpEnd },

		{ .Tag = "GTS_Sneak_Vore_Start",             .Handler = OnSneakStart },
		{ .Tag = "GTS_Sneak_Vore_Grab",              .Handler = OnHandGrabToObject },
		{ .Tag = "GTS_Sneak_Vore_OpenMouth",         .Handler = OnSneakOpenMouth },
		{ .Tag = "GTS_Sneak_Vore_Swallow",           .Handler = OnSneakSwallow },
		{ .Tag = "GTS_Sneak_Vore_CloseMouth" },
		{ .Tag = "GTS_Sneak_Vore_KillAll" },
		{ .Tag = "GTS_Sneak_Vore_SmileOn",           .Handler = OnSneakSmileOn },
		{ .Tag = "GTS_Sneak_Vore_SmileOff",          .Handler = OnSneakSmileOff },

		{ .Tag = "GTSBeh_CrawlVoring",               .Handler = OnCrawlStart },
		{ .Tag = "GTSCrawlVore_SmileOn",             .Handler = OnCrawlSmileOn },
		{ .Tag = "GTSCrawlVore_Grab",                .Handler = OnHandGrabToObject },
		{ .Tag = "GTSCrawl_ButtImpact",              .Handler = OnCrawlButtImpact },
		{ .Tag = "GTSCrawlVore_OpenMouth",           .Handler = OnCrawlOpenMouth },
		{ .Tag = "GTSCrawlVore_Swallow",             .Handler = OnCrawlSwallow },
		{ .Tag = "GTSCrawlVore_CloseMouth" },
		{ .Tag = "GTSCrawlVore_KillAll",             .Handler = OnCrawlKillAll },
		{ .Tag = "GTSCrawlVore_SmileOff",            .Handler = OnCrawlSmileOff },
		{ .Tag = "GTSBEH_CrawlVoreExit" },

		{ .Tag = "GTSBEH_Next", .Shared = true },
		{ .Tag = "GTSBEH_Exit", .Shared = true },
	};

	constexpr std::string_view VariantNames[] = {
		"Standing",
		"Sneak",
		"Crawl",
	};

	//----------------------------------------------------------------------------------------------
	// Input
	//----------------------------------------------------------------------------------------------

	// The target list is a decision, not a key mapping: how many tinies come along is capped by the
	// giant's perks inside GetVoreTargetsInFront.
	void StartEvent(const ManagedInputEvent&) {

		static Timer voreTimer = Timer(0.25);

		if (PlayerTarget::Engaged("Action.Vore.Start")) {
			PlayerTarget::Perform("Vore.Enter");
			return;
		}

		Actor* pred = RE::PlayerCharacter::GetSingleton();
		if (!pred || !voreTimer.ShouldRunFrame()) {
			return;
		}

		auto& controller = VoreController::GetSingleton();

		controller.AllowMessage(true);
		for (auto* prey : controller.GetVoreTargetsInFront(pred, 1)) {
			controller.StartVore(pred, prey);
		}
		controller.AllowMessage(false);
	}

	bool CanStartEvent() {

		if (PlayerTarget::Engaged("Action.Vore.Start")) {
			return PlayerTarget::CanPerform("Vore.Enter");
		}

		Actor* player = RE::PlayerCharacter::GetSingleton();
		return player && ActionRegistry::CanPerform(player, "Vore.Enter");
	}
}

namespace GTS::Actions {

	std::span<const GraphExpect> VoreNode::Signature() const { return ::Signature; }
	std::span<const std::string_view> VoreNode::ExitSignals() const { return ::ExitSignals; }
	std::string_view VoreNode::AbortSignal() const { return BEH_ABORT; }
	std::span<const EntryDef> VoreNode::Entries() const { return ::Entries; }
	std::span<const AnnotationDef> VoreNode::Annotations() const { return ::Annotations; }

	VoreNode::State* VoreNode::Get(RE::FormID a_Owner) {
		return m_State.Find(a_Owner);
	}

	VoreNode::State& VoreNode::GetOrAdd(RE::FormID a_Owner) {
		return m_State.GetOrAdd(a_Owner);
	}

	void VoreNode::RegisterInput() {
		Keybinds::NoteBindKind("Action.Vore.Start", true);
		InputManager::RegisterInputEvent("Action.Vore.Start", StartEvent, CanStartEvent);
	}

	bool VoreNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view, bool a_Explain) const {

		if (!CanDoActionBasedOnQuestProgress(a_Target, QuestAnimationType::kVore)) {
			return false;
		}

		auto& controller = VoreController::GetSingleton();

		controller.AllowMessage(a_Explain);
		const bool started = controller.StartVore(a_Actor, a_Target);
		controller.AllowMessage(false);

		return started;
	}

	bool VoreNode::CanEnter(const EntryContext& a_Ctx) const {

		auto* giant = a_Ctx.Actor();
		if (!giant) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kVore)) {
			return false;
		}

		if (IsPlayerFirstPerson(giant) || AnimationVars::General::IsGTSBusy(giant)) {
			return false;
		}

		return true;
	}

	void VoreNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		auto* giant = a_Ctx.Actor();
		if (!giant) {
			return;
		}

		switch (a_Ctx.Stance()) {
			case Stance::kCrawl: { state.Variant = Variant::kCrawl; break; }
			case Stance::kSneak: { state.Variant = Variant::kSneak; break; }
			default:             { state.Variant = Variant::kStanding; break; }
		}
	}

	// Every variant leaves the tinies held, collision-free and un-crushable while it runs. The legacy
	// files only undid that on their own last annotation, so a desync left the prey stuck to the hand
	// with no collision and the giant's camera locked to it.
	void VoreNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		{
			auto& data = VoreController::GetSingleton().GetVoreData(giant);
			data.AllowToBeVored(true);
			data.ReleaseAll();
		}

		if (giant) {

			StopRumble(RUMBLE_TAG_BODY, *giant, BODY_RUMBLE_NODES);
			StopRumble(RUMBLE_TAG_HAND_R, *giant, RHAND_RUMBLE_NODES);
			StopRumble(RUMBLE_TAG_HAND_L, *giant, LHAND_RUMBLE_NODES);

			AdjustFacialExpression(giant, 2, 0.0f, CharEmotionType::Expression);
			AdjustFacialExpression(giant, 3, 0.0f, CharEmotionType::Phenome);

			// The free camera is only handed back if this node took it.
			if (const State* state = m_State.Find(owner); state && state->FreeCamera) {
				EnableFreeCamera();
			}

			Utils_UpdateHighHeelBlend(giant, true);
		}

		if (a_Reason != ExitReason::kCompleted) {
			logger::debug("Vore: {:08X} left on {}", owner, ExitReasonName(a_Reason));
		}

		m_State.Forget(owner);
	}

	void VoreNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void VoreNode::OnReset() {
		m_State.Clear();
	}

	std::string_view VoreNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);
		if (!state) {
			return "";
		}

		if (state->Stage == Stage::kSwallowed) {
			return "swallowed";
		}

		return VariantNames[std::to_underlying(state->Variant)];
	}
}
