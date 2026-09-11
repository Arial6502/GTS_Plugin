#include "Actions/Movement/CrawlMovement.hpp"

#include "Managers/Animation/Utils/CrawlUtils.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"

/*
  ------------------------------------------------------------------------------ ANNOTATION ORDER
  Crawling is a stance, not an animation state, so this is a movement state rather than a node. The
  giant stays in it while stomps, slams, swipes, hugs and vore start and finish on top; a node is
  exclusive and would refuse all of them.

  Only the locomotion belongs here. The slams and swipes that are reachable while crawling are their
  own nodes, HandSlamNode and KickSwipeNode, because the same attacks exist in sneak.

  [ENTER ("GTSBeh_Crawl_On") / LEAVE ("GTSBeh_Crawl_Off")]
    No annotations of their own. GTS_IsCrawling asserting is the entry, dropping it is the exit.
  [WHILE CRAWLING, ON EVERY STEP]
    GTS_Crawl_Knee_Trans_Impact  -> both knees land at once, dropping into the stance
    GTS_Crawl_Hand_Trans_Impact  -> both hands, same
    GTSCrawl_KneeImpact_L / _R   -> one knee, plus thigh damage on that side
    GTSCrawl_HandImpact_L / _R   -> one hand

  **Info: the left hand impact does nothing while a tiny is held. The hand carrying someone is not
  **hitting the ground, and the grab has its own damage for whoever is in it.

  **Info: GTS_IsCrawling is crawl mode, not the crawl animation. It moves in lockstep with
  **GTS_CrawlEnabled and stays set through a prone entered out of a crawl, clearing only when crawl is
  **toggled off. Prone outranks this stance for that reason, and the impacts above stop landing while
  **one is running.

  **Info: GTS_IsCrawling survives a save. Loading one taken while crawling brings the actor back
  **crawling with the stance track empty, so the stance is never assumed to start false: the sweep in
  **MovementRegistry::Tick picks it up, and any of the impacts above starts it if the sweep has not
  **reached that actor yet.

  **Info: the stance is not usually requested. Pressing sneak while crawling is enabled puts the
  **actor down through the graph's own transition and GTSBeh_Crawl_On is never sent, so Crawl.On
  **exists for the toggle and everything else arrives unannounced. Leaving is the same in reverse:
  **standing up drops GTS_IsCrawling without Crawl.Off being asked for.

  **Info: a node runs on top of this, it does not replace it. A capture has a random growth still
  **running when the giant drops into a crawl, both tracked at once. That is the whole reason a stance
  **is not a node. A later capture has seven stomps and two swipes taken while crawling with no
  **MOVE_EXIT between them.

  **Info: the impacts keep firing while a node runs on top. GTSCrawl_KneeImpact_L / R land inside a
  **crawl slam and inside a crawl understomp, so the step damage stacks with the attack damage.
*/

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	void OnKneeTransition(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float scale = get_visual_scale(giant);

		DoCrawlingFunctions(giant, scale, 2.6f, Damage_Crawl_KneeImpact_Drop, CrawlEvent::LeftKnee, "LeftKnee", 1.8f, Radius_Crawl_KneeImpact_Fall, 1.15f, DamageSource::KneeDropLeft);
		DoCrawlingFunctions(giant, scale, 2.6f, Damage_Crawl_KneeImpact_Drop, CrawlEvent::RightKnee, "RightKnee", 1.8f, Radius_Crawl_KneeImpact_Fall, 1.15f, DamageSource::KneeDropRight);
	}

	void OnHandTransition(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();
		const float scale = get_visual_scale(giant);

		DoCrawlingFunctions(giant, scale, 2.15f, Damage_Crawl_HandImpact_Drop, CrawlEvent::LeftHand, "LeftHand", 1.6f, Radius_Crawl_HandImpact_Fall, 1.15f, DamageSource::HandDropLeft);
		DoCrawlingFunctions(giant, scale, 2.15f, Damage_Crawl_HandImpact_Drop, CrawlEvent::RightHand, "RightHand", 1.6f, Radius_Crawl_HandImpact_Fall, 1.15f, DamageSource::HandDropRight);
	}

	void OnKneeImpact(const ActionContext& a_Ctx, bool a_Right) {

		auto* giant = a_Ctx.Actor();
		const float scale = get_visual_scale(giant);

		DoCrawlingFunctions(giant, scale, 1.85f, Damage_Crawl_KneeImpact,
			a_Right ? CrawlEvent::RightKnee : CrawlEvent::LeftKnee,
			a_Right ? "RightKnee" : "LeftKnee",
			1.2f, Radius_Crawl_KneeImpact, 1.25f,
			a_Right ? DamageSource::KneeRight : DamageSource::KneeLeft);

		ApplyThighDamage(giant, a_Right, false, Radius_ThighCrush_ButtCrush_Drop, Damage_Crawl_KneeImpact * 0.75f, 0.35f, 1.0f, 14, DamageSource::ThighCrushed);
	}

	void OnKneeImpactLeft(const ActionContext& a_Ctx)  { OnKneeImpact(a_Ctx, false); }
	void OnKneeImpactRight(const ActionContext& a_Ctx) { OnKneeImpact(a_Ctx, true); }

	void OnHandImpactLeft(const ActionContext& a_Ctx) {

		auto* giant = a_Ctx.Actor();

		// The hand carrying someone is not on the ground.
		if (AnimationVars::Grab::HasGrabbedTiny(giant)) {
			return;
		}

		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.70f, Damage_Crawl_HandImpact, CrawlEvent::LeftHand, "LeftHand", 1.0f, Radius_Crawl_HandImpact, 1.25f, DamageSource::HandCrawlLeft);
	}

	void OnHandImpactRight(const ActionContext& a_Ctx) {
		auto* giant = a_Ctx.Actor();
		DoCrawlingFunctions(giant, get_visual_scale(giant), 1.70f, Damage_Crawl_HandImpact, CrawlEvent::RightHand, "RightHand", 1.0f, Radius_Crawl_HandImpact, 1.25f, DamageSource::HandCrawlRight);
	}

	//----------------------------------------------------------------------------------------------
	// Tables
	//----------------------------------------------------------------------------------------------

	constexpr GraphExpect Signature[] = {
		{ "GTS_IsCrawling", true },
	};

	// No keys. The toggle is a persistent setting pushed through UpdateCrawlAnimations, and the
	// keybind that flips that setting lives in InputFunctions with the rest of the toggles.
	constexpr EntryDef Entries[] = {
		{ .Action = "Crawl.On",  .Behaviour = "GTSBeh_Crawl_On" },
		{ .Action = "Crawl.Off", .Behaviour = "GTSBeh_Crawl_Off", .Exits = true },
	};

	constexpr AnnotationDef Annotations[] = {
		{ .Tag = "GTS_Crawl_Knee_Trans_Impact", .Handler = OnKneeTransition },
		{ .Tag = "GTS_Crawl_Hand_Trans_Impact", .Handler = OnHandTransition },
		{ .Tag = "GTSCrawl_KneeImpact_L",       .Handler = OnKneeImpactLeft },
		{ .Tag = "GTSCrawl_KneeImpact_R",       .Handler = OnKneeImpactRight },
		{ .Tag = "GTSCrawl_HandImpact_L",       .Handler = OnHandImpactLeft },
		{ .Tag = "GTSCrawl_HandImpact_R",       .Handler = OnHandImpactRight },
	};
}

namespace GTS::Actions {

	std::span<const GraphExpect> CrawlMovement::Signature() const     { return ::Signature; }
	std::span<const EntryDef> CrawlMovement::Entries() const          { return ::Entries; }
	std::span<const AnnotationDef> CrawlMovement::Annotations() const { return ::Annotations; }
}
