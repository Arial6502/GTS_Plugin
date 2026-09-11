#include "Actions/Nodes/ThighSandwich/ThighSandwichNode.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/PlayerTarget.hpp"
#include "Config/Keybinds.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/ThighSandwich/SandwichCommon.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Controllers/ThighSandwichController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  The group is one root state with two branches, told apart by GTS_IsButtState. This node is the
  thigh loop; ThighSandwichButtNode is the other. Control passes between them by handoff, and
  neither ends the group when it does.

  ----- Thigh Sandwich [("GTSBEH_ThighSandwich_Start")]
  [INTRO]
    EndAnimatedCamera        [NON GTS]
    EndAnimatedCameraDelta   [NON GTS]
    GTSSandwich_MoveBody_start   -> the tinies become the sandwich and are taken hold of here
    GTSSandwich_EnableRune
    GTSSandwich_SitStart          [NOT REGISTERED] empty in the legacy too
    GTSSandwich_MoveBody_end
    GTSSandwich_MoveLL_start      -> the intro closes the legs once on its way in
    GTSSandwich_ThighImpact_Initial
    GTSSandwich_MoveLL_end
    GTSSandwich_ThighLoop_Enter   [NOT REGISTERED] empty in the legacy too
    GTSBEH_Next                   -> settles into the idle loop
  [LIGHT ATTACK ("GTSBEH_ThighSandwich_Attack")]
    GTSSandwich_MoveLL_start
    GTSSandwich_ThighAttack_start [NOT REGISTERED] empty in the legacy too
    GTSSandwich_ThighImpact
    GTSSandwich_MoveLL_end
    GTSBEH_Next                   -> back to the idle loop
  [HEAVY ATTACK ("GTSBEH_ThighSandwich_Attack_H")]
    GTSSandwich_MoveLL_start_H
    GTSSandwich_ThighAttack_start_H  [NOT REGISTERED]
    GTSSandwich_ThighImpact_H
    GTSSandwich_MoveLL_end_H
    GTSBEH_Next                   -> back to the idle loop
  [EXIT ("GTSBEH_ThighSandwich_ExitLoop")]
    GTSSandwich_ThighLoop_Exit    -> the tinies are let go and pushed clear
    GTSSandwich_DisableRune
    GTSSandwich_DropDown
    GTSSandwich_FootImpact
    AnimObjectUnequip        [NON GTS]
    GTSSandwich_ExitAnim
  [CLEAN UP]
    GTSBEH_Exit
    GTS_ResetVars            [NON GTS]
    GTSSandwich_DisableRune       -> fires a second time here
    GTSBEH_Camera_Reset      [NON GTS]
    GTSBeh_Dummy             [NON GTS]

  **Info: the intro plays MoveLL_start / ThighImpact_Initial / MoveLL_end before the loop, so the
  **first close of the legs is part of entering and deals no damage.
  **The unbirth is reachable from this loop as well as from the butt branch, and takes the same
  **route as it does there. See ThighSandwichButtNode for that sequence.
*/

/*
   -------------------------------------------------------------------------- ANIMATION VARS
   ----- Thigh Sandwich [("GTSBEH_ThighSandwich_Start")]
   [GET IMMEDIATLY SET AT ANIMATION START TRIGGER]
     GTS_Busy                -> TRUE
     GTS_IsThighSandwiching  -> TRUE
     GTS_Sitting             -> TRUE
   (NON GTS)
     TDM_Dodge               -> TRUE
     TDM_LockRotation11      -> TRUE
     bAnimationDriven        -> TRUE
     bHumanoidFootIKEnable   -> FALSE
     bNoStagger              -> TRUE
   [ONE FRAME LATER]
     GTS_Ready               -> FALSE
   [DURING THE REST OF THE ANIMATION]
     **NO CHANGES**
   [HANDING OVER TO THE BUTT BRANCH]
     GTS_IsButtState         -> TRUE, set by the animation, not by the DLL
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_IsButtState is the only thing that separates the two branches, so it is in both
   **signatures with opposite values. The animation owns it, which is why neither node writes it.
*/

using namespace GTS;

namespace {

	using namespace GTS::Actions;
	using State = ThighSandwichNode::State;

