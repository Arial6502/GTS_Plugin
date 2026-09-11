#include "Actions/Nodes/Stomp/TrampleNode.hpp"
#include "Actions/Nodes/Stomp/StompCommon.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Audio/Footstep.hpp"
#include "Managers/Audio/Stomps.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/Rumble.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  The trample loop, which is a different animation state from the trample intro even though one runs
  straight into the other. The intro belongs to StompNode and asserts GTS_IsStomping; this asserts
  GTS_IsTrampling instead, so the two cannot be one node.

  Reached only by handoff. StompNode's Stomp.TrampleL / R sends GTSBEH_Trample_Start_L / R when the
  proximity check finds someone still under the foot after the first hit.

  Captured on the right side. Times are from one run and are illustrative only.

  [LOOP, about 1.6s a cycle and six cycles in the capture]
    GTS_Trample_Leg_Raise_L / R -> marks the foot busy, which is also an intro tag
    GTS_Trample_Leg_Lower_L / R [NOT HANDLED, and it was not handled before the port either]
    GTS_Trample_Impact_L / R    -> one stamp, +1.47 after the raise
  [FINISH]
    GTS_Trample_Finisher_L / R -> the last and much heavier hit
    GTS_Trample_Cam_End_L / R  -> camera released, stamina drain stopped, +0.87
    GTSBEH_Exit                -> +2.13, after the node has already asked to leave

  **Info: the leg raise is declared here and on StompNode. The intro sends it once and the loop sends
  **it every cycle, and the legacy file had one handler covering both because it had no intro and loop
  **split.

  **Info: kUnannounced. GTS_Trample_Cam_End is the last thing the loop says and it is not reliably an
  **ending, so the signature draining is the exit.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH WHEN THE LOOP STARTS]
     GTS_IsTrampling      -> TRUE   (graph owned, and this node's signature)
     GTS_IsUnderTrampling -> TRUE for the understomp variant only
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_IsStomping belongs to the intro and drops when the loop takes over, which is what the
   **handoff is waiting for.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;
	using namespace GTS::Actions::Stomping;

	void SpendStamina(Actor* a_Giant, float a_Amount) {
		DamageAV(a_Giant, ActorValue::kStamina, a_Amount * GetWasteMult(a_Giant));
	}

	// One stamp of the loop.
	void Stamp(Actor* a_Giant, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.15f * 1.25f : 1.15f;

		Rumbling::Once(a_Rumble, a_Giant, Rumble_Trample_Stage2 * smt * GetHighHeelsBonusDamage(a_Giant, true), 0.0f, a_Node, 1.1f);
		DoDamageEffect(a_Giant, Damage_Trample_Repeat * perk, Radius_Trample_Repeat, 1, 0.12f, a_Event, 1.10f, a_Source);
		StompManager::PlayNewOrOldStomps(a_Giant, 1.0f, a_Event, a_Node, false);
		DoDustExplosion(a_Giant, dust * smt, a_Event, a_Node);
		DoLaunch(a_Giant, 0.85f * perk, 3.8f * perk, a_Event);
		SpendStamina(a_Giant, 30.0f);

		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	// The last hit, which is where most of the damage is.
	void Finisher(Actor* a_Giant, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.65f * 1.25f : 1.65f;

		const float augment = PerkHandler::Perks_Cataclysmic_EmpowerStomp(a_Giant);
		const bool stacks = PerkHandler::Perks_Cataclysmic_HasStacks(a_Giant);

		DoDamageEffect(a_Giant, Damage_Trample_Finisher * perk * augment, Radius_Trample_Finisher, 1, 0.25f, a_Event, 0.85f, a_Source);
		DoLaunch(a_Giant, 1.25f * perk * augment, 5.0f * perk * augment, a_Event);
		StompManager::PlayNewOrOldStomps(a_Giant, 1.15f, a_Event, a_Node, true);
		Rumbling::Once(a_Rumble, a_Giant, Rumble_Trample_Stage3 * smt * GetHighHeelsBonusDamage(a_Giant, true), 0.0f, a_Node, 1.2f);

		if (!stacks) {
			DoDustExplosion(a_Giant, dust * smt, a_Event, a_Node);
		}
		else {
			for (const float exp : { 1.0f, 0.75f, 0.5f }) {
				DoDustExplosion(a_Giant, dust * smt * augment * exp, a_Event, a_Node);
			}
		}

		SpendStamina(a_Giant, 100.0f);

		FootStepManager::DoStrongSounds(a_Giant, 1.25f, a_Node);
		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnImpact(const ActionContext& a_Ctx, bool a_Right) {

		Stamp(a_Ctx.Actor(), a_Right,
			a_Right ? FootEvent::Right : FootEvent::Left,
			a_Right ? DamageSource::CrushedRight : DamageSource::CrushedLeft,
			a_Right ? NODE_FOOT_R : NODE_FOOT_L,
			a_Right ? "Trample2_R" : "Trample2_L");

		a_Ctx.SetCanEditAnimSpeed(false);

		if (a_Ctx.AnimSpeed() == 1.0f) {
			a_Ctx.SetAnimSpeed(1.15f);
		}
	}

	void OnImpactR(const ActionContext& a_Ctx) { OnImpact(a_Ctx, true); }
	void OnImpactL(const ActionContext& a_Ctx) { OnImpact(a_Ctx, false); }

	// The loop sends the intro's leg raise tag once per cycle. GTSManager skips the idle leg damage
	// on whichever foot is marked busy, so without this the raised foot keeps pushing tinies away.
	void OnLegRaise(const ActionContext& a_Ctx, bool a_Right) {
		SetBusyFoot(a_Ctx.Actor(), a_Right ? BusyFoot::RightFoot : BusyFoot::LeftFoot);
	}

	void OnLegRaiseR(const ActionContext& a_Ctx) { OnLegRaise(a_Ctx, true); }
	void OnLegRaiseL(const ActionContext& a_Ctx) { OnLegRaise(a_Ctx, false); }

	void OnFinisherR(const ActionContext& a_Ctx) {
		Finisher(a_Ctx.Actor(), true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "Trample2_R");
	}

	void OnFinisherL(const ActionContext& a_Ctx) {
		Finisher(a_Ctx.Actor(), false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "Trample2_L");
	}

	void OnCamEnd(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();

		ManageCamera(giant, false, a_Right ? CameraTracking::R_Foot : CameraTracking::L_Foot);
		DrainStamina(giant, "StaminaDrain_Trample", Runtime::PERK.GTSPerkDestructionBasics, false, 0.6f);

		a_Ctx.SetCanEditAnimSpeed(false);
		a_Ctx.SetAnimSpeed(1.0f);
		a_Ctx.RequestExit();
	}

	void OnCamEndR(const ActionContext& a_Ctx) { OnCamEnd(a_Ctx, true); }
	void OnCamEndL(const ActionContext& a_Ctx) { OnCamEnd(a_Ctx, false); }

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsTrampling", true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_Trample_Leg_Raise_L", .Handler = OnLegRaiseL },
		{ .Tag = "GTS_Trample_Leg_Raise_R", .Handler = OnLegRaiseR },
		{ .Tag = "GTS_Trample_Impact_L",   .Handler = OnImpactL },
		{ .Tag = "GTS_Trample_Impact_R",   .Handler = OnImpactR },
		{ .Tag = "GTS_Trample_Finisher_L", .Handler = OnFinisherL },
		{ .Tag = "GTS_Trample_Finisher_R", .Handler = OnFinisherR },
		{ .Tag = "GTS_Trample_Cam_End_L",  .Handler = OnCamEndL },
		{ .Tag = "GTS_Trample_Cam_End_R",  .Handler = OnCamEndR },
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> TrampleNode::Signature() const     { return ::Signature; }
	ExitPolicy TrampleNode::Exit() const                            { return ExitPolicy::kUnannounced; }
	std::span<const AnnotationDef> TrampleNode::Annotations() const { return ::Annotations; }

	void TrampleNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return;
		}

		ManageCamera(giant, false, CameraTracking::R_Foot);
		ManageCamera(giant, false, CameraTracking::L_Foot);
		DrainStamina(giant, "StaminaDrain_Trample", Runtime::PERK.GTSPerkDestructionBasics, false, 0.6f);
		SetBusyFoot(giant, BusyFoot::None);
	}
}
