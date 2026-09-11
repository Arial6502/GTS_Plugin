#include "Actions/Nodes/Size/ShrinkNode.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Captured standing and crouched. Times are from one run and are illustrative only.

  One trigger, two clips, and the stance picks which. The crouched one is the shorter of the two and
  fires neither the enter nor the slow down.

  [SHRINK, STANDING ("GTSBeh_Shrink_Trigger", keybind "Ability.Size.ShrinkBurst")]
    GTSShrink_Enter        [NOT HANDLED] same frame as the entry
    GTSShrink_StartShrink  -> the shrink task starts here
    GTSShrink_SlowShrink   [NOT HANDLED]
    GTSShrink_StopShrink   [NOT HANDLED]

  [SHRINK, SNEAKING OR CRAWLING (same trigger)]
    GTSShrink_StartShrink  -> as above
    GTSShrink_StopShrink   [NOT HANDLED]

  **Info: the annotations marked NOT HANDLED were empty in the old file too. They are declared so the
  **log shows them in order and does not count them as orphans.

  **Info: GTSShrink_Exit is declared and the graph never fires it, in either stance. It was in
  **ExitSignals on the strength of its name, which meant the drain was never armed and every shrink
  **reported a desync. The declaration stays so the tag is not an orphan if it ever starts firing.

  **Info: kUnannounced. Neither clip announces an ending, so the signature draining is the exit. The
  **vanilla return to idle (HeadTrackingOn, IdleStop, GTSBeh_Dummy) arrives about 0.6s before it and
  **belongs to the locomotion, not to this animation.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH ON THE TRIGGER]
     GTS_IsShrinking  -> TRUE   (graph owned, and this node's signature)
   [CLEANUP]
     GTS_IsShrinking  -> FALSE, which is what ends the node

   **Info: nothing in the DLL writes GTS_IsShrinking, so the signature is real evidence and the entry
   **needs no Confirm annotation.

   **Info: the variable is dropped about 0.6s after the animation visibly ends, so the node outlives
   **the clip by that much.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	constexpr std::string_view NODE_PELVIS = "NPC Pelvis [Pelv]";

	constexpr std::string_view TASKID_FMT_SHRINK = "ManualShrink_{}";

	constexpr std::string_view RUMBLE_SHRINK = "ShrinkButton";

	void StartShrink(Actor* a_Actor) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Actor->CreateRefHandle();

		{
			const float volume = std::clamp(get_visual_scale(a_Actor) * 0.10f, 0.10f, 1.0f);
			Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundShrink, a_Actor, volume, NODE_PELVIS);
		}

		TaskManager::Run(std::format(TASKID_FMT_SHRINK, a_Actor->formID), [=](auto&) {

			auto casterPtr = handle.get();

			if (!casterPtr) {
				return false;
			}

			auto* caster = casterPtr.get();
			const float elapsed = static_cast<float>(std::clamp((Time::WorldTimeElapsed() - start) * AnimationManager::GetAnimSpeed(caster), 0.01, 1.2));
			const float multiply = bezier_curve(elapsed, 0, 1.9f, 0.6f, 0, 2.0f, 1.0f);

			const float scale = get_visual_scale(caster);
			const float stamina = std::clamp(GetStaminaPercentage(caster), 0.05f, 1.0f);
			const float perk = Perk_GetCostReduction(caster);

			DamageAV(caster, ActorValue::kStamina, 0.15f * perk * scale * stamina * TimeScale() * multiply);

			// The floor is twice the minimum, so a full press cannot land the actor on it.
			if (get_target_scale(caster) < Minimum_Actor_Scale * 2.0f) {
				set_target_scale(caster, Minimum_Actor_Scale * 2.0f);
				return false;
			}

			// CalcPower already multiplies by scale.
			override_actor_scale(caster, -CalcPower(caster, 0.0080f * stamina * multiply, 0.0f, false), SizeEffectType::kNeutral);

			Rumbling::Once(RUMBLE_SHRINK, caster, 2.0f * stamina, 0.05f, NODE_PELVIS, 0.0f);

			return elapsed < 0.99f;
		});
	}

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	bool CanShrink(const EntryContext& a_Ctx) {
		return Runtime::HasPerk(a_Ctx.Actor(), Runtime::PERK.GTSPerkGrowthDesireAug);
	}

	bool VerifyShrink(const EntryContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		if (get_target_scale(giant) <= Minimum_Actor_Scale) {
			NotifyWithSound(giant, "You can't shrink any further");
			Rumbling::Once("CantGrow", giant, 0.25f, 0.05f);
			return false;
		}

		return true;
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	void OnStartShrink(const ActionContext& a_Ctx) {
		StartShrink(a_Ctx.Actor());
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsShrinking", true },
	};

	constexpr EntryDef Entries[] = {
		{ .Action = "Shrink.Manual", .Behaviour = "GTSBeh_Shrink_Trigger", .Guard = CanShrink, .Verify = VerifyShrink, .Input = "Ability.Size.ShrinkBurst" },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTSShrink_Enter" },
		{ .Tag = "GTSShrink_StartShrink", .Handler = OnStartShrink },
		{ .Tag = "GTSShrink_SlowShrink" },
		{ .Tag = "GTSShrink_StopShrink" },
		{ .Tag = "GTSShrink_Exit" },
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> ShrinkNode::Signature() const        { return ::Signature; }
	ExitPolicy ShrinkNode::Exit() const                               { return ExitPolicy::kUnannounced; }
	std::span<const EntryDef> ShrinkNode::Entries() const             { return ::Entries; }
	std::span<const AnnotationDef> ShrinkNode::Annotations() const    { return ::Annotations; }

	bool ShrinkNode::CanEnter(const EntryContext& a_Ctx) const {

		auto* giant = a_Ctx.Actor();

		if (!giant) {
			return false;
		}

		return !IsPlayerFirstPerson(giant) && !AnimationVars::General::IsGTSBusy(giant);
	}

	void ShrinkNode::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {
		TaskManager::Cancel(std::format(TASKID_FMT_SHRINK, a_Ctx.Owner()));
	}
}
