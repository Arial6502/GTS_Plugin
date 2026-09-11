#include "Actions/Nodes/Hug/HugNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/PlayerTarget.hpp"
#include "Config/Keybinds.hpp"
#include "Actions/Core/Possession.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Controllers/HugController.hpp"
#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Managers/Rumble.hpp"

#include "Config/Config.hpp"
#include "Magic/Effects/Common.hpp"
#include "Managers/Animation/Utils/AttachPoint.hpp"
#include "Managers/Animation/Utils/TurnTowards.hpp"
#include "Utils/DeathReport.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  All three stances share one entry pair: GTSBEH_HugAbsorbStart_A to the giant, and either
  GTSBEH_HugAbsorbStart_V or GTSBEH_HugAbsorbStart_Sneak_V to the tiny. Crawl takes the sneak one.

  The giant fires GTS_Hug_* and GTS_CH_*. The tiny fires GTS_Hug_Moan_Tiny and its _End, which reach
  PartnerAnnotations because a possessed actor has no node of its own.

  The hug is a loop, not a sequence. After the intro the giant sits in the idle until a key sends
  one of GTSBEH_HugAbsorbAtk (shrink), GTSBEH_HugHealStart_A (heal), GTSBEH_HugCrushStart_A (crush)
  or GTSBEH_HugAbsorbExitLoop (release). Every one of those returns to the idle on GTSBEH_Next,
  except the crush and the release, which leave.
*/

/*
  ----- Hug Standing / Sneak [("GTSBEH_HugAbsorbStart_A")]
  ----- Sneak differs only in the tiny's start event, so the giant's order is the same.
  [INTRO]
    IdleOffsetStop   [NON GTS]
    GTS_Hug_Catch
    GTS_Hug_Grab
    GTSBEH_Next               -> settles into the idle loop
    HeadTrackingOff  [NON GTS]
  [IDLE LOOP]
    GTS_Hug_FacialOn
    GTS_Hug_PullBack
    GTS_Hug_FacialOff
    FootScuffLeft / FootScuffRight  [NON GTS]
  [SHRINK ("GTSBEH_HugAbsorbAtk")]
    GTS_Hug_Grow
    GTS_Hug_Moan
    GTS_Hug_Moan_End
    GTSBEH_Next               -> back to the idle loop
  [HEAL ("GTSBEH_HugHealStart_A")]
    GTS_Hug_Heal
    GTS_Hug_Moan
    GTS_Hug_Moan_Tiny         [FROM THE TINY]
    GTS_Hug_Moan_End
    GTS_Hug_Moan_Tiny_End     [FROM THE TINY]
    GTSBEH_Next               -> back to the idle loop
  [RELEASE ("GTSBEH_HugAbsorbExitLoop")]
    HeadTrackingOn   [NON GTS]
    GTS_Hug_Release           -> the tiny is on the ground from here
    GTSBEH_Exit
  [CLEAN UP]
    HeadTrackingOn      [NON GTS]
    AnimObjectUnequip   [NON GTS]
    GTS_CH_BoobCameraOff
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailMTIdle          [NON GTS]

  **Info: GTS_Hug_Release lands roughly 1.4s before GTSBEH_Exit. The giant is still standing back up
  **for that gap, which is why the put-down is done at the annotation and not on the way out.
*/

/*
  ----- Hug Crush [("GTSBEH_HugCrushStart_A")]
  ----- Reachable from the idle loop of any stance. Does not return to it.
  [CRUSH LOOP]
    GTS_Hug_ShrinkPulse
    GTS_Hug_Moan
    GTS_Hug_Moan_End
    (repeats until the tiny runs out of health)
  [KILL]
    GTS_HugCrushedTiny        [NOT REGISTERED] the graph's own kill marker
    GTS_Hug_CrushTiny         -> the tiny dies here and its 3D is usually unloaded with it
    GTS_Hug_Moan
    GTS_Hug_Moan_End
  [EXIT]
    GTSBEH_Exit_Event   [NON GTS]
    AnimObjectUnequip   [NON GTS]
    GTS_CH_BoobCameraOff
    GTSBeh_HugCrushEnd        -> the exit signal, ~6.7s after the kill
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailMTIdle / tailSneakIdle  [NON GTS]

  **Info: the giant keeps animating for several seconds after GTS_Hug_CrushTiny with nothing left to
  **hold. State::TinyCrushed is what stops the node reaching for the corpse over that stretch.
*/

/*
  ----- Hug Crawling [("GTSBEH_HugAbsorbStart_A" + "GTSBEH_HugAbsorbStart_Sneak_V")]
  ----- The GTSCrawl_* impacts belong to the crawl locomotion, not to hugs. They fire throughout and
  ----- are handled by Crawling.cpp.
  [INTRO]
    IdleOffsetStop   [NON GTS]
    HeadTrackingOff  [NON GTS]
    GTS_Hug_SwitchToObjectA   -> the tiny rides ObjectA instead of the hug attach points
    GTS_CH_RuneStart
    GTS_Hug_Catch
    GTS_Hug_Grab
    GTS_CH_BoobCameraOn
    GTS_Butt_Reset      [NOT REGISTERED]
    GTSBEH_Next               -> settles into the idle loop
  [IDLE LOOP / SHRINK / HEAL]
    Same as standing.
  [RELEASE ("GTSBEH_HugAbsorbExitLoop")]
    HeadTrackingOn   [NON GTS]
    GTS_Hug_Release           -> the tiny is on the ground from here, same as the other stances
    GTS_CH_BoobCameraOff
    GTSBEH_Exit               -> ~1.8s later, the giant is still finishing its own half
  [CLEAN UP]
    HeadTrackingOn      [NON GTS]
    AnimObjectUnequip   [NON GTS]
    GTS_CH_BoobCameraOff
    GTSBEH_Camera_Reset
    IdleStop            [NON GTS]
    tailSneakIdle       [NON GTS]

  **Info: the crush path is the same as the others, except GTS_Hug_SwitchToObjectA fires again when
  **the crush starts and GTS_Hug_SwitchToDefault fires after the kill.
*/

/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/

