#include "Actions/Nodes/Size/GrowthNode.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/Audio/MoansLaughs.hpp"
#include "Managers/AI/AIFunctions.hpp"
#include "Managers/RandomGrowth.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Captured standing and crouched. Times are from one run and are illustrative only.

  Two clip sets, one animation state. Both assert GTS_IsGrowing and nothing else separates them in
  the graph, so they are one node with two entries rather than two nodes the idle sweep could not
  tell apart. GTS_Growth_Roll says which random clip the graph picked; it is 0 for the manual one.

  The manual growth has two clips of its own, and which one plays is the stance. They do not share a
  single annotation between them past the spurt, so the order below is split rather than merged.

  [MANUAL, STANDING ("GTSBeh_Grow_Trigger", keybind "Ability.Size.GrowBurst")]
    GTSGrowth_Enter               [NOT HANDLED] same frame as the entry
    GTSGrowth_SpurtStart        -> the growth task and the delayed moan start here
    GTSGrowth_SpurtSlowdownPoint  [NOT HANDLED]
    GTSGrowth_SpurtStop           [NOT HANDLED]
    GTSGrowth_Exit                [NOT HANDLED] the only announced ending either clip has

  [MANUAL, SNEAKING OR CRAWLING (same trigger, the graph picks the clip)]
    GTSGrowth_SpurtStart        -> as above
    GtsGrowth_Mouth_Open          [NOT HANDLED]
    GtsGrowth_Moan                [NOT HANDLED]
    GtsGrowth_Mouth_Close         [NOT HANDLED]
    GTSGrowth_SpurtStop           [NOT HANDLED]
    Nothing after that. GTSGrowth_Enter, _SpurtSlowdownPoint and _Exit are standing only.

  [RANDOM ("GTSBEH_Grow_Random", no keybind, requested by Managers/RandomGrowth.cpp)]
    GTS_RandomGrowth_Start      -> reads GTS_Growth_Roll and starts the task for that curve
    GTS_RandomGrowth_Peak       -> moan, and the scare pass if the giant has the terror perk
    GTS_RandomGrowth_Taper        [NOT HANDLED]
    GTS_RandomGrowth_End          [NOT HANDLED] the drain is armed here, and the variable takes
                                  another two to three seconds to drop
    One clip set, whatever the stance. Unlike the manual growth this does not vary.

  **Info: the annotations marked NOT HANDLED were empty in the old file too. They are declared so the
  **log shows them in order and does not count them as orphans.

  **Info: the graph refuses GTSBEH_Grow_Random while the actor is transitioning between stances, and
  **RandomGrowth asks on a timer that does not know about that. Five requests in eight were turned
  **down in one capture. That is the graph's own gate and the old file was refused in the same place.

  **Info: kUnannounced. Only the standing clip fires GTSGrowth_Exit; the crouched one ends on
  **GTSGrowth_SpurtStop and says nothing more. The vanilla return to idle (HeadTrackingOn, IdleStop,
  **GTSBeh_Dummy) arrives about 0.6s before the variable drops, and none of that belongs to this
  **animation. Without kUnannounced every crouched growth reported a desync and the registry sent an
  **abort into an animation that had already finished.

  **Info: GTS_ResetVars is not declared here. It is a generic cleanup tag that every animation fires,
  **so claiming it would report an orphan on each one. The old file used it to poll until growing
  **ended and then clear the roll; the node exits on exactly that condition, so OnExit clears it.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH ON EITHER TRIGGER]
     GTS_IsGrowing           -> TRUE   (graph owned, and this node's signature)
     GTS_Growth_Roll         -> 1..6 on the random trigger, 0 on the manual one. An int, so a watch
                                capture shows it as a bool: non zero reads as true and says nothing
                                about which clip was rolled.
   [CLEANUP]
     GTS_IsGrowing           -> FALSE, which is what ends the node
     GTS_Growth_Roll         -> 0, written by the DLL from OnExit

   **Info: nothing in the DLL writes GTS_IsGrowing, so the signature is real evidence and the entry
   **needs no Confirm annotation.

   **Info: the variable is dropped about 0.6s after the animation visibly ends, so the node outlives
   **the clip by that much. Nothing may read "is this node current" as "is the giant still growing".
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	using Kind = GrowthNode::Kind;
	using State = GrowthNode::State;

	constexpr std::string_view NODE_PELVIS = "NPC Pelvis [Pelv]";
	constexpr std::string_view NODE_COM = "NPC COM [COM ]";

	constexpr std::string_view TASKID_FMT_MANUAL = "ManualGrowth_{}";
	constexpr std::string_view TASKID_FMT_RANDOM = "RandomGrowth_{}";
	constexpr std::string_view TASKID_FMT_MOAN = "DelayMoan_{}";

	constexpr std::string_view RUMBLE_MANUAL = "GrowButton";
	constexpr std::string_view RUMBLE_RANDOM = "RandomGrowth";

	//----------------------------------------------------------------------------------------------
	// Manual growth
	//----------------------------------------------------------------------------------------------

	// The animation has no annotation at the point the moan belongs, so it is timed off the spurt.
	void DelayedMoan(Actor* a_Actor) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Actor->CreateRefHandle();

		TaskManager::Run(std::format(TASKID_FMT_MOAN, a_Actor->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			if (Time::WorldTimeElapsed() - start >= 0.15f / AnimationManager::GetAnimSpeed(giant)) {
				Task_FacialEmotionTask_Moan(giant, 1.25f, "GrowthMoan", 0.15f);
				Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Growth, CooldownSource::Emotion_Voice_Long);
				return false;
			}

			return true;
		});
	}

	void StartManualGrowth(Actor* a_Actor) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Actor->CreateRefHandle();

		{
			const float volume = std::clamp(get_visual_scale(a_Actor) / 8.0f, 0.20f, 1.0f);
			Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundGrowth, a_Actor, volume, NODE_PELVIS);
		}

		TaskManager::Run(std::format(TASKID_FMT_MANUAL, a_Actor->formID), [=](auto&) {

			auto casterPtr = handle.get();

			if (!casterPtr) {
				return false;
			}

			auto* caster = casterPtr.get();
			const float animspeed = AnimationManager::GetAnimSpeed(caster);
			const float elapsed = static_cast<float>(std::clamp((Time::WorldTimeElapsed() - start) * animspeed, 0.01, 1.2));
			const float multiply = bezier_curve(elapsed, 0, 1.9f, 0.6f, 0, 2.0f, 1.0f);

			const float scale = get_visual_scale(caster);
			const float stamina = std::clamp(GetStaminaPercentage(caster), 0.05f, 1.0f);
			const float perk = Perk_GetCostReduction(caster);

			DamageAV(caster, ActorValue::kStamina, 0.60f * perk * scale * stamina * TimeScale() * multiply);

			// The static value is not scaled by size; CalcPower does that part.
			override_actor_scale(caster, CalcPower(caster, 0.0080f * stamina * multiply * animspeed, 0.0f, false), SizeEffectType::kGrow);

			Rumbling::Once(RUMBLE_MANUAL, caster, 2.0f * stamina, 0.05f, NODE_PELVIS, 0.0f);

			return elapsed < 0.99f;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Random growth
	//----------------------------------------------------------------------------------------------

	// Which clip the graph rolled. The roll is done on the behaviour side, so this is the only way to
	// know which curve the animation is following.
	GrowthAnimation RolledAnimation(Actor* a_Actor) {
		return static_cast<GrowthAnimation>(AnimationVars::Growth::GrowthRoll(a_Actor));
	}

	float GrowthMultiplier(Actor* a_Actor) {

		const float multiplier = Runtime::HasPerkTeam(a_Actor, Runtime::PERK.GTSPerkRandomGrowthTerror) ? 1.3f : 1.0f;

		switch (RolledAnimation(a_Actor)) {
			case GrowthAnimation::Growth_1:
			{
				return 0.38f * multiplier * 1.45f;
			}
			case GrowthAnimation::Growth_5:
			case GrowthAnimation::Growth_2:
			{
				return 0.26f * multiplier * 1.40f;
			}
			case GrowthAnimation::Growth_6:
			case GrowthAnimation::Growth_3:
			{
				return 0.28f * multiplier * 1.35f;
			}
			case GrowthAnimation::Growth_4:
			{
				return 0.34f * multiplier * 1.25f;
			}
			default:
			{
				return 0.9f * multiplier;
			}
		}
	}

	// One curve per clip, so the size gain follows what the animation is doing. The desmos links are
	// where each was plotted.
	float GrowthCurve(float a_Elapsed, GrowthAnimation a_Anim) {

		switch (a_Anim) {
			case GrowthAnimation::Growth_1:
			{
				return bezier_curve(a_Elapsed * 0.32f, 0.2f, 2.1f, -0.2f, 0, 3.0f, 7.0f); // https://www.desmos.com/calculator/aqbzm5e97p
			}
			case GrowthAnimation::Growth_5:
			case GrowthAnimation::Growth_2:
			{
				return bezier_curve(a_Elapsed * 0.33f, 0.2f, 1.9f, 0, 0, 3.0f, 4.0f); // https://www.desmos.com/calculator/reqejljy19
			}
			case GrowthAnimation::Growth_6:
			case GrowthAnimation::Growth_3:
			{
				// Clamped, or the curve turns positive again and the actor grows after the clip is done.
				return bezier_curve(std::clamp(a_Elapsed, 0.0f, 2.0f) * 0.48f, 0.4f, 3, 0, 0, 3.0f, 1.9f); // https://www.desmos.com/calculator/liko5e9kca
			}
			case GrowthAnimation::Growth_4:
			{
				return bezier_curve(a_Elapsed * 0.275f, 0, 3.5f, 0.2f, 0, 1.0f, 0.68f); // https://www.desmos.com/calculator/7ynul48a93
			}
			default:
			{
				return 0.0f;
			}
		}
	}

	void StartRandomGrowth(Actor* a_Actor) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Actor->CreateRefHandle();

		const GrowthAnimation rolled = RolledAnimation(a_Actor);
		const float growthMult = GrowthMultiplier(a_Actor);

		TaskManager::Run(std::format(TASKID_FMT_RANDOM, a_Actor->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();
			const float animspeed = AnimationManager::GetAnimSpeed(giant);
			const float elapsed = static_cast<float>(std::clamp((Time::WorldTimeElapsed() - start) * animspeed, 0.0, 4.4));
			const float gain = std::clamp(GrowthCurve(elapsed, rolled), -0.01f, 1.0f);

			if (gain > 0) {
				override_actor_scale(giant, CalcPower(giant, 0.0080f * growthMult * gain * animspeed, 0.0f, false), SizeEffectType::kGrow);
				RandomGrowth::RestoreStats(giant, gain);
			}

			Rumbling::Once(RUMBLE_RANDOM, giant, 2.0f * gain, 0.0f, NODE_PELVIS, 0.0f);

			if (!IsActionOnCooldown(giant, CooldownSource::Misc_GrowthSound)) {
				ApplyActionCooldown(giant, CooldownSource::Misc_GrowthSound);
				const float volume = std::clamp(get_visual_scale(giant) / 8.0f, 0.20f, 1.0f);
				Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundGrowth, giant, volume * gain, NODE_PELVIS);
			}

			return AnimationVars::Growth::IsGrowing(giant) && !(elapsed > 1.8f && gain < 0.0f);
		});
	}

	// Everything hostile close enough to see it happen.
	void ScareNearby(Actor* a_Giant) {

		const float range = 212.0f * get_visual_scale(a_Giant);

		for (auto* tiny : find_actors()) {

			if (!tiny || tiny == a_Giant) {
				continue;
			}

			if (!IsHostile(a_Giant, tiny) && !IsHostile(tiny, a_Giant)) {
				continue;
			}

			if ((a_Giant->GetPosition() - tiny->GetPosition()).Length() <= range) {
				ChanceToScare(a_Giant, tiny, 4, 3, false);
			}
		}
	}

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	bool CanGrowManually(const EntryContext& a_Ctx) {
		return Runtime::HasPerk(a_Ctx.Actor(), Runtime::PERK.GTSPerkGrowthDesireAug);
	}

	bool VerifyGrow(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (get_target_scale(giant) >= get_max_scale(giant)) {
			NotifyWithSound(giant, "You can't grow any further");
			Rumbling::Once("CantGrow", giant, 0.25f, 0.05f);
			return false;
		}

		return true;
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnSpurtStart(const ActionContext& a_Ctx) {
		DelayedMoan(a_Ctx.Actor());
		StartManualGrowth(a_Ctx.Actor());
	}

	void OnRandomStart(const ActionContext& a_Ctx) {
		StartRandomGrowth(a_Ctx.Actor());
	}

	void OnRandomPeak(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		Sound_PlayMoans(giant, 1.0f, 0.14f, EmotionTriggerSource::Growth);
		Task_FacialEmotionTask_Moan(giant, 1.75f, "RandomGrow");

		if (Runtime::HasPerkTeam(giant, Runtime::PERK.GTSPerkRandomGrowthTerror)) {
			ScareNearby(giant);
		}
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsGrowing", true },
	};

	constexpr std::string_view ExitSignals[] = {
		"GTSGrowth_Exit",
		"GTS_RandomGrowth_End",
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "Growth.Manual", .Behaviour = "GTSBeh_Grow_Trigger", .Guard = CanGrowManually, .Verify = VerifyGrow, .Input = "Ability.Size.GrowBurst" },

		// Requested by RandomGrowth::OnMainUpdate, which has already decided. No guard of its own, or
		// an NPC without the manual perk could never play it.
		{ .Action = "Growth.Random", .Behaviour = "GTSBEH_Grow_Random" },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSGrowth_Enter" },
		{ .Tag = "GtsGrowth_Mouth_Open" },
		{ .Tag = "GTSGrowth_SpurtStart",        .Handler = OnSpurtStart },
		{ .Tag = "GtsGrowth_Moan" },
		{ .Tag = "GTSGrowth_SpurtSlowdownPoint" },
		{ .Tag = "GTSGrowth_SpurtStop" },
		{ .Tag = "GtsGrowth_Mouth_Close" },
		{ .Tag = "GTSGrowth_Exit" },

		{ .Tag = "GTS_RandomGrowth_Start",      .Handler = OnRandomStart },
		{ .Tag = "GTS_RandomGrowth_Peak",       .Handler = OnRandomPeak },
		{ .Tag = "GTS_RandomGrowth_Taper" },
		{ .Tag = "GTS_RandomGrowth_End" },
	};

	constexpr std::string_view StateNames[] = {
		"manual",
		"random",
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> GrowthNode::Signature() const        { return ::Signature; }
	ExitPolicy GrowthNode::Exit() const                               { return ExitPolicy::kUnannounced; }
	std::span<const std::string_view> GrowthNode::ExitSignals() const { return ::ExitSignals; }
	std::span<const EntryDef> GrowthNode::Entries() const             { return ::Entries; }
	std::span<const AnnotationDef> GrowthNode::Annotations() const    { return ::Annotations; }

	GrowthNode::State* GrowthNode::Get(RE::FormID a_Owner)      { return m_State.Find(a_Owner); }
	GrowthNode::State& GrowthNode::GetOrAdd(RE::FormID a_Owner) { return m_State.GetOrAdd(a_Owner); }

	bool GrowthNode::CanEnter(const EntryContext& a_Ctx) const {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return false;
		}

		return !IsPlayerFirstPerson(giant) && !AnimationVars::General::IsGTSBusy(giant);
	}

	void GrowthNode::OnEnter(const ActionContext& a_Ctx) {
		State& state = m_State.GetOrAdd(a_Ctx.Owner());
		state = State{};
		state.Kind = a_Ctx.EnteredVia() == "Growth.Random" ? Kind::kRandom : Kind::kManual;
	}

	void GrowthNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();

		TaskManager::Cancel(std::format(TASKID_FMT_MANUAL, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_RANDOM, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_MOAN, owner));

		// The graph picks the clip from this and never clears it. Leaving it set makes the next random
		// growth read a roll the animation did not make.
		if (auto* giant = a_Ctx.Actor()) {
			AnimationVars::Growth::SetGrowthRoll(giant, 0);
		}

		m_State.Forget(owner);
	}

	void GrowthNode::OnForget(RE::FormID a_Owner) {
		m_State.Forget(a_Owner);
	}

	void GrowthNode::OnReset() {
		m_State.Clear();
	}

	std::string_view GrowthNode::StateName(RE::FormID a_Owner) const {
		const State* state = m_State.Find(a_Owner);
		return state ? StateNames[std::to_underlying(state->Kind)] : std::string_view{};
	}
}
