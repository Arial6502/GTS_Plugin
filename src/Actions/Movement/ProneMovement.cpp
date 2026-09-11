#include "Actions/Movement/ProneMovement.hpp"
#include "Actions/Core/ActionRegistry.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Rumble.hpp"

#include "Magic/Effects/Common.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Prone is a stance, not a sequence. The giant lies down and stays there, walking, turning and
  rolling, until something takes her out of it. That is what a movement state is for: it holds for
  minutes at a time, blends locomotion, and does not stop a node starting on top of it.

  Three ways in, all of them the player's:
    GTSBeh_ProneStart        the toggle, from a held key
    GTSBeh_ProneStart_Dive   a running dive, from standing
    GTSBeh_ProneStart_Dive   the same behaviour from sneak or crawl, on its own key

  One way out: GTSBeh_ProneStop.

  [ENTER, ANY OF THE THREE]
    GTS_Prone_EnterSneak  -> forces the sneak flag on, so the game agrees the giant is crouched
    GTS_BodyDamage_ON     -> the body damage loop starts, and FPProning is set for the player
  [DIVE ONLY, ON TOP OF THE ABOVE]
    GTS_DiveSlide_ON      -> the heavier sliding damage, one burst of rumble across the body
    GTS_DiveSlide_OFF     -> the slide is over, back to the idle loop
  [EXIT ("GTSBeh_ProneStop")]
    GTS_BodyDamage_Off    -> clears FPProning, and the damage loop stops itself on the next tick

  **Info: there is no annotation at the end. GTS_BodyDamage_Off fires partway through the stand up,
  **and the graph announces nothing after it. A stance has no exit policy to set: the signature
  **draining is the exit, and it is always read as a completion.

  **Info: the exit is not on a key. Nothing in the keybind table defines SBO_DisableProne, so the old
  **input event for it could never fire. Pressing sneak while prone is what leaves, and that is read
  **in the CanProcess hook on VTABLE_SneakHandler (Hooks/Actor/Controls.cpp), which blocks the sneak
  **input and requests Prone.Exit in its place.

  **Info: prone and crawl are exclusive, but their signatures are not. GTS_IsCrawling is crawl mode,
  **not the crawl animation, and it stays set for the whole of a prone entered out of one - it only
  **clears when crawl is toggled off. Prone is listed after crawl so it outranks it, which is also why
  **turning crawl on while already prone does not pull the stance out from under it: the request is
  **for the mode underneath, and the sweep picks that up when the prone ends.

  **Info: the graph keeps firing the crawl knee and hand impacts through a prone. They no longer land,
  **which is right for two exclusive stances; the old files ran them because every handler was global.

  **Info: leaving is the graph's choice, not this node's. GTSBeh_ProneStop plays the outro that
  **matches whatever is underneath - crawl, sneak or standing - so nothing here may pick a target
  **stance. Whichever one it lands in is found by the sweep on the next tick.

  **Info: GTS_IsProne survives a save, and unlike crawling this stance has no annotation that repeats
  **while it holds. Loading a save taken while prone therefore leaves the graph saying prone with
  **nothing to re-announce it, so the sneak hook asks for Prone.Exit without checking whether the
  **stance is tracked. Gating that request on the track is what locked the player out of both sneaking
  **and standing up. MovementRegistry sends the behaviour for an entry whether the stance is current
  **or not, which is what makes that safe.
