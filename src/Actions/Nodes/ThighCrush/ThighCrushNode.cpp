#include "Actions/Nodes/ThighCrush/ThighCrushNode.hpp"
#include "Actions/Core/PlayerTarget.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Controllers/ThighCrushController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Audio/Footstep.hpp"
#include "Managers/AI/AIFunctions.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"

/* 
  ------------- ANNOTATION ORDER
 [INTRO] ("GTSBeh_TriggerSitdown")
   GTStosit
   GTSsitloopenter
   GTSBEH_Next
   GTSsitloopstart
   GTSsitloopend
 [IDLE LOOP]
   NOTHING
 [ATTACK] ("GTSBeh_StartThighCrush")
   GTSsitcrushlight_start
   GTSsitcrushlight_end
   GTSsitcrushheavy_start
   GTSsitcrushheavy_end
   GTSBEH_Next
 [IDLE LOOP]
   NOTHING
 [EXIT] ("GTSBeh_LeaveSitdown")
   GTSsitloopexit
   GTSstandL
   GTSstandR
   GTSstandR
   GTStoexit
   GTSBEH_Exit 
 [CLEANUP]
   GTS_ResetVars
   GTSBEH_Camera_Reset
   GTSBeh_Dummy

  **Info: When something happens and the actor resets either via IdleForceDefaultState or another form of reset,
  The behavior tree will immediatly skip to GTSBEH_Exit.
 */