	bool ThighSandwitchCondition_Start() {
		auto target = PlayerCharacter::GetSingleton();

		if (!CanDoActionBasedOnQuestProgress(target, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}
		if (AnimationVars::General::IsGTSBusy(target)) {
			return false;
		}
		if (AnimationVars::Crawl::IsCrawling(target)) {
			return false;
		}

		return true;
	}

	constexpr std::string_view RNODE = "NPC R Foot [Rft ]";
	constexpr std::string_view LNODE = "NPC L Foot [Lft ]";

	constexpr std::string_view TASKID_DRAIN = "StaminaDrain_Sandwich";
	constexpr std::string_view TASKID_DRAIN_IDLE = "StaminaDrain_Sandwich_Idle";

	constexpr std::string_view RUMBLE_BODY = "BodyRumble";
	constexpr std::string_view RUMBLE_LIGHT = "LLSandwich";
	constexpr std::string_view RUMBLE_HEAVY = "LLSandwichHeavy";

	// GTS_IsThighSandwiching covers the whole group. GTS_IsButtState is what separates the two
	// branches, so it is in both signatures with opposite values.
	constexpr GraphExpect Signature[] = {
		{ "GTS_IsThighSandwiching", true  },
		{ "GTS_IsButtState",        false },
	};

	constexpr std::string_view ExitSignals[] = {
		"GTS_TSB_ResetData",
		"GTSSandwich_ThighLoop_Exit",
		"GTSSandwich_DropDown",
		"GTSSandwich_ExitAnim",
		"GTSBEH_Exit",
	};

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	bool CanStart(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}

		if (AnimationVars::General::IsGTSBusy(giant) || a_Ctx.Stance() == Stance::kCrawl) {
			return false;
		}

		return true;
	}

	// Nothing else is offered once the unbirth is running: it owns the machine until it ends, and the
	// butt branch already refuses on the same grounds.
	bool CanAttack(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichNode::Get(a_Ctx.Owner());
		return a_Ctx.Actor() && !(state && state->Absorbing);
	}