/*
   ----- Hug Standing / Sneak [("GTSBEH_HugAbsorbStart_A")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
     GTS_Busy                -> TRUE
     GTS_Hugging             -> TRUE
     GTS_HuggingTeammate     -> TRUE   (written by the DLL, friendly hugs only)
   (NON GTS)
     TDM_Dodge               -> TRUE
     TDM_LockRotation11      -> TRUE
     bAnimationDriven        -> TRUE
     bNoStagger              -> TRUE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
   [AFTER GTS_Hug_Catch]
   (NON GTS)
     bIdleBeforeConversation -> FALSE
     bVoiceReady             -> FALSE
   [AFTER GTSBEH_Next]
   (NON GTS)
     tdmHeadtrackingBehavior -> FALSE
     tdmHeadtrackingBoth     -> FALSE
   [WHILE SHRINKING]
     GTS_IsHugAbsorbing      -> TRUE, back to FALSE on the GTSBEH_Next that ends it
     GTS_HuggingTeammate     -> FALSE  (the shrink is the rough act, so the graph is told so)
   [WHILE HEALING]
     GTS_IsHugHealing        -> TRUE, back to FALSE on the GTSBEH_Next that ends it
   [WHILE CRUSHING]
     IsHugCrushing           -> TRUE
     GTS_TinyAbsorbed        -> TRUE at GTS_HugCrushedTiny, FALSE again at the exit
   [AT THE RELEASE]
     GTS_HuggingTeammate     -> TRUE, then FALSE one frame after GTS_Hug_Release
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_HuggingTeammate is what picks the ally animations, not the event. The DLL writes it,
   **so it is deliberately absent from the node's signature - a node must not verify what it sets.
   **The shrink clears it, so SendRelease restores it from State::Friendly before letting go.
*/

