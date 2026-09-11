#include "Actions/Nodes/Kick/KickSwipeNode.hpp"
#include "Actions/Core/PlayerTarget.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Damage/LaunchObject.hpp"
#include "Managers/Perks/PerkHandler.hpp"

#include "Utils/Actions/AutoAim/AutoAimUtils.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  The kick and the hand swipe are the same two behaviours. GTSBeh_SwipeLight_L / R and
  GTSBeh_SwipeHeavy_L / R are what both the kick keys and the swipe keys send; the graph swings a leg
  when the giant is standing and an arm when she is sneaking or crawling. Only the low sweep has a
  behaviour of its own.

  They are one node because the two halves cannot be told apart before the send, and because
  splitting them would mean two nodes fighting over the same trigger. Which one is playing shows in
  the variables afterwards: GTS_IsKicking for the leg, GTS_IsHandAttacking for the arm.

  Captured standing and crawling. Times are from one run and are illustrative only. The light
  standing kick and the sneak swipes were not reached and are marked below.

  [KICK, STANDING ("GTSBeh_SwipeLight_L" / "R", keybind "Action.Kick.Light"; heavy on "Action.Kick.HeavyHigh")]
    GTS_Kick_Camera_On_L / R          +0.40 -> camera on the foot, speed set from the kick perk
    GTS_Kick_SwingLeg_L / R           +0.82 [NOT HANDLED]
    GTS_Kick_HitBox_Power_On_L / R    +0.82 -> the damage loop starts, and runs until the Off tag
    GTS_Kick_HitBox_Power_Off_L / R   +1.18
    GTS_Kick_Camera_Off_L / R         +3.09
    SwingLeg and the hit box On arrive on the same frame and either can be first.
    Only the heavy was captured. The light kick sends the same tags without _Power_.

  [LOW SWEEP ("GTSBEH_HeavyKickLow_L" / "R", keybind "Action.Kick.HeavyLow")]
    Same tags as the heavy kick, same times. Only the behaviour differs.

  [SWIPE, SNEAKING (same two behaviours, while sneaking)]
    GTS_Sneak_Swipe_On_L / R        -> arm and forearm damage loop
    GTS_Sneak_Swipe_Off_L / R
    GTS_Sneak_Swipe_Power_On_L / R  -> the heavy pair
    GTS_Sneak_Swipe_Power_Off_L / R
    GTS_Sneak_Swipe_ArmSfx_Start / End [NOT HANDLED]
    Not reached. Order is from the registrations.

  [SWIPE, CRAWLING (same again, while crawling)]
    GTS_Crawl_Swipe_On_L / R        +0.74
    GTS_Crawl_Swipe_Off_L / R       +1.21
    GTS_Crawl_Swipe_Power_On_L / R  +1.53 -> the heavy pair
    GTS_Crawl_Swipe_Power_Off_L / R +2.07
    GTS_Crawl_Swipe_ArmSfx_Start / End [NOT HANDLED, and neither fired]

  **Info: the standing kick sends no GTSBEH_Exit and the crawl swipe sends nothing after its Off
  **tag. Both leave on the signature draining, about 0.25s later.

  **Info: kUnannounced. The camera off tag ends a kick but the swipes have no equivalent, so the
  **signature draining is the exit for all of them.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH, STANDING]
     GTS_IsKicking       -> TRUE   (graph owned, and this node's signature)
   [SET BY THE GRAPH, SNEAKING OR CRAWLING]
     GTS_IsHandAttacking -> TRUE
     GTS_CanCombo        -> TRUE, written by the same modifier
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: the signature is the kicking variable, and Alive() covers the swipe one. A node has one
   **signature and these are two different variables for one animation state, so the swipe half is
   **answered for by the node rather than by the sweep.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Variant = KickSwipeNode::Variant;
	using State = KickSwipeNode::State;

	constexpr std::string_view TASKID_FMT_LEG = "LegKick_{}";
	constexpr std::string_view TASKID_FMT_SWIPE_L = "SwipeCollide_L_{}";
	constexpr std::string_view TASKID_FMT_SWIPE_R = "SwipeCollide_R_{}";
	constexpr std::string_view TASKID_FMT_HAND_L = "HandCollide_L_{}";
	constexpr std::string_view TASKID_FMT_HAND_R = "HandCollide_R_{}";

	//----------------------------------------------------------------------------------------------
	// Animation speed
	//----------------------------------------------------------------------------------------------

	// The actor level half of PerkHandler::KickPerk_ChangeAnimSpeed, which takes the old
	// AnimationEventData a node does not have.
	void KickSpeed(const ActionContext& a_Ctx, bool a_Reset) {

		if (a_Reset) {
			a_Ctx.SetCanEditAnimSpeed(false);
			a_Ctx.SetAnimSpeed(1.0f);
			return;
		}

		float speed = 1.0f;
		PerkHandler::KickPerk_ApplyKickSpeed(a_Ctx.Actor(), speed);

		a_Ctx.SetCanEditAnimSpeed(true);
		a_Ctx.SetAnimSpeed(speed);
	}

	//----------------------------------------------------------------------------------------------
	// The leg
	//----------------------------------------------------------------------------------------------

	void StartLegDamage(Actor* a_Giant, float a_Power, float a_Crush, float a_Push, bool a_Right, std::string_view a_Node, DamageSource a_Source) {

		ActorHandle handle = a_Giant->CreateRefHandle();
		std::vector<ObjectRefHandle> objects = GetNearbyObjects(a_Giant);

		TaskManager::Run(std::format(TASKID_FMT_LEG, a_Giant->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			auto* leg = find_node(giant, a_Node);

			if (leg) {

				const auto coords = GetFootCoordinates(giant, a_Right, false);

				if (!coords.empty()) {
					DoDamageAtPoint_Cooldown(giant, Radius_Kick, a_Power, leg, coords[1], 10, 0.30f, a_Crush, a_Push, a_Source);
					PushObjects(objects, giant, leg, a_Push, Radius_Kick, true);
				}
			}

			return true;
		});
	}

	void StopLegDamage(Actor* a_Giant) {
		DrainStamina(a_Giant, "StaminaDrain_StrongKick", Runtime::PERK.GTSPerkDestructionBasics, false, 8.0f);
		DrainStamina(a_Giant, "StaminaDrain_Kick", Runtime::PERK.GTSPerkDestructionBasics, false, 4.0f);
		TaskManager::Cancel(std::format(TASKID_FMT_LEG, a_Giant->formID));
	}

	//----------------------------------------------------------------------------------------------
	// The arm
	//----------------------------------------------------------------------------------------------

	// The sneak and crawl swipes hit the same two bones with the same call; only the task name, the
	// numbers and whether the heel blend is touched differ.
	void StartArmDamage(Actor* a_Giant, bool a_Right, float a_Power, float a_Crush, float a_Push, std::string_view a_Task, bool a_UpdateHeels) {

		ActorHandle handle = a_Giant->CreateRefHandle();
		std::vector<ObjectRefHandle> objects = GetNearbyObjects(a_Giant);

		const std::string_view forearm = a_Right ? "NPC R Forearm [RLar]" : "NPC L Forearm [LLar]";
		const std::string_view hand = a_Right ? "NPC R Hand [RHnd]" : "NPC L Hand [LHnd]";
		const DamageSource source = a_Right ? DamageSource::HandSwipeRight : DamageSource::HandSwipeLeft;

		TaskManager::Run(std::format("{}{}", a_Task, a_Giant->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			for (const std::string_view bone : { forearm, hand }) {

				auto* node = find_node(giant, bone);

				if (!node) {
					continue;
				}

				DoDamageAtPoint_Cooldown(giant, Radius_Sneak_HandSwipe, a_Power, node, NiPoint3(0, 0, 0), 10, 0.30f, a_Crush, a_Push, source);
				PushObjects(objects, giant, node, a_Push, Radius_Sneak_HandSwipe, false);
			}

			if (a_UpdateHeels) {
				Utils_UpdateHighHeelBlend(giant, false);
			}

			return true;
		});
	}

	void StopArmDamage(Actor* a_Giant) {
		TaskManager::Cancel(std::format(TASKID_FMT_SWIPE_L, a_Giant->formID));
		TaskManager::Cancel(std::format(TASKID_FMT_SWIPE_R, a_Giant->formID));
		TaskManager::Cancel(std::format(TASKID_FMT_HAND_L, a_Giant->formID));
		TaskManager::Cancel(std::format(TASKID_FMT_HAND_R, a_Giant->formID));
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: the leg
	//----------------------------------------------------------------------------------------------

	void OnKickCamOnR(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::R_Foot);
		KickSpeed(a_Ctx, false);
	}

	void OnKickCamOnL(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::L_Foot);
		KickSpeed(a_Ctx, false);
	}

	void OnKickCamOffR(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::R_Foot);
		KickSpeed(a_Ctx, true);
		a_Ctx.RequestExit();
	}

	void OnKickCamOffL(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::L_Foot);
		KickSpeed(a_Ctx, true);
		a_Ctx.RequestExit();
	}

	void OnKickHitR(const ActionContext& a_Ctx) {
		StartLegDamage(a_Ctx.Actor(), Damage_Kick, 1.8f, Push_Kick_Normal, true, "NPC R Toe0 [RToe]", DamageSource::KickedRight);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_StrongKick", Runtime::PERK.GTSPerkDestructionBasics, true, 4.0f);
	}

	void OnKickHitL(const ActionContext& a_Ctx) {
		StartLegDamage(a_Ctx.Actor(), Damage_Kick, 1.8f, Push_Kick_Normal, false, "NPC L Toe0 [LToe]", DamageSource::KickedLeft);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_StrongKick", Runtime::PERK.GTSPerkDestructionBasics, true, 4.0f);
	}

	void OnKickPowerHitR(const ActionContext& a_Ctx) {
		StartLegDamage(a_Ctx.Actor(), Damage_Kick_Strong, 1.8f, Push_Kick_Strong, true, "NPC R Toe0 [RToe]", DamageSource::KickedRight);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_StrongKick", Runtime::PERK.GTSPerkDestructionBasics, true, 8.0f);
	}

	void OnKickPowerHitL(const ActionContext& a_Ctx) {
		StartLegDamage(a_Ctx.Actor(), Damage_Kick_Strong, 1.8f, Push_Kick_Strong, false, "NPC L Toe0 [LToe]", DamageSource::KickedLeft);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_StrongKick", Runtime::PERK.GTSPerkDestructionBasics, true, 8.0f);
	}

	void OnKickHitOff(const ActionContext& a_Ctx) {
		StopLegDamage(a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: the arm, sneaking
	//----------------------------------------------------------------------------------------------

	void OnSneakSwipeOn(const ActionContext& a_Ctx, bool a_Right) {
		StartArmDamage(a_Ctx.Actor(), a_Right, Damage_Sneak_HandSwipe, 1.8f, Push_Sneak_HandSwipe, a_Right ? "SwipeCollide_R_" : "SwipeCollide_L_", true);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipe", Runtime::PERK.GTSPerkDestructionBasics, true, 4.0f);
		KickSpeed(a_Ctx, false);
	}

	void OnSneakSwipeOnR(const ActionContext& a_Ctx) { OnSneakSwipeOn(a_Ctx, true); }
	void OnSneakSwipeOnL(const ActionContext& a_Ctx) { OnSneakSwipeOn(a_Ctx, false); }

	void OnSneakSwipeOff(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipe", Runtime::PERK.GTSPerkDestructionBasics, false, 4.0f);
		StopArmDamage(a_Ctx.Actor());
		KickSpeed(a_Ctx, true);
	}

	void OnSneakSwipePowerOn(const ActionContext& a_Ctx, bool a_Right) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipeStrong", Runtime::PERK.GTSPerkDestructionBasics, true, 10.0f);
		StartArmDamage(a_Ctx.Actor(), a_Right, Damage_Sneak_HandSwipe_Strong, 1.4f, Push_Sneak_HandSwipe_Strong, a_Right ? "SwipeCollide_R_" : "SwipeCollide_L_", true);
		KickSpeed(a_Ctx, false);
	}

	void OnSneakSwipePowerOnR(const ActionContext& a_Ctx) { OnSneakSwipePowerOn(a_Ctx, true); }
	void OnSneakSwipePowerOnL(const ActionContext& a_Ctx) { OnSneakSwipePowerOn(a_Ctx, false); }

	void OnSneakSwipePowerOff(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipeStrong", Runtime::PERK.GTSPerkDestructionBasics, false, 10.0f);
		StopArmDamage(a_Ctx.Actor());
		KickSpeed(a_Ctx, true);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: the arm, crawling
	//----------------------------------------------------------------------------------------------

	void OnCrawlSwipeOn(const ActionContext& a_Ctx, bool a_Right) {
		StartArmDamage(a_Ctx.Actor(), a_Right, Damage_Crawl_HandSwipe, 1.6f, Push_Crawl_HandSwipe, a_Right ? "HandCollide_R_" : "HandCollide_L_", false);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipe", Runtime::PERK.GTSPerkDestructionBasics, true, 4.0f);
		KickSpeed(a_Ctx, false);
	}

	void OnCrawlSwipeOnR(const ActionContext& a_Ctx) { OnCrawlSwipeOn(a_Ctx, true); }
	void OnCrawlSwipeOnL(const ActionContext& a_Ctx) { OnCrawlSwipeOn(a_Ctx, false); }

	void OnCrawlSwipeOff(const ActionContext& a_Ctx) {
		StopArmDamage(a_Ctx.Actor());
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipe", Runtime::PERK.GTSPerkDestructionBasics, false, 4.0f);
		KickSpeed(a_Ctx, true);
	}

	void OnCrawlSwipePowerOn(const ActionContext& a_Ctx, bool a_Right) {
		StartArmDamage(a_Ctx.Actor(), a_Right, Damage_Crawl_HandSwipe * 2.0f, 1.3f, Push_Crawl_HandSwipe_Strong, a_Right ? "HandCollide_R_" : "HandCollide_L_", false);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipeStrong", Runtime::PERK.GTSPerkDestructionBasics, true, 10.0f);
		KickSpeed(a_Ctx, false);
	}

	void OnCrawlSwipePowerOnR(const ActionContext& a_Ctx) { OnCrawlSwipePowerOn(a_Ctx, true); }
	void OnCrawlSwipePowerOnL(const ActionContext& a_Ctx) { OnCrawlSwipePowerOn(a_Ctx, false); }

	void OnCrawlSwipePowerOff(const ActionContext& a_Ctx) {
		StopArmDamage(a_Ctx.Actor());
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_CrawlSwipeStrong", Runtime::PERK.GTSPerkDestructionBasics, false, 10.0f);
		KickSpeed(a_Ctx, true);
	}

	//----------------------------------------------------------------------------------------------
	// Guards and resolvers
	//----------------------------------------------------------------------------------------------

	bool CanKick(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant || !CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kStompsAndKicks) || AnimationVars::General::IsGTSBusy(giant)) {
			return false;
		}

		return !giant->IsSneaking() && !giant->AsActorState()->IsSprinting();
	}

	bool CanSwipe(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant || !CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kStompsAndKicks) || AnimationVars::General::IsGTSBusy(giant)) {
			return false;
		}

		return giant->IsSneaking();
	}

	// Kicks and swipes share their behaviours, so all four resolvers only differ in which aim they ask
	// for and what the tired message says.
	std::string_view PickSide(const EntryContext& a_Ctx, StompAimType a_Type, bool a_Strong, float a_Cost, std::string_view a_Message, std::string_view a_Left, std::string_view a_Right) {

		auto* giant = a_Ctx.Actor();

		if (GetAV(giant, ActorValue::kStamina) <= a_Cost * GetWasteMult(giant)) {
			NotifyWithSound(giant, a_Message);
			return {};
		}

		bool left = AutoAim_Miss_GetNextStompSide(giant, a_Type);
		left = AutoAim_Kick_DeterminePreferredKick(giant, left, a_Strong);

		return left ? a_Left : a_Right;
	}

	std::string_view ResolveKickLight(const EntryContext& a_Ctx) {
		return PickSide(a_Ctx, StompAimType::T1, false, 35.0f, "You're too tired for light kick", "GTSBeh_SwipeLight_L", "GTSBeh_SwipeLight_R");
	}

	std::string_view ResolveKickHeavy(const EntryContext& a_Ctx) {
		return PickSide(a_Ctx, StompAimType::T2, true, 110.0f, "You're too tired for strong kick", "GTSBeh_SwipeHeavy_L", "GTSBeh_SwipeHeavy_R");
	}

	std::string_view ResolveKickLow(const EntryContext& a_Ctx) {
		return PickSide(a_Ctx, StompAimType::T3, true, 110.0f, "You're too tired for strong kick", "GTSBEH_HeavyKickLow_L", "GTSBEH_HeavyKickLow_R");
	}

	std::string_view ResolveSwipeLight(const EntryContext& a_Ctx) {

		Utils_UpdateHighHeelBlend(a_Ctx.Actor(), false);
		return PickSide(a_Ctx, StompAimType::T4, false, 25.0f, "You're too tired for hand swipe", "GTSBeh_SwipeLight_L", "GTSBeh_SwipeLight_R");
	}

	std::string_view ResolveSwipeHeavy(const EntryContext& a_Ctx) {

		Utils_UpdateHighHeelBlend(a_Ctx.Actor(), false);
		return PickSide(a_Ctx, StompAimType::T4, true, 70.0f, "You're too tired for hand swipe", "GTSBeh_SwipeHeavy_L", "GTSBeh_SwipeHeavy_R");
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsKicking", true },
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "Kick.Light",   .Guard = CanKick,  .Input = "Action.Kick.Light",       .Resolve = ResolveKickLight },
		{ .Action = "Kick.Heavy",   .Guard = CanKick,  .Input = "Action.Kick.HeavyHigh",   .Resolve = ResolveKickHeavy },
		{ .Action = "Kick.LowSweep",.Guard = CanKick,  .Input = "Action.Kick.HeavyLow",    .Resolve = ResolveKickLow },

		{ .Action = "Swipe.Light",  .Guard = CanSwipe, .Input = "Action.Swipe.Light",      .Resolve = ResolveSwipeLight },
		{ .Action = "Swipe.Heavy",  .Guard = CanSwipe, .Input = "Action.Swipe.Heavy",      .Resolve = ResolveSwipeHeavy },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_Kick_Camera_On_R",       .Handler = OnKickCamOnR },
		{ .Tag = "GTS_Kick_Camera_On_L",       .Handler = OnKickCamOnL },
		{ .Tag = "GTS_Kick_Camera_Off_R",      .Handler = OnKickCamOffR },
		{ .Tag = "GTS_Kick_Camera_Off_L",      .Handler = OnKickCamOffL },
		{ .Tag = "GTS_Kick_SwingLeg_R" },
		{ .Tag = "GTS_Kick_SwingLeg_L" },
		{ .Tag = "GTS_Kick_HitBox_On_R",       .Handler = OnKickHitR },
		{ .Tag = "GTS_Kick_HitBox_On_L",       .Handler = OnKickHitL },
		{ .Tag = "GTS_Kick_HitBox_Off_R",      .Handler = OnKickHitOff },
		{ .Tag = "GTS_Kick_HitBox_Off_L",      .Handler = OnKickHitOff },
		{ .Tag = "GTS_Kick_HitBox_Power_On_R", .Handler = OnKickPowerHitR },
		{ .Tag = "GTS_Kick_HitBox_Power_On_L", .Handler = OnKickPowerHitL },
		{ .Tag = "GTS_Kick_HitBox_Power_Off_R",.Handler = OnKickHitOff },
		{ .Tag = "GTS_Kick_HitBox_Power_Off_L",.Handler = OnKickHitOff },

		{ .Tag = "GTS_Sneak_Swipe_On_R",        .Handler = OnSneakSwipeOnR },
		{ .Tag = "GTS_Sneak_Swipe_On_L",        .Handler = OnSneakSwipeOnL },
		{ .Tag = "GTS_Sneak_Swipe_Off_R",       .Handler = OnSneakSwipeOff },
		{ .Tag = "GTS_Sneak_Swipe_Off_L",       .Handler = OnSneakSwipeOff },
		{ .Tag = "GTS_Sneak_Swipe_Power_On_R",  .Handler = OnSneakSwipePowerOnR },
		{ .Tag = "GTS_Sneak_Swipe_Power_On_L",  .Handler = OnSneakSwipePowerOnL },
		{ .Tag = "GTS_Sneak_Swipe_Power_Off_R", .Handler = OnSneakSwipePowerOff },
		{ .Tag = "GTS_Sneak_Swipe_Power_Off_L", .Handler = OnSneakSwipePowerOff },
		{ .Tag = "GTS_Sneak_Swipe_ArmSfx_Start" },
		{ .Tag = "GTS_Sneak_Swipe_ArmSfx_End" },

		{ .Tag = "GTS_Crawl_Swipe_On_R",        .Handler = OnCrawlSwipeOnR },
		{ .Tag = "GTS_Crawl_Swipe_On_L",        .Handler = OnCrawlSwipeOnL },
		{ .Tag = "GTS_Crawl_Swipe_Off_R",       .Handler = OnCrawlSwipeOff },
		{ .Tag = "GTS_Crawl_Swipe_Off_L",       .Handler = OnCrawlSwipeOff },
		{ .Tag = "GTS_Crawl_Swipe_Power_On_R",  .Handler = OnCrawlSwipePowerOnR },
		{ .Tag = "GTS_Crawl_Swipe_Power_On_L",  .Handler = OnCrawlSwipePowerOnL },
		{ .Tag = "GTS_Crawl_Swipe_Power_Off_R", .Handler = OnCrawlSwipePowerOff },
		{ .Tag = "GTS_Crawl_Swipe_Power_Off_L", .Handler = OnCrawlSwipePowerOff },
		{ .Tag = "GTS_Crawl_Swipe_ArmSfx_Start" },
		{ .Tag = "GTS_Crawl_Swipe_ArmSfx_End" },
	};

	constexpr std::string_view StateNames[] = {
		"kick",
		"sneak swipe",
		"crawl swipe",
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> KickSwipeNode::Signature() const     { return ::Signature; }
	ExitPolicy KickSwipeNode::Exit() const                            { return ExitPolicy::kUnannounced; }
	bool KickSwipeNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool) const {
		return PlayerTarget::StartAimedOn(a_Actor, a_Target, a_Action);
	}

	std::span<const EntryDef> KickSwipeNode::Entries() const          { return ::Entries; }
	std::span<const AnnotationDef> KickSwipeNode::Annotations() const { return ::Annotations; }

	// The signature is the kick variable. A swipe is the same node playing the other half of the same
	// trigger, and it asserts a different variable, so the node answers for that one itself.
	Liveness KickSwipeNode::Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const {

		if (a_Actor && AnimationVars::Crawl::IsHandAttacking(a_Actor)) {
			return Liveness::kAlive;
		}

		return Liveness::kUnknown;
	}

	void KickSwipeNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		switch (a_Ctx.Stance()) {
			case Stance::kCrawl: { state.Variant = Variant::kCrawlSwipe; break; }
			case Stance::kSneak: { state.Variant = Variant::kSneakSwipe; break; }
			default:             { state.Variant = Variant::kKick; break; }
		}
	}

	void KickSwipeNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		auto* giant = a_Ctx.Actor();

		if (giant) {

			// Stopped here, not on the matching Off annotation: an interrupted kick or swipe never sends
			// one and its damage keeps running.
			StopLegDamage(giant);
			StopArmDamage(giant);

			DrainStamina(giant, "StaminaDrain_CrawlSwipe", Runtime::PERK.GTSPerkDestructionBasics, false, 4.0f);
			DrainStamina(giant, "StaminaDrain_CrawlSwipeStrong", Runtime::PERK.GTSPerkDestructionBasics, false, 10.0f);

			ManageCamera(giant, false, CameraTracking::R_Foot);
			ManageCamera(giant, false, CameraTracking::L_Foot);
		}

		m_State.Forget(a_Ctx.Owner());
	}

	void KickSwipeNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void KickSwipeNode::OnReset() {
		m_State.Clear();
	}

	std::string_view KickSwipeNode::StateName(RE::FormID a_Owner) const {
		const State* state = m_State.Find(a_Owner);
		return state ? StateNames[std::to_underlying(state->Variant)] : std::string_view{};
	}
}
