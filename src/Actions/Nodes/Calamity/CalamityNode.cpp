#include "Actions/Nodes/Calamity/CalamityNode.hpp"

#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Damage/TinyCalamity.hpp"
#include "Actions/Nodes/Calamity/CalamityCommon.hpp"

#include "Actions/Core/ActionRegistry.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Animation/Utils/TurnTowards.hpp"
#include "Managers/AI/AIFunctions.hpp"
#include "Managers/Audio/Footstep.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/Rumble.hpp"
#include "Managers/ShrinkToNothingManager.hpp"

#include "Config/Config.hpp"
#include "Magic/Effects/Common.hpp"

#include "Managers/Animation/Utils/AttachPoint.hpp"
#include "Utils/DeathReport.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Two animations, one node. Both are the giant casting Tiny Calamity on someone at arm's length, and
  both are driven by code rather than a key: nothing here is bound to input.

  They are one node because the graph cannot separate them. Both giant side clips run a
  BSIsActiveModifier that writes the same variable, GTS_isCastingShrink: GTS_ShrinkMag_Fatal_IAM for
  the erase and Giantess_TC_Shrink_IAM for the shrink. Two nodes on one signature would be picked
  apart by the idle sweep at random.

  Both captured. Times are from one run and are illustrative only.

  [SHRINK ("GTSBEH_TC_Shrink", from ShrinkUntil with animation set)]
    GTS_TC_RuneStart    +0.51 -> the rune on the hand starts at nothing and grows
    GTS_TC_ReadySound   +1.30 -> the grow task is cancelled, the rune is snapped to its full size
    GTS_TC_ShrinkStart  +2.50 -> the target gets its own rune, starts shrinking, is slowed and scared
    GTS_TC_RuneEnd      +4.21 -> movement speed restored, the rune shrinks away, cooldown applied
    GTS_TC_ShrinkStop   +4.31 -> the target list is dropped
    GTSBEH_Exit         +5.33 [NOT HANDLED]

  [ERASE ("GTSBeh_Shrink_Magic_Fatal_Start" on the giant, "GTSBeh_T_Shrink_Magic_Fatal_Start" on the
   target, from TinyCalamity_WrathfulCalamity)]
    GTS_LC_Start          +0.01 -> rune and particle on the target
    GTS_LC_AttachToObject +0.15 -> the target is scared, pinned to ObjectB and put into combat
    GTS_LC_FootstepL      +0.84 -> walking damage, either foot, any number of times
    GTS_LC_Shrink_1       +1.11 -> four steps down: 0.28, 0.20, 0.012, then a smile with no shrink
    GTS_LC_CameraON       +2.18 -> the camera follows the right hand
    GTS_LC_Shrink_2       +2.55
    GTS_LC_Liftup         +2.55 -> the target is lifted, particles start. Same frame as Shrink_2
    GTS_LC_FootstepR      +3.15
    GTS_LC_Shrink_3       +4.26
    GTS_LC_Shrink_4       +7.97
    GTS_LC_CameraOFF      +8.70 -> the camera is released before the snap, not after it
    GTS_LC_FingerSnap     +8.75 -> the target dies here, and the target list is dropped
    GTS_LC_FootstepL / _R +10.48, +11.27 -> two more after the target is already dead

  **Info: the erase sends both behaviours on the same frame, the giant's and the target's, and the
  **capture shows the target side landing on a dynamic FormID without trouble.

  **Info: the exit stays kUnannounced, and the capture is why rather than a guess. The shrink ends on
  **a plain GTSBEH_Exit about 0.5s before its signature drains; the erase sends nothing at all and
  **runs a further 0.9s past its last footstep. One tag cannot cover both, so the drain is the exit.
  **GTSBEH_Exit could be added to ExitSignals to get desync reporting back on the shrink half, at the
  **cost of that half exiting half a second earlier.

  **Info: the targets are not a Possession. The giant is not holding them, so nothing else may see a
  **slot as occupied; they live in CalamityCommon, keyed on the giant's FormID.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH ON EITHER TRIGGER, ON THE GIANT]
     GTS_isCastingShrink -> TRUE   (graph owned, and this node's signature)
     GTS_Busy            -> TRUE
   [SET BY THE GRAPH ON THE TARGET, ERASE ONLY]
     GTS_Being_Shrunk    -> TRUE   (Tiny_Shrink_Fatal_IAM in 0_master writes it)
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: nothing in the DLL writes either variable, so both are real evidence.

   **Info: the target's GTS_Being_Shrunk is set by the graph but nothing here reads it. A creature has
   **no GTS_ variables at all, so the particle loop asks CalamityCommon whether the actor is still a
   **target instead.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Kind = CalamityNode::Kind;
	using State = CalamityNode::State;

	constexpr std::string_view NODE_ROOT = "NPC Root [Root]";
	constexpr std::string_view NODE_HAND_R = "NPC R Hand [RHnd]";
	constexpr std::string_view NODE_FOOT_L = "NPC L Foot [Lft ]";
	constexpr std::string_view NODE_FOOT_R = "NPC R Foot [Rft ]";
	constexpr std::string_view NODE_ANIMOBJ_A = "AnimObjectA";
	constexpr std::string_view NODE_RUNE = "ShrinkRune-Obj";

	constexpr std::string_view TASKID_FMT_RUNE = "Calamity_{}_{}";
	constexpr std::string_view TASKID_FMT_ATTACH = "CalamityKill_{}_{}";
	constexpr std::string_view TASKID_FMT_EFFECTS = "CalamityEffects_{}";
	constexpr std::string_view TASKID_FMT_SHRINK = "ShrinkTowards_{}_{}";
	constexpr std::string_view TASKID_FMT_LAUNCH = "DelayLaunch_{}";

	//----------------------------------------------------------------------------------------------
	// The rune on the giant's hand
	//----------------------------------------------------------------------------------------------

	void SetRuneScale(Actor* a_Giant, float a_Scale) {

		if (auto* node = find_node(a_Giant, NODE_RUNE, false)) {
			node->local.scale = a_Scale;
			update_node(node);
		}
	}

	// Grows the rune in, or shrinks it away again. The two directions run on different clocks: growing
	// follows the game's own slowdown, shrinking follows the animation speed.
	void AnimateRune(Actor* a_Giant, bool a_Shrinking, float a_Speed, float a_Scale) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::Run(std::format(TASKID_FMT_RUNE, a_Giant->formID, a_Shrinking), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			auto* node = find_node(giant, NODE_RUNE, false);

			if (a_Shrinking) {

				const double elapsed = std::clamp(((Time::WorldTimeElapsed() - start) * AnimationManager::GetAnimSpeed(giant)) * a_Speed, 0.01, 0.98);

				if (node) {
					node->local.scale = static_cast<float>(std::clamp(0.60 - elapsed, 0.01, 1.0));
					update_node(node);
				}

				return elapsed < 0.98;
			}

			const double elapsed = std::clamp(((Time::WorldTimeElapsed() - start) * GetAnimationSlowdown(giant)) * a_Speed, 0.01, 9999.0);

			if (node) {
				node->local.scale = static_cast<float>(std::clamp(elapsed, 0.01, 1.0)) * a_Scale;
				update_node(node);
			}

			return elapsed < 1.0;
		});
	}

	void SlowDown(Actor* a_Tiny, float a_Value) {

		if (auto* data = Transient::GetActorData(a_Tiny)) {
			data->MovementSlowdown -= a_Value;
		}
	}

	//----------------------------------------------------------------------------------------------
	// Shrink
	//----------------------------------------------------------------------------------------------

	void SpawnTinyRune(Actor* a_Tiny) {

		if (auto* node = find_node(a_Tiny, NODE_ROOT)) {
			SpawnParticle(a_Tiny, 3.00f, "GTS/gts_tinyrune.nif", NiMatrix3(), node->world.translate, 1.0f, 7, node);
		}
	}

	//----------------------------------------------------------------------------------------------
	// Erase
	//----------------------------------------------------------------------------------------------

	void SpawnCalamityRune(Actor* a_Actor, float a_Mult) {

		if (auto* node = find_node(a_Actor, NODE_ANIMOBJ_A)) {
			SpawnParticle(a_Actor, 3.00f, "GTS/gts_calamityrune.nif", NiMatrix3(), node->world.translate, 1.0f * a_Mult, 7, node);
		}
	}

	void SpawnEffectsOverTime(Actor* a_Tiny) {

		ActorHandle handle = a_Tiny->CreateRefHandle();

		TaskManager::Run(std::format(TASKID_FMT_EFFECTS, a_Tiny->formID), [=](auto&) {

			auto tinyPtr = handle.get();

			if (!tinyPtr) {
				return false;
			}

			auto* tiny = tinyPtr.get();

			// The target list, not the tiny's graph. Creatures have no GTS_ variables.
			if (tiny->IsDead() || !Calamity::IsTarget(tiny->formID)) {
				return false;
			}

			if (!IsActionOnCooldown(tiny, CooldownSource::Misc_ShrinkParticle)) {
				SpawnCustomParticle(tiny, ParticleType::Red, NiPoint3(), NODE_ROOT, get_visual_scale(tiny) * 0.85f);
				ApplyActionCooldown(tiny, CooldownSource::Misc_ShrinkParticle);
			}

			return true;
		});
	}

	// Holds the target in the giant's hand for the length of the animation and keeps the two graphs
	// running at the same speed, or the halves drift apart.
	void AttachTarget(Actor* a_Giant, Actor* a_Tiny) {

		ActorHandle giantHandle = a_Giant->CreateRefHandle();
		ActorHandle tinyHandle = a_Tiny->CreateRefHandle();

		TaskManager::Run(std::format(TASKID_FMT_ATTACH, a_Giant->formID, a_Tiny->formID), [=](auto&) {

			auto giantHandleRef = giantHandle.get();
			auto tinyHandleRef = tinyHandle.get();

			if (!giantHandleRef || !tinyHandleRef) {
				return false;
			}

			auto* giant = giantHandleRef.get();
			auto* tiny = tinyHandleRef.get();

			ShutUp(tiny);
			ShutUp(giant);

			Anims_FixAnimationDesync(giant, tiny, false);

			if (giant->IsDead() || tiny->IsDead() || !AnimationVars::General::IsGTSBusy(giant)) {
				return false;
			}

			AttachToObjectB(giant, tiny);
			ForceRagdoll(tiny, false);

			return FaceOpposite(giant, tiny);
		});
	}

	void ShrinkTargetsUntil(RE::FormID a_Giant, float a_To, float a_Magnitude) {

		for (auto* tiny : Calamity::Targets(a_Giant)) {

			ActorHandle handle = tiny->CreateRefHandle();

			TaskManager::Run(std::format(TASKID_FMT_SHRINK, tiny->formID, a_To), [=](auto&) {

				auto* actor = handle.get().get();

				if (!actor || get_visual_scale(actor) <= a_To) {
					return false;
				}

				override_actor_scale(actor, -a_Magnitude * 0.225f * TimeScale(), SizeEffectType::kNeutral);

				if (get_target_scale(actor) < a_To) {
					set_target_scale(actor, a_To);
					return false;
				}

				return true;
			});
		}
	}

	void EraseTarget(Actor* a_Giant, Actor* a_Tiny) {

		ReportDeath(a_Giant, a_Tiny, DamageSource::EraseFromExistence);

		AdvanceQuestProgression(a_Giant, a_Tiny, QuestStage::ShrinkToNothing, 0.25f, false);
		ModSizeExperience(a_Giant, 0.24f * 0.25f);
		ShrinkToNothingManager::SpawnDeathEffects(a_Tiny);
		a_Tiny->Attacked(a_Giant);

		KillActor(a_Giant, a_Tiny, Config::Audio.bMuteFingerSnapDeathScreams);
		AddSMTDuration(a_Giant, 5.0f);

		DecreaseShoutCooldown(a_Giant);

		PerkHandler::UpdatePerkValues(a_Giant, PerkUpdate::Perk_LifeForceAbsorption);
		ShrinkToNothingManager::TransferInventoryTask(a_Giant, a_Tiny);
	}

	// One frame late, so a grind that starts on the same annotation wins over the launch.
	void DelayedLaunch(Actor* a_Giant, float a_Radius, float a_Power, FootEvent a_Event) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::Run(std::format(TASKID_FMT_LAUNCH, a_Giant->formID), [=](auto&) {

			auto handleRef = handle.get();

			if (!handleRef) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start > 0.03) {
				LaunchTask(handleRef.get(), a_Radius, a_Power, a_Event);
				return false;
			}

			return true;
		});
	}

	void Footstep(Actor* a_Giant, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.45f : 1.25f;

		Rumbling::Once(a_Rumble, a_Giant, Rumble_Stomp_Normal * smt * GetHighHeelsBonusDamage(a_Giant, true), 0.0f, a_Node, 1.10f);
		DoDamageEffect(a_Giant, Damage_Walk_Defaut * perk, Radius_Stomp, 10, 0.25f, a_Event, 1.0f, a_Source);
		DoDustExplosion(a_Giant, dust + 0.05f, a_Event, a_Node);
		DoFootstepSound(a_Giant, 1.0f, a_Event, a_Node);

		DrainStamina(a_Giant, "StaminaDrain_Stomp", Runtime::PERK.GTSPerkDestructionBasics, false, 1.8f);

		DelayedLaunch(a_Giant, 0.90f * perk, 2.2f, a_Event);

		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations, shrink
	//----------------------------------------------------------------------------------------------

	void OnRuneStart(const ActionContext& a_Ctx) {
		SetRuneScale(a_Ctx.Actor(), 0.01f);
		AnimateRune(a_Ctx.Actor(), false, 0.6f, 0.70f);
	}

	void OnReadySound(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		TaskManager::Cancel(std::format(TASKID_FMT_RUNE, a_Ctx.Owner(), false));
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_RuneReady, giant, 1.0f, NODE_HAND_R);
		SetRuneScale(giant, 0.60f);

		if (giant->IsPlayerRef()) {
			shake_camera(giant, 0.80f, 0.35f);
		}
		else {
			Rumbling::Once("Calamity_R", giant, 4.25f, 0.15f, NODE_HAND_R, 0.0f);
		}
	}

	void OnShrinkStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {

			SpawnTinyRune(tiny);
			ShrinkUntil(giant, tiny, Calamity::ShrinkUntil(tiny), 0.26f, false);
			tiny->StartCombat(giant);

			ChanceToScare(giant, tiny, 6.0f, 6, false);
			SlowDown(tiny, 0.60f);
			CalamityNode::GetOrAdd(a_Ctx.Owner()).Slowed = true;

			Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_SpawnRune, tiny, 1.0f, NODE_ROOT);
			Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_Absorb, tiny, 1.0f, NODE_ROOT);
		}
	}

	void OnRuneEnd(const ActionContext& a_Ctx) {

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {
			SlowDown(tiny, -0.60f);
		}

		CalamityNode::GetOrAdd(a_Ctx.Owner()).Slowed = false;

		AnimateRune(a_Ctx.Actor(), true, 1.4f, 0.60f);
		ApplyActionCooldown(a_Ctx.Actor(), CooldownSource::Misc_TinyCalamity_Shrink);
	}

	void OnShrinkStop(const ActionContext& a_Ctx) {
		Calamity::ClearTargets(a_Ctx.Owner());
	}

	//----------------------------------------------------------------------------------------------
	// Annotations, erase
	//----------------------------------------------------------------------------------------------

	void OnEraseStart(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {
			Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_SpawnRune, tiny, 1.0f, NODE_ROOT);
			SpawnCustomParticle(tiny, ParticleType::Red, NiPoint3(), NODE_ROOT, 0.75f);

			// On the giant's own AnimObjectA, not the target's. That is where the rune belongs.
			SpawnCalamityRune(giant, 1.0f);
		}
	}

	void OnAttachToObject(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {
			ChanceToScare(giant, tiny, 6.0f, 1, false);
			AttachTarget(giant, tiny);
			tiny->StartCombat(giant);
		}
	}

	void OnLiftup(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_RuneReady, giant, 1.0f, NODE_HAND_R);
		Task_FacialEmotionTask_Anger(giant, 1.25f + RandomFloat(0.01f, 0.25f), "LiftUpSmile", 0.125f);

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {
			SpawnCustomParticle(tiny, ParticleType::Red, NiPoint3(), NODE_ANIMOBJ_A, 1.50f);
			SpawnCustomParticle(tiny, ParticleType::Red, NiPoint3(), NODE_ANIMOBJ_A, 1.25f);
			SpawnCustomParticle(tiny, ParticleType::Red, NiPoint3(), NODE_ANIMOBJ_A, 1.00f);
			SpawnEffectsOverTime(tiny);
		}

		Rumbling::Once("LiftUp", giant, 6.0f, 0.025f, true);
	}

	void OnCameraOn(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Hand_Right);
	}

	void OnCameraOff(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Hand_Right);
	}

	void OnShrinkStep1(const ActionContext& a_Ctx) {
		ShrinkTargetsUntil(a_Ctx.Owner(), 0.28f, 0.0420f);
	}

	void OnShrinkStep2(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 1.55f + RandomFloat(0.01f, 0.25f), "KillSmile1", 0.9f, 0.6f);
		ShrinkTargetsUntil(a_Ctx.Owner(), 0.20f, 0.0175f);
	}

	void OnShrinkStep3(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 1.55f + RandomFloat(0.01f, 0.25f), "KillSmile2", 0.9f, 0.6f);
		ShrinkTargetsUntil(a_Ctx.Owner(), 0.012f, 0.00525f);
	}

	void OnShrinkStep4(const ActionContext& a_Ctx) {
		Task_FacialEmotionTask_Smile(a_Ctx.Actor(), 1.55f + RandomFloat(0.01f, 0.25f), "KillSmile3", 0.9f, 0.6f);
	}

	void OnFingerSnap(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Task_FacialEmotionTask_SlightSmile(giant, 2.6f + RandomFloat(0.35f, 0.75f), "SnapSmile", 0.75f);

		if (RandomInt(1, 4) >= 1) {
			Sound_PlayLaughs(giant, 1.0f, 0.14f, EmotionTriggerSource::Superiority);
		}

		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundTinyCalamity_FingerSnap, giant, 1.0f, NODE_HAND_R);
		Rumbling::Once("FingerSnap", giant, 7.25f, 0.025f, true);

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {
			EraseTarget(giant, tiny);
		}

		Calamity::ClearTargets(a_Ctx.Owner());
	}

	void OnFootstepLeft(const ActionContext& a_Ctx) {
		Footstep(a_Ctx.Actor(), false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "NormalRumbleL");
	}

	void OnFootstepRight(const ActionContext& a_Ctx) {
		Footstep(a_Ctx.Actor(), true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "NormalRumbleR");
	}

	//----------------------------------------------------------------------------------------------
	// Entries
	//----------------------------------------------------------------------------------------------

	// The target's half of the erase. Best effort, like every partner send: a creature has no GTS_
	// variables and refuses it, and nothing here waits on the answer.
	void TakeErase(const EntryContext& a_Ctx) {

		for (auto* tiny : Calamity::Targets(a_Ctx.Owner())) {
			ActionRegistry::Notify(tiny, "GTSBeh_T_Shrink_Magic_Fatal_Start");
		}
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_isCastingShrink", true },
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "Calamity.Shrink", .Behaviour = "GTSBEH_TC_Shrink" },
		{ .Action = "Calamity.Erase",  .Behaviour = "GTSBeh_Shrink_Magic_Fatal_Start", .OnTaken = TakeErase },
	};

	// Activate erases whoever is in front while Tiny Calamity is up. The press is only swallowed when
	// a target was actually taken, so activate keeps working on doors and containers.
	bool OnActivate(RE::Actor* a_Actor) {

		if (!TinyCalamityActive(a_Actor)) {
			return false;
		}

		auto preys = VoreController::GetSingleton().GetVoreTargetsInFront(a_Actor, 1);

		return TinyCalamity_WrathfulCalamity(a_Actor, preys);
	}

	constexpr VanillaBlock Blocks[] = {
		{ .UserEvent = "Activate", .Handle = OnActivate },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_TC_RuneStart",      .Handler = OnRuneStart },
		{ .Tag = "GTS_TC_ReadySound",     .Handler = OnReadySound },
		{ .Tag = "GTS_TC_ShrinkStart",    .Handler = OnShrinkStart },
		{ .Tag = "GTS_TC_RuneEnd",        .Handler = OnRuneEnd },
		{ .Tag = "GTS_TC_ShrinkStop",     .Handler = OnShrinkStop },

		{ .Tag = "GTS_LC_Start",          .Handler = OnEraseStart },
		{ .Tag = "GTS_LC_CameraON",       .Handler = OnCameraOn },
		{ .Tag = "GTS_LC_CameraOFF",      .Handler = OnCameraOff },
		{ .Tag = "GTS_LC_AttachToObject", .Handler = OnAttachToObject },
		{ .Tag = "GTS_LC_Liftup",         .Handler = OnLiftup },
		{ .Tag = "GTS_LC_Shrink_1",       .Handler = OnShrinkStep1 },
		{ .Tag = "GTS_LC_Shrink_2",       .Handler = OnShrinkStep2 },
		{ .Tag = "GTS_LC_Shrink_3",       .Handler = OnShrinkStep3 },
		{ .Tag = "GTS_LC_Shrink_4",       .Handler = OnShrinkStep4 },
		{ .Tag = "GTS_LC_FingerSnap",     .Handler = OnFingerSnap },
		{ .Tag = "GTS_LC_FootstepL",      .Handler = OnFootstepLeft },
		{ .Tag = "GTS_LC_FootstepR",      .Handler = OnFootstepRight },
	};

	constexpr std::string_view StateNames[] = {
		"shrink",
		"erase",
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> CalamityNode::Signature() const     { return ::Signature; }
	ExitPolicy CalamityNode::Exit() const                            { return ExitPolicy::kUnannounced; }
	std::span<const EntryDef> CalamityNode::Entries() const          { return ::Entries; }
	std::span<const AnnotationDef> CalamityNode::Annotations() const { return ::Annotations; }
	std::span<const VanillaBlock> CalamityNode::Blocks() const      { return ::Blocks; }

	CalamityNode::State* CalamityNode::Get(RE::FormID a_Owner)      { return m_State.Find(a_Owner); }
	CalamityNode::State& CalamityNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	bool CalamityNode::CanEnter(const EntryContext& a_Ctx) const {
		return a_Ctx.Actor() != nullptr;
	}

	void CalamityNode::OnEnter(const ActionContext& a_Ctx) {
		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};
		state.Kind = a_Ctx.EnteredVia() == "Calamity.Erase" ? Kind::kErase : Kind::kShrink;
	}

	void CalamityNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();

		// Everything this node starts is stopped here, on every path out. Unwinding through the last
		// annotation alone leaves an abort with the rune part grown and the attach loop running.
		TaskManager::Cancel(std::format(TASKID_FMT_RUNE, owner, true));
		TaskManager::Cancel(std::format(TASKID_FMT_RUNE, owner, false));
		TaskManager::Cancel(std::format(TASKID_FMT_LAUNCH, owner));

		const State* state = m_State.Find(owner);
		const bool slowed = state && state->Slowed;

		for (auto* tiny : Calamity::Targets(owner)) {
			TaskManager::Cancel(std::format(TASKID_FMT_ATTACH, owner, tiny->formID));
			TaskManager::Cancel(std::format(TASKID_FMT_EFFECTS, tiny->formID));

			if (slowed) {
				SlowDown(tiny, -0.60f);
			}
		}

		if (auto* giant = a_Ctx.Actor()) {
			ManageCamera(giant, false, CameraTracking::Hand_Right);
		}

		Calamity::ClearTargets(owner);
		m_State.Forget(owner);
	}

	void CalamityNode::OnForget(RE::FormID a_Owner) {
		Calamity::ClearTargets(a_Owner);
		m_State.Forget(a_Owner);
	}

	void CalamityNode::OnReset() {
		m_State.Clear();
	}

	std::string_view CalamityNode::StateName(RE::FormID a_Owner) const {
		const State* state = m_State.Find(a_Owner);
		return state ? StateNames[std::to_underlying(state->Kind)] : std::string_view{};
	}
}