*/
/*
   -------------------------------------------------------------------------- ANIMATION VARS
   [SET BY THE GRAPH ON ANY OF THE THREE TRIGGERS]
     GTS_IsProne        -> TRUE   (graph owned, and this node's signature)
     GTS_IsProneDiving  -> TRUE for the dive only, back to FALSE when the slide ends
   [WHILE ROLLING]
     GTS_IsProneRolling -> TRUE
   [CLEANUP]
     GTS_IsProne        -> FALSE, which is what ends the node

   **Info: nothing in the DLL writes GTS_IsProne, so the signature is real evidence.

   **Info: AnimationVars::Prone::IsProne is not the same thing as the signature. For a player who is
   **sneaking in first person it returns Transient::FPProning, a DLL flag, because there is no first
   **person prone behaviour. The signature reads the graph variable, so it is unaffected. OnExit
   **clears FPProning on every path, since GTS_BodyDamage_Off does not fire on an abort.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	constexpr std::array BODY_NODES = {
		"NPC R Thigh [RThg]"sv,
		"NPC L Thigh [LThg]"sv,
		"NPC R Butt"sv,
		"NPC L Butt"sv,
		"NPC Spine [Spn0]"sv,
		"NPC Spine1 [Spn1]"sv,
		"NPC Spine2 [Spn2]"sv,
	};

	constexpr std::string_view BREAST_L = "NPC L Breast";
	constexpr std::string_view BREAST_R = "NPC R Breast";
	constexpr std::string_view BREAST_L03 = "L Breast03";
	constexpr std::string_view BREAST_R03 = "R Breast03";

	constexpr std::string_view TASKID_FMT_DOT = "BodyDOT_{}";
	constexpr std::string_view TASKID_FMT_SLIDE = "BodyDOT_Slide_{}";

	struct BreastPair {
		NiAVObject* Left = nullptr;
		NiAVObject* Right = nullptr;
		std::string_view LeftName;
		std::string_view RightName;

		[[nodiscard]] bool Found() const { return Left && Right; }
	};

	// The 03 bones come from the newer body meshes. Fall back to the plain ones, and report nothing at
	// all if neither pair is there, which is what ends the damage task.
	BreastPair BreastNodes(Actor* a_Giant) {

		if (auto* left = find_node(a_Giant, BREAST_L03), * right = find_node(a_Giant, BREAST_R03); left && right) {
			return { left, right, BREAST_L03, BREAST_R03 };
		}

		return { find_node(a_Giant, BREAST_L), find_node(a_Giant, BREAST_R), BREAST_L, BREAST_R };
	}

	// Shared by both loops. They differ only in how hard they hit and how far the damage reaches.
	bool DamageUnderBody(Actor* a_Giant, float a_Damage, float a_BodyRadius, float a_Random, float a_Bonus) {

		for (const auto& node : BODY_NODES) {
			if (auto* bone = find_node(a_Giant, node)) {
				DoDamageAtPoint(a_Giant, a_BodyRadius, Damage_BreastCrush_BodyDOT * a_Damage, bone, a_Random, 0.10f, a_Bonus, DamageSource::BodyCrush);
			}
		}

		ApplyThighDamage(a_Giant, true, false, Radius_ThighCrush_Idle, Damage_BreastCrush_BodyDOT * a_Damage * 0.6f, 0.10f, a_Bonus, 200, DamageSource::ThighCrushed);
		ApplyThighDamage(a_Giant, false, false, Radius_ThighCrush_Idle, Damage_BreastCrush_BodyDOT * a_Damage * 0.6f, 0.10f, a_Bonus, 200, DamageSource::ThighCrushed);

		const BreastPair breasts = BreastNodes(a_Giant);

		if (!breasts.Found()) {
			return false;
		}

		DoDamageAtPoint(a_Giant, Radius_BreastCrush_BreastDOT, Damage_BreastCrush_BreastDOT * a_Damage, breasts.Left, a_Random, 0.10f, a_Bonus, DamageSource::BreastImpact);
		DoDamageAtPoint(a_Giant, Radius_BreastCrush_BreastDOT, Damage_BreastCrush_BreastDOT * a_Damage, breasts.Right, a_Random, 0.10f, a_Bonus, DamageSource::BreastImpact);

		return true;
	}

	// Runs for as long as the giant is lying down.
	void StartBodyDamage(Actor* a_Giant) {

		const float damage = 2.0f * TimeScale();
		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::Run(std::format(TASKID_FMT_DOT, a_Giant->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			if (!AnimationVars::Prone::IsProne(giant)) {
				return false;
			}

			return DamageUnderBody(giant, damage, Radius_Proning_BodyDOT, 400, 1.33f);
		});
	}

	// The dive. Nine times the damage of lying still, over a shorter reach, plus one burst of rumble
	// on every bone that is about to hit the ground.
	void StartSlideDamage(Actor* a_Giant) {

		const float damage = 18.0f * TimeScale();
		ActorHandle handle = a_Giant->CreateRefHandle();

		{
			for (const auto& node : BODY_NODES) {
				if (find_node(a_Giant, node)) {
					Rumbling::Once(std::format("Node: {}", node), a_Giant, 0.10f, 0.02f, node, 0.0f);
				}
			}

			if (const BreastPair breasts = BreastNodes(a_Giant); breasts.Found()) {
				Rumbling::Once("BreastDot_L", a_Giant, 0.10f, 0.025f, breasts.LeftName, 0.0f);
				Rumbling::Once("BreastDot_R", a_Giant, 0.10f, 0.025f, breasts.RightName, 0.0f);
			}
		}

		TaskManager::Run(std::format(TASKID_FMT_SLIDE, a_Giant->formID), [=](auto&) {

			auto giantPtr = handle.get();

			if (!giantPtr) {
				return false;
			}

			auto* giant = giantPtr.get();

			if (!AnimationVars::Prone::IsProne(giant)) {
				return false;
			}

			return DamageUnderBody(giant, damage, Radius_BreastCrush_BodyDOT, 200, 1.0f);
		});
	}

	//----------------------------------------------------------------------------------------------
	// Guards
	//----------------------------------------------------------------------------------------------

	bool DiveFromStanding(const EntryContext& a_Ctx) {
		return a_Ctx.Stance() == Stance::kStanding;
	}

	// Both dive keys are the same physical keys, so exactly one of the two guards has to answer true.
	// Stance rather than IsSneaking, because a crawling actor reports as sneaking and the key that
	// covers crawling is this one.
	bool DiveFromCrouch(const EntryContext& a_Ctx) {
		return a_Ctx.Stance() != Stance::kStanding;
	}

	//----------------------------------------------------------------------------------------------
	// Annotations
	//----------------------------------------------------------------------------------------------

	// The graph puts the giant on the floor but does not touch the game's own sneak flag, so the game
	// still thinks she is standing up.
	void OnEnterSneak(const ActionContext& a_Ctx) {
		SetSneaking(a_Ctx.Actor(), true, 1);
		AnimationVars::Other::SetVanillaSneaking(a_Ctx.Actor(), true);
	}

	void OnBodyDamageOn(const ActionContext& a_Ctx) {
		SetProneState(a_Ctx.Actor(), true);
		StartBodyDamage(a_Ctx.Actor());
	}

	void OnBodyDamageOff(const ActionContext& a_Ctx) {
		SetProneState(a_Ctx.Actor(), false);
	}

	void OnSlideOn(const ActionContext& a_Ctx) {
		StartSlideDamage(a_Ctx.Actor());
	}

	void OnSlideOff(const ActionContext& a_Ctx) {
		TaskManager::Cancel(std::format(TASKID_FMT_SLIDE, a_Ctx.Owner()));
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsProne", true },
	};

	// Prone.Exit is here rather than in a separate table because a stance has no actions: leaving is
	// an entry that sends its behaviour and lets the signature drop. It carries no Input, since
	// nothing in the keybind table defines a key for it and the sneak hook asks for it by name.
	constexpr EntryDef Entries[] = {
		{ .Action = "Prone.Enter",     .Behaviour = "GTSBeh_ProneStart",      .Input = "Movement.Prone.Enter" },
		{ .Action = "Prone.Dive",      .Behaviour = "GTSBeh_ProneStart_Dive", .Guard = DiveFromStanding, .Input = "Movement.Prone.DiveStand" },
		{ .Action = "Prone.DiveSneak", .Behaviour = "GTSBeh_ProneStart_Dive", .Guard = DiveFromCrouch,   .Input = "Movement.Prone.DiveSneak" },
		{ .Action = "Prone.Exit",      .Behaviour = "GTSBeh_ProneStop", .Exits = true },
	};

	// Prone has no key of its own to leave by, so sneak is it: the press is spent standing up and the
	// game never sees it. Prone leaves into a crouch, so the next press is the one that stands up.
	//
	// Asked for without checking whether the stance is tracked. GTS_IsProne survives a save, so after
	// a load the graph says prone while the stance track is still empty, and gating on it left the
	// player unable to sneak or to stand up.
	bool OnSneak(RE::Actor* a_Actor) {

		if (!AnimationVars::Prone::IsProne(a_Actor) || !a_Actor->IsSneaking()) {
			return false;
		}

		ActionRegistry::Perform(a_Actor, "Prone.Exit");

		return true;
	}

	constexpr VanillaBlock Blocks[] = {
		{ .UserEvent = "Sneak", .Handle = OnSneak },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_Prone_EnterSneak", .Handler = OnEnterSneak },
		{ .Tag = "GTS_BodyDamage_ON",    .Handler = OnBodyDamageOn },
		{ .Tag = "GTS_BodyDamage_Off",   .Handler = OnBodyDamageOff },
		{ .Tag = "GTS_DiveSlide_ON",     .Handler = OnSlideOn },
		{ .Tag = "GTS_DiveSlide_OFF",    .Handler = OnSlideOff },
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> ProneMovement::Signature() const     { return ::Signature; }
	std::span<const EntryDef> ProneMovement::Entries() const          { return ::Entries; }
	std::span<const AnnotationDef> ProneMovement::Annotations() const { return ::Annotations; }
	std::span<const VanillaBlock> ProneMovement::Blocks() const   { return ::Blocks; }

	bool ProneMovement::CanEnter(const EntryContext& a_Ctx) const {
		return a_Ctx.Actor() != nullptr;
	}

	void ProneMovement::OnExit(const ActionContext& a_Ctx, ExitReason a_Reason) {

		const RE::FormID owner = a_Ctx.Owner();

		TaskManager::Cancel(std::format(TASKID_FMT_DOT, owner));
		TaskManager::Cancel(std::format(TASKID_FMT_SLIDE, owner));

		// GTS_BodyDamage_Off is the only thing that clears this, and it does not fire on an abort or a
		// desync. Left set, a first person player keeps reading as prone forever.
		if (auto* giant = a_Ctx.Actor()) {
			SetProneState(giant, false);
		}
	}
}