/*
   ----- Hug Crawling [("GTSBEH_HugAbsorbStart_A")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
     GTS_Busy                -> TRUE
     GTS_Hugging             -> TRUE
     GTS_HuggingTeammate     -> TRUE   (written by the DLL, friendly hugs only)
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
   [SHORTLY AFTER]
   (NON GTS)
     IsSneaking              -> flips several times over the first 0.3s
     bVoiceReady             -> FALSE
   [DURING THE REST OF THE ANIMATION]
     Same as standing.
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: IsSneaking is unusable as a stance test here. It is the raw actor flag, it flickers on
   **entry, and SetSneaking overwrites it on every hug exit, so OnEntryTaken reads GTS_IsCrawling
   **first and only falls back to it when the giant is not crawling.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Variant = HugNode::Variant;
	using State = HugNode::State;

	constexpr std::string_view NODE_HAND_L = "NPC L Hand [LHnd]";
	constexpr std::string_view NODE_HAND_R = "NPC R Hand [RHnd]";
	constexpr std::string_view NODE_COM = "NPC COM [COM ]";

	constexpr std::string_view TASKID_FMT_STAMINA = "HugStaminaDrain_{}";
	constexpr std::string_view TASKID_FMT_GROWTH  = "HugCrushGrowth_{}";
	constexpr std::string_view TASKID_FMT_STEAL   = "HugStealSize_{}";

	// Behaviours. The giant and the tiny each get their own half of every paired step.
	constexpr std::string_view BEH_START_GIANT    = "GTSBEH_HugAbsorbStart_A";
	constexpr std::string_view BEH_START_TINY     = "GTSBEH_HugAbsorbStart_V";
	constexpr std::string_view BEH_START_TINY_S   = "GTSBEH_HugAbsorbStart_Sneak_V";
	constexpr std::string_view BEH_SHRINK_GIANT   = "GTSBEH_HugAbsorbAtk";
	constexpr std::string_view BEH_SHRINK_TINY    = "GTSBEH_HugAbsorbAtk_V";
	constexpr std::string_view BEH_CRUSH_GIANT    = "GTSBEH_HugCrushStart_A";
	constexpr std::string_view BEH_CRUSH_TINY     = "GTSBEH_HugCrushStart_V";
	constexpr std::string_view BEH_HEAL_GIANT     = "GTSBEH_HugHealStart_A";
	constexpr std::string_view BEH_HEAL_TINY_F    = "GTSBEH_HugHealStart_Fem_V";
	constexpr std::string_view BEH_HEAL_TINY_M    = "GTSBEH_HugHealStart_Mal_V";
	constexpr std::string_view BEH_RELEASE        = "GTSBEH_HugAbsorbExitLoop";
	constexpr std::string_view BEH_ABORT          = "GTSBEH_PairedAbort";

	RE::Actor* Tiny(const ActionContext& a_Ctx) {
		return HugNode::Hugged(a_Ctx.Owner());
	}

	void DelayedHurtStamina(Actor* a_Giant, float a_Damage) {

		const std::string name = std::format(TASKID_FMT_STAMINA, a_Giant->formID);
		auto handle = a_Giant->CreateRefHandle();
		const double start = Time::WorldTimeElapsed();

		TaskManager::Run(name, [=](auto&) {

			if (!handle) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start < 0.6) {
				return true;
			}

			if (auto ptr = handle.get()) {
				DamageAV(ptr.get(), ActorValue::kStamina, a_Damage);
			}

			return false;
		});
	}

	void ShrinkPulse_DecreaseSize(Actor* a_Tiny, float a_Scale) {

		constexpr float min_scale = 0.04f;

		if (get_target_scale(a_Tiny) > min_scale) {
			set_target_scale(a_Tiny, a_Scale * 0.48f);
		}
		else {
			set_target_scale(a_Tiny, min_scale);
		}
	}

	void ShrinkPulse_GainSize(Actor* a_Giant, Actor* a_Tiny, bool a_Task) {

		float increase = Runtime::HasPerkTeam(a_Giant, Runtime::PERK.GTSPerkHugsGreed) ? 1.15f : 1.0f;

		if (!a_Task) {

			float steal = get_visual_scale(a_Tiny) * 0.035f * increase * 0.6f;

			// Crawl has one more shrink event, so the per pulse amount is lower.
			if (AnimationVars::Crawl::IsCrawling(a_Giant)) {
				steal *= 0.8f;
			}

			update_target_scale(a_Giant, steal, SizeEffectType::kGrow);
			return;
		}

		const std::string name = std::format(TASKID_FMT_GROWTH, a_Giant->formID);
		auto handle = a_Giant->CreateRefHandle();
		const double start = Time::WorldTimeElapsed();
		const float original = VoreController::ReadOriginalScale(a_Tiny);

		TaskManager::Run(name, [=](auto&) {

			if (!handle) {
				return false;
			}

			const float elapsed = static_cast<float>(Time::WorldTimeElapsed() - start);
			if (elapsed > 0.95f) {
				return false;
			}

			const float formula = std::min(bezier_curve(elapsed, 0.2f, 1.9f, 0, 0, 3.0f, 4.0f), 1.0f);
			const float grow = 0.000235f * 8 * original * increase * TimeScale() * formula * 0.6f;

			auto ptr = handle.get();

			if (!ptr) {
				return false;
			}

			override_actor_scale(ptr.get(), grow, SizeEffectType::kNeutral);
			return true;
		});
	}

	void ShakeCamera(Actor* a_Giant) {

		if (a_Giant->IsPlayerRef()) {
			shake_camera(a_Giant, 0.75f, 0.35f);
			return;
		}

		Rumbling::Once("HugGrab_L", a_Giant, Rumble_Hugs_Catch, 0.15f, NODE_HAND_L, 0.0f);
		Rumbling::Once("HugGrab_R", a_Giant, Rumble_Hugs_Catch, 0.15f, NODE_HAND_R, 0.0f);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks
	//----------------------------------------------------------------------------------------------

	// The shake lands on whichever annotation is the pick-up for that stance: the crouched sets grab
	// on the catch, standing grabs a beat later.
	void OnCatch(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (a_Ctx.Stance() != Stance::kStanding) {
			ShakeCamera(giant);
		}

		if (auto* tiny = Tiny(a_Ctx)) {
			DisableCollisions(tiny, giant);
			VoreController::RecordOriginalScale(tiny);
		}
	}

	void OnGrab(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Tiny(a_Ctx);

		if (!tiny) {
			return;
		}

		SetBeingHeld(tiny, true);

		if (a_Ctx.Stance() == Stance::kStanding) {
			ShakeCamera(giant);
		}
	}

	// Two seconds of taking size, started by the grow annotation. Ported from HugShrink so the node
	// owns its own task: the legacy one was named by that file and outlived every exit path here.
	void StealSize(Actor* a_Giant, Actor* a_Tiny) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		UpdateFriendlyHugs(a_Giant, a_Tiny, true);

		const std::string name = std::format(TASKID_FMT_STEAL, a_Giant->formID);
		const RE::ActorHandle giantHandle = a_Giant->GetHandle();
		const RE::ActorHandle tinyHandle = a_Tiny->GetHandle();

		TaskManager::RunFor(name, 2.0f, [=](auto&) {

			auto giantPtr = giantHandle.get();
			auto tinyPtr = tinyHandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			auto* tiny = tinyPtr.get();

			float shrink = 14.0f;
			float stamina = 0.35f;

			if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkHugsGreed)) {
				shrink *= 1.25f;
				stamina *= 0.75f;
			}

			stamina *= Perk_GetCostReduction(giant);

			const float difference = get_scale_difference(giant, tiny, SizeType::VisualScale, false, true);

			if (difference >= GetHugShrinkThreshold(giant) || get_target_scale(tiny) < Minimum_Actor_Scale) {
				Notify("{} stole all available size", giant->GetDisplayFullName());
				return false;
			}

			DamageAV(tiny, ActorValue::kStamina, 0.60f * TimeScale());
			DamageAV(giant, ActorValue::kStamina, 0.50f * stamina * TimeScale());

			TransferSize(giant, tiny, false, shrink, GetHugStealRate(giant) * 0.85f, false, ShrinkSource::Hugs);
			ModSizeExperience(giant, 0.00020f);

			if (!IsTeammate(tiny) && Config::Gameplay.ActionSettings.bNonLethalHugsHostile) {
				tiny->Attacked(giant);
			}

			Rumbling::Once("HugSteal", giant, Rumble_Hugs_Shrink, 0.12f, NODE_COM, 0.0f, true);
			return true;
		});
	}

	void OnGrow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Tiny(a_Ctx);

		if (!tiny) {
			return;
		}

		if (!IsTeammate(tiny) && Config::Gameplay.ActionSettings.bNonLethalHugsHostile) {
			tiny->Attacked(giant);
		}

		StealSize(giant, tiny);
	}

	void OnMoan(const ActionContext& a_Ctx) {

		const State* state = HugNode::Get(a_Ctx.Owner());
		if (state && state->MoansBlocked) {
			return;
		}

		Task_FacialEmotionTask_Moan(a_Ctx.Actor(), 1.15f, "HugMoan", RandomFloat(0.0f, 0.45f));
		Sound_PlayMoans(a_Ctx.Actor(), 1.0f, 0.14f, EmotionTriggerSource::HugDrain);
	}

	void OnMoanEnd(const ActionContext& a_Ctx) {
		HugNode::GetOrAdd(a_Ctx.Owner()).MoansBlocked = false;
	}

	void OnFacialOn(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 2, 1.0f, CharEmotionType::Expression);
	}

	void OnFacialOff(const ActionContext& a_Ctx) {
		AdjustFacialExpression(a_Ctx.Actor(), 2, 0.0f, CharEmotionType::Expression);
	}

	void OnPullBack(const ActionContext& a_Ctx) {

		if (RandomInt(0, 5) < 4) {
			return;
		}

		Sound_PlayLaughs(a_Ctx.Actor(), 1.0f, 0.14f, EmotionTriggerSource::Struggle);
		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 2.25f, "HugSmile");
	}

	void OnShrinkPulse(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Tiny(a_Ctx);

		if (!tiny) {
			return;
		}

		const float scale = get_visual_scale(tiny);

		if (!IsTeammate(tiny) && Config::Gameplay.ActionSettings.bNonLethalHugsHostile) {
			tiny->Attacked(giant);
		}

		ShrinkPulse_DecreaseSize(tiny, scale);
		ShrinkPulse_GainSize(giant, tiny, false);

		Rumbling::For("ShrinkPulse", giant, Rumble_Hugs_Shrink, 0.10f, NODE_COM, 0.50f / AnimationManager::GetAnimSpeed(giant), 0.0f, true);
		ModSizeExperience(giant, scale / 6);
	}

	void OnCrushTiny(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Tiny(a_Ctx);

		State& state = HugNode::GetOrAdd(a_Ctx.Owner());
		state.MoansBlocked = true;
		state.Crushing = true;
		state.TinyCrushed = true;

		// The steal runs for two seconds after GTS_Hug_Grow and does not know the tiny has died. A
		// crushed tiny is usually set to a disintegrated critical stage and has its 3D unloaded, so
		// what would be left is a task calling TransferSize and Attacked on a corpse.
		TaskManager::Cancel(std::format(TASKID_FMT_STEAL, a_Ctx.Owner()));

		if (!tiny) {
			return;
		}

		HugCrushOther(giant, tiny);
		ReportDeath(giant, tiny, DamageSource::Hugs);
		Rumbling::For("HugCrush", giant, Rumble_Hugs_HugCrush, 0.10f, NODE_COM, 0.15f, 0.0f, true);

		AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Phenome);
		AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Modifier);
		AdjustFacialExpression(giant, 1, 0.0f, CharEmotionType::Modifier);

		Task_ApplyAbsorbCooldown(giant);
		ShrinkPulse_GainSize(giant, tiny, true);

		Task_FacialEmotionTask_Moan(giant, 1.85f, "HugMoan", RandomFloat(0.0f, 0.45f));
		Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Absorption, CooldownSource::Emotion_Voice_Long);

		if (giant->IsPlayerRef()) {
			AdjustSizeReserve(giant, 0.0225f);
			AdjustMassLimit(0.0075f, giant);
		}
	}

	void OnCrushEnd(const ActionContext& a_Ctx) {
		SetSneaking(a_Ctx.Actor(), false, 0);
	}

	void OnSwitchToObjectA(const ActionContext& a_Ctx) {
		Attachment_SetTargetNode(a_Ctx.Actor(), AttachToNode::ObjectA);
	}

	// Restores the tiny and reports whether the heal should keep running.
	bool RestoreHealth(Actor* a_Giant, Actor* a_Tiny) {

		static Timer HeartTimer = Timer(0.5);

		const float hp = GetAV(a_Tiny, ActorValue::kHealth);
		const float maxhp = GetMaxAV(a_Tiny, ActorValue::kHealth);
		const bool healing = AnimationVars::Hug::IsHugHealing(a_Giant);

		if (healing && HeartTimer.ShouldRunFrame()) {
			SpawnHearts(a_Giant, a_Tiny, 0.0f, 2.4f, true);
		}

		if (!healing && hp >= maxhp) {

			// A follower healing another follower counts, which is why the giant is tested too.
			const bool friendly = a_Tiny->IsPlayerRef() || IsTeammate(a_Tiny) || (IsTeammate(a_Giant) && IsTeammate(a_Tiny));

			if (friendly && !Config::Gameplay.ActionSettings.bHugsStopAtFullHP) {
				return true;
			}

			if (a_Giant->IsPlayerRef()) {
				Notify("{} health is full", a_Tiny->GetDisplayFullName());
			}

			return false;
		}

		if (a_Giant->IsPlayerRef()) {
			shake_camera(a_Giant, 0.30f * (get_visual_scale(a_Giant) / get_visual_scale(a_Tiny)), 0.05f);
		}
		else {
			Rumbling::Once("HugSteal", a_Giant, Rumble_Hugs_Heal, 0.02f);
		}

		a_Tiny->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, maxhp * 0.004f * 0.15f * TimeScale());

		return healing;
	}

	void OnHeal(const ActionContext& a_Ctx) {
		if (Tiny(a_Ctx)) {
			HugNode::GetOrAdd(a_Ctx.Owner()).Healing = true;
		}
	}

	// The friendly release, played instead of shoving the tiny away.
	// The frame the tiny actually reaches the ground. Everything holding onto them ends here, not when
	// the giant finishes standing back up.
	void OnRelease(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		State& state = HugNode::GetOrAdd(a_Ctx.Owner());
		state.PutDown = true;

		if (auto* tiny = Tiny(a_Ctx); tiny && giant) {

			SetBeingHeld(tiny, false);
			EnableCollisions(tiny);
			Anims_FixAnimationDesync(giant, tiny, true);

			// Every stance lands the tiny on this annotation, crawl included. A hostile is thrown clear
			// here and a friendly is left to finish the release the graph is already playing for them.
			if (!state.Friendly && tiny->Is3DLoaded()) {
				PushForward(giant, tiny, 300.0f);
			}
		}

		a_Ctx.RequestExit();
	}

	void OnTinyRuneFX(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float scale = get_visual_scale(giant) * 0.33f;

		for (const std::string_view name : {NODE_HAND_L, NODE_HAND_R}) {
			if (auto* node = find_node(giant, name)) {
				SpawnParticle(giant, 6.00f, "GTS/gts_tinyrune_bind.nif", NiMatrix3(), node->world.translate, scale, 7, node);
			}
		}

		for (const std::string_view name : {"NPC L Foot [Lft ]"sv, "NPC R Foot [Rft ]"sv}) {
			if (auto* node = find_node(giant, name)) {
				SpawnParticle(giant, 6.00f, "GTS/gts_tinyrune_bind_leg.nif", NiMatrix3(), node->world.translate, scale, 7, node);
			}
		}
	}

	void OnRuneStart(const ActionContext& a_Ctx) {

		auto* tiny = Tiny(a_Ctx);
		if (!tiny) {
			return;
		}

		if (auto* object = find_node(tiny, "AnimObjectB")) {
			SpawnParticle(tiny, 3.00f, "GTS/gts_chugrune.nif", NiMatrix3(), object->world.translate, get_visual_scale(tiny), 7, object);
		}
	}

	void OnBoobCameraOn(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Breasts_02);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks - the hugged actor
	//----------------------------------------------------------------------------------------------

	void TinyEmotions(RE::Actor* a_Tiny, bool a_On) {

		if (!a_Tiny) {
			return;
		}

		const float blink = a_On ? 1.0f : 0.0f;
		const float mouth = a_On ? 0.75f : 0.0f;

		if (a_On && RandomInt(0, 8) <= 1) {
			Sound_PlayMoans(a_Tiny, 1.0f, 0.14f, EmotionTriggerSource::Vore);
		}

		AdjustFacialExpression(a_Tiny, 0, blink, CharEmotionType::Modifier);
		AdjustFacialExpression(a_Tiny, 1, blink, CharEmotionType::Modifier);
		AdjustFacialExpression(a_Tiny, 0, mouth, CharEmotionType::Phenome);
	}

	void OnTinyMoan(const ActionContext& a_Ctx) {
		TinyEmotions(a_Ctx.Partner(), true);
	}

	void OnTinyMoanEnd(const ActionContext& a_Ctx) {
		TinyEmotions(a_Ctx.Partner(), false);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	// GTS_HuggingTeammate and GTS_IsFollower are written by the DLL, so neither can be part of what
	// the DLL then verifies.
	constexpr GraphExpect Signature[] = {
		{ "GTS_Hugging", true },
		{ "GTS_Busy",    true },
	};

	// A hug cannot continue without whoever it is holding.
	constexpr PossessionSlot RequiredSlots[] = { PossessionSlot::kArms };

	// What the graph asserts while each action runs. The registry refuses a request while these hold,
	// which is what stops a held key restarting an animation that is already playing.
	constexpr GraphExpect AssertsHealing[]  = { { "GTS_IsHugHealing",   true } };
	constexpr GraphExpect AssertsCrushing[] = { { "IsHugCrushing",      true } };
	constexpr GraphExpect AssertsShrinking[]= { { "GTS_IsHugAbsorbing", true } };

	constexpr std::string_view ExitSignals[] = {
		"GTSBeh_HugCrushEnd",
		"GTS_Hug_Release",
	};

	bool CanStart(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		if (!giant) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kHugs)) {
			return false;
		}

		if (AnimationVars::General::IsGTSBusy(giant) || AnimationVars::Grab::HasGrabbedTiny(giant)) {
			return false;
		}

		if (!AnimationVars::General::CanDoPaired(giant) || AnimationVars::Other::IsSynced(giant)) {
			return false;
		}

		return true;
	}

	// The cooldown is deliberately not in the guard. A guard that answers false kills the keybind, so
	// the player would press hug during the cooldown and get nothing at all instead of being told.
	bool VerifyStart(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (giant && IsActionOnCooldown(giant, CooldownSource::Action_Hugs)) {
			HugAnimationController::Hugs_OnCooldownMessage(giant);
			return false;
		}

		return true;
	}

	// The tiny's half of the intro. Sneak has its own, everything else shares one.
	void OnEntryTaken(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return;
		}

		// Crawling and sneaking share the tiny's start event.
		const Stance stance = a_Ctx.Stance();
		const bool crawling = stance == Stance::kCrawl;
		const bool sneaking = stance != Stance::kStanding;

		if (sneaking && !crawling) {
			// Sneaking is turned off so the footstep sounds keep working.
			SetSneaking(giant, true, 0);
		}

		// The tiny picks its own stance set from these, so they come from the same two values that
		// chose the event.
		AnimationVars::Tiny::SetIsBeingCrawlHugged(tiny, crawling);
		AnimationVars::Tiny::SetIsBeingSneakHugged(tiny, sneaking && !crawling);

		ActionRegistry::Notify(tiny, sneaking ? BEH_START_TINY_S : BEH_START_TINY);
	}

	// Above this the giant is too big to hold someone gently. Hugs steal size, so a hug that started
	// inside the range can grow out of it.
	constexpr float GentleMax = 3.0f;

	[[nodiscard]] bool WithinGentleRange(Actor* a_Giant, Actor* a_Tiny) {
		const float difference = get_scale_difference(a_Giant, a_Tiny, SizeType::VisualScale, false, true);
		return difference < GentleMax && difference >= Action_Hug;
	}

	// Shrinking and healing both stop once there is nothing left to take.
	bool CanDrain(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return false;
		}

		return true;
	}

	bool VerifyDrain(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return false;
		}

		const bool drained = get_scale_difference(giant, tiny, SizeType::VisualScale, false, true) >= GetHugShrinkThreshold(giant)
			|| get_visual_scale(tiny) <= Minimum_Actor_Scale;

		if (!drained) {
			return true;
		}

		if (!AnimationVars::Hug::IsHugCrushing(giant) && !AnimationVars::Hug::IsHugHealing(giant)) {
			NotifyWithSound(giant, "All available size was drained");
			shake_camera(giant, 0.45f, 0.30f);
		}

		return false;
	}

	bool CanCrush(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return false;
		}

		return true;
	}

	bool VerifyCrush(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return false;
		}

		if (IsActionOnCooldown(giant, CooldownSource::Action_HugAbsorbOther)) {
			NotifyWithSound(giant, std::format("Hug Crush is on a cooldown: {:.1f} sec", GetRemainingCooldown(giant, CooldownSource::Action_HugAbsorbOther)));
			return false;
		}

		const float health = GetHealthPercentage(tiny);
		const float threshold = GetHugCrushThreshold(giant, tiny, true);

		if (health <= threshold || TinyCalamityActive(giant)) {
			return true;
		}

		// Mighty Cuddles forces it through at the cost of the giant's whole stamina bar.
		if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkHugMightyCuddles) && GetStaminaPercentage(giant) >= 0.75f) {
			return true;
		}

		NotifyWithSound(giant, std::format("{} is too healthy to be hug crushed", tiny->GetDisplayFullName()));
		shake_camera(giant, 0.45f, 0.30f);
		Notify("Health: {:.0f}%; Requirement: {:.0f}%", health * 100.0f, threshold * 100.0f);

		return false;
	}

	bool CanRelease(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant || !HugNode::Hugged(a_Ctx.Owner())) {
			return false;
		}

		return !AnimationVars::Hug::IsHugCrushing(giant) && !AnimationVars::Hug::IsHugHealing(giant);
	}

	void TakeShrink(const ActionContext& a_Ctx) {

		auto* tiny = Tiny(a_Ctx);
		if (tiny) {
			a_Ctx.SendTo(tiny, BEH_SHRINK_TINY);
			UpdateFriendlyHugs(a_Ctx.Actor(), tiny, true);
		}
	}

	void TakeCrush(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Tiny(a_Ctx);

		if (!tiny) {
			return;
		}

		a_Ctx.SendTo(tiny, BEH_CRUSH_TINY);
		HugNode::GetOrAdd(a_Ctx.Owner()).Crushing = true;

		// The forced crush is the only path that costs the whole bar.
		const bool forced = GetHealthPercentage(tiny) > GetHugCrushThreshold(giant, tiny, true) && !TinyCalamityActive(giant);

		if (forced) {
			DelayedHurtStamina(giant, GetAV(giant, ActorValue::kStamina) * 1.10f);
		}
		else if (TinyCalamityActive(giant)) {
			DelayedHurtStamina(giant, 60.0f);
		}
	}

	// Healing reaches a willing tiny and nobody else. A heal restores the tiny instead of taking from
	// them, so it is not gated on there being size left to drain.
	bool CanHeal(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return false;
		}

		if (!Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkHugsLovingEmbrace)) {
			return false;
		}

		return true;
	}

	bool VerifyHeal(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = HugNode::Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return false;
		}

		if (IsHostile(tiny, giant) || !(IsTeammate(tiny) || tiny->IsPlayerRef())) {
			Notify("You can not heal {}, they are fighting you", tiny->GetDisplayFullName());
			return false;
		}

		if (!WithinGentleRange(giant, tiny)) {
			Notify("It is difficult to gently hug {}", tiny->GetDisplayFullName());
			shake_camera(giant, 0.50f, 0.15f);
			return false;
		}

		return true;
	}

	void TakeHeal(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto* tiny = Tiny(a_Ctx);

		if (!tiny) {
			return;
		}

		UpdateFriendlyHugs(giant, tiny, false);
		a_Ctx.Send(BEH_HEAL_GIANT);
		a_Ctx.SendTo(tiny, IsFemale(tiny) ? BEH_HEAL_TINY_F : BEH_HEAL_TINY_M);

		HugNode::GetOrAdd(a_Ctx.Owner()).Healing = true;
	}

	// Which release the graph plays is picked from GTS_HuggingTeammate, not from the event, and the
	// shrink clears that variable. Restore the branch decided at entry before sending, or a follower
	// gets the rough release while the node still believes the hug was gentle and skips the shove.
	void SendRelease(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return;
		}

		// The tiny may already be gone - unloaded, disabled, or pushed out of the hug. The giant still
		// has to be told to leave, so its half goes out either way.
		if (auto* tiny = Tiny(a_Ctx)) {

			const HugNode::State* state = HugNode::Get(a_Ctx.Owner());
			UpdateFriendlyHugs(giant, tiny, !(state && state->Friendly));

			a_Ctx.SendTo(tiny, BEH_RELEASE);
		}

		a_Ctx.Send(BEH_RELEASE);
	}

	void TakeRelease(const ActionContext& a_Ctx) {
		SendRelease(a_Ctx);
	}

	constexpr EntryDef Entries[] = {
		{ .Action = "Hug.Enter", .Behaviour = BEH_START_GIANT, .Guard = CanStart, .Verify = VerifyStart, .BlockedBy = kBlockedByHand, .OnTaken = OnEntryTaken },
	};

	constexpr ActionDef Actions[] = {
		{ .Action = "Hug.Shrink",  .Behaviour = BEH_SHRINK_GIANT, .Guard = CanDrain,  .Verify = VerifyDrain, .OnTaken = TakeShrink, .Asserts = AssertsShrinking, .Cooldown = 0.20f, .Input = "Action.Hugs.Shrink" },
		{ .Action = "Hug.Heal",    .Behaviour = "",               .Guard = CanHeal,   .Verify = VerifyHeal,  .OnTaken = TakeHeal,   .Asserts = AssertsHealing,   .Cooldown = 0.20f, .Input = "Action.Hugs.Heal" },
		{ .Action = "Hug.Crush",   .Behaviour = BEH_CRUSH_GIANT,  .Guard = CanCrush,  .Verify = VerifyCrush, .OnTaken = TakeCrush,  .Asserts = AssertsCrushing,  .Cooldown = 0.35f, .Input = "Action.Hugs.Crush" },
		// Both releases send from OnTaken rather than through Behaviour, because the graph variable
		// that picks the friendly animation has to be right before the event goes out.
		{ .Action = "Hug.Release", .Behaviour = "",               .Guard = CanRelease, .OnTaken = TakeRelease, .Exits = true, .Cooldown = 0.20f, .Input = "Action.Hugs.Exit" },

		// Cancelled from outside, with no guard and no key: a hit that shakes the tiny loose, the AI
		// giving up. Aborts rather than Exits, so the registry sends AbortSignal and leaves instead of
		// waiting for an ending the animation is not going to reach.
		{ .Action = "Hug.Cancel",  .Behaviour = "",               .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_Hug_Catch",             .Handler = OnCatch },
		{ .Tag = "GTS_Hug_Grab",              .Handler = OnGrab },
		{ .Tag = "GTS_Hug_Grow",              .Handler = OnGrow },
		{ .Tag = "GTS_Hug_Moan",              .Handler = OnMoan },
		{ .Tag = "GTS_Hug_Moan_End",          .Handler = OnMoanEnd },
		{ .Tag = "GTS_Hug_PullBack",          .Handler = OnPullBack },
		{ .Tag = "GTS_Hug_FacialOn",          .Handler = OnFacialOn },
		{ .Tag = "GTS_Hug_FacialOff",         .Handler = OnFacialOff },
		{ .Tag = "GTS_Hug_ShrinkPulse",       .Handler = OnShrinkPulse },
		{ .Tag = "GTS_Hug_CrushTiny",         .Handler = OnCrushTiny, .ReleasesPartners = true },
		{ .Tag = "GTSBeh_HugCrushEnd",        .Handler = OnCrushEnd },
		{ .Tag = "GTS_Hug_SwitchToObjectA",   .Handler = OnSwitchToObjectA },
		{ .Tag = "GTS_Hug_Heal",              .Handler = OnHeal },
		{ .Tag = "GTS_Hug_Release",           .Handler = OnRelease,   .ReleasesPartners = true },
		{ .Tag = "GTS_CH_Tiny_FXStart",       .Handler = OnTinyRuneFX },
		{ .Tag = "GTS_CH_RuneStart",          .Handler = OnRuneStart },
		{ .Tag = "GTS_CH_BoobCameraOn",       .Handler = OnBoobCameraOn },

		// Fire and do nothing. Claimed so they are not reported as unclaimed.
		{ .Tag = "GTS_Hug_RunShrinkTask" },
		{ .Tag = "GTS_Hug_StopShrinkTask" },
		{ .Tag = "GTS_Hug_SwitchToDefault" },
		{ .Tag = "GTSBEH_HugAbsorbAtk" },
		{ .Tag = "GTS_CH_RuneEnd" },
		{ .Tag = "GTS_CH_BoobCameraOff" },

		{ .Tag = "GTSBEH_Next", .Shared = true },
		{ .Tag = "GTSBEH_Exit", .Shared = true },
	};

	// Fired by the hugged actor, not the giant. The legacy files handled these as if the tiny were
	// the giant, which is why they read data.giant and got the wrong actor's emotions.
	constexpr AnnotationDef PartnerAnnotations[] = {
		{ .Tag = "GTS_Hug_Moan_Tiny",     .Handler = OnTinyMoan },
		{ .Tag = "GTS_Hug_Moan_Tiny_End", .Handler = OnTinyMoanEnd },
	};

	constexpr std::string_view VariantNames[] = {
		"Standing",
		"Sneak",
		"Crawl",
	};

	//----------------------------------------------------------------------------------------------
	// Input
	//----------------------------------------------------------------------------------------------

	void StartEvent(const ManagedInputEvent&) {

		if (PlayerTarget::Engaged("Action.Hugs.Start")) {
			PlayerTarget::Perform("Hug.Enter");
			return;
		}

		Actor* player = GetPlayerOrControlled();
		if (!player) {
			return;
		}

		auto& hugs = HugAnimationController::GetSingleton();

		for (auto* prey : hugs.GetHugTargetsInFront(player, 1)) {
			HugAnimationController::StartHug(player, prey);
		}
	}

	bool CanStartEvent() {

		if (PlayerTarget::Engaged("Action.Hugs.Start")) {
			return PlayerTarget::CanPerform("Hug.Enter");
		}

		Actor* player = GetPlayerOrControlled();
		return player && ActionRegistry::CanPerform(player, "Hug.Enter");
	}
}

namespace GTS::Actions {

	std::span<const GraphExpect> HugNode::Signature() const { return ::Signature; }
	std::span<const std::string_view> HugNode::ExitSignals() const { return ::ExitSignals; }
	std::string_view HugNode::AbortSignal() const { return BEH_ABORT; }
	std::span<const PossessionSlot> HugNode::RequiredSlots() const { return ::RequiredSlots; }
	std::span<const EntryDef> HugNode::Entries() const { return ::Entries; }
	std::span<const ActionDef> HugNode::Actions() const { return ::Actions; }
	std::span<const AnnotationDef> HugNode::Annotations() const { return ::Annotations; }
	std::span<const AnnotationDef> HugNode::PartnerAnnotations() const { return ::PartnerAnnotations; }

	HugNode::State* HugNode::Get(RE::FormID a_Owner) {
		return m_State.Find(a_Owner);
	}

	HugNode::State& HugNode::GetOrAdd(RE::FormID a_Owner) {
		return m_State.GetOrAdd(a_Owner);
	}

	bool HugNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view, bool a_Explain) const {

		if (!CanDoActionBasedOnQuestProgress(a_Target, QuestAnimationType::kHugs)) {
			return false;
		}

		auto& hugs = HugAnimationController::GetSingleton();

		hugs.AllowMessage(a_Explain);
		const bool started = HugAnimationController::StartHug(a_Actor, a_Target);
		hugs.AllowMessage(false);

		return started;
	}

	RE::Actor* HugNode::Hugged(RE::FormID a_Owner) {
		return Possession::FirstActor(a_Owner, PossessionSlot::kArms);
	}

	void HugNode::RegisterInput() {
		Keybinds::NoteBindKind("Action.Hugs.Start", true);
		InputManager::RegisterInputEvent("Action.Hugs.Start", StartEvent, CanStartEvent);
	}

	// The controller binds the partner immediately before it asks, so a real request always has one.
	bool HugNode::CanEnter(const EntryContext& a_Ctx) const {
		return a_Ctx.Deferred(a_Ctx.Occupied(PossessionSlot::kArms));
	}

	void HugNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		auto* giant = a_Ctx.Actor();
		auto* tiny = Hugged(a_Ctx.Owner());

		if (!giant) {
			return;
		}

		switch (a_Ctx.Stance()) {
			case Stance::kCrawl: { state.Variant = Variant::kCrawl; break; }
			case Stance::kSneak: { state.Variant = Variant::kSneak; break; }
			default:             { state.Variant = Variant::kStanding; break; }
		}

		// Decided once. Read per frame, a follower turns into a victim halfway through the hug the
		// moment their standing shifts.
		state.Friendly = AnimationVars::Hug::IsHuggingTeammate(giant);

		// No fear here. A hug is the one action that does not frighten the actor it takes hold of, and
		// a follower being hugged affectionately least of all.
		if (tiny) {
			VoreController::RecordOriginalScale(tiny);
		}
	}

	// The per frame half of the hug: keep the pair facing and attached, drain, blend, and give up when
	// the hug is no longer viable. The legacy version was a task that ran alongside the animation and
	// had to re-check whether the animation was still playing; here the node is only ticked while it
	// is, so those checks are gone.
	void HugNode::OnUpdate(const ActionContext& a_Ctx, float a_Delta) {

		auto* giant = a_Ctx.Actor();
		State& state = m_State.GetOrAdd(a_Ctx.Owner());

		// Past the put-down or the crush there is nothing left to hold, and the giant plays out the
		// rest of the animation on its own. Dragging a released follower along for those seconds is
		// what walked them back to the player, and reaching for a crushed tiny whose 3D the game has
		// already unloaded made every frame fail its attach and abort the hug.
		if (state.PutDown || state.TinyCrushed) {
			return;
		}

		// The arms slot is a RequiredSlot, so the registry has already left if the tiny is gone.
		auto* tiny = Hugged(a_Ctx.Owner());

		if (!giant || !tiny) {
			return;
		}

		ApplyActionCooldown(giant, CooldownSource::Action_Hugs);

		ShutUp(tiny);
		ShutUp(giant);

		if (!FaceOpposite(giant, tiny)) {
			a_Ctx.Abort();
			return;
		}

		const float difference = get_scale_difference(giant, tiny, SizeType::VisualScale, false, true);

		float drain = 3.4f;
		if (state.Friendly) {
			drain *= 1.5f;
		}

		GrabStaminaDrain(giant, tiny, difference * 2 * drain);
		DamageAV(tiny, ActorValue::kStamina, 0.125f * TimeScale());
		ModSizeExperience(giant, 0.00005f);

		Utils_UpdateHugBehaviors(giant, tiny);
		Anims_FixAnimationDesync(giant, tiny, false);

		ForceRagdoll(tiny, false);

		auto giantHandle = giant->CreateRefHandle();
		auto tinyHandle = tiny->CreateRefHandle();

		if (state.Healing) {

			// The graph owns whether a heal is running. Latching it here made every later action look
			// like a heal that had just ended, which is what fired the warning on a plain shrink.
			if (!AnimationVars::Hug::IsHugHealing(giant)) {
				state.Healing = false;
			}
			else {

				HugAttach(giantHandle, tinyHandle);

				float cost = Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkHugsGreed) ? 0.75f : 1.0f;
				cost *= 0.35f * Perk_GetCostReduction(giant);

				// Hugs steal size, so a heal that started in range can grow out of it. That ends the
				// heal and nothing else: the hug itself is still valid.
				if (!WithinGentleRange(giant, tiny)) {

					if (giant->IsPlayerRef()) {
						shake_camera(giant, 0.50f, 0.15f);
						Notify("It is difficult to gently hug {}", tiny->GetDisplayFullName());
					}

					state.Healing = false;
					return;
				}

				DamageAV(tiny, ActorValue::kStamina, -(0.45f * TimeScale()));
				DamageAV(giant, ActorValue::kStamina, 0.25f * cost * TimeScale());

				RestoreHealth(giant, tiny);
				return;
			}
		}

		const bool crushing = state.Crushing || AnimationVars::Hug::IsHugCrushing(giant);
		const bool lost = giant->IsDead() || tiny->IsDead() || IsEscapingInteraction(tiny);

		if (!crushing) {

			// Dead or struggling free. There is no release animation to play over that, so it ends here
			// and the registry sends the abort signal.
			if (lost) {
				a_Ctx.Abort();
				return;
			}

			if (difference < Action_Hug || GetAV(giant, ActorValue::kStamina) <= 2.0f) {

				// The hug is finished, but it ends through its own release rather than being cut off,
				// for a hostile as much as a follower. GTS_Hug_Release is what lands the tiny, and
				// OnRelease decides there whether they are set down or thrown clear. The behaviour goes
				// out once: the animation takes several seconds and re-sending it restarts it.
				if (!state.Released) {
					state.Released = true;
					SendRelease(a_Ctx);
				}

				HugAttach(giantHandle, tinyHandle);
				return;
			}
		}
		else if (lost && !AnimationVars::Hug::IsHasAbsorbedTiny(giant)) {
			a_Ctx.Abort();
			return;
		}

		// Crawl animations are configured for ObjectA, everything else uses the hug attach points.
		if (state.Variant == Variant::kCrawl) {
			AttachToObjectA(giant, tiny);
			return;
		}

		if (!HugAttach(giantHandle, tinyHandle)) {
			a_Ctx.Abort();
		}
	}

	// Cleanup::ReleaseAll handles the tiny's flags, collision and fear. What is left here is the pair
	// specific half: the graph variables the DLL wrote, the shove, and the tasks.
	void HugNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();
		auto* tiny = Hugged(owner);

		const State* state = m_State.Find(owner);
		const bool friendly = state && state->Friendly;

		TaskManager::Cancel(std::format(TASKID_FMT_STAMINA, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_GROWTH, owner));

		TaskManager::Cancel(std::format(TASKID_FMT_STEAL, owner));

		if (giant) {

			SetSneaking(giant, false, 0);

			AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Phenome);
			AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Modifier);
			AdjustFacialExpression(giant, 1, 0.0f, CharEmotionType::Modifier);
			AdjustFacialExpression(giant, 2, 0.0f, CharEmotionType::Expression);

			Attachment_SetTargetNode(giant, AttachToNode::None);
		}

		if (tiny && giant) {

			// Both halves of the pair carry these and the DLL is what set them.
			UpdateFriendlyHugs(giant, tiny, true);
			Anims_FixAnimationDesync(giant, tiny, true);

			// Only when the animation never reached its put-down. The normal path shoves there.
			if (!friendly && !(state && state->PutDown) && tiny->Is3DLoaded()) {
				PushForward(giant, tiny, 300.0f);
			}

			TinyEmotions(tiny, false);
		}

		if (a_Reason != ExitReason::kCompleted) {
			logger::debug("Hug: {:08X} left on {}", owner, ExitReasonName(a_Reason));
		}

		m_State.Forget(owner);
	}

	void HugNode::Cancel(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		auto* tiny = Hugged(a_Giant->formID);

		if (!tiny) {
			return;
		}

		if (a_Giant->IsPlayerRef()) {
			shake_camera(a_Giant, 0.25f * (get_visual_scale(a_Giant) / get_visual_scale(tiny)), 0.35f);
		}
		else {
			Rumbling::Once("HugRelease", a_Giant, Rumble_Hugs_Release, 0.10f, true);
		}

		Notify("{} was saved from hugs of {}", tiny->GetDisplayFullName(), a_Giant->GetDisplayFullName());
		ActionRegistry::Perform(a_Giant, "Hug.Cancel");
	}

	void HugNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void HugNode::OnReset() {
		m_State.Clear();
	}

	std::string_view HugNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);
		if (!state) {
			return "";
		}

		if (state->Crushing) {
			return "Crushing";
		}

		if (state->Healing) {
			return "Healing";
		}

		return state->Friendly ? "Friendly" : VariantNames[std::to_underlying(state->Variant)];
	}
}
