#include "Actions/Reactions/ModSupportReactions.hpp"

#include "Actions/Core/ActionReactions.hpp"

#include "Managers/Animation/Controllers/ThighSandwichController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Damage/CollisionDamage.hpp"
#include "Managers/CrushManager.hpp"

#include "Utils/DeathReport.hpp"

/*
  The tags another mod's animation can fire to reach this one. None of them belong to a GTS animation
  state, so none of them belong to a node: they arrive on whatever actor is playing someone else's
  clip, and there is nothing to enter or leave.

  Published for other mods:
    GTScrush_caster / GTScrush_victim     Thick Thighs Take Lives. Crushes anyone in a kill move
                                          near the actor, if the size difference is large enough.
    GTS_FootSwipe_L_ON / _OFF             a foot and thigh damage loop that runs between the two.
    GTS_FootSwipe_R_ON / _OFF
    MCO_SecondDodge                       Modern Combat Overhaul. Full stomp effects on both feet.
    SoundPlay.MCO_DodgeSound
    GTS_CustomDamage_Butt_On / _Off       declared and empty, see below.
    GTS_CustomDamage_Legs_On / _Off
    GTS_CustomDamage_FullBody_On / _Off
    GTS_CustomDamage_Cleavage_On / _Off

  Used by this mod's own animations as well:
    GTS_ToLeft, GTS_ToRight, GTS_ToAnimA, GTS_ToAnimB   move the attach point a held actor is
                                          pinned to. Grab play and the thigh sandwich fire these,
                                          which is why they are here and not in either node: they
                                          have to work whichever animation is running, including one
                                          this mod does not own.

  **Info: the eight GTS_CustomDamage_* tags have no handler. They are a published interface with
  **nothing wired to them yet, declared so the names are reserved and a capture shows them arriving.

  **Info: the old file also registered the trigger "Tiny_ExitAnims" for GTSBEH_Tiny_Abort. Nothing
  **ever called it, so it is not carried over.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	constexpr std::string_view NODE_FOOT_R = "NPC R Foot [Rft ]";
	constexpr std::string_view NODE_FOOT_L = "NPC L Foot [Lft ]";

	constexpr std::string_view TASKID_FMT_SWIPE_L = "FootSwipeL_{}";
	constexpr std::string_view TASKID_FMT_SWIPE_R = "FootSwipeR_{}";

	// Anyone already in a kill move close enough and small enough to be crushed by it.
	void TriggerKillZone(Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		constexpr float BASE_CHECK_DISTANCE = 90.0f;

		const float ratio = TinyCalamityActive(a_Giant) ? 0.8f : 3.0f;
		const float giantScale = get_visual_scale(a_Giant);
		const NiPoint3 giantLocation = a_Giant->GetPosition();

		for (auto* other : find_actors()) {

			if (!other || other == a_Giant || !other->IsInKillMove()) {
				continue;
			}

			if (giantScale / get_visual_scale(other) <= ratio) {
				continue;
			}

			if ((other->GetPosition() - giantLocation).Length() < BASE_CHECK_DISTANCE * giantScale * 3) {
				ReportDeath(a_Giant, other, DamageSource::Booty);
				CrushManager::Crush(a_Giant, other);
			}
		}
	}

	// Runs until the matching OFF cancels it.
	void StartFootSwipe(Actor* a_Giant, bool a_Right, std::string_view a_Name) {

		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::Run(a_Name, [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			CollisionDamage::DoFootCollision(giant, Damage_ThighCrush_CrossLegs_Out, Radius_ThighCrush_Idle, 10, 0.1f, 0.95f, DamageSource::Crushed, a_Right, true, false, false);
			ApplyThighDamage(giant, a_Right, true, Radius_ThighCrush_Idle, Damage_ThighCrush_CrossLegs_Out, 0.1f, 0.95f, 10, DamageSource::ThighCrushed);

			return true;
		});
	}

	// Both feet land at once, with the full stomp effect on each.
	void DoubleStomp(Actor* a_Giant) {

		DoDamageEffect(a_Giant, Damage_Stomp, Radius_Stomp, 10, 0.20f, FootEvent::Right, 1.0f, DamageSource::CrushedRight);
		DoDamageEffect(a_Giant, Damage_Stomp, Radius_Stomp, 10, 0.20f, FootEvent::Left, 1.0f, DamageSource::CrushedLeft);

		DoFootstepSound(a_Giant, 1.0f, FootEvent::Right, NODE_FOOT_R);
		DoFootstepSound(a_Giant, 1.0f, FootEvent::Left, NODE_FOOT_L);

		DoDustExplosion(a_Giant, 1.0f, FootEvent::Right, NODE_FOOT_R);
		DoDustExplosion(a_Giant, 1.0f, FootEvent::Left, NODE_FOOT_L);

		DoLaunch(a_Giant, 0.90f, 1.35f, FootEvent::Right);
		DoLaunch(a_Giant, 0.90f, 1.35f, FootEvent::Left);
	}

	//----------------------------------------------------------------------------------------------
	// Handlers
	//----------------------------------------------------------------------------------------------

	void OnCrushCaster(RE::Actor* a_Actor) {
		TriggerKillZone(a_Actor);
	}

	// Fired on the one being crushed, so the crush is run from the player instead.
	void OnCrushVictim(RE::Actor* a_Actor) {

		if (!a_Actor->IsPlayerRef()) {
			TriggerKillZone(PlayerCharacter::GetSingleton());
		}
	}

	void OnFootSwipeLeftOn(RE::Actor* a_Actor) {
		StartFootSwipe(a_Actor, false, std::format(TASKID_FMT_SWIPE_L, a_Actor->formID));
	}

	void OnFootSwipeLeftOff(RE::Actor* a_Actor) {
		TaskManager::Cancel(std::format(TASKID_FMT_SWIPE_L, a_Actor->formID));
	}

	void OnFootSwipeRightOn(RE::Actor* a_Actor) {
		StartFootSwipe(a_Actor, true, std::format(TASKID_FMT_SWIPE_R, a_Actor->formID));
	}

	void OnFootSwipeRightOff(RE::Actor* a_Actor) {
		TaskManager::Cancel(std::format(TASKID_FMT_SWIPE_R, a_Actor->formID));
	}

	void OnDodge(RE::Actor* a_Actor) {
		DoubleStomp(a_Actor);
	}

	void OnToLeft(RE::Actor* a_Actor) {
		Attachment_SetTargetNode(a_Actor, AttachToNode::ObjectL);
	}

	void OnToRight(RE::Actor* a_Actor) {

		// The sandwich reuses this tag to end its suffocation, so the two have to happen together.
		if (AnimationVars::Action::IsThighGrinding(a_Actor) || AnimationVars::Action::IsThighSandwiching(a_Actor)) {
			auto& data = ThighSandwichController::GetSingleton().GetSandwichingData(a_Actor);
			data.EnableSuffocate(false);
			data.SetSuffocateMult(1.0f);
		}

		Attachment_SetTargetNode(a_Actor, AttachToNode::ObjectR);
	}

	void OnToAnimA(RE::Actor* a_Actor) {
		Attachment_SetTargetNode(a_Actor, AttachToNode::ObjectA);
	}

	void OnToAnimB(RE::Actor* a_Actor) {
		Attachment_SetTargetNode(a_Actor, AttachToNode::ObjectB);
	}

	//----------------------------------------------------------------------------------------------
	// Table
	//----------------------------------------------------------------------------------------------

	constexpr ReactionDef Table[] = {
		{ .Tag = "GTScrush_caster",              .Handler = OnCrushCaster },
		{ .Tag = "GTScrush_victim",              .Handler = OnCrushVictim },

		{ .Tag = "GTS_FootSwipe_L_ON",           .Handler = OnFootSwipeLeftOn },
		{ .Tag = "GTS_FootSwipe_L_OFF",          .Handler = OnFootSwipeLeftOff },
		{ .Tag = "GTS_FootSwipe_R_ON",           .Handler = OnFootSwipeRightOn },
		{ .Tag = "GTS_FootSwipe_R_OFF",          .Handler = OnFootSwipeRightOff },

		{ .Tag = "MCO_SecondDodge",              .Handler = OnDodge },
		{ .Tag = "SoundPlay.MCO_DodgeSound",     .Handler = OnDodge },

		{ .Tag = "GTS_ToLeft",                   .Handler = OnToLeft },
		{ .Tag = "GTS_ToRight",                  .Handler = OnToRight },
		{ .Tag = "GTS_ToAnimA",                  .Handler = OnToAnimA },
		{ .Tag = "GTS_ToAnimB",                  .Handler = OnToAnimB },

		{ .Tag = "GTS_CustomDamage_Butt_On" },
		{ .Tag = "GTS_CustomDamage_Butt_Off" },
		{ .Tag = "GTS_CustomDamage_Legs_On" },
		{ .Tag = "GTS_CustomDamage_Legs_Off" },
		{ .Tag = "GTS_CustomDamage_FullBody_On" },
		{ .Tag = "GTS_CustomDamage_FullBody_Off" },
		{ .Tag = "GTS_CustomDamage_Cleavage_On" },
		{ .Tag = "GTS_CustomDamage_Cleavage_Off" },
	};
}

namespace GTS::Actions {

	void RegisterModSupportReactions() {
		Reactions::Register(Table, "ModSupport");
	}
}
