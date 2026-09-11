#include "Actions/Nodes/ThighSandwich/ThighSandwichButtNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/ThighSandwich/SandwichCommon.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"
#include "Actions/Nodes/Crush/CrushCommon.hpp"
#include "Utils/Actions/ButtCrushUtils.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Reached only by handoff from ThighSandwichNode, and left the same way. The graph's own machine:

    0 Butt_Intro        -> 1 on GTSBEH_Next
    1 Butt_Loop         -> Stop, Grow, HeavyAttack, LightAttack, UB, GrindStart
    2 Butt_Outro          its clip fires GTSBEH_ButtState_Leave, which returns to the thigh loop
    3 Butt_Growth       -> 1 on GTSBEH_Next
    4 Butt_Atk_Heavy    -> 1 on GTSBEH_Next, or 9 on GTSBEH_TinyDied
    5 Butt_Atk_Light    -> 1 on GTSBEH_Next, or 9 on GTSBEH_TinyDied
    6 Butt_UB
    7 Butt_DOT (grind)  -> 1 on GTSBEH_ButtState_GrindLeave, 8 on Finisher, 9 on TinyDied
    8 Butt_DOT_Finisher
    9 Butt_Outro_NoTiny

  Stop and GrindStop are requests, not endings. Each moves the machine to an outro clip, and that
  clip fires its own Leave event when it is nearly over. The DLL never sends either Leave.

  ----- Butt State [("GTSBEH_ButtState_Start" + "GTSBEH_T_ButtState_Start")]
  [INTRO]
    GTS_ToRight             [NOT REGISTERED]
    GTS_ToAnimA             [NOT REGISTERED]
    GTS_TSB_SitDownSoft     -> the giant is on them, suffocation starts
    GTSBEH_Next             -> settles into the idle loop
  [LIGHT / HEAVY ATTACK ("GTSBEH_ButtState_LightAttack" / "_HeavyAttack")]
    GTS_TSB_Stand
    GTS_TSB_Fall
    GTS_TSB_LandSmall / GTS_TSB_LandHeavy
    GTSBEH_Next             -> back to the idle loop
  [GROW ("GTSBEH_ButtState_Grow")]
    GTSButtCrush_GrowthStart        borrowed from the butt crush clip
    GTSBEH_ButtCrush_GrowthFinish
    GTSBEH_Next             -> back to the idle loop
  [GRIND ("GTSBEH_ButtState_GrindStart")]
    GTSBEH_Next
    GTS_TSB_DOT_Start       -> the damage task starts here
    ... on "GTSBEH_ButtState_GrindStop":
    GTS_TSB_DOT_Stop
    GTSBEH_ButtState_GrindLeave     -> the outro clip announces itself, back to the idle loop
  [FINISHER ("GTSBEH_ButtState_Finisher")]
    GTS_TSB_LandMid, GTS_TSB_LandMid    the first two hits deal nothing
    GTS_TSB_LandFinisher
    GTS_TSB_LandFloor
  [UNBIRTH ("GTSBEH_Sandwich_UB" + "GTSBEH_T_Sandwich_UB")]
    GTS_ToRight             [NOT REGISTERED]
    GTS_TSB_SitDown
    GTS_ToAnimA             [NOT REGISTERED]
    GTS_TSB_TinyInserted
    GTS_TSB_TinyPushStart
    GTS_TSB_UBStart
    GTS_TSB_TinyFullyIn
    GTS_TSB_UBEnjoy
    GTS_TSB_TinyKill        -> the tinies are absorbed here
    GTS_TSB_UBAbsorb
    GTS_TSB_ResetData       -> arms the exit
    GTS_TSB_UBEnd
  [CLEAN UP]
    GTSBEH_Exit
    GTS_ResetVars            [NON GTS]
    GTSSandwich_DisableRune
    GTSBEH_Camera_Reset      [NON GTS]
    GTSBeh_Dummy             [NON GTS]

  **Info: the grind only reaches the finisher through the damage task. Once a tiny drops below the
  **threshold the task asks for Sandwich.Butt.Finisher rather than simply stopping, which is what
  **hands it the kill.