/*
 ------------- ANIMATION VARS
 [GET IMMEDIATLY SET AT ANIMATION START TRIGGER ("GTSBeh_TriggerSitdown")]
   GTS_Busy                -> TRUE]
   GTS_IsThighCrushing     -> TRUE
   GTS_Ready               -> FALSE
   GTS_Sitting             -> TRUE
 [NON GTS]
   TDM_Dodge               -> TRUE
   TDM_LockRotation11      -> TRUE
   bAnimationDriven        -> TRUE
   bHumanoidFootIKEnable   -> FALSE
   bIdleBeforeConversation -> FALSE
   bNoStagger              -> TRUE
   bVoiceReady             -> FALSE
 [DURING ANIMATION (IDLE/ATTACK)]
   **NO CHANGES**
 [EXIT ("GTSBeh_LeaveSitdown")]
   **ALL PREVIOUS STATED VARS INVERT**
   
  **Info: Once set the animation vars never change untill the action is stoped via exit.
 */

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Stage = ThighCrushNode::Stage;
	using State = ThighCrushNode::State;

	constexpr std::string_view NODE_FOOT_R  = "NPC R Foot [Rft ]";
	constexpr std::string_view NODE_FOOT_L  = "NPC L Foot [Lft ]";
	constexpr std::string_view NODE_THIGH_R = "NPC R Thigh [RThg]";
	constexpr std::string_view NODE_THIGH_L = "NPC L Thigh [LThg]";

	constexpr std::string_view ANIMATIONTYPE_TAG = "ThighCrush";

	constexpr std::string_view TASKID_THIGH_IDLE_L  = "ThighIdleL";
	constexpr std::string_view TASKID_THIGH_IDLE_R  = "ThighIdleR";
	constexpr std::string_view TASKID_THIGH_LIGHT_L = "ThighLightL";
	constexpr std::string_view TASKID_THIGH_LIGHT_R = "ThighLightR";
	constexpr std::string_view TASKID_THIGH_HEAVY_L = "ThighHeavyL";
	constexpr std::string_view TASKID_THIGH_HEAVY_R = "ThighHeavyR";

	constexpr std::string_view TASKID_FMT_THIGH_COLLISION         = "ThighCrushCollisionTask_{}_{}";
	constexpr std::string_view TASKID_FMT_THIGH_COLLISION_PASSIVE = "ThighCrusCollisionTaskPassive_{}";

	constexpr std::string_view STAMINA_DRAIN_TAG = "ThighCrush_StaminaDrain";

	constexpr std::string_view RUMBLE_TAG_ATKLIGHT     = "ThighCrushLight_Rumble";
	constexpr std::string_view RUMBLE_TAG_ATKLIGHT_END = "ThighCrushLight_Rumble_End";
	constexpr std::string_view RUMBLE_TAG_ATKHEAVY     = "ThighCrushHeavy_Rumble";
	constexpr std::string_view RUMBLE_TAG_ATKHEAVY_END = "ThighCrushHeavy_Rumble_End";
	constexpr std::string_view RUMBLE_TAG_BODY         = "ThighCrushHeavy_Rumble_Body";

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

	constexpr std::array LEG_RUMBLE_NODES = {
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

	//---------------
	// Rumble Tasks
	//---------------

	void LegRumblingOnce(std::string_view a_Tag, Actor& a_Actor, float a_Power, float a_Halflife) {
		for (const auto& node : LEG_RUMBLE_NODES) {
			Rumbling::Once(std::format("{}_{}", a_Tag, node), &a_Actor, a_Power, a_Halflife, node, 0.0f);
		}
	}

	void StartLegRumbling(std::string_view a_Tag, Actor& a_Actor, float a_Power, float a_Halflife) {
		for (const auto& node : LEG_RUMBLE_NODES) {
			Rumbling::Start(std::format("{}_{}", a_Tag, node), &a_Actor, a_Power / LEG_RUMBLE_NODES.size(), a_Halflife, node);
		}
	}

	void StopLegRumbling(std::string_view a_Tag, Actor& a_Actor) {
		for (const auto& node : LEG_RUMBLE_NODES) {
			Rumbling::Stop(std::format("{}_{}", a_Tag, node), &a_Actor);
		}
	}

	void StartBodyRumble(std::string_view a_Tag, Actor& a_Actor, float a_Power, float a_Halflife) {
		for (const auto& node : BODY_RUMBLE_NODES) {
			Rumbling::Start(std::format("{}_{}", a_Tag, node), &a_Actor, a_Power / BODY_RUMBLE_NODES.size(), a_Halflife, node);
		}
	}

	void StopBodyRumble(std::string_view a_Tag, Actor& a_Actor) {
		for (const auto& node : BODY_RUMBLE_NODES) {
			Rumbling::Stop(std::format("{}_{}", a_Tag, node), &a_Actor);
		}
	}

	//-----------------
	// Hit Detection
	//-----------------

	std::string CollisionTaskName(RE::FormID a_Owner, std::string_view a_Suffix) {
		return std::format(TASKID_FMT_THIGH_COLLISION, a_Owner, a_Suffix);
	}

	void RunThighCollisionTask(Actor* a_actor, bool a_Right, bool a_CooldownCheck, float a_Radius, float a_Damage, float a_BbMult, float a_CrushThreshold, int a_Random, std::string_view a_TaskName) {

		const std::string name = CollisionTaskName(a_actor->formID, a_TaskName);
		auto actorHandle = a_actor->CreateRefHandle();

		TaskManager::Run(name, [=](auto&) {

			auto actorPtr = actorHandle.get();

			if (!actorPtr) {
				return false;
			}

			Actor* actor = actorPtr.get();

			if (!AnimationVars::Action::IsThighCrushing(actor)) {
				return false;
			}

			const float animSpeedBonus = AnimationManager::GetBonusAnimationSpeed(actor);

			ApplyThighDamage(actor, a_Right, a_CooldownCheck, a_Radius, a_Damage * animSpeedBonus, a_BbMult, a_CrushThreshold, a_Random, DamageSource::ThighCrushed);

			return true;
		});
	}

	void RunPassiveThighCollisionTask(Actor* a_actor) {

		const std::string name = std::format(TASKID_FMT_THIGH_COLLISION_PASSIVE, a_actor->formID);
		auto actorHandle = a_actor->CreateRefHandle();

		TaskManager::Run(name, [=](auto&) {

			auto actorPtr = actorHandle.get();

			if (!actorPtr) {
				return false;
			}

			auto* actor = actorPtr.get();

			if (!AnimationVars::Action::IsThighCrushing(actor)) {
				return false;
			}

			NiAVObject* thighL = find_node(actor, NODE_THIGH_L);
			NiAVObject* thighR = find_node(actor, NODE_THIGH_R);

			if (!thighL || !thighR) {
				return false;
			}

			const float timescale = TimeScale();
			DoDamageAtPoint(actor, Radius_ThighCrush_Thigh_DOT, Damage_ThighCrush_Thigh_DOT * timescale, thighL, 100, 0.20f, 2.5f, DamageSource::ThighCrushed);
			DoDamageAtPoint(actor, Radius_ThighCrush_Thigh_DOT, Damage_ThighCrush_Thigh_DOT * timescale, thighR, 100, 0.20f, 2.5f, DamageSource::ThighCrushed);

			return true;
		});
	}

	void CancelCollisionTasks(RE::FormID a_Owner, std::initializer_list<std::string_view> a_Suffixes) {
		for (const auto& suffix : a_Suffixes) {
			TaskManager::Cancel(CollisionTaskName(a_Owner, suffix));
		}
	}

	void StartIdleCollision(Actor* a_Giant, float a_CrushThreshold) {
		RunThighCollisionTask(a_Giant, true, false, Radius_ThighCrush_Idle, Damage_ThighCrush_Legs_Idle, 0.02f, a_CrushThreshold, 600, TASKID_THIGH_IDLE_R);
		RunThighCollisionTask(a_Giant, false, false, Radius_ThighCrush_Idle, Damage_ThighCrush_Legs_Idle, 0.02f, a_CrushThreshold, 600, TASKID_THIGH_IDLE_L);
	}

	//-------------------
	// Tiny Management
	//-------------------

	//TODO: Tiny management needs improving. Freeze never really did anything.
	//TODO: Needs a way to lock tinies in place or move them like other anims do.

	void FreezeTinies(Actor* a_Giant, float a_Duration) {

		for (auto* tiny : ThighCrushController::GetSingleton().GetThighTargetsInFront(a_Giant, 1000)) {
			if (tiny && !tiny->IsPlayerRef() && !IsTeammate(tiny)) {
				ForceFlee(a_Giant, tiny, a_Duration, false);
			}
		}
	}

	// Randomised animation speed, NPCs only. The player's speed is determined by perks/AnimSpeedBonus.
	void AdjustAnimationSpeed(const ActionContext& a_Ctx, float a_Force, int a_Range, bool a_Reset) {

		auto* giant = a_Ctx.Actor();
		if (!giant || giant->IsPlayerRef()) {
			return;
		}

		if (a_Reset) {
			a_Ctx.SetAnimSpeed(1.0f);
			return;
		}

		const int rng = 100 + RandomInt(1, a_Range);
		a_Ctx.SetAnimSpeed(static_cast<float>(rng) / 100.0f * a_Force);
	}

	void GetUpFootstepDamage(Actor* a_Giant, float a_AnimSpeed, float a_Mult, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Thighs(a_Giant);
		float shake = Rumble_ThighCrush_StandUp * a_Mult * GetHighHeelsBonusDamage(a_Giant, true);

		if (TinyCalamityActive(a_Giant)) {
			shake = 2.0f;
		}

		DoDamageEffect(a_Giant, Damage_ThighCrush_Stand_Up * a_Mult * perk, Radius_ThighCrush_Stand_Up, 25, 0.20f, a_Event, 1.0f, a_Source);
		DoLaunch(a_Giant, 0.65f * a_Mult * perk, 2.0f * a_AnimSpeed, a_Event);
		Rumbling::Once(std::string(a_Rumble), a_Giant, shake, 0.10f, a_Node, 0.0f);
		DoFootstepSound(a_Giant, 1.0f, a_Event, a_Node);
		DoDustExplosion(a_Giant, 1.0f, a_Event, a_Node);
	}

	void SetDrain(const ActionContext& a_Ctx, bool& a_Flag, bool a_Enable, float a_Power) {

		if (a_Flag == a_Enable) {
			return;
		}

		a_Flag = a_Enable;
		DrainStamina(a_Ctx.Actor(), STAMINA_DRAIN_TAG, Runtime::PERK.GTSPerkThighAbilities, a_Enable, a_Power);
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks
	//----------------------------------------------------------------------------------------------

	void OnToSit(const ActionContext& a_Ctx) {

		auto* actor = a_Ctx.Actor();
		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());
		state.Stage = Stage::kSittingDown;

		StartLegRumbling(ANIMATIONTYPE_TAG, *actor, 1.35f, 0.10f);
		ManageCamera(actor, true, CameraTracking::Thigh_Crush);

		StartIdleCollision(actor, 2.0f);
		RunPassiveThighCollisionTask(actor);

		FreezeTinies(actor, 2.8f / AnimationManager::GetAnimSpeed(actor));
	}

	void OnSitLoopEnter(const ActionContext& a_Ctx) {
		StartLegRumbling(ANIMATIONTYPE_TAG, *a_Ctx.Actor(), 1.25f * a_Ctx.AnimSpeed(), 0.10f);
		a_Ctx.SetHHDisabled(true, 4.0f);
	}

	void OnSitLoopStart(const ActionContext& a_Ctx) {
		StopLegRumbling(ANIMATIONTYPE_TAG, *a_Ctx.Actor());
		ThighCrushNode::GetOrAdd(a_Ctx.Owner()).Stage = Stage::kIdle;
	}

	void OnSitLoopEnd(const ActionContext& a_Ctx) {
		StopLegRumbling(ANIMATIONTYPE_TAG, *a_Ctx.Actor());
		ThighCrushNode::GetOrAdd(a_Ctx.Owner()).Stage = Stage::kIdle;
	}

	void OnLightStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());
		state.Stage = Stage::kLight;

		StartLegRumbling(RUMBLE_TAG_ATKLIGHT, *giant, Rumble_ThighCrush_LegSpread_Light_Loop, 0.12f);
		SetDrain(a_Ctx, state.DrainingLight, true, 1.0f);

		CancelCollisionTasks(a_Ctx.Owner(), {TASKID_THIGH_IDLE_R, TASKID_THIGH_IDLE_L});

		RunThighCollisionTask(giant, true, true, Radius_ThighCrush_Spread_Out, Damage_ThighCrush_CrossLegs_Out, 0.10f, 1.70f, 50, TASKID_THIGH_LIGHT_R);
		RunThighCollisionTask(giant, false, true, Radius_ThighCrush_Spread_Out, Damage_ThighCrush_CrossLegs_Out, 0.10f, 1.70f, 50, TASKID_THIGH_LIGHT_L);

		AdjustAnimationSpeed(a_Ctx, 1.0f, 85, false);
	}

	void OnLightEnd(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());

		a_Ctx.SetCanEditAnimSpeed(true);

		StopLegRumbling(RUMBLE_TAG_ATKLIGHT, *giant);
		LegRumblingOnce(RUMBLE_TAG_ATKLIGHT_END, *giant, Rumble_ThighCrush_LegSpread_Light_End, 0.10f);

		SetDrain(a_Ctx, state.DrainingLight, false, 1.0f);

		CancelCollisionTasks(a_Ctx.Owner(), { TASKID_THIGH_LIGHT_R, TASKID_THIGH_LIGHT_L });
		AdjustAnimationSpeed(a_Ctx, 1.0f, 85, true);
	}

	void OnHeavyStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());
		state.Stage = Stage::kHeavy;

		SetDrain(a_Ctx, state.DrainingHeavy, true, 2.5f);

		CancelCollisionTasks(a_Ctx.Owner(), { TASKID_THIGH_IDLE_R, TASKID_THIGH_IDLE_L});

		RunThighCollisionTask(giant, true, true, Radius_ThighCrush_Spread_In, Damage_ThighCrush_CrossLegs_In, 0.25f, 1.4f, 25, TASKID_THIGH_HEAVY_R);
		RunThighCollisionTask(giant, false, true, Radius_ThighCrush_Spread_In, Damage_ThighCrush_CrossLegs_In, 0.25f, 1.4f, 25, TASKID_THIGH_HEAVY_L);

		StartLegRumbling(RUMBLE_TAG_ATKHEAVY, *giant, Rumble_ThighCrush_LegSpread_Heavy_Loop, 0.10f);

		AdjustAnimationSpeed(a_Ctx, 1.5f, 125, false);
	}

	void OnHeavyEnd(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());

		SetDrain(a_Ctx, state.DrainingHeavy, false, 2.5f);

		StopLegRumbling(RUMBLE_TAG_ATKHEAVY, *giant);
		LegRumblingOnce(RUMBLE_TAG_ATKHEAVY_END, *giant, Rumble_ThighCrush_LegCross_Heavy_End, 0.10f);

		CancelCollisionTasks(a_Ctx.Owner(), { TASKID_THIGH_HEAVY_R, TASKID_THIGH_HEAVY_L });
		StartIdleCollision(giant, 3.0f);

		AdjustAnimationSpeed(a_Ctx, 1.0f, 125, true);
		state.Stage = Stage::kIdle;
	}

	void OnSitLoopExit(const ActionContext& a_Ctx) {

		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());
		state.Stage = Stage::kStandingUp;
		state.RightSteps = 0;

		a_Ctx.SetHHDisabled(false, 1.0f);
		a_Ctx.SetCanEditAnimSpeed(false);
		a_Ctx.SetAnimSpeed(1.0f);

		StartBodyRumble(RUMBLE_TAG_BODY, *a_Ctx.Actor(), 0.25f, 0.12f);
	}

	void OnStandRight(const ActionContext& a_Ctx) {

		State& state = ThighCrushNode::GetOrAdd(a_Ctx.Owner());

		if (++state.RightSteps > 1) {
			return;
		}

		GetUpFootstepDamage(a_Ctx.Actor(), a_Ctx.AnimSpeed(), 1.0f, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "ThighCrushStompR");
		FootStepManager::PlayVanillaFootstepSounds(a_Ctx.Actor(), true);
	}

	void OnStandLeft(const ActionContext& a_Ctx) {
		GetUpFootstepDamage(a_Ctx.Actor(), a_Ctx.AnimSpeed(), 1.0f, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "ThighCrushStompL");
		FootStepManager::PlayVanillaFootstepSounds(a_Ctx.Actor(), false);
	}

	void OnBehNext(const ActionContext& a_Ctx) {
		a_Ctx.SetAnimSpeed(1.0f);
		a_Ctx.SetCanEditAnimSpeed(false);
	}

	void OnToExit(const ActionContext& a_Ctx) {
		StopBodyRumble(RUMBLE_TAG_BODY, *a_Ctx.Actor());
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Thigh_Crush);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	//A signature defines the expected animation vars and states for this given node.
	constexpr GraphExpect Signature[] = {
		{ "GTS_Sitting",           true  },
		{ "GTS_IsThighCrushing",   true  },
		{ "GTS_Busy",              true  }
	};

	//Annotations after which the signature is expected to drain rather than disagree.
	//GTSBEH_Exit is deliberately absent: a forced reset skips straight to it, so treating it as an
	//exit signal would report every reset as a clean completion.
	constexpr std::string_view ExitSignals[] = {
		"GTSsitloopexit",
		"GTStoexit",
	};

	bool CanAttack(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		if (!giant) {
			return false;
		}

		// One attack plays a light hit and then a heavy one. Asking again before it returns to the
		// idle is what produced the duplicate notifications the capture measured.
		const State* state = ThighCrushNode::Get(a_Ctx.Owner());
		if (!state || state->Stage != Stage::kIdle) {
			return false;
		}

		return true;
	}

	bool VerifyAttack(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return false;
		}

		float cost = 40.0f;
		if (Runtime::HasPerk(giant, Runtime::PERK.GTSPerkThighAbilities)) {
			cost *= 0.65f;
		}

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, "You're too tired to perform this attack!");
			return false;
		}

		return true;
	}

	bool CanExit(const ActionContext&) {
		return !IsFreeCameraEnabled();
	}

	// Input names are the existing keybind events. Renaming one drops the user's saved binding.
	constexpr EntryDef Entries[] = {
		{ .Action = "ThighCrush.Enter", .Behaviour = "GTSBeh_TriggerSitdown", .BlockedBy = kBlockedByHand, .Input = "Action.ThighCrush.Start" },
	};

	constexpr ActionDef Actions[] = {
		{ .Action = "ThighCrush.Attack", .Behaviour = "GTSBeh_StartThighCrush", .Guard = CanAttack, .Verify = VerifyAttack, .Cooldown = 0.35f, .Input = "Action.ThighCrush.Attack" },
		{ .Action = "ThighCrush.Exit",   .Behaviour = "GTSBeh_LeaveSitdown",    .Guard = CanExit, .Exits = true, .Input = "Action.ThighCrush.Exit" },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTStosit",                 .Handler = OnToSit },
		{ .Tag = "GTSsitloopenter",          .Handler = OnSitLoopEnter },
		{ .Tag = "GTSsitloopstart",          .Handler = OnSitLoopStart },
		{ .Tag = "GTSsitloopend",            .Handler = OnSitLoopEnd },
		{ .Tag = "GTSsitcrushlight_start",   .Handler = OnLightStart },
		{ .Tag = "GTSsitcrushlight_end",     .Handler = OnLightEnd },
		{ .Tag = "GTSsitcrushheavy_start",   .Handler = OnHeavyStart },
		{ .Tag = "GTSsitcrushheavy_end",     .Handler = OnHeavyEnd },
		{ .Tag = "GTSsitloopexit",           .Handler = OnSitLoopExit },
		{ .Tag = "GTSstandR",                .Handler = OnStandRight },
		{ .Tag = "GTSstandL",                .Handler = OnStandLeft },
		{ .Tag = "GTStoexit",                .Handler = OnToExit },
		{ .Tag = "GTSBEH_Next",              .Handler = OnBehNext, .Shared = true },
		{ .Tag = "GTSBEH_Exit",              .Shared = true },
	};

	constexpr std::string_view StageNames[] = {
		"sitting down",
		"idle",
		"light",
		"heavy",
		"standing up",
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> ThighCrushNode::Signature() const        { return ::Signature; }
	std::span<const std::string_view> ThighCrushNode::ExitSignals() const { return ::ExitSignals; }
	bool ThighCrushNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool) const {
		return PlayerTarget::StartAimedOn(a_Actor, a_Target, a_Action);
	}

	std::span<const EntryDef> ThighCrushNode::Entries() const             { return ::Entries; }
	std::span<const ActionDef> ThighCrushNode::Actions() const            { return ::Actions; }
	std::span<const AnnotationDef> ThighCrushNode::Annotations() const    { return ::Annotations; }

	ThighCrushNode::State* ThighCrushNode::Get(RE::FormID a_Owner) {
		return m_State.Find(a_Owner);
	}

	ThighCrushNode::State& ThighCrushNode::GetOrAdd(RE::FormID a_Owner) {
		return m_State.GetOrAdd(a_Owner);
	}

	bool ThighCrushNode::CanEnter(const EntryContext& a_Ctx) const {

		auto* giant = a_Ctx.Actor();
		if (!giant) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}

		if (IsPlayerFirstPerson(giant) || AnimationVars::Crawl::IsCrawling(giant)) {
			return false;
		}

		// Sitting on furniture asserts the same GTS_Sitting the signature reads.
		if (giant->AsActorState()->GetSitSleepState() == SIT_SLEEP_STATE::kIsSitting) {
			return false;
		}

		return true;
	}

	//Startup, create new state.
	void ThighCrushNode::OnEnter(const ActionContext& a_Ctx) {
		m_State.GetOrAdd(a_Ctx.Owner()) = State{};
	}

	// Everything started anywhere in this node is stopped here, on every path out. Unwinding through
	// GTStoexit alone leaves a desync or an actor reset with the collision tasks, the leg rumble and
	// the stamina drain still running.
	void ThighCrushNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* actor = a_Ctx.Actor();

		CancelCollisionTasks(owner, {
			TASKID_THIGH_IDLE_R,  TASKID_THIGH_IDLE_L,
			TASKID_THIGH_LIGHT_R, TASKID_THIGH_LIGHT_L,
			TASKID_THIGH_HEAVY_R, TASKID_THIGH_HEAVY_L,
		});

		TaskManager::Cancel(std::format(TASKID_FMT_THIGH_COLLISION_PASSIVE, owner));

		if (State* state = m_State.Find(owner)) {
			SetDrain(a_Ctx, state->DrainingLight, false, 1.0f);
			SetDrain(a_Ctx, state->DrainingHeavy, false, 2.5f);
		}

		if (actor) {
			StopLegRumbling(ANIMATIONTYPE_TAG, *actor);
			StopLegRumbling(RUMBLE_TAG_ATKLIGHT, *actor);
			StopLegRumbling(RUMBLE_TAG_ATKHEAVY, *actor);
			StopBodyRumble(RUMBLE_TAG_BODY, *actor);
		}

		if (a_Reason != ExitReason::kCompleted) {
			logger::debug("ThighCrush: {:08X} left on {}", owner, ExitReasonName(a_Reason));
		}

		m_State.Forget(owner);
	}

	void ThighCrushNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void ThighCrushNode::OnReset() {
		m_State.Clear();
	}

	std::string_view ThighCrushNode::StateName(RE::FormID a_Owner) const {
		const State* state = m_State.Find(a_Owner);
		return state ? StageNames[std::to_underlying(state->Stage)] : "";
	}
}
