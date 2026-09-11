#include "Actions/Nodes/Crush/BoobCrushNode.hpp"

#include "Actions/Core/ActionCleanup.hpp"
#include "Actions/Core/Possession.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"
#include "Utils/Actions/ButtCrushUtils.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
*/
/*
  ----- BoobCrush Fast (Quick) (Crawling)]
  [INTRO]
	IdleOffsetStop [NON GTS]
	GTS_BoobCrush_Smile_On
	GTSCrawl_HandImpact_R
	GTSCrawl_KneeImpact_L
	GTS_BoobCrush_TrackBody
	GTSCrawl_KneeImpact_R
	GTSCrawl_HandImpact_L
    GTS_BoobCrush_DOT_Start_Loop
	GTSBEH_Next
  [FALL DOWN]
    GTSCrawl_KneeImpact_R
    GTSCrawl_KneeImpact_L
    GTS_BoobCrush_BreastImpact
  [EXIT]
	GTSCrawl_KneeImpact_R
    GTSCrawl_KneeImpact_L
    GTS_BoobCrush_Smile_Off
    GTS_BoobCrush_LoseSize
    GTSCrawl_KneeImpact_L
    GTS_BoobCrush_UnTrackBody
    GTSCrawl_HandImpact_L
    GTSCrawl_KneeImpact_R
    GTSCrawl_HandImpact_R
    GTSCrawl_HandImpact_L
    GTSBEH_Exit
  [CLEAN UP]
    GTS_ResetVars
    GTSBEH_Camera_Reset
    HeadTrackingOn [NON GTS]
    IdleStop       [NON GTS]
    tailSneakIdle  [NON GTS]
    GTSBeh_Dummy
*/
/*
  ----- BoobCrush (Normal) (Crawling)
  ----- Whole loop, Start, Idle, Grow, Attack (In that Order)
 [INTRO]
   IdleOffsetStop
   GTSBeh_BoobCrush_Enter
   GTS_BoobCrush_Smile_On
   GTSCrawl_HandImpact_R
   GTSCrawl_KneeImpact_L
   GTS_BoobCrush_TrackBody
   GTSCrawl_KneeImpact_R
   GTSCrawl_HandImpact_L
   GTS_BoobCrush_DOT_Start_Loop
   GTSBEH_Next
 [LOOP/IDLE]
   GTSCrawl_KneeImpact_L
   GTSCrawl_HandImpact_R
   GTSCrawl_KneeImpact_R
   GTSCrawl_KneeImpact_L
   GTSCrawl_HandImpact_R
 [GROW]
   GTS_BoobCrush_Grow_Start
   GTS_BoobCrush_Grow_Stop
   GTSBEH_Next
 [ATTACK]
   GTS_BoobCrush_BreastImpact
   GTSCrawl_KneeImpact_R
   GTSCrawl_KneeImpact_L
   GTS_BoobCrush_DOT_Start
   GTSCrawl_HandImpact_R
   GTS_BoobCrush_Smile_Off
   GTSCrawl_HandImpact_L
   GTSCrawl_KneeImpact_L
 [STAND UP/EXIT]
   GTS_BoobCrush_UnTrackBody
   GTS_BoobCrush_DOT_End
   GTS_BoobCrush_LoseSize
   GTSCrawl_HandImpact_L
   GTSCrawl_KneeImpact_R
   GTSCrawl_HandImpact_R
   GTSBEH_Exit|
 [CLEANUP]
   GTS_ResetVars
   GTSBEH_Camera_Reset
   HeadTrackingOn
   IdleStop
   tailSneakIdle
   GTSBeh_Dummy
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
*/
/*
   ----- BoobCrush Fast (Quick) (Crawling)
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
	 GTS_Busy                -> TRUE
	 GTS_Ready               -> FALSE
   (NON GTS)
     IsSneaking              -> False
	 TDM_Dodge               -> TRUE
	 TDM_LockRotation11      -> TRUE
	 bAnimationDriven        -> TRUE
	 bIdleBeforeConversation -> FALSE
	 bNoStagger              -> TRUE
	 bVoiceReady             -> FALSE
   [ON IMPACT WITH GROUND]
	 **NOTIHING**
   [EXIT]
	 **ALL PREVIOUS STATED VARS INVERT**
*/
/*
 ----- BoobCrush Normal (Crawling)]
 [INTRO]
	 GTS_Busy                -> TRUE
	 GTS_IsButtCrushing      -> TRUE
	 GTS_IsCrawlButtCrush    -> TRUE
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
	 **NOTHING**
 [STAND UP/EXIT]
	**NO CHANGES**
 [CLEANUP]
	 GTS_Busy                -> FALSE
	 GTS_IsButtCrushing      -> FALSE
	 GTS_Ready               -> TRUE
	 GTS_IsCrawlButtCrush    -> FALSE
 (NON GTS)
	 TDM_Dodge               -> FALSE
	 TDM_LockRotation11      -> FALSE
	 bAnimationDriven        -> FALSE
	 bIdleBeforeConversation -> TRUE
	 bNoStagger              -> FALSE
	 bVoiceReady             -> TRUE
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using State = BoobCrushNode::State;

	constexpr std::string_view RUMBLE_TAG_GROWTH = "BoobCrush_Growth_Rumble";

	constexpr std::string_view TASKID_FMT_DOT     = "BoobCrush_BreastDOT_{}";
	constexpr std::string_view TASKID_FMT_DRAIN   = "BoobCrush_LayingDrain_{}";

	constexpr std::string_view NODE_BREASTNPC_L = "NPC L Breast";
	constexpr std::string_view NODE_BREASTNPC_R = "NPC R Breast";
	constexpr std::string_view NODE_BREAST02_R = "R Breast02";
	constexpr std::string_view NODE_BREAST02_L = "L Breast02";
	constexpr std::string_view NODE_BREAST03_R = "R Breast03";
	constexpr std::string_view NODE_BREAST03_L = "L Breast03";

	// The body set plus the four breast bones, which is the only difference from butt crush.
	std::span<const std::string_view> RumbleNodes() {
		return Crush::BreastRumbleNodes();
	}

	void StartRumble(std::string_view a_Tag, Actor& a_Actor, float a_Power, float a_Halflife) {
		Crush::StartRumble(a_Tag, a_Actor, a_Power, a_Halflife, RumbleNodes());
	}

	void StopRumble(std::string_view a_Tag, Actor& a_Actor) {
		Crush::StopRumble(a_Tag, a_Actor, RumbleNodes());
	}

	void StartDamageOverTime(Actor* a_Giant) {

		const std::string name = std::format(TASKID_FMT_DOT, a_Giant->formID);
		auto handle = a_Giant->CreateRefHandle();
		const float damage = GetButtCrushDamage(a_Giant) * TimeScale();

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			if (!AnimationVars::ButtCrush::IsButtCrushing(giant)) {
				return false;
			}

			//const float shake = ;

			for (const auto& NodeName : Crush::ImpactBodyNodes()) {
				if (auto* node = find_node(giant, NodeName)) {
					DoDamageAtPoint(giant, Radius_BreastCrush_BodyDOT, Damage_BreastCrush_BodyDOT * damage, node, 400, 0.10f, 1.33f, DamageSource::BodyCrush);
					Rumbling::Once(std::format("BoobCrushDOT_{}", NodeName), giant, Rumble_Cleavage_HoverLoop, 0.02f, NodeName, 0.0f);
				}
			}

			auto* breastL = find_node(giant, NODE_BREASTNPC_L);
			auto* breastR = find_node(giant, NODE_BREASTNPC_R);
			auto* breastL02 = find_node(giant, NODE_BREAST02_L);
			auto* breastR02 = find_node(giant, NODE_BREAST02_R);

			if (breastL02 && breastR02) {
				Rumbling::Once("BreastDOT_L", giant, Rumble_Cleavage_HoverLoop, 0.025f, NODE_BREAST03_L, 0.0f);
				Rumbling::Once("BreastDOT_R", giant, Rumble_Cleavage_HoverLoop, 0.025f, NODE_BREAST03_R, 0.0f);
				DoDamageAtPoint(giant, Radius_BreastCrush_BreastDOT, Damage_BreastCrush_BreastDOT * damage, breastL02, 400, 0.10f, 1.33f, DamageSource::BreastImpact);
				DoDamageAtPoint(giant, Radius_BreastCrush_BreastDOT, Damage_BreastCrush_BreastDOT * damage, breastR02, 400, 0.10f, 1.33f, DamageSource::BreastImpact);
				return true;
			}

			if (breastL && breastR) {
				Rumbling::Once("BreastDOT_L", giant, Rumble_Cleavage_HoverLoop, 0.025f, NODE_BREASTNPC_L, 0.0f);
				Rumbling::Once("BreastDOT_R", giant, Rumble_Cleavage_HoverLoop, 0.025f, NODE_BREASTNPC_R, 0.0f);
				DoDamageAtPoint(giant, Radius_BreastCrush_BreastDOT, Damage_BreastCrush_BreastDOT * damage, breastL, 400, 0.10f, 1.33f, DamageSource::BreastImpact);
				DoDamageAtPoint(giant, Radius_BreastCrush_BreastDOT, Damage_BreastCrush_BreastDOT * damage, breastR, 400, 0.10f, 1.33f, DamageSource::BreastImpact);
				return true;
			}

			return false;
		});

		TaskManager::ChangeUpdate(name, UpdateKind::Havok);
	}

	void StartLayingDrain(Actor* a_Giant) {

		const std::string name = std::format(TASKID_FMT_DRAIN, a_Giant->formID);
		auto handle = a_Giant->CreateRefHandle();

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			DamageAV(giant, ActorValue::kStamina, 0.12f * GetButtCrushCost(giant, false));

			return AnimationVars::ButtCrush::IsButtCrushing(giant);
		});
	}

	void InflictBodyDamage(Actor* a_Giant) {

		const float damage = GetButtCrushDamage(a_Giant);
		const float perk = GetPerkBonus_Basics(a_Giant);

		for (const auto& name : Crush::ImpactBodyNodes()) {
			if (auto* node = find_node(a_Giant, name)) {
				DoDamageAtPoint(a_Giant, Radius_BreastCrush_BodyImpact, Damage_BreastCrush_Body * damage, node, 400, 0.10f, 0.8f, DamageSource::BodyCrush);
				DoLaunch(a_Giant, 2.25f * perk, 5.0f, node);
				Rumbling::Once(std::format("BoobCrushImpact_{}", name), a_Giant, 1.00f * damage, 0.035f, name, 0.0f);
			}
		}
	}

	void InflictBreastDamage(const ActionContext& a_Ctx) {

		auto* actor = a_Ctx.Actor();
		const float damage = GetButtCrushDamage(actor);
		const float perk = GetPerkBonus_Basics(actor);

		if (auto* victim = BoobCrushNode::Victim(a_Ctx.Owner())) {
			SetBeingEaten(victim, false); // Allow to be staggered
		}

		InflictBodyDamage(actor);

		float dust = 1.0f;
		float smt = 1.0f;

		if (TinyCalamityActive(actor)) {
			dust = 1.25f;
			smt = 1.5f;
		}

		auto* breastL = find_node(actor, NODE_BREASTNPC_L);
		auto* breastR = find_node(actor, NODE_BREASTNPC_R);
		auto* breastL02 = find_node(actor, NODE_BREAST02_L);
		auto* breastR02 = find_node(actor, NODE_BREAST02_R);

		const float shake = Rumble_Cleavage_Impact * dust * damage;

		if (breastL02 && breastR02) {
			DoDamageAtPoint(actor, Radius_BreastCrush_BreastImpact, Damage_BreastCrush_BreastImpact * damage, breastL02, 4, 0.70f, 0.8f, DamageSource::BreastImpact);
			DoDamageAtPoint(actor, Radius_BreastCrush_BreastImpact, Damage_BreastCrush_BreastImpact * damage, breastR02, 4, 0.70f, 0.8f, DamageSource::BreastImpact);
			DoDustExplosion(actor, 1.25f * dust + damage / 10, FootEvent::Left, NODE_BREAST03_L);
			DoDustExplosion(actor, 1.25f * dust + damage / 10, FootEvent::Right, NODE_BREAST03_R);
			DoFootstepSound(actor, 1.25f, FootEvent::Right, NODE_BREAST03_R);
			DoFootstepSound(actor, 1.25f, FootEvent::Left, NODE_BREAST03_L);
			DoLaunch(actor, 2.25f * perk, 5.0f, FootEvent::Breasts);
			Rumbling::Once("BreastL", actor, shake * smt, 0.075f, NODE_BREAST03_L, 0.0f);
			Rumbling::Once("BreastR", actor, shake * smt, 0.075f, NODE_BREAST03_R, 0.0f);
			ModGrowthCount(actor, 0, true);
		}
		else if (breastL && breastR) {
			DoDamageAtPoint(actor, Radius_BreastCrush_BreastImpact, Damage_BreastCrush_BreastImpact * damage, breastL, 4, 0.70f, 0.8f, DamageSource::BreastImpact);
			DoDamageAtPoint(actor, Radius_BreastCrush_BreastImpact, Damage_BreastCrush_BreastImpact * damage, breastR, 4, 0.70f, 0.8f, DamageSource::BreastImpact);
			DoDustExplosion(actor, 1.25f * dust + damage / 10, FootEvent::Left, NODE_BREASTNPC_L);
			DoDustExplosion(actor, 1.25f * dust + damage / 10, FootEvent::Right, NODE_BREASTNPC_R);
			DoFootstepSound(actor, 1.25f, FootEvent::Left, NODE_BREASTNPC_L);
			DoFootstepSound(actor, 1.25f, FootEvent::Right, NODE_BREASTNPC_R);

			DoLaunch(actor, 2.25f * perk, 5.0f, FootEvent::Breasts);
			Rumbling::Once("BreastL", actor, shake * smt, 0.075f, NODE_BREASTNPC_L, 0.0f);
			Rumbling::Once("BreastR", actor, shake * smt, 0.075f, NODE_BREASTNPC_R, 0.0f);
			ModGrowthCount(actor, 0, true);
		}
		else if (!breastR) {
			Notify("Error: Missing Breast Nodes");
			Notify("Error: Rffects not inflicted");
			Notify("Suggestion: Install Female body replacer");
		}
		else {
			Notify("Error: Missing 3BB Breast Nodes");
			Notify("Error: Effects not inflicted");
			Notify("Suggestion: Install 3BB/SMP Body");
		}
	}

	//----------------------------------------------------------------------------------------------
	// Annotation Callbacks
	//----------------------------------------------------------------------------------------------

	void OnTrackBody(const ActionContext& a_Ctx) {
		Crush::RecordStart(a_Ctx.Actor());
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::ObjectB);
	}

	void OnUnTrackBody(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::ObjectB);
	}

	void OnSmileOn(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		AdjustFacialExpression(giant, 0, 1.0f, CharEmotionType::Modifier); // blink L
		AdjustFacialExpression(giant, 1, 1.0f, CharEmotionType::Modifier); // blink R
		AdjustFacialExpression(giant, 2, 1.0f, CharEmotionType::Expression);

		ApplyButtCrushCooldownTask(giant);
	}

	void OnSmileOff(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Modifier);
		AdjustFacialExpression(giant, 1, 0.0f, CharEmotionType::Modifier);
		AdjustFacialExpression(giant, 2, 0.0f, CharEmotionType::Expression);
	}

	void OnBreastImpact(const ActionContext& a_Ctx) {
		BoobCrushNode::GetOrAdd(a_Ctx.Owner()).Impacted = true;
		InflictBreastDamage(a_Ctx);
	}

	void OnDotStart(const ActionContext& a_Ctx) {
		StartLayingDrain(a_Ctx.Actor());
	}

	void OnDotStartLoop(const ActionContext& a_Ctx) {
		BoobCrushNode::GetOrAdd(a_Ctx.Owner()).DOTing = true;
		StartDamageOverTime(a_Ctx.Actor());
	}

	void OnDotEnd(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		BoobCrushNode::GetOrAdd(a_Ctx.Owner()).DOTing = false;

		TaskManager::Cancel(std::format(TASKID_FMT_DOT, a_Ctx.Owner()));
		TaskManager::Cancel(std::format(TASKID_FMT_DRAIN, a_Ctx.Owner()));
		ModGrowthCount(giant, 0, true);
	}

	void OnGrowStart(const ActionContext& a_Ctx) {
		auto* actor = a_Ctx.Actor();

		Sound_PlayMoans(actor, 1.0f, 0.14f, EmotionTriggerSource::Growth, CooldownSource::Emotion_Voice_Long);
		Crush::ApplyGrowth(a_Ctx, "BreastCrushGrowth", 1.0f);
		Task_FacialEmotionTask_Moan(actor, 0.675f, "BreastCrushGrowth", 0.20f);
		StartRumble(RUMBLE_TAG_GROWTH, *actor, 0.06f, 0.60f);
	}

	void OnGrowStop(const ActionContext& a_Ctx) {
		StopRumble(RUMBLE_TAG_GROWTH, *a_Ctx.Actor());
	}

	void OnLoseSize(const ActionContext& a_Ctx) {
		Crush::RestoreSize(a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	// GTS_IsCrawlButtCrush is the discriminator. It's what differentiates this from the other variants.
	constexpr GraphExpect Signature[] = {
		{ "GTS_IsButtCrushing",   true },
		{ "GTS_IsCrawlButtCrush", true },
		{ "GTS_Busy",             true },
	};

	// Leaves the root Giantess_State from any point inside it. Vore, both crushes and thigh crush
	// all run in that state, and nothing else in the DLL sends this.
	constexpr std::string_view BEH_ABORT = "GTSBeh_ExitEvents";

	constexpr std::string_view ExitSignals[] = {
		"GTS_BoobCrush_DOT_End",
		"GTS_BoobCrush_LoseSize",
		"GTS_BoobCrush_UnTrackBody",
	};

	bool CanGrow(const ActionContext& a_Ctx) {
		const State* state = BoobCrushNode::Get(a_Ctx.Owner());
		return Crush::CanGrow(a_Ctx, !state || state->Quick);
	}

	// From testing: The quick crawl variant sets neither GTS_IsButtCrushing nor GTS_IsCrawlButtCrush, so
	// Have to key it off GTS_Busy as there are no other unique animVars.
	constexpr GraphExpect QuickSignature[] = {
		{ "GTS_Busy", true },
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "BoobCrush.Enter",      .Behaviour = "GTSBEH_ButtCrush_Start",     .Guard = Crush::CanStart, .Verify = Crush::VerifyStart, .BlockedBy = kBlockedByHand },
		{ .Action = "BoobCrush.EnterQuick", .Behaviour = "GTSBEH_ButtCrush_StartFast", .Guard = Crush::CanStart, .Verify = Crush::VerifyStart, .BlockedBy = kBlockedByHand, .Signature = QuickSignature },
	};

	// The same keys as ButtCrushNode. Each node scopes its own binding to itself, so the key reaches
	// whichever of the two is running.
	constexpr ActionDef Actions[] = {
		{ .Action = "Crush.Grow",   .Behaviour = "GTSBEH_ButtCrush_Grow",   .Guard = CanGrow, .Verify = Crush::VerifyGrow, .Cooldown = 0.35f, .Input = "Action.Crush.Grow" },
		{ .Action = "Crush.Attack", .Behaviour = "GTSBEH_ButtCrush_Attack", .Exits = true, .Cooldown = 0.35f, .Input = "Action.Crush.Attack" },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_BoobCrush_TrackBody",      .Handler = OnTrackBody },
		{ .Tag = "GTS_BoobCrush_UnTrackBody",    .Handler = OnUnTrackBody },
		{ .Tag = "GTS_BoobCrush_Smile_On",       .Handler = OnSmileOn },
		{ .Tag = "GTS_BoobCrush_Smile_Off",      .Handler = OnSmileOff },
		{ .Tag = "GTS_BoobCrush_BreastImpact",   .Handler = OnBreastImpact },
		{ .Tag = "GTS_BoobCrush_DOT_Start",      .Handler = OnDotStart },
		{ .Tag = "GTS_BoobCrush_DOT_Start_Loop", .Handler = OnDotStartLoop },
		{ .Tag = "GTS_BoobCrush_DOT_End",        .Handler = OnDotEnd },
		{ .Tag = "GTS_BoobCrush_Grow_Start",     .Handler = OnGrowStart },
		{ .Tag = "GTS_BoobCrush_Grow_Stop",      .Handler = OnGrowStop },
		{ .Tag = "GTS_BoobCrush_LoseSize",       .Handler = OnLoseSize },

		// Unused, but registered.
		{ .Tag = "GTSBeh_BoobCrush_Enter" },
		{ .Tag = "GTSBEH_Next", .Shared = true },
		{ .Tag = "GTSBEH_Exit", .Shared = true },
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> BoobCrushNode::Signature() const { return ::Signature; }
	std::span<const std::string_view> BoobCrushNode::ExitSignals() const { return ::ExitSignals; }
	std::string_view BoobCrushNode::AbortSignal() const { return BEH_ABORT; }
	std::span<const EntryDef> BoobCrushNode::Entries() const { return ::Entries; }
	std::span<const ActionDef> BoobCrushNode::Actions() const { return ::Actions; }
	std::span<const AnnotationDef> BoobCrushNode::Annotations() const { return ::Annotations; }

	BoobCrushNode::State* BoobCrushNode::Get(RE::FormID a_Owner) {
		return m_State.Find(a_Owner);
	}

	BoobCrushNode::State& BoobCrushNode::GetOrAdd(RE::FormID a_Owner) {
		return m_State.GetOrAdd(a_Owner);
	}

	RE::Actor* BoobCrushNode::Victim(RE::FormID a_Owner) {
		return Possession::FirstActor(a_Owner, PossessionSlot::kBreasts);
	}

	// The crawling stance is this node's, every other one belongs to ButtCrushNode.
	bool BoobCrushNode::CanEnter(const EntryContext& a_Ctx) const {
		return Crush::CanEnter(a_Ctx, true);
	}

	void BoobCrushNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};
		state.Quick = a_Ctx.EnteredVia() == "BoobCrush.EnterQuick";

		Crush::RecordStart(a_Ctx.Actor());
	}

	void BoobCrushNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		TaskManager::Cancel(std::format(TASKID_FMT_DOT, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_DRAIN, owner));

		Crush::CancelHeldTasks(owner, PossessionSlot::kBreasts);

		if (giant) {
			StopRumble(RUMBLE_TAG_GROWTH, *giant);

			AdjustFacialExpression(giant, 0, 0.0f, CharEmotionType::Modifier);
			AdjustFacialExpression(giant, 1, 0.0f, CharEmotionType::Modifier);
			AdjustFacialExpression(giant, 2, 0.0f, CharEmotionType::Expression);

			Crush::RestoreSize(giant);
		}

		if (a_Reason != ExitReason::kCompleted) {
			logger::debug("BoobCrush: {:08X} left on {}", owner, ExitReasonName(a_Reason));
		}

		m_State.Forget(owner);
	}

	void BoobCrushNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void BoobCrushNode::OnReset() {
		m_State.Clear();
	}

	std::string_view BoobCrushNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);
		if (!state) {
			return "";
		}

		//[D]amage [O]ver [T]ime
		if (state->DOTing) {
			return "LayingDOT";
		}

		if (state->Impacted) {
			return "ImpactHit";
		}

		return state->Quick ? "Quick" : "Entering";
	}
}