*/

/*
   -------------------------------------------------------------------------- ANIMATION VARS
   ----- Butt State [("GTSBEH_ButtState_Start")]
   [SET WHEN THE BRANCH IS ENTERED]
     GTS_IsButtState         -> TRUE
   [WHILE ANY ACTION RUNS]
   (NON GTS)
     GTSBeh_DontExit         -> TRUE, back to FALSE on the GTSBEH_Next that ends it
   [WHILE GRINDING]
     GTS_IsButtGrinding      -> TRUE, back to FALSE after GTSBEH_ButtState_GrindLeave
   [WHILE UNBIRTHING]
     GTS_IsVoring            -> TRUE
   [CLEANUP]
     **EVERY VAR THE GROUP SET INVERTS, BOTH BRANCHES AT ONCE**

   **Info: GTS_IsButtState is cleared by the animation well after the branch has visibly ended. A
   **capture measured ten seconds between GTSBEH_ButtState_Stop and the variable falling, four of
   **them with the thigh loop already playing. The registry cannot leave before the variable says
   **so, so anything the thigh loop fires in that window arrives here and is reported as orphaned.
   **
   **GTS_IsVoring is asserted during the unbirth, which is also half of VoreNode's signature. It is
   **harmless while this node is active, because the idle sweep only runs for an actor doing nothing.
*/

using namespace GTS;

namespace {

	using namespace GTS::Actions;
	using State = ThighSandwichButtNode::State;

