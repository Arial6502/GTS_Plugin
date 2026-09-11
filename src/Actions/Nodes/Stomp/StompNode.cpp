#include "Actions/Nodes/Stomp/StompNode.hpp"
#include "Actions/Core/PlayerTarget.hpp"
#include "Actions/Nodes/Stomp/StompCommon.hpp"
#include "Actions/Core/Possession.hpp"
#include "Actions/Nodes/Grab/GrabCommon.hpp"

#include "Config/Config.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Audio/Footstep.hpp"
#include "Managers/Audio/Stomps.hpp"
#include "Managers/ExplosionManager.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"

#include "Systems/Rays/Raycast.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Every foot attack the giant makes from her feet, in one node, because the graph gives them one
  variable. Fourteen BSIsActiveModifiers write GTS_IsStomping: the light stomp, the heavy stomp, both
  understomps, the foot grind, the under grind, and the intros of both trample kinds. Splitting them
  would leave the idle sweep guessing which one it had found.

  The understomp is not a separate animation, it is the same trigger at a steeper angle. Auto aim
  writes GTS_StompBlend and the graph interpolates; at the far end it plays the understomp instead.
  Which understomp depends on the stance, and the stance is what Variant records.

  Captured standing, sneaking and crawling. Times are from one run and are illustrative only.
  The grind, both trample kinds and the full body drop were not reached and are marked below.

  [LIGHT STOMP, STANDING ("GtsModStompAnimLeft" / "Right", keybind "Action.Stomp.StartLight")]
    GTSstompstartL / R    +0.27 -> stamina drain starts, camera follows the foot, speed to 1.35
    GTSstompimpactL / R   +0.91 -> the hit. Rolls for the grind, which is the second animation
    GTSStompendL / R      +1.90 -> speed back to 1.0
    GTSBEH_Exit           +2.69

  **Info: GTSstompland and GTS_Next are declared and neither fired in this capture. They stay
  **declared so they are not counted as orphans if another clip sends them.

  [HEAVY STOMP, STANDING ("GTSBeh_StrongStomp_StartLeft" / "Right", keybind "Action.Stomp.StartHeavy")]
    GTS_StrongStomp_Start               +0.08
    GTS_StrongStomp_LL / LR_Start       +0.12 -> camera, stamina, leg rumble
    GTS_StrongStomp_LL / LR_Middle      +0.52 -> speed to 1.55
    GTS_StrongStomp_LL / LR_End         +1.52
    GTS_StrongStomp_ImpactL / R         +1.85 -> the hit, after the End tag and not before it
    GTS_StrongStomp_ReturnLL / RL_Start +3.31 -> leg rumble while the leg comes back
    GTSBEH_Exit                         +4.11

  **Info: GTS_StrongStomp_Return*_End and GTS_StrongStomp_End are declared and neither fired.

  [UNDERSTOMP, STANDING ("GTSBeh_UnderStomp_StartL" / "R" at a steep angle)]
    GTS_UnderStomp_CamOnL / R   +0.34
    GTS_UnderStomp_ImpactL / R  +1.04 -> the hit, and rolls for the under grind
    GTS_UnderStomp_CamOffL / R  +2.64
    GTSBEH_Exit                 +2.97
    The strong variant is the same shape with _Strong on every tag. Not captured standing: every
    strong understomp in the capture was taken sneaking and played the butt slam instead.

  [UNDERSTOMP, SNEAKING - the butt slam ("GTSBeh_UnderStomp_Start_StrongL" / "R")]
    GTS_UnderStomp_ButtCamOn         +0.47
    GTS_UnderStomp_Butt_DisableHH    +0.54 -> heels off, or the giant lands on her toes
    GTS_UnderStomp_ButtImpact        +1.07 -> the butt lands
    GTS_UnderStomp_ButtLegImpact     +1.33 -> both thighs and both foot tips
    GTS_UnderStomp_Butt_EnableHH     +2.66 -> heels back on, before the giant is up
    GTS_UnderStomp_Butt_RecFootstepL +2.86 -> getting back up, two to three times over
    GTS_UnderStomp_ButtCamOff        +3.79
    GTS_UnderStomp_Butt_RecFootstepR +5.85 -> the last one lands after the camera is released
    GTSBEH_Exit                      +5.92

  [SNEAK SLAM, LIGHT ("GtsModStompAnimLeft" / "Right" while sneaking)]
    GTS_Sneak_Slam_Raise_Arm_L / R +0.47
    GTS_Sneak_Slam_Impact_L / R    +1.20
    GTS_Sneak_Slam_Cam_Off_L / R   +3.66
    GTSBEH_Exit                    +4.39

  [SNEAK SLAM, STRONG ("GTSBeh_StrongStomp_StartLeft" / "Right" while sneaking)]
    GTS_Sneak_SlamStrong_Raise_Arm_L / R       +0.80
    GTS_Sneak_SlamStrong_Lower_Arm_L / R       +1.53
    GTS_Sneak_SlamStrong_Impact_L / R          +1.73
    GTS_Sneak_SlamStrong_Impact_Secondary_L/ R +4.72
    GTS_Sneak_Slam_Cam_Off_L / R               +4.99 -> the light slam's tag, shared
    GTSBEH_Exit                                +5.92

  [UNDERSTOMP, CRAWLING - the hand slam ("GTSBeh_UnderStomp_StartL" / "R" while crawling)]
    GTS_UnderStomp_Crawl_CamOnL / R  +0.33
    GTS_UnderStomp_Crawl_ImpactL / R +1.20
    GTS_UnderStomp_Crawl_CamOffL / R +2.53
    GTSBEH_Exit                      +3.66

  **Info: the right side clip also sends GTSCrawl_SlamStrong_Raise_Arm_R, _Lower_Arm_R and
  **_Impact_R, interleaved with the three tags above. The left side sends none of them. That is in
  **the animation data, so a right side crawl understomp lands a full strong hand slam hit on top of
  **the understomp hit and the left side does not.

  [CRAWL SLAM, LIGHT ("GtsModStompAnimLeft" / "Right" while crawling)]
    GTSCrawl_Slam_Raise_Arm_L / R +0.33
    GTSCrawl_Slam_Lower_Arm_L / R +1.00
    GTSCrawl_Slam_Impact_L / R    +1.20
    GTSCrawl_Slam_Cam_Off_L / R   +2.86
    GTSBEH_Exit                   +3.66

  [CRAWL SLAM, STRONG ("GTSBeh_StrongStomp_StartLeft" / "Right" while crawling)]
    GTSCrawl_SlamStrong_Raise_Arm_L / R +0.27
    GTSCrawl_SlamStrong_Lower_Arm_L / R +1.60
    GTSCrawl_SlamStrong_Impact_L / R    +1.80
    Only seen riding along with the right side crawl understomp above, never on its own.

  [UNDERSTOMP, CRAWLING AND STRONG - the full body drop]
    GTS_UnderStomp_Crawl_BodyCamOn
    GTS_UnderStomp_Crawl_BodyImpact -> breasts, spine, thighs and butt all at once
    GTS_UnderStomp_Crawl_BodyCamOff
    Not reached. Order is from the registrations.

  [FOOT GRIND ("GTSBEH_StartFootGrindL" / "R", or the _UnderGrind_ pair)]
    GTSstomp_FootGrindL / R_Enter   -> the damage loop starts
    GTSstomp_FootGrindL / R_MV_S    -> one rotation, several times over
    GTSstomp_FootGrindL / R_MV_E
    GTSstomp_FootGrindL / R_Impact  -> the foot comes down again
    GTSstomp_FootGrindL / R_Exit    -> the loop stops and the tiny is let go
    Not reached, it needs a tiny underfoot. Order is from the registrations.

  [FINGER GRIND (the same GTSBEH_StartFootGrind behaviour, sent while sneaking)]
    GTS_Sneak_FingerGrind_CameraOn_R / L  +0.67
    GTS_Sneak_FingerGrind_Impact_R / L    +2.59
    GTS_Sneak_FingerGrind_Rotation_R / L  +5.11, then about every 3s
    GTS_Sneak_FingerGrind_Finisher_R / L  +12.73
    GTS_Sneak_FingerGrind_CameraOff_R / L +15.03

  **Info: the finger grind and the foot grind are the same behaviour and the same GTS_IsFootGrinding
  **variable. The graph plays the finger clip because the giant is sneaking, the same way the sneak
  **slam replaces the stomp. FingerGrindCheck asks for it one frame after a light sneak slam lands,
  **and only when the giant is more than Action_FingerGrind (6.0) times the target's size.

  [TRAMPLE INTRO ("GTSBeh_Trample_L" / "R", keybind "Action.Stomp.StartTrample")]
    GTS_Trample_Leg_Raise_L / R
    GTS_Trample_Cam_Start_L / R
    GTS_Trample_Footstep_L / R  -> the first hit, and where the loop is asked for
    Handing over to TrampleNode on GTSBEH_Trample_Start_L / R.
    Not reached. Order is from the registrations.

  **Info: four of the understomp variants assert no GTS variable at all. Their modifiers -
  **Giantess_UnderStomp_L/R_Sneak_IAM, _Strong_L/R_Sneak_IAM, _L/R_Crawling_IAM and
  **_Strong_L/R_Crawling_IAM - bind only tdmHeadtrackingBehavior. There is nothing for a signature to
  **read, so the node answers Alive() for itself while one of them runs, and the entry is confirmed by
  **its own first annotation rather than by the graph.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH ON A STANDING STOMP, UNDERSTOMP OR TRAMPLE INTRO]
     GTS_IsStomping       -> TRUE   (graph owned, and this node's signature)
   [WHILE THE GRIND RUNS, ON TOP OF THE ABOVE]
     GTS_IsFootGrinding   -> TRUE
     GTS_IsUnderGrinding  -> TRUE for the understomp grind only
   [WRITTEN BY THE DLL BEFORE THE BEHAVIOUR IS SENT]
     GTS_StompBlend       -> the aim angle. GTS_StompBlend_X and _Y are the newer pair and need
                             behaviours that are not in yet, so all three are written and only the
                             first is read.
   [NOT SET AT ALL BY THE SNEAK AND CRAWL UNDERSTOMPS]
     **NOTHING**
   [CLEANUP]
     **ALL PREVIOUS STATED VARS INVERT**

   **Info: GTS_EnableAlternativeStomp picks which light stomp clip plays. It is bound to the
   **startStateId of GTS_Stomp_L_Anim_Picker and _R_Anim_Picker and to nothing else, so it reaches
   **the light stomp and no other attack. GTSManager pushes it per actor from the Actions settings;
   **nothing here writes it.

   **Info: GTS_IsAlternativeGrind is the graph's answer to the same question for the grind, written by
   **GTS_Stomp_L/R_Normal_IAM. The grind reads it to decide whether the tiny needs detaching by hand.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;
	using namespace GTS::Actions::Stomping;

	using Variant = StompNode::Variant;
	using State = StompNode::State;

	constexpr std::array R_LEG_RUMBLE_NODES = {
		"NPC L Toe0 [LToe]"sv,
		"NPC L Calf [LClf]"sv,
		"NPC L PreRearCalf"sv,
		"NPC L FrontThigh"sv,
		"NPC L RearCalf [RrClf]"sv,
	};

	constexpr std::array L_LEG_RUMBLE_NODES = {
		"NPC R Toe0 [RToe]"sv,
		"NPC R Calf [RClf]"sv,
		"NPC R PreRearCalf"sv,
		"NPC R FrontThigh"sv,
		"NPC R RearCalf [RrClf]"sv,
	};

	constexpr std::array BODY_NODES = {
		"NPC R Thigh [RThg]"sv,
		"NPC L Thigh [LThg]"sv,
		"NPC R Butt"sv,
		"NPC L Butt"sv,
		"NPC Spine [Spn0]"sv,
		"NPC Spine1 [Spn1]"sv,
		"NPC Spine2 [Spn2]"sv,
	};

	constexpr std::string_view TASKID_FMT_GRIND = "FootGrind_{}_{}";
	constexpr std::string_view TASKID_FMT_GRIND_DOT = "FootGrindDOT_{}";
	constexpr std::string_view TASKID_FMT_GRIND_ROT = "FootGrindRot_{}";
	constexpr std::string_view TASKID_FMT_LAND = "StompLand_{}_{}";
	constexpr std::string_view TASKID_FMT_UNDER_STRONG = "StrongUnderStomp_{}";
	constexpr std::string_view TASKID_FMT_BUTT = "UnderButtCrushAttack_{}";

	constexpr std::string_view TASK_LAUNCH_STOMP = "DelayLaunch";
	constexpr std::string_view TASK_LAUNCH_TRAMPLE = "DelayLaunch_Trample";

	//----------------------------------------------------------------------------------------------
	// Shared
	//----------------------------------------------------------------------------------------------

	void StopLoopRumble(Actor* a_Giant) {
		Rumbling::Stop("StompR_Loop", a_Giant);
		Rumbling::Stop("StompL_Loop", a_Giant);
	}

	void LegRumble(Actor* a_Giant, std::string_view a_Tag, bool a_Right, bool a_Start, float a_Power = 0.0f, float a_Halflife = 0.0f) {

		for (const auto& node : a_Right ? R_LEG_RUMBLE_NODES : L_LEG_RUMBLE_NODES) {

			const std::string name = std::format("{}{}", a_Tag, node);

			if (a_Start) {
				Rumbling::Start(name, a_Giant, a_Power, a_Halflife, node);
			}
			else {
				Rumbling::Stop(name, a_Giant);
			}
		}
	}

	void ImpactRumble(Actor* a_Giant, float a_Base, std::string_view a_Node, std::string_view a_Name, bool a_HighHeels, float a_Mult) {

		float smt = TinyCalamityActive(a_Giant) ? 1.5f : 1.0f;

		if (a_HighHeels) {
			smt *= GetHighHeelsBonusDamage(a_Giant, true);
		}

		Rumbling::Once(a_Name, a_Giant, a_Base * smt * a_Mult, 0.0f, a_Node, 1.25f);
	}

	void ApplyDustRing(Actor* a_Giant, FootEvent a_Kind, std::string_view a_Node, float a_Mult) {

		Impact impact{
			.actor = a_Giant,
			.kind = a_Kind,
			.scale = get_visual_scale(a_Giant),
			.modifier = a_Mult,
			.nodes = find_node(a_Giant, a_Node),
		};

		ExplosionManager::GetSingleton().OnImpact(impact);
	}

	//----------------------------------------------------------------------------------------------
	// Light stomp
	//----------------------------------------------------------------------------------------------

	void StompImpact(const ActionContext& a_Ctx, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		const float speed = a_Ctx.AnimSpeed();
		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.45f : 1.25f;

		Rumbling::Once(a_Rumble, giant, Rumble_Stomp_Normal * smt * GetHighHeelsBonusDamage(giant, true), 0.0f, a_Node, 1.10f);
		DoDamageEffect(giant, Damage_Stomp * perk, Radius_Stomp, 10, 0.25f, a_Event, 1.0f, a_Source);
		DoDustExplosion(giant, dust + (speed * 0.05f), a_Event, a_Node);
		StompManager::PlayNewOrOldStomps(giant, 1.0f, a_Event, a_Node, false);

		DrainStamina(giant, "StaminaDrain_Stomp", Runtime::PERK.GTSPerkDestructionBasics, false, 1.8f);

		{
			const float chance = giant->IsPlayerRef()
				? Config::Gameplay.ActionSettings.fPlayerStompGrindChance
				: Config::AI.Stomp.fStompGrindProbability;

			if (RandomBool(chance)) {
				FootGrindCheck(giant, Radius_Stomp, a_Right, FootActionType::Grind_Normal);
			}
		}

		DelayedLaunch(giant, TASK_LAUNCH_STOMP, 0.80f * perk, 2.0f * speed, a_Event);
		FootStepManager::PlayVanillaFootstepSounds(giant, a_Right);

		SetBusyFoot(giant, BusyFoot::None);
	}

	// The clip lands the foot a little before the animation says it does, so the second hit waits.
	void StompLand(const ActionContext& a_Ctx, bool a_Right, FootEvent a_Event, std::string_view a_Node, std::string_view a_Rumble) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		const float speed = a_Ctx.AnimSpeed();
		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.35f : 0.85f;
		const float power = Rumble_Stomp_Land_Normal * smt * GetHighHeelsBonusDamage(giant, true);

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = giant->CreateRefHandle();

		TaskManager::RunFor(std::format(TASKID_FMT_LAND, giant->formID, start), 1.0f, [=](auto&) {

			auto handleRef = handle.get();

			if (!handleRef) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start <= 0.075) {
				return true;
			}

			auto* ref = handleRef.get();

			Rumbling::Once(a_Rumble, ref, power, 0.0f, a_Node, 0.0f);
			DoDamageEffect(ref, Damage_Stomp * perk, Radius_Stomp, 25, 0.25f, a_Event, 1.0f, DamageSource::CrushedRight);
			DoDustExplosion(ref, dust + (speed * 0.05f), a_Event, a_Node);
			StompManager::PlayNewOrOldStomps(ref, 1.0f + speed / 14, a_Event, NODE_FOOT_R, false);

			LaunchTask(ref, 0.90f * perk, 3.2f + speed / 2, a_Event);
			FootStepManager::PlayVanillaFootstepSounds(ref, a_Right);

			return false;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Heavy stomp
	//----------------------------------------------------------------------------------------------

	void StrongStompImpact(const ActionContext& a_Ctx, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		const float speed = a_Ctx.AnimSpeed();
		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.85f : 1.0f;
		const float damage = calamity ? 1.25f : 1.0f;

		const float augment = PerkHandler::Perks_Cataclysmic_EmpowerStomp(giant);
		const bool stacks = PerkHandler::Perks_Cataclysmic_HasStacks(giant);

		DoDamageEffect(giant, Damage_Stomp_Strong * damage * perk * augment, Radius_Stomp_Strong, 5, 0.35f, a_Event, 1.0f, a_Source);
		ImpactRumble(giant, Rumble_Stomp_Strong, a_Node, a_Rumble, true, 1.0f);

		if (!stacks) {
			DoDustExplosion(giant, 1.33f * (smt + (speed * 0.05f)), a_Event, a_Node);
		}
		else {
			for (const float exp : { 1.0f, 0.75f, 0.5f }) {
				DoDustExplosion(giant, 1.33f * (smt + (speed * 0.05f)) * augment * exp, a_Event, a_Node);
			}
		}

		DrainStamina(giant, "StaminaDrain_StrongStomp", Runtime::PERK.GTSPerkDestructionBasics, false, 3.4f);
		StompManager::PlayNewOrOldStomps(giant, smt + (speed / 10), a_Event, a_Node, true);
		LaunchTask(giant, 1.05f * perk * augment, (3.6f + speed / 2) * augment, a_Event);
		FootStepManager::DoStrongSounds(giant, 1.15f + speed / 20, a_Node);
		FootStepManager::PlayVanillaFootstepSounds(giant, a_Right);

		SetBusyFoot(giant, BusyFoot::None);
	}

	//----------------------------------------------------------------------------------------------
	// Understomp, standing
	//----------------------------------------------------------------------------------------------

	void UnderStompImpact(const ActionContext& a_Ctx, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.75f : 1.0f;

		float damage = giant->IsSneaking() ? 0.75f : 1.0f;

		if (calamity) {
			damage *= 1.25f;
		}

		DoDamageEffect(giant, Damage_Stomp_Under_Light * damage * perk, Radius_Stomp_Strong, 8, 0.30f, a_Event, 1.0f, a_Source, false);
		ImpactRumble(giant, Rumble_Stomp_Under_Light, a_Node, a_Rumble, true, 1.0f);
		DoDustExplosion(giant, smt, a_Event, a_Node);

		DrainStamina(giant, "StaminaDrain_Stomp", Runtime::PERK.GTSPerkDestructionBasics, false, 1.4f);
		StompManager::PlayNewOrOldStomps(giant, smt, a_Event, a_Node, false);
		LaunchTask(giant, 0.825f * perk, 2.10f, a_Event);
		FootStepManager::PlayVanillaFootstepSounds(giant, a_Right);

		SetBusyFoot(giant, BusyFoot::None);

		// There is no grind clip for the crawling stance.
		if (!AnimationVars::Crawl::IsCrawling(giant)) {

			const float chance = giant->IsPlayerRef()
				? Config::Gameplay.ActionSettings.fPlayerUnderstompGrindChance
				: Config::AI.Stomp.fUnderstompGrindProbability;

			if (RandomBool(chance)) {
				FootGrindCheck(giant, Radius_Trample, a_Right, FootActionType::Grind_UnderStomp);
			}
		}
	}

	// The clip's impact annotation is early, so the hit waits a frame.
	void UnderStompStrongImpact(const ActionContext& a_Ctx, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		const float speed = a_Ctx.AnimSpeed();
		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.75f : 1.0f;
		const float damage = calamity ? 1.25f : 1.0f;

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = giant->CreateRefHandle();

		TaskManager::RunFor(std::format(TASKID_FMT_UNDER_STRONG, giant->formID), 1.0f, [=](auto&) {

			auto handleRef = handle.get();

			if (!handleRef) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start <= 0.05) {
				return true;
			}

			auto* ref = handleRef.get();
			const float augment = PerkHandler::Perks_Cataclysmic_EmpowerStomp(ref);
			const bool stacks = PerkHandler::Perks_Cataclysmic_HasStacks(ref);

			DoDamageEffect(ref, Damage_Stomp_Under_Strong * damage * perk * augment, Radius_Stomp_Strong, 8, 0.30f, a_Event, 1.0f, a_Source, false);
			ImpactRumble(ref, Rumble_Stomp_Under_Strong, a_Node, a_Rumble, true, 1.0f);

			if (!stacks) {
				DoDustExplosion(ref, smt * augment, a_Event, a_Node);
			}
			else {
				for (const float exp : { 1.0f, 0.75f, 0.5f }) {
					DoDustExplosion(ref, smt * augment * exp, a_Event, a_Node);
				}
			}

			DrainStamina(ref, "StaminaDrain_StrongStomp", Runtime::PERK.GTSPerkDestructionBasics, false, 5.1f);
			StompManager::PlayNewOrOldStomps(ref, smt, a_Event, a_Node, true);
			LaunchTask(ref, 1.05f * perk * augment, 3.60f * augment, a_Event);
			FootStepManager::DoStrongSounds(ref, 1.10f + speed / 20, a_Node);
			FootStepManager::PlayVanillaFootstepSounds(ref, a_Right);

			SetBusyFoot(ref, BusyFoot::None);

			return false;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Understomp, sneaking: the butt slam
	//----------------------------------------------------------------------------------------------

	void ButtRecoveryStep(Actor* a_Giant, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float smt = calamity ? 1.75f : 1.0f;
		const float damage = calamity ? 1.25f : 1.0f;

		DoDamageEffect(a_Giant, Damage_Walk_Defaut * damage * perk, Radius_Stomp_Strong, 8, 0.30f, a_Event, 1.0f, a_Source, false);
		DoDustExplosion(a_Giant, smt, a_Event, a_Node);
		ImpactRumble(a_Giant, Rumble_Stomp_Under_Strong, a_Node, a_Rumble, true, 0.8f);
		DoFootstepSound(a_Giant, smt, a_Event, a_Node);

		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	void ButtFootTipDamage(Actor* a_Giant, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float smt = calamity ? 1.75f : 1.0f;
		const float damage = calamity ? 1.25f : 1.0f;

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::RunFor(std::format(TASKID_FMT_UNDER_STRONG, a_Giant->formID), 1.0f, [=](auto&) {

			auto refPtr = handle.get();

			if (!refPtr) {
				return false;
			}

			auto* ref = refPtr.get();

			DoDamageEffect(ref, Damage_Stomp_Under_LegLand * damage * perk, Radius_Stomp_Strong, 8, 0.20f, a_Event, 1.0f, a_Source, false);
			DoDustExplosion(ref, smt, a_Event, a_Node);
			ImpactRumble(ref, Rumble_Stomp_Under_Strong, a_Node, "FootTip", true, 1.0f);
			DoFootstepSound(ref, smt, a_Event, a_Node);
			LaunchTask(ref, 0.85f * perk, 1.40f, a_Event);

			return false;
		});
	}

	void ButtImpact(Actor* a_Giant) {

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float dust = calamity ? 1.25f : 1.0f;
		const float smt = calamity ? 1.5f : 1.0f;

		DrainStamina(a_Giant, "StaminaDrain_UnderButtCrush", Runtime::PERK.GTSPerkDestructionBasics, false, 18.0f);

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::RunFor(std::format(TASKID_FMT_BUTT, a_Giant->formID), 1.0f, [=](auto&) {

			auto handleRef = handle.get();

			if (!handleRef) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start <= 0.07) {
				return true;
			}

			auto* ref = handleRef.get();

			auto* thighL = find_node(ref, "NPC L Thigh [LThg]");
			auto* thighR = find_node(ref, "NPC R Thigh [RThg]");
			auto* buttR = find_node(ref, "NPC R Butt");
			auto* buttL = find_node(ref, "NPC L Butt");

			if (!buttR || !buttL || !thighL || !thighR) {

				// Not every body replacer has these. Say so rather than doing nothing.
				Notify("Error: Missing Butt or Thigh Nodes");
				Notify("Error: effects not inflicted");
				Notify("install 3BBB/XP32 Skeleton");
				return false;
			}

			constexpr float damage = 1.15f;
			const float power = Rumble_ButtCrush_UnderStomp_ButtImpact / 2 * dust * damage;

			DoDamageAtPoint(ref, Radius_UnderStomp_Butt_Impact, Damage_ButtCrush_Under_ButtImpact * damage, thighL, 8, 0.35f, 0.925f, DamageSource::Booty);
			DoDamageAtPoint(ref, Radius_UnderStomp_Butt_Impact, Damage_ButtCrush_Under_ButtImpact * damage, thighR, 8, 0.35f, 0.925f, DamageSource::Booty);
			DoDustExplosion(ref, 1.45f * dust * damage, FootEvent::Butt, "NPC R Butt");
			DoDustExplosion(ref, 1.45f * dust * damage, FootEvent::Butt, "NPC L Butt");
			DoFootstepSound(ref, 1.05f, FootEvent::Right, NODE_FOOT_R);
			DoLaunch(ref, 1.65f * perk, 5.0f, FootEvent::Butt);

			Rumbling::Once("Butt_L", ref, power * smt, 0.075f, "NPC R Butt", 0.0f);
			Rumbling::Once("Butt_R", ref, power * smt, 0.075f, "NPC L Butt", 0.0f);

			return false;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Understomp, crawling and strong: the full body drop
	//----------------------------------------------------------------------------------------------

	void BodyImpact(Actor* a_Giant) {

		const float perk = GetPerkBonus_Basics(a_Giant);

		for (const auto& node : BODY_NODES) {

			auto* bone = find_node(a_Giant, node);

			if (!bone) {
				continue;
			}

			DoDamageAtPoint(a_Giant, Radius_BreastCrush_BodyImpact, Damage_Stomp_Under_Breast_Body, bone, 400, 0.10f, 0.8f, DamageSource::BodyCrush);
			Rumbling::Once(std::format("Node: {}", node), a_Giant, 0.6f, 0.035f, node, 0.0f);
			DoLaunch(a_Giant, 1.75f * perk, 3.8f, bone);
		}

		for (const bool side : { true, false }) {
			ApplyThighDamage(a_Giant, side, false, Radius_ThighCrush_Spread_Out, Damage_Stomp_Under_Breast_Legs, 0.15f, 1.0f, 8, DamageSource::ThighCrushed);
		}
	}

	void BreastImpact(Actor* a_Giant) {

		auto* left = find_node(a_Giant, "L Breast03");
		auto* right = find_node(a_Giant, "R Breast03");

		if (!left || !right) {
			left = find_node(a_Giant, "NPC L Breast");
			right = find_node(a_Giant, "NPC R Breast");
		}

		if (!left || !right) {
			Notify("Error: Missing Breast Nodes");
			Notify("Error: effects not inflicted");
			Notify("Suggestion: install Female body replacer");
			return;
		}

		const float perk = GetPerkBonus_Basics(a_Giant);
		const bool calamity = TinyCalamityActive(a_Giant);
		const float dust = calamity ? 1.25f : 1.0f;
		const float smt = calamity ? 1.5f : 1.0f;
		constexpr float damage = 1.25f;
		const float power = Rumble_Cleavage_Impact * dust * damage;

		DoDamageAtPoint(a_Giant, Radius_BreastCrush_BreastImpact, Damage_BreastCrush_BreastImpact * damage, left, 4, 0.70f, 0.8f, DamageSource::BreastImpact);
		DoDamageAtPoint(a_Giant, Radius_BreastCrush_BreastImpact, Damage_BreastCrush_BreastImpact * damage, right, 4, 0.70f, 0.8f, DamageSource::BreastImpact);
		DoDustExplosion(a_Giant, 1.25f * dust + damage / 10, FootEvent::Right, "NPC R Breast");
		DoDustExplosion(a_Giant, 1.25f * dust + damage / 10, FootEvent::Left, "NPC L Breast");
		Rumbling::Once("Breast_L", a_Giant, power * smt, 0.075f, "NPC L Breast", 0.0f);
		Rumbling::Once("Breast_R", a_Giant, power * smt, 0.075f, "NPC R Breast", 0.0f);
		DoFootstepSound(a_Giant, 1.25f, FootEvent::Right, "NPC R Breast");
		DoFootstepSound(a_Giant, 1.25f, FootEvent::Right, "NPC L Breast");
		DoLaunch(a_Giant, 2.25f * perk, 5.0f, FootEvent::Breasts);
	}

	//----------------------------------------------------------------------------------------------
	// Foot grind
	//----------------------------------------------------------------------------------------------

	void CancelGrind(Actor* a_Giant) {

		if (AnimationVars::Action::IsFootGrinding(a_Giant)) {
			AnimationVars::Action::SetIsFootGrinding(a_Giant, false);
		}

		TaskManager::Cancel(std::format(TASKID_FMT_GRIND, a_Giant->formID, "Left_Light"));
		TaskManager::Cancel(std::format(TASKID_FMT_GRIND, a_Giant->formID, "Right_Light"));
		TaskManager::Cancel(std::format(TASKID_FMT_GRIND_DOT, a_Giant->formID));
		TaskManager::Cancel(std::format(TASKID_FMT_GRIND_ROT, a_Giant->formID));
	}

	void StartGrindDamage(Actor* a_Giant, FootEvent a_Event, std::string_view a_Task) {

		ActorHandle handle = a_Giant->CreateRefHandle();
		const std::string rumble = std::format(TASKID_FMT_GRIND_DOT, a_Giant->formID);

		TaskManager::Run(std::format(TASKID_FMT_GRIND, a_Giant->formID, a_Task), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			if (!AnimationVars::Action::IsFootGrinding(giant)) {
				return false;
			}

			Laugh_Chance(giant, 2.2f, "FootGrind");
			Rumbling::Once(rumble, giant, Rumble_FootGrind_DOT, 0.025f, NODE_FOOT_R, 0.0f);

			const float speed = AnimationManager::GetBonusAnimationSpeed(giant) * TimeScale();
			DoDamageEffect(giant, Damage_Foot_Grind_DOT * speed, Radius_Foot_Grind_DOT, 10000, 0.025f, a_Event, 2.5f, DamageSource::FootGrindedRight);

			return true;
		});
	}

	void GrindRotation(Actor* a_Giant, std::string_view a_Node, FootEvent a_Kind, DamageSource a_Source) {

		Laugh_Chance(a_Giant, 2.2f, "FootGrind");

		const float speed = AnimationManager::GetBonusAnimationSpeed(a_Giant);
		const bool sneaking = a_Giant->IsSneaking();

		// The sneak clip rotates fewer times, so each one counts for more.
		const float dot = sneaking ? Damage_Foot_Grind_Rotate * 2.25f : Damage_Foot_Grind_Rotate;
		const float ring = sneaking ? 0.9f * 0.85f : 0.9f;

		Rumbling::Once(std::format(TASKID_FMT_GRIND_ROT, a_Giant->formID), a_Giant, Rumble_FootGrind_Rotate * speed, 0.025f, a_Node, 0.0f);
		DoDamageEffect(a_Giant, dot, Radius_Foot_Grind_DOT, 10, 0.15f, a_Kind, 1.6f, a_Source);
		ApplyDustRing(a_Giant, a_Kind, a_Node, ring);
	}

	void GrindImpact(Actor* a_Giant, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		const float perk = GetPerkBonus_Basics(a_Giant);

		ApplyDustRing(a_Giant, a_Event, a_Node, 1.05f);
		StompManager::PlayNewOrOldStomps(a_Giant, 1.0f, a_Event, a_Node, false);
		DoDamageEffect(a_Giant, Damage_Foot_Grind_Impact, Radius_Foot_Grind_Impact, 20, 0.15f, a_Event, 1.0f, a_Source);
		LaunchTask(a_Giant, 0.75f * perk, 1.35f * perk, a_Event);
		DamageAV(a_Giant, ActorValue::kStamina, 30.0f * GetWasteMult(a_Giant));

		float power = Rumble_FootGrind_Impact * GetHighHeelsBonusDamage(a_Giant, true);

		if (TinyCalamityActive(a_Giant)) {
			power *= 1.5f;
		}

		Rumbling::Once(a_Rumble, a_Giant, power, 0.05f, a_Node, 0.0f);
		FootStepManager::PlayVanillaFootstepSounds(a_Giant, a_Right);
	}

	//----------------------------------------------------------------------------------------------
	// Trample intro
	//----------------------------------------------------------------------------------------------

	void TrampleFirstHit(const ActionContext& a_Ctx, bool a_Right, FootEvent a_Event, DamageSource a_Source, std::string_view a_Node, std::string_view a_Rumble) {

		auto* giant = a_Ctx.Actor();
		const float perk = GetPerkBonus_Basics(giant);
		const bool calamity = TinyCalamityActive(giant);
		const float smt = calamity ? 1.5f : 1.0f;
		const float dust = calamity ? 1.25f : 1.0f;

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = giant->CreateRefHandle();

		TaskManager::RunFor(std::format("TrampleAttack_{}", giant->formID), 1.0f, [=](auto&) {

			auto handleRef = handle.get();

			if (!handleRef) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start <= 0.06) {
				return true;
			}

			auto* ref = handleRef.get();

			DoDamageEffect(ref, Damage_Trample * perk, Radius_Trample, 100, 0.10f, a_Event, 1.10f, a_Source);
			DrainStamina(ref, "StaminaDrain_Trample", Runtime::PERK.GTSPerkDestructionBasics, true, 0.6f);

			Rumbling::Once(a_Rumble, ref, Rumble_Trample_Stage1 * smt * GetHighHeelsBonusDamage(ref, true), 0.0f, a_Node, 0.0f);
			DoDustExplosion(ref, dust * smt, a_Event, a_Node);
			StompManager::PlayNewOrOldStomps(ref, 1.0f, a_Event, a_Node, false);

			FootGrindCheck(ref, Radius_Trample, a_Right, FootActionType::Trample_NormalOrUnder);
			DelayedLaunch(ref, TASK_LAUNCH_TRAMPLE, 0.65f * perk, 3.0f * perk, a_Event);
			FootStepManager::PlayVanillaFootstepSounds(ref, a_Right);

			return false;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Animation speed
	//----------------------------------------------------------------------------------------------

	void SetSpeed(const ActionContext& a_Ctx, float a_Speed, bool a_Editable) {
		a_Ctx.SetCanEditAnimSpeed(a_Editable);
		a_Ctx.SetAnimSpeed(a_Speed);
	}

	void ResetSpeed(const ActionContext& a_Ctx) {
		SetSpeed(a_Ctx, 1.0f, false);
	}

	// The actor level half of PerkHandler::Perks_Cataclysmic_BuffStompSpeed. The perk handler's own
	// version takes the old AnimationEventData, which a node does not have.
	void CataclysmicSpeed(const ActionContext& a_Ctx, bool a_Reset) {

		auto* giant = a_Ctx.Actor();

		if (!a_Reset && PerkHandler::Perks_Cataclysmic_HasStacks(giant)) {
			const float legendary = std::clamp(GetLegendaryLevel(giant), 0.0f, 2.0f);
			SetSpeed(a_Ctx, a_Ctx.AnimSpeed() * (1.265f + (0.265f * legendary)), true);
		}
		else {
			ResetSpeed(a_Ctx);
		}
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: light stomp
	//----------------------------------------------------------------------------------------------

	void OnStompStart(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();

		DrainStamina(giant, "StaminaDrain_Stomp", Runtime::PERK.GTSPerkDestructionBasics, true, 1.8f);
		Rumbling::Start(a_Right ? "StompR_Loop" : "StompL_Loop", giant, 0.25f, 0.15f, a_Right ? NODE_FOOT_R : NODE_FOOT_L);
		ManageCamera(giant, true, a_Right ? CameraTracking::R_Foot : CameraTracking::L_Foot);
		SetBusyFoot(giant, a_Right ? BusyFoot::RightFoot : BusyFoot::LeftFoot);

		SetSpeed(a_Ctx, giant->IsPlayerRef() ? 1.35f : 1.35f + GetRandomBoost() / 2, true);
	}

	void OnStompStartR(const ActionContext& a_Ctx) { OnStompStart(a_Ctx, true); }
	void OnStompStartL(const ActionContext& a_Ctx) { OnStompStart(a_Ctx, false); }

	void OnStompImpactR(const ActionContext& a_Ctx) {
		StompImpact(a_Ctx, true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "StompR");
		StopLoopRumble(a_Ctx.Actor());
	}

	void OnStompImpactL(const ActionContext& a_Ctx) {
		StompImpact(a_Ctx, false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "StompL");
		StopLoopRumble(a_Ctx.Actor());
	}

	void OnStompLandR(const ActionContext& a_Ctx) {
		StompLand(a_Ctx, true, FootEvent::Right, NODE_FOOT_R, "StompLand");
		StopLoopRumble(a_Ctx.Actor());
	}

	void OnStompLandL(const ActionContext& a_Ctx) {
		StompLand(a_Ctx, false, FootEvent::Left, NODE_FOOT_L, "StompLand");
		StopLoopRumble(a_Ctx.Actor());
	}

	void OnStompEnd(const ActionContext& a_Ctx) {
		ResetSpeed(a_Ctx);
	}

	void OnNext(const ActionContext& a_Ctx) {
		StopLoopRumble(a_Ctx.Actor());
		Rumbling::Stop("StompR", a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: heavy stomp
	//----------------------------------------------------------------------------------------------

	void OnStrongStart(const ActionContext& a_Ctx) {
		SetSpeed(a_Ctx, 1.35f, false);
	}

	void OnStrongLegStart(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();

		if (!giant->IsPlayerRef()) {
			SetSpeed(a_Ctx, a_Ctx.AnimSpeed() + GetRandomBoost() / 3, true);
		}
		else {
			a_Ctx.SetCanEditAnimSpeed(true);
		}

		ManageCamera(giant, true, a_Right ? CameraTracking::R_Foot : CameraTracking::L_Foot);
		DrainStamina(giant, "StaminaDrain_StrongStomp", Runtime::PERK.GTSPerkDestructionBasics, true, 3.4f);
		SetBusyFoot(giant, a_Right ? BusyFoot::RightFoot : BusyFoot::LeftFoot);

		CataclysmicSpeed(a_Ctx, false);
	}

	void OnStrongLegStartR(const ActionContext& a_Ctx) { OnStrongLegStart(a_Ctx, true); }
	void OnStrongLegStartL(const ActionContext& a_Ctx) { OnStrongLegStart(a_Ctx, false); }

	void OnStrongLegMiddle(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		SetSpeed(a_Ctx, giant->IsPlayerRef() ? 1.55f : 1.55f + GetRandomBoost(), a_Ctx.AnimSpeed() != 1.0f);
		CataclysmicSpeed(a_Ctx, false);
	}

	void OnStrongLegEndR(const ActionContext& a_Ctx) { LegRumble(a_Ctx.Actor(), "StrongStompR", true, false); }
	void OnStrongLegEndL(const ActionContext& a_Ctx) { LegRumble(a_Ctx.Actor(), "StrongStompL", false, false); }

	void OnStrongImpactR(const ActionContext& a_Ctx) {
		StrongStompImpact(a_Ctx, true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "HeavyStompR");
		CataclysmicSpeed(a_Ctx, true);
	}

	void OnStrongImpactL(const ActionContext& a_Ctx) {
		StrongStompImpact(a_Ctx, false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "HeavyStompL");
		CataclysmicSpeed(a_Ctx, true);
	}

	void OnStrongReturnStartR(const ActionContext& a_Ctx) { LegRumble(a_Ctx.Actor(), "StrongStompR", true, true, 0.25f, 0.10f); }
	void OnStrongReturnStartL(const ActionContext& a_Ctx) { LegRumble(a_Ctx.Actor(), "StrongStompL", false, true, 0.25f, 0.10f); }

	void OnStrongReturnEndR(const ActionContext& a_Ctx) {
		LegRumble(a_Ctx.Actor(), "StrongStompR", true, false);
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::R_Foot);
	}

	void OnStrongReturnEndL(const ActionContext& a_Ctx) {
		LegRumble(a_Ctx.Actor(), "StrongStompL", false, false);
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::L_Foot);
	}

	void OnStrongEnd(const ActionContext& a_Ctx) {
		LegRumble(a_Ctx.Actor(), "StrongStompR", true, false);
		LegRumble(a_Ctx.Actor(), "StrongStompL", false, false);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: understomp, standing
	//----------------------------------------------------------------------------------------------

	void OnUnderCamOn(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();

		DrainStamina(giant, "StaminaDrain_Stomp", Runtime::PERK.GTSPerkDestructionBasics, true, 1.4f);
		ManageCamera(giant, true, a_Right ? CameraTracking::R_Foot : CameraTracking::L_Foot);
		SetBusyFoot(giant, a_Right ? BusyFoot::RightFoot : BusyFoot::LeftFoot);
		SetSpeed(a_Ctx, 1.125f, true);
	}

	void OnUnderCamOnR(const ActionContext& a_Ctx) { OnUnderCamOn(a_Ctx, true); }
	void OnUnderCamOnL(const ActionContext& a_Ctx) { OnUnderCamOn(a_Ctx, false); }

	void OnUnderCamOffR(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::R_Foot);
		ResetSpeed(a_Ctx);
	}

	void OnUnderCamOffL(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::L_Foot);
		ResetSpeed(a_Ctx);
	}

	void OnUnderImpactR(const ActionContext& a_Ctx) {
		UnderStompImpact(a_Ctx, true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "HeavyStompR");
	}

	void OnUnderImpactL(const ActionContext& a_Ctx) {
		UnderStompImpact(a_Ctx, false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "HeavyStompL");
	}

	void OnUnderStrongCamOn(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();

		DrainStamina(giant, "StaminaDrain_StrongStomp", Runtime::PERK.GTSPerkDestructionBasics, true, 5.1f);
		ManageCamera(giant, true, a_Right ? CameraTracking::R_Foot : CameraTracking::L_Foot);
		SetBusyFoot(giant, a_Right ? BusyFoot::RightFoot : BusyFoot::LeftFoot);

		CataclysmicSpeed(a_Ctx, false);
	}

	void OnUnderStrongCamOnR(const ActionContext& a_Ctx) { OnUnderStrongCamOn(a_Ctx, true); }
	void OnUnderStrongCamOnL(const ActionContext& a_Ctx) { OnUnderStrongCamOn(a_Ctx, false); }

	void OnUnderStrongCamOffR(const ActionContext& a_Ctx) { ManageCamera(a_Ctx.Actor(), false, CameraTracking::R_Foot); }
	void OnUnderStrongCamOffL(const ActionContext& a_Ctx) { ManageCamera(a_Ctx.Actor(), false, CameraTracking::L_Foot); }

	void OnUnderStrongImpactR(const ActionContext& a_Ctx) {
		UnderStompStrongImpact(a_Ctx, true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "HeavyStompR");
		CataclysmicSpeed(a_Ctx, true);
	}

	void OnUnderStrongImpactL(const ActionContext& a_Ctx) {
		UnderStompStrongImpact(a_Ctx, false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "HeavyStompL");
		CataclysmicSpeed(a_Ctx, true);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: understomp, sneaking
	//----------------------------------------------------------------------------------------------

	void OnButtCamOn(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_UnderButtCrush", Runtime::PERK.GTSPerkDestructionBasics, true, 18.0f);
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Butt);
	}

	void OnButtCamOff(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Butt);
		a_Ctx.RequestExit();
	}

	void OnButtDisableHH(const ActionContext& a_Ctx) { a_Ctx.SetHHDisabled(true, 6.0f); }
	void OnButtEnableHH(const ActionContext& a_Ctx)  { a_Ctx.SetHHDisabled(false, 3.25f); }

	void OnButtRecFootstepL(const ActionContext& a_Ctx) {
		ButtRecoveryStep(a_Ctx.Actor(), false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "RecFSL");
	}

	void OnButtRecFootstepR(const ActionContext& a_Ctx) {
		ButtRecoveryStep(a_Ctx.Actor(), true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "RecFSR");
	}

	void OnButtImpact(const ActionContext& a_Ctx) {
		ButtImpact(a_Ctx.Actor());
	}

	void OnButtLegImpact(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		for (const bool side : { true, false }) {
			ApplyThighDamage(giant, side, false, Radius_ThighCrush_Spread_In, Damage_Stomp_Under_LegLand, 0.15f, 1.0f, 8, DamageSource::ThighCrushed);
		}

		ButtFootTipDamage(giant, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L);
		ButtFootTipDamage(giant, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: understomp, crawling
	//----------------------------------------------------------------------------------------------

	void OnCrawlSlamCamOn(const ActionContext& a_Ctx, bool a_Right) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_HandSlam", Runtime::PERK.GTSPerkDestructionBasics, true, 1.5f);
		ManageCamera(a_Ctx.Actor(), true, a_Right ? CameraTracking::ForearmTwist_2_Right : CameraTracking::ForearmTwist_2_Left);
	}

	void OnCrawlSlamCamOnR(const ActionContext& a_Ctx) { OnCrawlSlamCamOn(a_Ctx, true); }
	void OnCrawlSlamCamOnL(const ActionContext& a_Ctx) { OnCrawlSlamCamOn(a_Ctx, false); }

	void OnCrawlSlamCamOffR(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::ForearmTwist_2_Right);
		a_Ctx.RequestExit();
	}

	void OnCrawlSlamCamOffL(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::ForearmTwist_2_Left);
		a_Ctx.RequestExit();
	}

	void OnCrawlSlamImpact(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.05f, Damage_Sneak_HandSlam_Sneak,
			a_Right ? CrawlEvent::RightHand : CrawlEvent::LeftHand,
			a_Right ? "RightHandRumble" : "LeftHandRumble",
			0.9f, Radius_Sneak_HandSlam, 1.3f,
			a_Right ? DamageSource::HandSlamRight : DamageSource::HandSlamLeft);

		DrainStamina(giant, "StaminaDrain_HandSlam", Runtime::PERK.GTSPerkDestructionBasics, false, 1.5f);
	}

	void OnCrawlSlamImpactR(const ActionContext& a_Ctx) { OnCrawlSlamImpact(a_Ctx, true); }
	void OnCrawlSlamImpactL(const ActionContext& a_Ctx) { OnCrawlSlamImpact(a_Ctx, false); }

	void OnCrawlBodyCamOn(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_BreastAttack", Runtime::PERK.GTSPerkDestructionBasics, true, 20.0f);
		ManageCamera(a_Ctx.Actor(), true, CameraTracking::Breasts_02);
	}

	void OnCrawlBodyCamOff(const ActionContext& a_Ctx) {
		ManageCamera(a_Ctx.Actor(), false, CameraTracking::Breasts_02);
		a_Ctx.RequestExit();
	}

	void OnCrawlBodyImpact(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_BreastAttack", Runtime::PERK.GTSPerkDestructionBasics, false, 20.0f);
		BreastImpact(a_Ctx.Actor());
		BodyImpact(a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: foot grind
	//----------------------------------------------------------------------------------------------

	void OnGrindEnter(const ActionContext& a_Ctx, FootEvent a_Event, std::string_view a_Task) {

		StompNode::GetOrAdd(a_Ctx.Owner()).GrindRotations = 0;

		SetSpeed(a_Ctx, 1.0f, true);
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_FootGrind", Runtime::PERK.GTSPerkDestructionBasics, true, 0.25f);
		StartGrindDamage(a_Ctx.Actor(), a_Event, a_Task);
	}

	void OnGrindEnterL(const ActionContext& a_Ctx) { OnGrindEnter(a_Ctx, FootEvent::Left, "Left_Light"); }
	void OnGrindEnterR(const ActionContext& a_Ctx) { OnGrindEnter(a_Ctx, FootEvent::Right, "Right_Light"); }

	void OnGrindRotateL(const ActionContext& a_Ctx) {
		GrindRotation(a_Ctx.Actor(), NODE_FOOT_L, FootEvent::Left, DamageSource::FootGrindedLeft);
		++StompNode::GetOrAdd(a_Ctx.Owner()).GrindRotations;
	}

	void OnGrindRotateR(const ActionContext& a_Ctx) {
		GrindRotation(a_Ctx.Actor(), NODE_FOOT_R, FootEvent::Right, DamageSource::FootGrindedRight);
		++StompNode::GetOrAdd(a_Ctx.Owner()).GrindRotations;
	}

	// Sonderbain's grind clip fires its exit late, so the tiny stays stuck to the foot for about a
	// second after the grind is over. Counting the rotations is how that clip is told apart from the
	// alternative one, which does not need it.
	void ReleaseStuckTiny(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (AnimationVars::Stomp::IsAlternativeGrindEnabled(giant) || AnimationVars::Action::IsUnderGrinding(giant)) {
			return;
		}

		State& state = StompNode::GetOrAdd(a_Ctx.Owner());

		if (state.GrindRotations >= 6) {
			CancelGrind(giant);
			state.GrindRotations = 0;
		}
	}

	void OnGrindMoveEndL(const ActionContext& a_Ctx) {
		ApplyDustRing(a_Ctx.Actor(), FootEvent::Left, NODE_FOOT_L, 0.9f);
		ReleaseStuckTiny(a_Ctx);
	}

	void OnGrindMoveEndR(const ActionContext& a_Ctx) {
		ApplyDustRing(a_Ctx.Actor(), FootEvent::Right, NODE_FOOT_R, 0.9f);
		ReleaseStuckTiny(a_Ctx);
	}

	void OnGrindImpactL(const ActionContext& a_Ctx) {
		GrindImpact(a_Ctx.Actor(), false, FootEvent::Left, DamageSource::FootGrindedLeft_Impact, NODE_FOOT_L, "GrindStompL");
	}

	void OnGrindImpactR(const ActionContext& a_Ctx) {
		GrindImpact(a_Ctx.Actor(), true, FootEvent::Right, DamageSource::FootGrindedRight_Impact, NODE_FOOT_R, "GrindStompR");
	}

	void OnGrindExit(const ActionContext& a_Ctx) {
		DrainStamina(a_Ctx.Actor(), "StaminaDrain_FootGrind", Runtime::PERK.GTSPerkDestructionBasics, false, 0.25f);
		CancelGrind(a_Ctx.Actor());
		StompNode::GetOrAdd(a_Ctx.Owner()).GrindRotations = 0;
		ResetSpeed(a_Ctx);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: trample intro
	//----------------------------------------------------------------------------------------------

	void OnTrampleRaise(const ActionContext& a_Ctx, bool a_Right) {

		a_Ctx.SetCanEditAnimSpeed(false);

		if (a_Ctx.AnimSpeed() == 1.0f) {
			a_Ctx.SetAnimSpeed(1.3f);
		}

		SetBusyFoot(a_Ctx.Actor(), a_Right ? BusyFoot::RightFoot : BusyFoot::LeftFoot);
	}

	void OnTrampleRaiseR(const ActionContext& a_Ctx) { OnTrampleRaise(a_Ctx, true); }
	void OnTrampleRaiseL(const ActionContext& a_Ctx) { OnTrampleRaise(a_Ctx, false); }

	void OnTrampleCamStartR(const ActionContext& a_Ctx) { ManageCamera(a_Ctx.Actor(), true, CameraTracking::R_Foot); }
	void OnTrampleCamStartL(const ActionContext& a_Ctx) { ManageCamera(a_Ctx.Actor(), true, CameraTracking::L_Foot); }

	void OnTrampleFootstepR(const ActionContext& a_Ctx) {
		TrampleFirstHit(a_Ctx, true, FootEvent::Right, DamageSource::CrushedRight, NODE_FOOT_R, "TrampleR");
		SetBusyFoot(a_Ctx.Actor(), BusyFoot::None);
		ResetSpeed(a_Ctx);
	}

	void OnTrampleFootstepL(const ActionContext& a_Ctx) {
		TrampleFirstHit(a_Ctx, false, FootEvent::Left, DamageSource::CrushedLeft, NODE_FOOT_L, "TrampleL");
		SetBusyFoot(a_Ctx.Actor(), BusyFoot::None);
		ResetSpeed(a_Ctx);
	}

	//----------------------------------------------------------------------------------------------
	// Hand slam, the sneak and crawl form of both stomps
	//----------------------------------------------------------------------------------------------

	constexpr std::string_view NODE_FINGER_R = "NPC R Finger12 [RF12]";
	constexpr std::string_view NODE_FINGER_L = "NPC L Finger12 [LF12]";

	constexpr std::string_view TASKID_FMT_FINGER = "FingerShrink_{}";
	constexpr std::string_view TASKID_FMT_FINGER_CHECK = "FingerGrindCheck_{}_{}";

	void TrackHand(Actor* a_Giant, bool a_Right, bool a_Enable) {
		ManageCamera(a_Giant, a_Enable, a_Right ? CameraTracking::ForearmTwist_2_Right : CameraTracking::ForearmTwist_2_Left);
	}

	void StopSlamStamina(Actor* a_Giant) {
		DrainStamina(a_Giant, "StaminaDrain_StrongSneakSlam", Runtime::PERK.GTSPerkDestructionBasics, false, 2.2f);
		DrainStamina(a_Giant, "StaminaDrain_FingerGrind", Runtime::PERK.GTSPerkDestructionBasics, false, 0.8f);
		DrainStamina(a_Giant, "StaminaDrain_SneakSlam", Runtime::PERK.GTSPerkDestructionBasics, false, 1.4f);
	}

	void FingerDamage(Actor* a_Giant, DamageSource a_Source, bool a_Right, float a_Radius, float a_Damage, float a_Crush, float a_Shrink) {

		const std::string_view node = a_Right ? NODE_FINGER_R : NODE_FINGER_L;
		const DamageSource source = a_Right ? a_Source : DamageSource::LeftFinger;

		ApplyFingerDamage(a_Giant, a_Radius, a_Damage, find_node(a_Giant, node), 50, 0.10f, a_Crush, -0.038f * a_Shrink, source);
	}

	void FingerSounds(Actor* a_Giant, std::string_view a_Node, float a_Mult) {

		if (auto* node = find_node(a_Giant, a_Node)) {
			DoCrawlingSounds(a_Giant, get_visual_scale(a_Giant) * 0.72f * a_Mult, node, FootEvent::Left);
		}
	}

	// A dust puff on the ground under the finger rather than at the finger, so it lands on the floor
	// when the giant is bent over it.
	void FingerVisuals(Actor* a_Giant, std::string_view a_Node, float a_Threshold, float a_Multiplier) {

		auto* node = find_node(a_Giant, a_Node);

		if (!node) {
			return;
		}

		float scale = get_visual_scale(a_Giant);
		float multiplier = a_Multiplier;

		if (TinyCalamityActive(a_Giant)) {
			scale += 2.6f;
			multiplier *= 0.33f;
		}

		if (scale < a_Threshold || a_Giant->AsActorState()->IsSwimming()) {
			return;
		}

		const NiPoint3 location = node->world.translate;
		const NiPoint3 start = location + NiPoint3(0.0f, 0.0f, MeterToGameUnit(-0.05f * scale));

		bool success = false;
		NiPoint3 spawn = CastRay(a_Giant, start, NiPoint3(0.0f, 0.0f, -1.0f), MeterToGameUnit(std::max(1.05f * scale, 1.05f)), success);

		if (!success) {
			spawn = location;
			spawn.z = a_Giant->GetPosition().z;
		}

		const bool allowed = a_Giant->IsPlayerRef() ? Config::Gameplay.bPlayerAnimEffects : Config::Gameplay.bNPCAnimEffects;

		if (allowed) {
			SpawnParticle(a_Giant, 4.60f, "GTS/Effects/Footstep.nif", NiMatrix3(), spawn, (scale * multiplier) * 1.8f, 7, nullptr);
		}
	}

	void StartFingerShrink(Actor* a_Giant, bool a_Right) {

		ActorHandle handle = a_Giant->CreateRefHandle();
		const std::string_view node = a_Right ? NODE_FINGER_R : NODE_FINGER_L;
		const DamageSource source = a_Right ? DamageSource::RightFinger : DamageSource::LeftFinger;

		TaskManager::Run(std::format(TASKID_FMT_FINGER, a_Giant->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			if (!AnimationVars::Action::IsFootGrinding(giant)) {
				return false;
			}

			ApplyFingerDamage(giant, Radius_Sneak_FingerGrind_DOT, Damage_Sneak_FingerGrind_DOT, find_node(giant, node), 200, 0.05f, 3.0f, -0.0004f, source);
			return true;
		});
	}

	// A frame late, so an actor the slam just killed is already dead and does not get ground as well.
	void CheckForFingerGrind(Actor* a_Giant, CrawlEvent a_Event, bool a_Right, std::string_view a_Tag) {

		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::RunOnce(std::format(TASKID_FMT_FINGER_CHECK, a_Giant->formID, a_Tag), [=](auto&) {

			if (!handle) {
				return;
			}

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return;
			}

			auto* giant = giantPtr.get();

			FingerGrindCheck(giant, a_Event, a_Right, Radius_Sneak_HandSlam);
			StartFingerShrink(giant, a_Right);
		});
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: hand slam
	//----------------------------------------------------------------------------------------------

	void OnSlamRaise(const ActionContext& a_Ctx, bool a_Right, bool a_Strong) {

		auto* giant = a_Ctx.Actor();

		Utils_UpdateHighHeelBlend(giant, false);
		TrackHand(giant, a_Right, true);

		if (a_Strong) {
			DrainStamina(giant, "StaminaDrain_StrongSneakSlam", Runtime::PERK.GTSPerkDestructionBasics, true, 2.2f);
		}
		else {
			DrainStamina(giant, "StaminaDrain_SneakSlam", Runtime::PERK.GTSPerkDestructionBasics, true, 1.4f);
		}
	}

	void OnSlamRaiseR(const ActionContext& a_Ctx) { OnSlamRaise(a_Ctx, true, false); }
	void OnSlamRaiseL(const ActionContext& a_Ctx) { OnSlamRaise(a_Ctx, false, false); }
	void OnSlamStrongRaiseR(const ActionContext& a_Ctx) { OnSlamRaise(a_Ctx, true, true); }
	void OnSlamStrongRaiseL(const ActionContext& a_Ctx) { OnSlamRaise(a_Ctx, false, true); }

	void OnSlamImpactR(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.15f, Damage_Sneak_HandSlam, CrawlEvent::RightHand, "RightHandRumble", 0.9f, Radius_Sneak_HandSlam, 1.35f, DamageSource::HandSlamRight);
		CheckForFingerGrind(giant, CrawlEvent::RightHand, true, "RH");
	}

	void OnSlamImpactL(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.15f, Damage_Sneak_HandSlam, CrawlEvent::LeftHand, "LeftHandRumble", 0.9f, Radius_Sneak_HandSlam, 1.35f, DamageSource::HandSlamLeft);

		// The left hand is the one that carries. Whoever is in it takes the hit instead of the ground,
		// and there is nothing to grind afterwards.
		if (Possession::Occupied(a_Ctx.Owner(), PossessionSlot::kHand)) {
			Grabbing::DamageActorInHand(giant, Damage_Sneak_HandSlam * 0.6f);
			return;
		}

		CheckForFingerGrind(giant, CrawlEvent::LeftHand, false, "LH");
	}

	void OnSlamCamOffR(const ActionContext& a_Ctx) {
		TrackHand(a_Ctx.Actor(), true, false);
		StopSlamStamina(a_Ctx.Actor());
		a_Ctx.RequestExit();
	}

	void OnSlamCamOffL(const ActionContext& a_Ctx) {
		TrackHand(a_Ctx.Actor(), false, false);
		StopSlamStamina(a_Ctx.Actor());
		a_Ctx.RequestExit();
	}

	void OnSlamStrongImpactR(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 2.75f, Damage_Sneak_HandSlam_Strong, CrawlEvent::RightHand, "RightHandRumble", 1.6f, Radius_Sneak_HandSlam_Strong, 1.0f, DamageSource::HandSlamRight);
		DrainStamina(giant, "StaminaDrain_StrongSneakSlam", Runtime::PERK.GTSPerkDestructionBasics, false, 2.2f);
	}

	void OnSlamStrongImpactL(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 2.75f, Damage_Sneak_HandSlam_Strong, CrawlEvent::LeftHand, "LeftHandRumble", 1.6f, Radius_Sneak_HandSlam_Strong, 1.0f, DamageSource::HandSlamLeft);
		DrainStamina(giant, "StaminaDrain_StrongSneakSlam", Runtime::PERK.GTSPerkDestructionBasics, false, 2.2f);
		Grabbing::DamageActorInHand(giant, Damage_Sneak_HandSlam_Strong * 0.6f);
	}

	void OnSlamStrongSecondaryR(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DoCrawlingFunctions(giant, get_visual_scale(giant) * 0.8f, 0.85f, Damage_Sneak_HandSlam_Strong_Secondary, CrawlEvent::RightHand, "RightHandRumble", 0.8f, Radius_Sneak_HandSlam_Strong_Recover, 2.0f, DamageSource::HandSlamRight);
	}

	void OnSlamStrongSecondaryL(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DoCrawlingFunctions(giant, get_visual_scale(giant) * 0.8f, 0.85f, Damage_Sneak_HandSlam_Strong_Secondary, CrawlEvent::LeftHand, "LeftHandRumble", 0.8f, Radius_Sneak_HandSlam_Strong_Recover, 2.0f, DamageSource::HandSlamLeft);
		Grabbing::DamageActorInHand(giant, Damage_Sneak_HandSlam_Strong_Secondary * 0.6f);
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: crawl slam, which is the same attack with its own tags
	//----------------------------------------------------------------------------------------------

	void OnCrawlHandRaise(const ActionContext& a_Ctx, bool a_Right, bool a_Strong) {

		auto* giant = a_Ctx.Actor();

		DrainStamina(giant, a_Strong ? "StaminaDrain_CrawlStompStrong" : "StaminaDrain_CrawlStomp", Runtime::PERK.GTSPerkDestructionBasics, true, a_Strong ? 2.3f : 1.4f);

		if (Config::General.bTrackBonesDuringAnim) {
			TrackHand(giant, a_Right, true);
		}
	}

	void OnCrawlHandRaiseR(const ActionContext& a_Ctx) { OnCrawlHandRaise(a_Ctx, true, false); }
	void OnCrawlHandRaiseL(const ActionContext& a_Ctx) { OnCrawlHandRaise(a_Ctx, false, false); }
	void OnCrawlHandStrongRaiseR(const ActionContext& a_Ctx) { OnCrawlHandRaise(a_Ctx, true, true); }
	void OnCrawlHandStrongRaiseL(const ActionContext& a_Ctx) { OnCrawlHandRaise(a_Ctx, false, true); }

	void StopCrawlSlamStamina(Actor* a_Giant) {
		DrainStamina(a_Giant, "StaminaDrain_CrawlStomp", Runtime::PERK.GTSPerkDestructionBasics, false, 1.4f);
		DrainStamina(a_Giant, "StaminaDrain_CrawlStompStrong", Runtime::PERK.GTSPerkDestructionBasics, false, 2.3f);
	}

	void OnCrawlSlamHitR(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.4f, Damage_Crawl_HandSlam, CrawlEvent::RightHand, "RightHandRumble", 0.9f, Radius_Crawl_Slam, 1.15f, DamageSource::HandSlamRight);
		StopCrawlSlamStamina(giant);
	}

	void OnCrawlSlamHitL(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.4f, Damage_Crawl_HandSlam, CrawlEvent::LeftHand, "LeftHandRumble", 0.9f, Radius_Crawl_Slam, 1.15f, DamageSource::HandSlamLeft);
		StopCrawlSlamStamina(giant);
		Grabbing::DamageActorInHand(giant, Damage_Crawl_HandSlam * 0.6f);
	}

	void OnCrawlSlamStrongHitR(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 2.1f, Damage_Crawl_HandSlam_Strong, CrawlEvent::RightHand, "RightHandRumble", 1.2f, Radius_Crawl_Slam_Strong, 1.0f, DamageSource::HandSlamRight);
		StopCrawlSlamStamina(giant);
	}

	void OnCrawlSlamStrongHitL(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		DoCrawlingFunctions(giant, get_visual_scale(giant), 2.1f, Damage_Crawl_HandSlam_Strong, CrawlEvent::LeftHand, "LeftHandRumble", 1.2f, Radius_Crawl_Slam_Strong, 1.0f, DamageSource::HandSlamLeft);
		StopCrawlSlamStamina(giant);
		Grabbing::DamageActorInHand(giant, Damage_Crawl_HandSlam_Strong * 0.6f);
	}

	void OnCrawlSlamCamEndR(const ActionContext& a_Ctx) {

		if (Config::General.bTrackBonesDuringAnim) {
			TrackHand(a_Ctx.Actor(), true, false);
		}

		a_Ctx.RequestExit();
	}

	void OnCrawlSlamCamEndL(const ActionContext& a_Ctx) {

		if (Config::General.bTrackBonesDuringAnim) {
			TrackHand(a_Ctx.Actor(), false, false);
		}

		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Annotations: finger grind, the second animation after a sneak slam
	//----------------------------------------------------------------------------------------------

	void OnFingerCamOnR(const ActionContext& a_Ctx) { TrackHand(a_Ctx.Actor(), true, true); }
	void OnFingerCamOnL(const ActionContext& a_Ctx) { TrackHand(a_Ctx.Actor(), false, true); }

	void OnFingerImpact(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();
		const std::string_view node = a_Right ? NODE_FINGER_R : NODE_FINGER_L;

		FingerDamage(giant, a_Right ? DamageSource::RightFinger_Impact : DamageSource::LeftFinger_Impact, a_Right, Radius_Sneak_FingerGrind_Impact, Damage_Sneak_FingerGrind_Impact, 2.8f, 1.2f);
		FingerSounds(giant, node, 1.0f);
		FingerVisuals(giant, node, 2.6f, 1.0f);

		Rumbling::Once("Finger", giant, Rumble_FingerGrind_Impact, 0.025f, node, 0.0f);
		DrainStamina(giant, "StaminaDrain_FingerGrind", Runtime::PERK.GTSPerkDestructionBasics, true, 0.8f);
	}

	void OnFingerImpactR(const ActionContext& a_Ctx) { OnFingerImpact(a_Ctx, true); }
	void OnFingerImpactL(const ActionContext& a_Ctx) { OnFingerImpact(a_Ctx, false); }

	void OnFingerRotation(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();
		const std::string_view node = a_Right ? NODE_FINGER_R : NODE_FINGER_L;

		FingerDamage(giant, a_Right ? DamageSource::RightFinger : DamageSource::LeftFinger, a_Right, Radius_Sneak_FingerGrind_DOT, Damage_Sneak_FingerGrind_DOT, 3.2f, 0.8f);
		Rumbling::Once("FingerROT", giant, Rumble_FingerGrind_Rotate, 0.025f, node, 0.0f);
		FingerVisuals(giant, node, 2.6f, 0.85f);
	}

	void OnFingerRotationR(const ActionContext& a_Ctx) { OnFingerRotation(a_Ctx, true); }
	void OnFingerRotationL(const ActionContext& a_Ctx) { OnFingerRotation(a_Ctx, false); }

	void OnFingerFinisher(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();
		const std::string_view node = a_Right ? NODE_FINGER_R : NODE_FINGER_L;

		FingerDamage(giant, a_Right ? DamageSource::RightFinger_Impact : DamageSource::LeftFinger, a_Right, Radius_Sneak_FingerGrind_Finisher, Damage_Sneak_FingerGrind_Finisher, 2.4f, 4.0f);
		Rumbling::Once("FingerFIN", giant, Rumble_FingerGrind_Finisher, 0.045f, node, 0.0f);
		FingerVisuals(giant, node, 2.6f, 1.25f);
		FingerSounds(giant, node, 1.5f);
		StopSlamStamina(giant);
	}

	void OnFingerFinisherR(const ActionContext& a_Ctx) { OnFingerFinisher(a_Ctx, true); }
	void OnFingerFinisherL(const ActionContext& a_Ctx) { OnFingerFinisher(a_Ctx, false); }

	void OnFingerCamOffR(const ActionContext& a_Ctx) {
		TrackHand(a_Ctx.Actor(), true, false);
		a_Ctx.RequestExit();
	}

	void OnFingerCamOffL(const ActionContext& a_Ctx) {
		TrackHand(a_Ctx.Actor(), false, false);
		a_Ctx.RequestExit();
	}

	//----------------------------------------------------------------------------------------------
	// Guards and resolvers
	//----------------------------------------------------------------------------------------------

	bool CanStomp(const EntryContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		return giant && CanDoActionBasedOnQuestProgress(giant, QuestAnimationType::kStompsAndKicks) && !AnimationVars::General::IsGTSBusy(giant);
	}

	bool CanTrample(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (!CanStomp(a_Ctx)) {
			return false;
		}

		return !AnimationVars::Crawl::IsCrawling(giant) && !giant->IsSneaking() && !AnimationVars::Prone::IsProne(giant);
	}

	// Aim, then name the behaviour. The aim writes the blend the graph interpolates, so it has to run
	// before the send whatever the answer is, which is why this is a resolver and not a guard.
	std::string_view ResolveLight(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const Aim aim = Stomping::Resolve(giant, StompAimType::T1, false, false);

		const float cost = Runtime::HasPerk(giant, Runtime::PERK.GTSPerkDestructionBasics) ? 25.0f * 0.65f : 25.0f;

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, "You're too tired to perform stomp");
			return {};
		}

		if (aim.Under) {
			return aim.Left ? "GTSBeh_UnderStomp_StartL" : "GTSBeh_UnderStomp_StartR";
		}

		return aim.Left ? "GtsModStompAnimLeft" : "GtsModStompAnimRight";
	}

	std::string_view ResolveStrong(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const Aim aim = Stomping::Resolve(giant, StompAimType::T2, true, false);

		float cost = 70.0f * GetWasteMult(giant);
		std::string_view message = "You're too tired to perform heavy stomp";

		// The sneak and crawl understomps are whole body attacks and cost accordingly.
		if (giant->IsSneaking() && aim.Under) {

			if (AnimationVars::Crawl::IsCrawling(giant)) {
				message = "You're too tired to perform sneak breast crush";
				cost *= 2.5f;
			}
			else {
				message = "You're too tired to perform sneak butt crush";
				cost *= 1.8f;
			}
		}

		if (GetAV(giant, ActorValue::kStamina) <= cost) {
			NotifyWithSound(giant, message);
			return {};
		}

		if (aim.Under) {
			return aim.Left ? "GTSBeh_UnderStomp_Start_StrongL" : "GTSBeh_UnderStomp_Start_StrongR";
		}

		return aim.Left ? "GTSBeh_StrongStomp_StartLeft" : "GTSBeh_StrongStomp_StartRight";
	}

	std::string_view ResolveTrample(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const Aim aim = Stomping::Resolve(giant, StompAimType::T3, false, true);

		if (GetAV(giant, ActorValue::kStamina) <= 35.0f * GetWasteMult(giant)) {
			NotifyWithSound(giant, "You're too tired to perform trample");
			return {};
		}

		if (aim.Under) {
			return aim.Left ? "GTSBeh_UnderTrample_StartL" : "GTSBeh_UnderTrample_StartR";
		}

		return aim.Left ? "GTSBeh_Trample_L" : "GTSBeh_Trample_R";
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsStomping", true },
	};

	constexpr GraphExpect AssertsGrinding[] = { { "GTS_IsFootGrinding", true } };

	constexpr std::string_view ExitSignals[] = {
		"GTSBEH_Exit",
		"GTS_StrongStomp_End",
		"GTS_UnderStomp_CamOffR",
		"GTS_UnderStomp_CamOffL",
		"GTS_UnderStomp_CamOff_StrongR",
		"GTS_UnderStomp_CamOff_StrongL",
		"GTSstomp_FootGrindL_Exit",
		"GTSstomp_FootGrindR_Exit",
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "Stomp.Light",   .Guard = CanStomp,   .Input = "Action.Stomp.StartLight",   .Resolve = ResolveLight },
		{ .Action = "Stomp.Strong",  .Guard = CanStomp,   .Input = "Action.Stomp.StartHeavy",   .Resolve = ResolveStrong },
		{ .Action = "Stomp.Trample", .Guard = CanTrample, .Input = "Action.Stomp.StartTrample", .Resolve = ResolveTrample },
	};

	// The grind and the trample loop are asked for by the proximity check that runs when a stomp
	// lands, never by a key. Both continue the animation that is already playing.
	constexpr ActionDef Actions[] = {
		{ .Action = "Stomp.GrindL",      .Behaviour = "GTSBEH_StartFootGrindL",  .Asserts = AssertsGrinding },
		{ .Action = "Stomp.GrindR",      .Behaviour = "GTSBEH_StartFootGrindR",  .Asserts = AssertsGrinding },
		{ .Action = "Stomp.UnderGrindL", .Behaviour = "GTSBEH_StartUnderGrindL", .Asserts = AssertsGrinding },
		{ .Action = "Stomp.UnderGrindR", .Behaviour = "GTSBEH_StartUnderGrindR", .Asserts = AssertsGrinding },

		{ .Action = "Stomp.TrampleL",    .Behaviour = "GTSBEH_Trample_Start_L",  .HandOff = ActionId::kTrample },
		{ .Action = "Stomp.TrampleR",    .Behaviour = "GTSBEH_Trample_Start_R",  .HandOff = ActionId::kTrample },

		{ .Action = "Stomp.Cancel",      .Behaviour = "",                        .Aborts = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSstompstartR",                 .Handler = OnStompStartR },
		{ .Tag = "GTSstompstartL",                 .Handler = OnStompStartL },
		{ .Tag = "GTSstompimpactR",                .Handler = OnStompImpactR },
		{ .Tag = "GTSstompimpactL",                .Handler = OnStompImpactL },
		{ .Tag = "GTSstomplandR",                  .Handler = OnStompLandR },
		{ .Tag = "GTSstomplandL",                  .Handler = OnStompLandL },
		{ .Tag = "GTSStompendR",                   .Handler = OnStompEnd },
		{ .Tag = "GTSStompendL",                   .Handler = OnStompEnd },
		{ .Tag = "GTS_Next",                       .Handler = OnNext, .Shared = true },
		{ .Tag = "GTSBEH_Exit",                    .Handler = OnNext, .Shared = true },

		{ .Tag = "GTS_StrongStomp_Start",          .Handler = OnStrongStart },
		{ .Tag = "GTS_StrongStomp_LR_Start",       .Handler = OnStrongLegStartR },
		{ .Tag = "GTS_StrongStomp_LL_Start",       .Handler = OnStrongLegStartL },
		{ .Tag = "GTS_StrongStomp_LR_Middle",      .Handler = OnStrongLegMiddle },
		{ .Tag = "GTS_StrongStomp_LL_Middle",      .Handler = OnStrongLegMiddle },
		{ .Tag = "GTS_StrongStomp_LR_End",         .Handler = OnStrongLegEndR },
		{ .Tag = "GTS_StrongStomp_LL_End",         .Handler = OnStrongLegEndL },
		{ .Tag = "GTS_StrongStomp_ImpactR",        .Handler = OnStrongImpactR },
		{ .Tag = "GTS_StrongStomp_ImpactL",        .Handler = OnStrongImpactL },
		{ .Tag = "GTS_StrongStomp_ReturnRL_Start", .Handler = OnStrongReturnStartR },
		{ .Tag = "GTS_StrongStomp_ReturnLL_Start", .Handler = OnStrongReturnStartL },
		{ .Tag = "GTS_StrongStomp_ReturnRL_End",   .Handler = OnStrongReturnEndR },
		{ .Tag = "GTS_StrongStomp_ReturnLL_End",   .Handler = OnStrongReturnEndL },
		{ .Tag = "GTS_StrongStomp_End",            .Handler = OnStrongEnd },

		{ .Tag = "GTS_UnderStomp_CamOnR",          .Handler = OnUnderCamOnR },
		{ .Tag = "GTS_UnderStomp_CamOnL",          .Handler = OnUnderCamOnL },
		{ .Tag = "GTS_UnderStomp_CamOffR",         .Handler = OnUnderCamOffR },
		{ .Tag = "GTS_UnderStomp_CamOffL",         .Handler = OnUnderCamOffL },
		{ .Tag = "GTS_UnderStomp_ImpactR",         .Handler = OnUnderImpactR },
		{ .Tag = "GTS_UnderStomp_ImpactL",         .Handler = OnUnderImpactL },

		{ .Tag = "GTS_UnderStomp_CamOn_StrongR",   .Handler = OnUnderStrongCamOnR },
		{ .Tag = "GTS_UnderStomp_CamOn_StrongL",   .Handler = OnUnderStrongCamOnL },
		{ .Tag = "GTS_UnderStomp_CamOff_StrongR",  .Handler = OnUnderStrongCamOffR },
		{ .Tag = "GTS_UnderStomp_CamOff_StrongL",  .Handler = OnUnderStrongCamOffL },
		{ .Tag = "GTS_UnderStomp_Impact_StrongR",  .Handler = OnUnderStrongImpactR },
		{ .Tag = "GTS_UnderStomp_Impact_StrongL",  .Handler = OnUnderStrongImpactL },

		{ .Tag = "GTS_UnderStomp_ButtCamOn",       .Handler = OnButtCamOn },
		{ .Tag = "GTS_UnderStomp_ButtCamOff",      .Handler = OnButtCamOff },
		{ .Tag = "GTS_UnderStomp_Butt_DisableHH",  .Handler = OnButtDisableHH },
		{ .Tag = "GTS_UnderStomp_Butt_EnableHH",   .Handler = OnButtEnableHH },
		{ .Tag = "GTS_UnderStomp_Butt_RecFootstepL", .Handler = OnButtRecFootstepL },
		{ .Tag = "GTS_UnderStomp_Butt_RecFootstepR", .Handler = OnButtRecFootstepR },
		{ .Tag = "GTS_UnderStomp_ButtImpact",      .Handler = OnButtImpact },
		{ .Tag = "GTS_UnderStomp_ButtLegImpact",   .Handler = OnButtLegImpact },

		{ .Tag = "GTS_UnderStomp_Crawl_CamOnR",    .Handler = OnCrawlSlamCamOnR },
		{ .Tag = "GTS_UnderStomp_Crawl_CamOnL",    .Handler = OnCrawlSlamCamOnL },
		{ .Tag = "GTS_UnderStomp_Crawl_CamOffR",   .Handler = OnCrawlSlamCamOffR },
		{ .Tag = "GTS_UnderStomp_Crawl_CamOffL",   .Handler = OnCrawlSlamCamOffL },
		{ .Tag = "GTS_UnderStomp_Crawl_ImpactR",   .Handler = OnCrawlSlamImpactR },
		{ .Tag = "GTS_UnderStomp_Crawl_ImpactL",   .Handler = OnCrawlSlamImpactL },

		{ .Tag = "GTS_UnderStomp_Crawl_BodyCamOn", .Handler = OnCrawlBodyCamOn },
		{ .Tag = "GTS_UnderStomp_Crawl_BodyCamOff",.Handler = OnCrawlBodyCamOff },
		{ .Tag = "GTS_UnderStomp_Crawl_BodyImpact",.Handler = OnCrawlBodyImpact },

		{ .Tag = "GTSstomp_FootGrindL_Enter",      .Handler = OnGrindEnterL },
		{ .Tag = "GTSstomp_FootGrindR_Enter",      .Handler = OnGrindEnterR },
		{ .Tag = "GTSstomp_FootGrindL_MV_S",       .Handler = OnGrindRotateL },
		{ .Tag = "GTSstomp_FootGrindR_MV_S",       .Handler = OnGrindRotateR },
		{ .Tag = "GTSstomp_FootGrindL_MV_E",       .Handler = OnGrindMoveEndL },
		{ .Tag = "GTSstomp_FootGrindR_MV_E",       .Handler = OnGrindMoveEndR },
		{ .Tag = "GTSstomp_FootGrindL_Impact",     .Handler = OnGrindImpactL },
		{ .Tag = "GTSstomp_FootGrindR_Impact",     .Handler = OnGrindImpactR },
		{ .Tag = "GTSstomp_FootGrindL_Exit",       .Handler = OnGrindExit },
		{ .Tag = "GTSstomp_FootGrindR_Exit",       .Handler = OnGrindExit },

		{ .Tag = "GTS_Trample_Leg_Raise_L",        .Handler = OnTrampleRaiseL },
		{ .Tag = "GTS_Trample_Leg_Raise_R",        .Handler = OnTrampleRaiseR },
		{ .Tag = "GTS_Trample_Cam_Start_L",        .Handler = OnTrampleCamStartL },
		{ .Tag = "GTS_Trample_Cam_Start_R",        .Handler = OnTrampleCamStartR },
		{ .Tag = "GTS_Trample_Footstep_L",         .Handler = OnTrampleFootstepL },
		{ .Tag = "GTS_Trample_Footstep_R",         .Handler = OnTrampleFootstepR },

		{ .Tag = "GTS_Sneak_Slam_Raise_Arm_R",     .Handler = OnSlamRaiseR },
		{ .Tag = "GTS_Sneak_Slam_Raise_Arm_L",     .Handler = OnSlamRaiseL },
		{ .Tag = "GTS_Sneak_Slam_Impact_R",        .Handler = OnSlamImpactR },
		{ .Tag = "GTS_Sneak_Slam_Impact_L",        .Handler = OnSlamImpactL },
		{ .Tag = "GTS_Sneak_Slam_Cam_Off_R",       .Handler = OnSlamCamOffR },
		{ .Tag = "GTS_Sneak_Slam_Cam_Off_L",       .Handler = OnSlamCamOffL },

		{ .Tag = "GTS_Sneak_SlamStrong_Raise_Arm_R", .Handler = OnSlamStrongRaiseR },
		{ .Tag = "GTS_Sneak_SlamStrong_Raise_Arm_L", .Handler = OnSlamStrongRaiseL },
		{ .Tag = "GTS_Sneak_SlamStrong_Lower_Arm_R" },
		{ .Tag = "GTS_Sneak_SlamStrong_Lower_Arm_L" },
		{ .Tag = "GTS_Sneak_SlamStrong_Impact_R",  .Handler = OnSlamStrongImpactR },
		{ .Tag = "GTS_Sneak_SlamStrong_Impact_L",  .Handler = OnSlamStrongImpactL },
		{ .Tag = "GTS_Sneak_SlamStrong_Impact_Secondary_R", .Handler = OnSlamStrongSecondaryR },
		{ .Tag = "GTS_Sneak_SlamStrong_Impact_Secondary_L", .Handler = OnSlamStrongSecondaryL },

		{ .Tag = "GTSCrawl_Slam_Raise_Arm_R",      .Handler = OnCrawlHandRaiseR },
		{ .Tag = "GTSCrawl_Slam_Raise_Arm_L",      .Handler = OnCrawlHandRaiseL },
		{ .Tag = "GTSCrawl_SlamStrong_Raise_Arm_R",.Handler = OnCrawlHandStrongRaiseR },
		{ .Tag = "GTSCrawl_SlamStrong_Raise_Arm_L",.Handler = OnCrawlHandStrongRaiseL },
		{ .Tag = "GTSCrawl_Slam_Lower_Arm_R" },
		{ .Tag = "GTSCrawl_Slam_Lower_Arm_L" },
		{ .Tag = "GTSCrawl_SlamStrong_Lower_Arm_R" },
		{ .Tag = "GTSCrawl_SlamStrong_Lower_Arm_L" },
		{ .Tag = "GTSCrawl_Slam_Impact_R",         .Handler = OnCrawlSlamHitR },
		{ .Tag = "GTSCrawl_Slam_Impact_L",         .Handler = OnCrawlSlamHitL },
		{ .Tag = "GTSCrawl_SlamStrong_Impact_R",   .Handler = OnCrawlSlamStrongHitR },
		{ .Tag = "GTSCrawl_SlamStrong_Impact_L",   .Handler = OnCrawlSlamStrongHitL },
		{ .Tag = "GTSCrawl_Slam_Cam_Off_R",        .Handler = OnCrawlSlamCamEndR },
		{ .Tag = "GTSCrawl_Slam_Cam_Off_L",        .Handler = OnCrawlSlamCamEndL },

		{ .Tag = "GTS_Sneak_FingerGrind_CameraOn_R",  .Handler = OnFingerCamOnR },
		{ .Tag = "GTS_Sneak_FingerGrind_CameraOn_L",  .Handler = OnFingerCamOnL },
		{ .Tag = "GTS_Sneak_FingerGrind_Impact_R",    .Handler = OnFingerImpactR },
		{ .Tag = "GTS_Sneak_FingerGrind_Impact_L",    .Handler = OnFingerImpactL },
		{ .Tag = "GTS_Sneak_FingerGrind_Rotation_R",  .Handler = OnFingerRotationR },
		{ .Tag = "GTS_Sneak_FingerGrind_Rotation_L",  .Handler = OnFingerRotationL },
		{ .Tag = "GTS_Sneak_FingerGrind_Finisher_R",  .Handler = OnFingerFinisherR },
		{ .Tag = "GTS_Sneak_FingerGrind_Finisher_L",  .Handler = OnFingerFinisherL },
		{ .Tag = "GTS_Sneak_FingerGrind_CameraOff_R", .Handler = OnFingerCamOffR },
		{ .Tag = "GTS_Sneak_FingerGrind_CameraOff_L", .Handler = OnFingerCamOffL },
	};

	constexpr std::string_view StateNames[] = {
		"standing",
		"sneak",
		"crawl",
		"crawl strong",
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> StompNode::Signature() const        { return ::Signature; }
	std::span<const std::string_view> StompNode::ExitSignals() const { return ::ExitSignals; }
	bool StompNode::StartOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action, bool) const {
		return PlayerTarget::StartAimedOn(a_Actor, a_Target, a_Action);
	}

	std::span<const EntryDef> StompNode::Entries() const             { return ::Entries; }
	std::span<const ActionDef> StompNode::Actions() const            { return ::Actions; }
	std::span<const AnnotationDef> StompNode::Annotations() const    { return ::Annotations; }

	StompNode::State* StompNode::Get(RE::FormID a_Owner)      { return m_State.Find(a_Owner); }
	StompNode::State& StompNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	// The signature is GTS_IsStomping, which only the standing clips write. The hand slam writes its
	// own pair instead, and the four sneak and crawl understomps write nothing at all. Both are
	// answered for here, since a node has one signature and this animation state has three.
	Liveness StompNode::Alive(RE::FormID a_Owner, RE::Actor* a_Actor) const {

		if (a_Actor && (AnimationVars::Crawl::IsHandStomping(a_Actor) || AnimationVars::Crawl::IsHandStompingStrong(a_Actor))) {
			return Liveness::kAlive;
		}

		const State* state = m_State.Find(a_Owner);

		if (state && state->Silent) {
			return Liveness::kAlive;
		}

		return Liveness::kUnknown;
	}

	void StompNode::OnEnter(const ActionContext& a_Ctx) {

		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};

		auto* giant = a_Ctx.Actor();

		switch (a_Ctx.Stance()) {
			case Stance::kCrawl:
			{
				state.Variant = a_Ctx.EnteredVia() == "Stomp.Strong" ? Variant::kCrawlStrong : Variant::kCrawl;
				break;
			}
			case Stance::kSneak:
			{
				state.Variant = Variant::kSneak;
				break;
			}
			default:
			{
				state.Variant = Variant::kStanding;
				break;
			}
		}

		// Nothing is asserting anything. That is one of the four understomp variants whose modifier
		// binds only tdmHeadtrackingBehavior, so the node has to hold itself up until its own last
		// annotation asks to leave.
		state.Silent = giant
			&& !AnimationVars::Action::IsStomping(giant)
			&& !AnimationVars::Crawl::IsHandStomping(giant)
			&& !AnimationVars::Crawl::IsHandStompingStrong(giant);
	}

	void StompNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();
		auto* giant = a_Ctx.Actor();

		if (giant) {

			// Unwound here rather than through the last annotation, which a desync never reaches: that
			// leaves the grind loop running, the leg rumble going and the camera stuck on a foot.
			CancelGrind(giant);
			StopLoopRumble(giant);
			LegRumble(giant, "StrongStompR", true, false);
			LegRumble(giant, "StrongStompL", false, false);

			ManageCamera(giant, false, CameraTracking::R_Foot);
			ManageCamera(giant, false, CameraTracking::L_Foot);
			ManageCamera(giant, false, CameraTracking::Butt);
			ManageCamera(giant, false, CameraTracking::Breasts_02);
			ManageCamera(giant, false, CameraTracking::ForearmTwist_2_Right);
			ManageCamera(giant, false, CameraTracking::ForearmTwist_2_Left);

			DrainStamina(giant, "StaminaDrain_Stomp", Runtime::PERK.GTSPerkDestructionBasics, false, 1.8f);
			DrainStamina(giant, "StaminaDrain_StrongStomp", Runtime::PERK.GTSPerkDestructionBasics, false, 2.8f);

			SetBusyFoot(giant, BusyFoot::None);
		}

		m_State.Forget(owner);
	}

	void StompNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void StompNode::OnReset() {
		m_State.Clear();
	}

	std::string_view StompNode::StateName(RE::FormID a_Owner) const {
		const State* state = m_State.Find(a_Owner);
		return state ? StateNames[std::to_underlying(state->Variant)] : std::string_view{};
	}
}