	bool VerifyLight(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float cost = Sandwich::StaminaCost(giant, 20.0f);

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, "You're too tired to perform thigh sandwich");
			return false;
		}

		return true;
	}

	bool VerifyHeavy(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float cost = Sandwich::StaminaCost(giant, 35.0f);

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, "You're too tired to perform strong thigh sandwich");
			return false;
		}

		return true;
	}

	bool CanExit(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichNode::Get(a_Ctx.Owner());
		return !IsFreeCameraEnabled() && !(state && state->Absorbing);
	}

	// The butt branch needs someone to sit on. The legacy only checked this on the input, so an AI or
	// a console request could enter it with an empty sandwich and stand back up immediately.
	bool CanButtStart(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichNode::Get(a_Ctx.Owner());
		return !Sandwich::Empty(a_Ctx.Actor()) && !(state && state->Absorbing);
	}

	//----------------------------------------------------------------------------------------------
	// Takes
	//----------------------------------------------------------------------------------------------

	void TakeAttack(const ActionContext& a_Ctx) {
		ThighSandwichNode::GetOrAdd(a_Ctx.Owner()).Heavy = false;
	}

	void TakeAttackHeavy(const ActionContext& a_Ctx) {
		ThighSandwichNode::GetOrAdd(a_Ctx.Owner()).Heavy = true;
	}

	// The unbirth is offered here as well as in the butt branch: the graph allows it from both idles,
	// and the legacy code only ever wired the butt one.
	bool CanUnbirth(const ActionContext& a_Ctx) {
		const State* state = ThighSandwichNode::Get(a_Ctx.Owner());
		return !Sandwich::Empty(a_Ctx.Actor()) && !(state && state->Absorbing);
	}

	void TakeUnbirth(const ActionContext& a_Ctx) {
		Sandwich::TakeUnbirth(a_Ctx);
		ThighSandwichNode::GetOrAdd(a_Ctx.Owner()).Absorbing = true;
	}

	void OnUnbirthInserted(const ActionContext& a_Ctx) {
		ThighSandwichNode::GetOrAdd(a_Ctx.Owner()).Absorbing = true;
		Sandwich::OnUnbirthInserted(a_Ctx);
	}

	// Both halves of the pair start together, the way every other paired action in the group does.
	void TakeButtStart(const ActionContext& a_Ctx) {
		a_Ctx.SendToHeld(Sandwich::Slot, "GTSBEH_T_ButtState_Start");
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnMoveBodyStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		auto& data = ThighSandwichController::GetSingleton().GetSandwichingData(giant);

		// The controller leaves the chosen targets on the giant's transient until the animation asks
		// for them, so this is where they become the sandwich.
		if (auto* transient = Transient::GetActorData(giant)) {
			for (auto* tiny : transient->toSandwich) {
				if (tiny) {
					data.AddTiny(tiny);
				}
			}
			transient->toSandwich.clear();
		}

		Sandwich::Claim(a_Ctx);

		for (auto* tiny : Sandwich::Tinies(giant)) {
			AllowToBeCrushed(tiny, false);
			SetBeingHeld(tiny, true);
			DisableCollisions(tiny, giant);
		}

		data.MoveActors(true);
		Sandwich::Suffocate(giant, false);
		Sandwich::StartLegRumble(RUMBLE_BODY, giant, 0.5f, 0.25f);
	}

	void OnEnableRune(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Thigh_Sandwich);
		Runtime::CastSpell(a_Ctx.Actor(), a_Ctx.Actor(), Runtime::SPEL.GTSSpellThighRune);
	}

	void OnMoveBodyEnd(const ActionContext& a_Ctx) {
		Sandwich::StopLegRumble(RUMBLE_BODY, a_Ctx.Actor());
	}

	void OnMoveLegStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		a_Ctx.SetCanEditAnimSpeed(true);
		a_Ctx.SetAnimSpeed(giant->IsPlayerRef() ? 1.66f : 1.66f + GetRandomBoost());

		Sandwich::Suffocate(giant, false);
		Sandwich::StartLegRumble(RUMBLE_LIGHT, giant, 0.10f, 0.12f);
		DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, true, 1.0f);
	}

	void OnMoveLegStartHeavy(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		a_Ctx.SetCanEditAnimSpeed(true);
		a_Ctx.SetAnimSpeed(giant->IsPlayerRef() ? 1.66f : 1.66f + GetRandomBoost());

		Sandwich::Suffocate(giant, false);
		Sandwich::StartLegRumble(RUMBLE_HEAVY, giant, 0.15f, 0.15f);
		DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, true, 2.5f);
	}

	void CloseThighs(const ActionContext& a_Ctx, float a_Damage, bool a_Attacked, float a_Rumble, float a_Drain, std::string_view a_SoundNode, float a_Volume) {

		auto* giant = a_Ctx.Actor();

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundThighSandwichImpact, giant, a_Volume, a_SoundNode);
		Sandwich::Suffocate(giant, true, 1.0f);

		ThighSandwichNode::GetOrAdd(a_Ctx.Owner()).Closed = true;

		for (auto* tiny : Sandwich::Tinies(giant)) {

			if (a_Damage > 0.0f) {
				Sandwich::ThighDamage(giant, tiny, a_Damage);
			}

			if (a_Attacked) {
				tiny->Attacked(giant);
			}

			// Damage can have killed and removed them, and a disintegrated tiny has no graph left.
			if (tiny->Is3DLoaded()) {
				ActionRegistry::Notify(tiny, "ragdoll");
			}

			AllowToBeCrushed(tiny, true);
		}

		Rumbling::Once("ThighImpact", giant, a_Rumble, 0.15f, "AnimObjectA", 0.0f);
		DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, false, a_Drain);
	}

	void OnThighImpactInitial(const ActionContext& a_Ctx) {
		// The first close of the legs deals nothing, so a tiny already near death is not killed by
		// the wind-up before the attack it belongs to has landed.
		CloseThighs(a_Ctx, 0.0f, false, Rumble_ThighSandwich_ThighImpact, 1.0f, "AnimObjectB", 1.0f);
	}

	void OnThighImpact(const ActionContext& a_Ctx) {
		CloseThighs(a_Ctx, Damage_ThighSandwich_Impact_Light, false, Rumble_ThighSandwich_ThighImpact, 1.0f, "AnimObjectB", 1.0f);
	}

	void OnThighImpactHeavy(const ActionContext& a_Ctx) {
		CloseThighs(a_Ctx, Damage_ThighSandwich_Impact_Heavy, true, Rumble_ThighSandwich_ThighImpact_Heavy, 2.5f, "AnimObjectA", 1.2f);
	}

	void OnMoveLegEnd(const ActionContext& a_Ctx) {

		a_Ctx.SetCanEditAnimSpeed(false);
		a_Ctx.SetAnimSpeed(1.0f);

		// Both tags exist and only one of them is running. Stopping the wrong one leaves the other
		// rumbling for the rest of the sandwich, which is what the two separate end events were for.
		const State* state = ThighSandwichNode::Get(a_Ctx.Owner());
		Sandwich::StopLegRumble(state && state->Heavy ? RUMBLE_HEAVY : RUMBLE_LIGHT, a_Ctx.Actor());

		ThighSandwichNode::GetOrAdd(a_Ctx.Owner()).Closed = false;
	}

	void OnThighLoopExit(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, false, CameraTracking::Thigh_Sandwich);
		Sandwich::Suffocate(giant, false);

		for (auto* tiny : Sandwich::Tinies(giant)) {
			SetBeingHeld(tiny, false);
			PushActorAway(giant, tiny, 1.0f);
			EnableCollisions(tiny);
		}

		ThighSandwichController::GetSingleton().GetSandwichingData(giant).MoveActors(false);
		DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, false, 2.5f);
	}

	void OnDropDown(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (auto* tiny : Sandwich::Tinies(giant)) {
			AllowToBeCrushed(tiny, true);
		}

		Sandwich::ResetData(giant);

		DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, false, 2.5f);
		DrainStamina(giant, TASKID_DRAIN_IDLE, Runtime::PERK.GTSPerkThighAbilities, false, 0.25f);
	}

	void OnFootImpact(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Thighs(giant);

		DoFootstepSound(giant, 1.05f, FootEvent::Right, RNODE);
		DoFootstepSound(giant, 1.05f, FootEvent::Left, LNODE);
		DoDustExplosion(giant, 2.0f, FootEvent::Right, RNODE);
		DoDustExplosion(giant, 2.0f, FootEvent::Left, LNODE);

		DoDamageEffect(giant, Damage_ThighSandwich_FallDownImpact * perk, Radius_ThighSandwich_FootFallDown, 10, 0.20f, FootEvent::Right, 1.0f, DamageSource::CrushedRight);
		DoDamageEffect(giant, Damage_ThighSandwich_FallDownImpact * perk, Radius_ThighSandwich_FootFallDown, 10, 0.20f, FootEvent::Left, 1.0f, DamageSource::CrushedLeft);

		DoLaunch(giant, 0.85f * perk, 3.2f, FootEvent::Right);
		DoLaunch(giant, 0.85f * perk, 3.2f, FootEvent::Left);

		float power = Rumble_ThighSandwich_DropDown / 2 * GetHighHeelsBonusDamage(giant, true);

		if (TinyCalamityActive(giant)) {
			power *= 2.0f;
		}

		Rumbling::Once("ThighDropDown_R", giant, power, 0.10f, RNODE, 0.0f);
		Rumbling::Once("ThighDropDown_L", giant, power, 0.10f, LNODE, 0.0f);
	}

	void OnDisableRune(const ActionContext& a_Ctx) {
		Sandwich::DisableRune(a_Ctx.Actor());
	}

	void OnExitAnim(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (auto* tiny : Sandwich::Tinies(giant)) {
			SetBeingHeld(tiny, false);
			EnableCollisions(tiny);
		}

		DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, false, 2.5f);
		ManageCamera(giant, false, CameraTracking::Thigh_Sandwich);
	}

	void OnGraphExit(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Thigh_Sandwich);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr EntryDef Entries[] = {
		{ .Action = "Sandwich.Enter", .Behaviour = "GTSBEH_ThighSandwich_Start", .Guard = CanStart, .BlockedBy = kBlockedByHand },
	};

	constexpr ActionDef ActionTable[] = {
		{ .Action = "Sandwich.Attack",      .Behaviour = "GTSBEH_ThighSandwich_Attack",   .Guard = CanAttack,     .Verify = VerifyLight, .OnTaken = TakeAttack,      .Cooldown = 0.30f, .Input = "Action.ThighSandwich.AttackLight" },
		{ .Action = "Sandwich.AttackHeavy", .Behaviour = "GTSBEH_ThighSandwich_Attack_H", .Guard = CanAttack,     .Verify = VerifyHeavy, .OnTaken = TakeAttackHeavy, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.AttackHeavy" },
		{ .Action = "Sandwich.Exit",        .Behaviour = "GTSBEH_ThighSandwich_ExitLoop", .Guard = CanExit,       .Exits = true, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.Exit" },

		// Hands the group over to the butt branch. 
		{ .Action = "Sandwich.Butt.Start",  .Behaviour = "GTSBEH_ButtState_Start",        .Guard = CanButtStart,  .OnTaken = TakeButtStart, .HandOff = ActionId::kThighSandwichButt, .Cooldown = 0.30f, .Input = "Action.ThighSandwich.ButtBranch.Enter" },
		{ .Action = "Sandwich.UB",          .Behaviour = Sandwich::BEH_UNBIRTH,           .Guard = CanUnbirth,    .OnTaken = TakeUnbirth, .Cooldown = 0.50f, .Input = "Action.ThighSandwich.ButtBranch.Unbirth" },

		// Unguarded, no key. For a cancel from outside the group.
		{ .Action = "Sandwich.Cancel",      .Behaviour = "",                              .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSSandwich_MoveBody_start",     .Handler = OnMoveBodyStart },
		{ .Tag = "GTSSandwich_EnableRune",         .Handler = OnEnableRune },
		{ .Tag = "GTSSandwich_MoveBody_end",       .Handler = OnMoveBodyEnd },
		{ .Tag = "GTSSandwich_MoveLL_start",       .Handler = OnMoveLegStart },
		{ .Tag = "GTSSandwich_MoveLL_start_H",     .Handler = OnMoveLegStartHeavy },
		{ .Tag = "GTSSandwich_ThighImpact_Initial",.Handler = OnThighImpactInitial },
		{ .Tag = "GTSSandwich_ThighImpact",        .Handler = OnThighImpact },
		{ .Tag = "GTSSandwich_ThighImpact_H",      .Handler = OnThighImpactHeavy },
		{ .Tag = "GTSSandwich_MoveLL_end",         .Handler = OnMoveLegEnd },
		{ .Tag = "GTSSandwich_MoveLL_end_H",       .Handler = OnMoveLegEnd },
		{ .Tag = "GTSSandwich_ThighLoop_Exit",     .Handler = OnThighLoopExit },
		{ .Tag = "GTSSandwich_DropDown",           .Handler = OnDropDown },
		{ .Tag = "GTSSandwich_FootImpact",         .Handler = OnFootImpact },
		{ .Tag = "GTSSandwich_DisableRune",        .Handler = OnDisableRune },
		{ .Tag = "GTSSandwich_ExitAnim",           .Handler = OnExitAnim },
		{ .Tag = "GTSBEH_Exit",                    .Handler = OnGraphExit, .Shared = true },
		{ .Tag = "GTSBEH_Next",                    .Shared = true },

		// The unbirth path, shared with the butt branch because the graph offers it from both.
		{ .Tag = "GTS_TSB_TinyInserted",           .Handler = OnUnbirthInserted, .Shared = true },
		{ .Tag = "GTS_TSB_TinyKill",               .Handler = Sandwich::OnUnbirthKill, .Shared = true },
		{ .Tag = "GTS_TSB_ResetData",              .Handler = Sandwich::OnResetData, .Shared = true },
		{ .Tag = "GTS_TSB_TinyPushStart",          .Shared = true },
		{ .Tag = "GTS_TSB_TinyFullyIn",            .Shared = true },
		{ .Tag = "GTS_TSB_UBStart",                .Shared = true },
		{ .Tag = "GTS_TSB_UBEnjoy",                .Shared = true },
		{ .Tag = "GTS_TSB_UBEnd",                  .Shared = true },
		{ .Tag = "GTS_TSB_UBAbsorb",               .Shared = true },
	};

	//----------------------------------------------------------------------------------------------
	// Input
	//----------------------------------------------------------------------------------------------

	void StartEvent(const ManagedInputEvent&) {

		if (PlayerTarget::Engaged("Action.ThighSandwich.Start")) {
			PlayerTarget::Perform("Sandwich.Enter");
			return;
		}

		auto* player = PlayerCharacter::GetSingleton();
		auto& controller = ThighSandwichController::GetSingleton();

		for (auto* prey : controller.GetSandwichTargetsInFront(player, 1)) {

			controller.StartSandwiching(player, prey);

			if (auto* node = find_node(player, "GiantessRune", false)) {
				node->local.scale = 0.01f;
				update_node(node);
			}
		}
	}

}

namespace GTS::Actions {

	std::span<const GraphExpect> ThighSandwichNode::Signature() const { return ::Signature; }
	std::string_view ThighSandwichNode::AbortSignal() const { return Sandwich::BEH_ABORT; }
	std::span<const std::string_view> ThighSandwichNode::ExitSignals() const { return ::ExitSignals; }
	std::span<const EntryDef> ThighSandwichNode::Entries() const { return ::Entries; }
	std::span<const ActionDef> ThighSandwichNode::Actions() const { return ::ActionTable; }
	std::span<const AnnotationDef> ThighSandwichNode::Annotations() const { return ::Annotations; }

	ThighSandwichNode::State* ThighSandwichNode::Get(RE::FormID a_Owner) { return m_State.Find(a_Owner); }
	ThighSandwichNode::State& ThighSandwichNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	bool ThighSandwichNode::CanEnter(const EntryContext&) const { return true; }

	void ThighSandwichNode::RegisterInput() {
		Keybinds::NoteBindKind("Action.ThighSandwich.Start", true);
		InputManager::RegisterInputEvent("Action.ThighSandwich.Start", StartEvent, ThighSandwitchCondition_Start);
	}

	bool ThighSandwichNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view, bool a_Explain) const {

		if (!CanDoActionBasedOnQuestProgress(a_Target, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}

		auto& controller = ThighSandwichController::GetSingleton();

		controller.AllowMessage(a_Explain);
		const bool started = ThighSandwichController::StartSandwiching(a_Actor, a_Target);
		controller.AllowMessage(false);

		return started;
	}

	void ThighSandwichNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		// Coming back from the butt branch the tinies are already held and the intro will not run
		// again, so the slot is re-read rather than rebuilt.
		Sandwich::Claim(a_Ctx);
	}

	// A handoff is the group changing branch, so nothing that belongs to the group is undone here.
	// The registry already keeps its own cleanup out of a handoff for the same reason.
	void ThighSandwichNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		// Stopped on every way out, the handoff included. Handing over mid attack means the matching
		// MoveLL_end never arrives, and these are sustained rumbles that run until they are stopped.
		if (giant) {
			Sandwich::StopLegRumble(RUMBLE_BODY, giant);
			Sandwich::StopLegRumble(RUMBLE_LIGHT, giant);
			Sandwich::StopLegRumble(RUMBLE_HEAVY, giant);
		}

		if (KeepsState(a_Reason)) {
			return;
		}

		if (giant) {

			DrainStamina(giant, TASKID_DRAIN, Runtime::PERK.GTSPerkThighAbilities, false, 2.5f);
			DrainStamina(giant, TASKID_DRAIN_IDLE, Runtime::PERK.GTSPerkThighAbilities, false, 0.25f);

			Sandwich::DisableRune(giant);
			Sandwich::ResetData(giant);

			ThighSandwichController::GetSingleton().GetSandwichingData(giant).MoveActors(false);
		}

		m_State.Forget(owner);
	}

	void ThighSandwichNode::OnForget(RE::FormID a_Owner) { m_State.Forget(a_Owner); }
	void ThighSandwichNode::OnReset() { m_State.Clear(); }

	std::string_view ThighSandwichNode::StateName(RE::FormID a_Owner) const {

		const State* state = m_State.Find(a_Owner);

		if (!state) {
			return "";
		}

		if (state->Closed) {
			return state->Heavy ? "Heavy" : "Light";
		}

		return "Thighs";
	}
}