	constexpr float LightAttackStamina = 35.0f;
	constexpr float HeavyAttackStamina = 75.0f;

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsThighSandwiching", true },
		{ "GTS_IsButtState",        true },
	};

	// Leaving to the thigh loop is a handoff and arms its own drain. These are the ways this branch
	// ends the group outright: the last tiny died, or the giant stood up after the finisher or the
	// unbirth.
	constexpr std::string_view ExitSignals[] = {
		"GTS_TSB_LandFloor",
		"GTS_TSB_ResetData",
		"GTSBEH_Exit",
	};

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	// One unbirth at a time. Without this a second press during the swallow is taken and the graph
	// restarts the animation part way through.
	bool CanUnbirth(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichButtNode::Get(a_Ctx.Owner());
		return !Sandwich::Empty(a_Ctx.Actor()) && !(state && state->Absorbing);
	}

	// The grind owns the machine while it runs, so nothing else is offered until it is over.
	bool CanAct(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichButtNode::Get(a_Ctx.Owner());
		return !Sandwich::Empty(a_Ctx.Actor()) && !(state && (state->Grinding || state->Absorbing));
	}

	bool VerifyLight(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (GetAV(giant, ActorValue::kStamina) <= Sandwich::StaminaCost(giant, LightAttackStamina)) {
			NotifyWithSound(giant, "You're too tired to perform Light Butt Attack");
			return false;
		}

		return true;
	}

	bool VerifyHeavy(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (GetAV(giant, ActorValue::kStamina) <= Sandwich::StaminaCost(giant, HeavyAttackStamina)) {
			NotifyWithSound(giant, "You're too tired to perform Heavy Butt Attack");
			return false;
		}

		return true;
	}

	bool CanGrow(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichButtNode::Get(a_Ctx.Owner());
		return Runtime::HasPerkTeam(a_Ctx.Actor(), Runtime::PERK.GTSPerkButtCrushAug2) && !(state && state->Absorbing);
	}

	bool VerifyGrow(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!ButtCrush_IsAbleToGrow(giant, GetGrowthLimit(giant))) {
			NotifyWithSound(giant, "Your body can't grow any further");
			return false;
		}

		return true;
	}

	bool CanGrindStart(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichButtNode::Get(a_Ctx.Owner());
		return !Sandwich::Empty(a_Ctx.Actor()) && !(state && (state->Grinding || state->Absorbing));
	}

	bool CanGrindStop(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichButtNode::Get(a_Ctx.Owner());
		return state && state->Grinding;
	}

	//----------------------------------------------------------------------------------------------
	// Takes. Every one of these is paired: the giant's half and a _T to everyone in the slot.
	//----------------------------------------------------------------------------------------------

	void TakeStop(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_Stop");
	}

	void TakeLight(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_LightAttack");
	}

	void TakeHeavy(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_HeavyAttack");
	}

	void TakeGrow(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_Grow");
	}

	void TakeGrindStart(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_GrindStart");
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = true;
	}

	void TakeGrindStop(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_GrindStop");
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = false;
	}

	// Only the grind reaches this, when a tiny drops below the threshold. Suffocation is turned off
	// because the finisher does the killing from here.
	void TakeFinisher(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_Finisher");
		Sandwich::Suffocate(a_Ctx.Actor(), false, 1.0f);
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = false;
	}

	void TakeUnbirth(const ActionContext& a_Ctx) {
		Sandwich::TakeUnbirth(a_Ctx);
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Absorbing = true;
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnSitDownSoft(const ActionContext& a_Ctx) {
		Sandwich::Suffocate(a_Ctx.Actor(), true, 1.5f);
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Butt);
	}

	void OnLandSmall(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DamageAV(giant, ActorValue::kStamina, Sandwich::StaminaCost(giant, LightAttackStamina));
		Sandwich::ButtDamage(giant, Damage_ThighSandwich_Butt_Light, false);
	}

	// The first two hits of the finisher. They cost stamina and make noise but deal nothing.
	void OnLandMid(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DamageAV(giant, ActorValue::kStamina, Sandwich::StaminaCost(giant, LightAttackStamina));
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundFootstepHighHeels_2x, giant, 1.0f, "AnimObjectA");
		Rumbling::Once("ButtImpact", giant, Rumble_ThighSandwich_ButtImpact, 0.15f, "AnimObjectA", 0.0f);
	}

	void OnLandHeavy(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DamageAV(giant, ActorValue::kStamina, Sandwich::StaminaCost(giant, HeavyAttackStamina));
		Sandwich::ButtDamage(giant, Damage_ThighSandwich_Butt_Heavy, false);
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundFootstepHighHeels_2x, giant, 1.0f, "AnimObjectA");
		Rumbling::Once("ButtImpact", giant, Rumble_ThighSandwich_ButtImpact_Heavy, 0.15f, "AnimObjectA", 0.0f);
	}

	void OnLandFinisher(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Sandwich::ButtDamage(giant, Damage_ThighSandwich_Butt_Heavy, false, 4.0f);

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundFootstepHighHeels_2x, giant, 1.0f, "AnimObjectA");
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_ReachedSpeed, giant, 1.0f, "AnimObjectA");
		Rumbling::Once("ButtImpactFinisher", giant, Rumble_ThighSandwich_ButtImpact_Finisher, 0.15f, "AnimObjectA", 0.0f);

		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = false;
		Sandwich::ResetData(giant);
	}

	void OnLandFloor(const ActionContext& a_Ctx) {
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = false;
		Sandwich::ResetData(a_Ctx.Actor());
	}

	void OnGrindStart(const ActionContext& a_Ctx) {
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = true;
		Sandwich::ButtDamageOverTime(a_Ctx.Actor());
	}

	// The task ends itself once the graph stops reporting the grind, so this only agrees with it.
	void OnGrindStop(const ActionContext& a_Ctx) {
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Grinding = false;
	}

	// The butt branch reuses the butt crush growth clip, so it fires those annotations. ButtCrushNode
	// claims them too, but it is never the active node here, so this branch has to grow the giant.
	void OnGrowthStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Crush::ApplyGrowth(a_Ctx, "SandwichButtGrowth", 0.25f);

		Task_FacialEmotionTask_Moan(giant, 1.25f, "GrowthMoan", 0.15f);
		Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Growth, CooldownSource::Emotion_Voice_Long);

		Rumbling::Start("SandwichButtGrowth", giant, Rumble_ButtCrush_Growth, 0.30f);
	}

	void OnGrowthFinish(const ActionContext& a_Ctx) {
		Rumbling::Stop("SandwichButtGrowth", a_Ctx.Actor());
	}

	void OnTinyInserted(const ActionContext& a_Ctx) {
		ThighSandwichButtNode::GetOrAdd(a_Ctx.Owner()).Absorbing = true;
		Sandwich::OnUnbirthInserted(a_Ctx);
	}

	void OnDisableRune(const ActionContext& a_Ctx) {
		Sandwich::DisableRune(a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr ActionDef ActionTable[] = {
		// Back to the thigh loop. A handoff, so the tinies stay held across it.
		{ .Action = "Sandwich.Butt.Stop",       .Behaviour = "GTSBEH_ButtState_Stop",        .OnTaken = TakeStop,       .HandOff = ActionId::kThighSandwich, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.ButtBranch.Exit" },

		{ .Action = "Sandwich.Butt.Light",      .Behaviour = "GTSBEH_ButtState_LightAttack", .Guard = CanAct,        .Verify = VerifyLight, .OnTaken = TakeLight, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.ButtBranch.AttackLight" },
		{ .Action = "Sandwich.Butt.Heavy",      .Behaviour = "GTSBEH_ButtState_HeavyAttack", .Guard = CanAct,        .Verify = VerifyHeavy, .OnTaken = TakeHeavy, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.ButtBranch.AttackHeavy" },
		{ .Action = "Sandwich.Butt.Grow",       .Behaviour = "GTSBEH_ButtState_Grow",        .Guard = CanGrow,       .Verify = VerifyGrow,  .OnTaken = TakeGrow,  .Cooldown = 0.35f, .Input = "Action.ThighSandwich.ButtBranch.Grow" },

		{ .Action = "Sandwich.Butt.GrindStart", .Behaviour = "GTSBEH_ButtState_GrindStart",  .Guard = CanGrindStart, .OnTaken = TakeGrindStart, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.ButtBranch.GrindStart" },
		{ .Action = "Sandwich.Butt.GrindStop",  .Behaviour = "GTSBEH_ButtState_GrindStop",   .Guard = CanGrindStop,  .OnTaken = TakeGrindStop,  .Cooldown = 0.30f, .Input = "Action.ThighSandwich.ButtBranch.GrindStop" },

		// Offered from the thigh loop as well. The graph allows it from both idles on purpose, and the
		// legacy code only ever wired this one.
		{ .Action = "Sandwich.UB",              .Behaviour = Sandwich::BEH_UNBIRTH,          .Guard = CanUnbirth,    .OnTaken = TakeUnbirth, .Cooldown = 0.50f, .Input = "Action.ThighSandwich.ButtBranch.Unbirth" },

		// No key and no guard: the grind asks for this itself once a tiny is low enough. The graph only
		// offers it from the grind state, so it cannot be reached any other way.
		{ .Action = "Sandwich.Butt.Finisher",   .Behaviour = "GTSBEH_ButtState_Finisher",    .OnTaken = TakeFinisher },

		{ .Action = "Sandwich.Cancel",          .Behaviour = "",                             .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_TSB_SitDownSoft",     .Handler = OnSitDownSoft },
		{ .Tag = "GTS_TSB_LandSmall",       .Handler = OnLandSmall },
		{ .Tag = "GTS_TSB_LandMid",         .Handler = OnLandMid },
		{ .Tag = "GTS_TSB_LandHeavy",       .Handler = OnLandHeavy },
		{ .Tag = "GTS_TSB_LandFinisher",    .Handler = OnLandFinisher },
		{ .Tag = "GTS_TSB_LandFloor",       .Handler = OnLandFloor },
		{ .Tag = "GTS_TSB_DOT_Start",       .Handler = OnGrindStart },
		{ .Tag = "GTS_TSB_DOT_Stop",        .Handler = OnGrindStop },
		{ .Tag = "GTS_TSB_TinyInserted",    .Handler = OnTinyInserted },
		{ .Tag = "GTS_TSB_TinyKill",        .Handler = Sandwich::OnUnbirthKill, .ReleasesPartners = true },
		{ .Tag = "GTS_TSB_ResetData",       .Handler = Sandwich::OnResetData },
		{ .Tag = "GTSButtCrush_GrowthStart",      .Handler = OnGrowthStart, .Shared = true },
		{ .Tag = "GTSBEH_ButtCrush_GrowthFinish", .Handler = OnGrowthFinish, .Shared = true },
		{ .Tag = "GTSSandwich_DisableRune", .Handler = OnDisableRune, .Shared = true },

		// Fire, carry no work of their own, and are listed so the trace does not report them as
		// belonging to nobody.
		{ .Tag = "GTS_TSB_SitDown" },
		{ .Tag = "GTS_TSB_TinyThigh" },
		{ .Tag = "GTS_TSB_Stand" },
		{ .Tag = "GTS_TSB_Fall" },
		{ .Tag = "GTS_TSB_DustButt" },
		{ .Tag = "GTS_TSB_TinyPushStart" },
		{ .Tag = "GTS_TSB_TinyFullyIn" },
		{ .Tag = "GTS_TSB_UBStart" },
		{ .Tag = "GTS_TSB_UBEnjoy" },
		{ .Tag = "GTS_TSB_UBEnd" },
		{ .Tag = "GTS_TSB_UBAbsorb" },
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> ThighSandwichButtNode::Signature() const { return ::Signature; }
	std::string_view ThighSandwichButtNode::AbortSignal() const { return Sandwich::BEH_ABORT; }
	std::span<const std::string_view> ThighSandwichButtNode::ExitSignals() const { return ::ExitSignals; }
	std::span<const ActionDef> ThighSandwichButtNode::Actions() const { return ::ActionTable; }
	std::span<const AnnotationDef> ThighSandwichButtNode::Annotations() const { return ::Annotations; }

	ThighSandwichButtNode::State* ThighSandwichButtNode::Get(RE::FormID a_Owner) { return m_State.Find(a_Owner); }
	ThighSandwichButtNode::State& ThighSandwichButtNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	void ThighSandwichButtNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		// Held by the thigh loop already. Re-read rather than rebuilt, because the handoff does not
		// run the intro that gathers them.
		Sandwich::Claim(a_Ctx);
	}

	void ThighSandwichButtNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		// Cancelled on every way out, the handoff included. The task only stops itself once the graph
		// stops reporting the grind, and that variable is cleared seconds after the branch has visibly
		// ended - long enough to keep dealing grind damage well into the thigh loop.
		TaskManager::Cancel(std::format("SandwichButtDOT_{}", owner));

		// Going back to the thigh loop. The group continues, so its actors, camera and rune stay.
		if (KeepsState(a_Reason)) {
			m_State.Forget(owner);
			return;
		}

		if (giant) {

			ManageCamera(giant, false, CameraTracking::Butt);
			Sandwich::DisableRune(giant);
			Sandwich::ResetData(giant);
		}

		m_State.Forget(owner);
	}

	void ThighSandwichButtNode::OnForget(RE::FormID a_Owner) { m_State.Forget(a_Owner); }
	void ThighSandwichButtNode::OnReset() { m_State.Clear(); }

	std::string_view ThighSandwichButtNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);

		if (!state) {
			return "";
		}

		if (state->Absorbing) {
			return "Unbirth";
		}

		return state->Grinding ? "Grinding" : "Butt";
	}
}
