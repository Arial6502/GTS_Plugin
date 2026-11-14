#include "Managers/AI/Adapters.hpp"
#include "Managers/Animation/Controllers/GrabAnimationController.hpp"
#include "Managers/Animation/Controllers/HugController.hpp"
#include "Actions/Nodes/Hug/HugNode.hpp"
#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Controllers/ButtCrushController.hpp"
#include "Managers/Animation/Controllers/ThighCrushController.hpp"
#include "Managers/Animation/Controllers/ThighSandwichController.hpp"
#include "Managers/Damage/TinyCalamity.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Config/Config.hpp"

namespace {

	using namespace GTS;
	using Actions::ActionId;
	using Actions::Stance;

	constexpr std::uint8_t UprightOnly = AI::StanceBit(Stance::kStanding);
	constexpr std::uint8_t CanSneak    = UprightOnly | AI::StanceBit(Stance::kSneak);
	constexpr std::uint8_t CanCrawl    = CanSneak | AI::StanceBit(Stance::kCrawl);

	bool Accepted(RE::Actor* a_Actor, std::string_view a_Action) {
		return Actions::ActionRegistry::Perform(a_Actor, a_Action) == Actions::RequestResult::kAccepted;
	}

	constexpr ActionId GrabNodes[]     = { ActionId::kGrab, ActionId::kGrabPlay, ActionId::kCleavage };
	constexpr ActionId HugNodes[]      = { ActionId::kHug };
	constexpr ActionId VoreNodes[]     = { ActionId::kVore };
	constexpr ActionId ButtNodes[]     = { ActionId::kButtCrush, ActionId::kBoobCrush };
	constexpr ActionId ThighNodes[]    = { ActionId::kThighCrush };
	constexpr ActionId SandwichNodes[] = { ActionId::kThighSandwich, ActionId::kThighSandwichButt };
	constexpr ActionId StompNodes[]    = { ActionId::kStomp, ActionId::kTrample };
	constexpr ActionId KickNodes[]     = { ActionId::kKickSwipe };
	constexpr ActionId CalamityNodes[] = { ActionId::kCalamity };
	constexpr ActionId GrowthNodes[]   = { ActionId::kGrowth };
	constexpr ActionId ShrinkNodes[]   = { ActionId::kShrink };

	// Foot attacks reach further crawling, and only land on someone enough smaller to be worth it.
	constexpr float StompDistance = 50.0f;
	constexpr float StompSizeRatio = 1.5f;

	float StompReach(RE::Actor* a_Pred) {
		const float crawling = AnimationVars::Crawl::IsCrawling(a_Pred) ? 2.0f : 1.0f;
		return StompDistance * get_visual_scale(a_Pred) * crawling;
	}

	// CrushCheck needs difference > Action_Crush * crushmult, where crushmult is what the action hands
	// the damage call. A stomp passes 1.0; kicks and swipes hit harder per blow but need far more of a
	// size gap to crush, and the gentlest of them sets the bar for the whole stance.
	constexpr float StompCrushMult = 1.0f;
	constexpr float KickCrushMult  = 1.8f;   // standing kicks, all three
	constexpr float SneakCrushMult = 1.4f;   // the stronger sneak swipe
	constexpr float CrawlCrushMult = 1.3f;   // the stronger crawl swipe

	// Whether hitting this corpse would actually crush it. Measured the way CrushCheck measures it,
	// without the calamity bonus and with high heels, which is not how the size gate below is measured.
	bool WouldCrush(RE::Actor* a_Pred, RE::Actor* a_Prey, float a_CrushMult) {
		return get_scale_difference(a_Pred, a_Prey, SizeType::VisualScale, false, true) > Action_Crush * a_CrushMult;
	}

	float FootCrushMult(RE::Actor* a_Pred) {

		if (AnimationVars::Crawl::IsCrawling(a_Pred)) {
			return CrawlCrushMult;
		}

		return a_Pred->IsSneaking() ? SneakCrushMult : KickCrushMult;
	}

	bool StompQualifies(RE::Actor* a_Pred, RE::Actor* a_Prey) {

		if (!a_Pred || !a_Prey || a_Pred == a_Prey) {
			return false;
		}

		// Followers do not stomp each other when the setting protects them.
		if (!a_Prey->IsPlayerRef() && !IsHostile(a_Pred, a_Prey) && Config::General.bProtectFollowers &&
			IsTeammate(a_Pred) && IsTeammate(a_Prey)) {
			return false;
		}

		if (!CanPerformActionOn(a_Pred, a_Prey, false)) {
			return false;
		}

		// Hitting a corpse only means anything if the blow crushes it, so that is the bar rather than
		// the one for deciding a stomp is worth starting.
		if (a_Prey->IsDead()) {
			return WouldCrush(a_Pred, a_Prey, StompCrushMult);
		}

		return get_scale_difference(a_Pred, a_Prey, SizeType::VisualScale, true, false) > StompSizeRatio;
	}

	// Same rules, but a kick or a swipe needs a far larger gap before it crushes anything.
	bool KickQualifies(RE::Actor* a_Pred, RE::Actor* a_Prey) {

		if (!a_Prey || !a_Prey->IsDead()) {
			return StompQualifies(a_Pred, a_Prey);
		}

		return StompQualifies(a_Pred, a_Prey) && WouldCrush(a_Pred, a_Prey, FootCrushMult(a_Pred));
	}

	bool Standing(RE::Actor* a_Actor, RE::Actor*) {
		return !AnimationVars::Crawl::IsCrawling(a_Actor);
	}

	bool Crawling(RE::Actor* a_Actor, RE::Actor*) {
		return AnimationVars::Crawl::IsCrawling(a_Actor);
	}

	// Trample has no crawling or sneaking variant.
	bool Upright(RE::Actor* a_Actor, RE::Actor*) {
		return !AnimationVars::Crawl::IsCrawling(a_Actor) && !a_Actor->IsSneaking();
	}

	const std::array StompStarts = {
		AI::Choice{ "Stomp.Light",   ActionId::kNone, &Config::AI.Stomp.fLightProb },
		AI::Choice{ "Stomp.Strong",  ActionId::kNone, &Config::AI.Stomp.fStrongProb },
		AI::Choice{ "Stomp.Trample", ActionId::kNone, &Config::AI.Stomp.fTrampleProb, &Upright },
	};

	// Standing gives kicks, crawling gives swipes. The old AI branched on the same check.
	const std::array KickStarts = {
		AI::Choice{ "Kick.Light",    ActionId::kNone, &Config::AI.KickSwipe.fKickLightProb,  &Standing },
		AI::Choice{ "Kick.Heavy",    ActionId::kNone, &Config::AI.KickSwipe.fKickHeavyProb,  &Standing },
		AI::Choice{ "Kick.LowSweep", ActionId::kNone, &Config::AI.KickSwipe.fKickLowProb,    &Standing },
		AI::Choice{ "Swipe.Light",   ActionId::kNone, &Config::AI.KickSwipe.fSwipeLightProb, &Crawling },
		AI::Choice{ "Swipe.Heavy",   ActionId::kNone, &Config::AI.KickSwipe.fSwipeHeavyProb, &Crawling },
	};

	// Past this the actor is already big enough that shrinking someone adds nothing.
	constexpr float CalamityMaxScale = 2.0f;

	// The shout is what applies the effect the other two calamity nodes need, so it is a plain self
	// action rather than anything the Actions system knows about.
	bool CalamityReady(RE::Actor* a_Actor) {
		return Runtime::HasPerk(a_Actor, Runtime::PERK.GTSPerkTinyCalamity) &&
		       a_Actor->IsInCombat() &&
		       get_visual_scale(a_Actor) <= CalamityMaxScale &&
		       !TinyCalamityActive(a_Actor);
	}

	bool WrathReady(RE::Actor* a_Actor) {
		return TinyCalamityActive(a_Actor) &&
		       !IsActionOnCooldown(a_Actor, CooldownSource::Misc_TinyCalamity_WrathfulCalamity);
	}

	// Growth.Random is the involuntary one and belongs to RandomGrowth, which decides for itself. The
	// AI grows on purpose, so it asks for the manual action and is refused by its guard like anyone else.
	const std::array GrowthStarts = {
		AI::Choice{ "Growth.Manual", ActionId::kNone, &Config::AI.Growth.fProbability },
	};

	const std::array ShrinkStarts = {
		AI::Choice{ "Shrink.Manual", ActionId::kNone, &Config::AI.Shrink.fProbability },
	};

	const std::array GrabSubs = {
		AI::Choice{ "Grab.Throw",          ActionId::kGrab,     &Config::AI.Grab.fThrowProb },
		AI::Choice{ "Grab.Vore",           ActionId::kGrab,     &Config::AI.Grab.fVoreProb },
		AI::Choice{ "Grab.Attack",         ActionId::kGrab,     &Config::AI.Grab.fCrushProb },
		AI::Choice{ "Grab.Release",        ActionId::kGrab,     &Config::AI.Grab.fReleaseProb },
		AI::Choice{ "Grab.Unstore",        ActionId::kGrab,     &Config::AI.Grab.fUnstoreProb },
		AI::Choice{ "Grab.Play",           ActionId::kGrab,     &Config::AI.Grab.fGrabPlayStartProb },
		AI::Choice{ "Cleavage.Enter",      ActionId::kGrab,     &Config::AI.Grab.fCleavageProb },

		AI::Choice{ "GrabPlay.Crush",      ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayHeavyCrushProb },
		AI::Choice{ "GrabPlay.Vore",       ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayVoreProb },
		AI::Choice{ "GrabPlay.Kiss",       ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayKissProb },
		AI::Choice{ "GrabPlay.KissVore",   ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayKissVoreProb },
		AI::Choice{ "GrabPlay.Poke",       ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayPokeProb },
		AI::Choice{ "GrabPlay.Flick",      ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayFlickProb },
		AI::Choice{ "GrabPlay.Sandwich",   ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlaySandwichProb },
		AI::Choice{ "GrabPlay.GrindStart", ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayGrindStartProb },
		AI::Choice{ "GrabPlay.GrindStop",  ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayGrindStopProb },
		AI::Choice{ "GrabPlay.Exit",       ActionId::kGrabPlay, &Config::AI.Grab.fGrabPlayExitProb },

		AI::Choice{ "Cleavage.Light",      ActionId::kCleavage, &Config::AI.Grab.fCleavageAttackProb },
		AI::Choice{ "Cleavage.Heavy",      ActionId::kCleavage, &Config::AI.Grab.fCleavageHeavyProb },
		AI::Choice{ "Cleavage.Absorb",     ActionId::kCleavage, &Config::AI.Grab.fCleavageAbsorbProb },
		AI::Choice{ "Cleavage.Vore",       ActionId::kCleavage, &Config::AI.Grab.fCleavageVoreProb },
		AI::Choice{ "Cleavage.Suffocate",  ActionId::kCleavage, &Config::AI.Grab.fCleavageSuffocateProb },
		AI::Choice{ "Cleavage.SufoStop",   ActionId::kCleavage, &Config::AI.Grab.fCleavageSufoStopProb },
		AI::Choice{ "Cleavage.DotStart",   ActionId::kCleavage, &Config::AI.Grab.fStrangleChance },
		AI::Choice{ "Cleavage.DotStop",    ActionId::kCleavage, &Config::AI.Grab.fCleavageDotStopProb },
		AI::Choice{ "Cleavage.Exit",       ActionId::kCleavage, &Config::AI.Grab.fCleavageStopProb },
	};

	// Healing a hostile makes no sense, and the kill rows respect the two friendly-fire toggles.
	bool Friendly(RE::Actor* a_Performer, RE::Actor* a_Partner) {
		return a_Partner && (IsTeammate(a_Partner) || a_Partner->IsPlayerRef()) && !IsHostile(a_Partner, a_Performer);
	}

	bool HealAllowed(RE::Actor* a_Performer, RE::Actor* a_Partner) {
		return Friendly(a_Performer, a_Partner);
	}

	// Shrinking a friendly is its own setting, so the two rows split the same action between them.
	bool ShrinkHostile(RE::Actor* a_Performer, RE::Actor* a_Partner) {
		return a_Partner && !Friendly(a_Performer, a_Partner);
	}

	// Nothing left to take. Friendlies are only let go when the setting says so; everyone else always.
	bool HugFinished(RE::Actor* a_Performer, RE::Actor* a_Partner) {

		if (!a_Partner || !Actions::HugNode::Drained(a_Performer, a_Partner)) {
			return false;
		}

		return Config::AI.Hugs.bStopIfCantShrink || !Friendly(a_Performer, a_Partner);
	}

	bool KillAllowed(RE::Actor* a_Performer, RE::Actor* a_Partner) {

		if (!a_Partner) {
			return false;
		}

		if (a_Partner->IsPlayerRef() || IsTeammate(a_Partner)) {
			return Config::AI.Hugs.bKillFollowersOrPlayer;
		}

		if (!IsHostile(a_Partner, a_Performer) && !IsHostile(a_Performer, a_Partner)) {
			return Config::AI.Hugs.bKillFriendlies;
		}

		return true;
	}

	const std::array HugSubs = {
		AI::Choice{ "Hug.Heal",    ActionId::kHug, &Config::AI.Hugs.fHealProb,    &HealAllowed },
		AI::Choice{ "Hug.Shrink",  ActionId::kHug, &Config::AI.Hugs.fShrinkProb,         &ShrinkHostile },
		AI::Choice{ "Hug.Shrink",  ActionId::kHug, &Config::AI.Hugs.fFriendlyShrinkProb, &Friendly },
		AI::Choice{ "Hug.Crush",   ActionId::kHug, &Config::AI.Hugs.fKillProb,    &KillAllowed },
		AI::Choice{ "Hug.Release", ActionId::kHug, &Config::AI.Hugs.fReleaseProb },
	};

	// Both crush nodes expose the same two actions, so each row is listed against both.
	const std::array CrushSubs = {
		AI::Choice{ "Crush.Grow",   ActionId::kButtCrush, &Config::AI.ButtCrush.fGrowProb },
		AI::Choice{ "Crush.Attack", ActionId::kButtCrush, &Config::AI.ButtCrush.fCrushProb },
		AI::Choice{ "Crush.Grow",   ActionId::kBoobCrush, &Config::AI.ButtCrush.fGrowProb },
		AI::Choice{ "Crush.Attack", ActionId::kBoobCrush, &Config::AI.ButtCrush.fCrushProb },
	};

	const std::array ThighSubs = {
		AI::Choice{ "ThighCrush.Attack", ActionId::kThighCrush, &Config::AI.ThighCrush.fProbabilityHeavy },
	};

	// The butt node has its own namespace of actions, so the two halves share no names at all.
	const std::array SandwichSubs = {
		AI::Choice{ "Sandwich.Attack",          ActionId::kThighSandwich,     &Config::AI.ThighSandwich.fLightAttackProb },
		AI::Choice{ "Sandwich.AttackHeavy",     ActionId::kThighSandwich,     &Config::AI.ThighSandwich.fHeavyAttackPob },
		AI::Choice{ "Sandwich.Butt.Start",      ActionId::kThighSandwich,     &Config::AI.ThighSandwich.fEnterButtModeProb },
		AI::Choice{ "Sandwich.UB",              ActionId::kThighSandwich,     &Config::AI.ThighSandwich.fButtVoreProb },
		AI::Choice{ "Sandwich.Exit",            ActionId::kThighSandwich,     &Config::AI.ThighSandwich.fExitProb, nullptr, true },

		AI::Choice{ "Sandwich.Butt.Light",      ActionId::kThighSandwichButt, &Config::AI.ThighSandwich.fButtLAtkProb },
		AI::Choice{ "Sandwich.Butt.Heavy",      ActionId::kThighSandwichButt, &Config::AI.ThighSandwich.fButtHAtkProb },
		AI::Choice{ "Sandwich.Butt.Grow",       ActionId::kThighSandwichButt, &Config::AI.ThighSandwich.fButtGrowProb },
		AI::Choice{ "Sandwich.Butt.GrindStart", ActionId::kThighSandwichButt, &Config::AI.ThighSandwich.fButtGrindStart },
		AI::Choice{ "Sandwich.Butt.GrindStop",  ActionId::kThighSandwichButt, &Config::AI.ThighSandwich.fButtGrindStop },
		AI::Choice{ "Sandwich.Butt.Stop",       ActionId::kThighSandwichButt, &Config::AI.ThighSandwich.fButtExitProb, nullptr, true },
	};

	const std::array Adapters = {

		AI::Adapter{
			.Name = "Grab",
			.Stances = CanCrawl,
			.Nodes = GrabNodes,
			.Enabled = &Config::AI.Grab.bEnableAction,
			.Chance = &Config::AI.Grab.fProbability,
			.Interval = &Config::AI.Grab.fInterval,
			.Idle = &Config::AI.Grab.fIdleProb,
			.Shape = &TargetShape::Grab,
			.Reach = &GrabAnimationController::Reach,
			.Qualifies = &GrabAnimationController::Qualifies,
			.Start = &GrabAnimationController::StartGrab,
			.Subs = GrabSubs,
		},

		AI::Adapter{
			.Name = "Hug",
			.Stances = CanCrawl,
			.Nodes = HugNodes,
			.Enabled = &Config::AI.Hugs.bEnableAction,
			.Chance = &Config::AI.Hugs.fProbability,
			.Interval = &Config::AI.Hugs.fInterval,
			.Idle = &Config::AI.Hugs.fIdleProb,
			.Shape = &TargetShape::Hug,
			.Finished = &HugFinished,
			.Reach = &HugAnimationController::Reach,
			.Qualifies = &HugAnimationController::Qualifies,
			.Start = &HugAnimationController::StartHug,
			.Subs = HugSubs,
			.Bail = "Hug.Release",
		},

		AI::Adapter{
			.Name = "Vore",
			.Stances = CanCrawl,
			.Nodes = VoreNodes,
			.Enabled = &Config::AI.Vore.bEnableAction,
			.Chance = &Config::AI.Vore.fProbability,
			.Interval = &Config::AI.fMasterTimer,
			.Shape = &TargetShape::Vore,
			.Reach = &VoreController::Reach,
			.Qualifies = &VoreController::Qualifies,
			.Start = [](RE::Actor* a_Pred, RE::Actor* a_Prey) {
				return VoreController::GetSingleton().StartVore(a_Pred, a_Prey);
			},
		},

		AI::Adapter{
			.Name = "ButtCrush",
			.Nodes = ButtNodes,
			.Enabled = &Config::AI.ButtCrush.bEnableAction,
			.Chance = &Config::AI.ButtCrush.fProbability,
			.Interval = &Config::AI.ButtCrush.fInterval,
			.Idle = &Config::AI.ButtCrush.fIdleProb,
			.Shape = &TargetShape::ButtCrush,
			.Reach = &ButtCrushController::Reach,
			.Qualifies = &ButtCrushController::Qualifies,
			.Start = [](RE::Actor* a_Pred, RE::Actor* a_Prey) {
				const bool targeted = RandomFloat(0.0f, 100.0f) < Config::AI.ButtCrush.fButtCrushTypeProb;
				return ButtCrushController::StartButtCrush(a_Pred, a_Prey, true, targeted ? "Enter" : "EnterQuick");
			},
			.Subs = CrushSubs,
		},

		AI::Adapter{
			.Name = "ThighCrush",
			.Nodes = ThighNodes,
			.Enabled = &Config::AI.ThighCrush.bEnableAction,
			.Chance = &Config::AI.ThighCrush.fProbability,
			.Interval = &Config::AI.ThighCrush.fInterval,
			.Idle = &Config::AI.ThighCrush.fIdleProb,
			.Shape = &TargetShape::ThighCrush,
			.Reach = &ThighCrushController::Reach,
			.Qualifies = &ThighCrushController::Qualifies,
			.Start = [](RE::Actor* a_Pred, RE::Actor*) {
				return Accepted(a_Pred, "ThighCrush.Enter");
			},
			.Subs = ThighSubs,
			.Bail = "ThighCrush.Exit",
		},

		AI::Adapter{
			.Name = "ThighSandwich",
			.Nodes = SandwichNodes,
			.Enabled = &Config::AI.ThighSandwich.bEnableAction,
			.Chance = &Config::AI.ThighSandwich.fProbability,
			.Interval = &Config::AI.ThighSandwich.fInterval,
			.Idle = &Config::AI.ThighSandwich.fIdleProb,
			.Shape = &TargetShape::Sandwich,
			.Reach = &ThighSandwichController::Reach,
			.Qualifies = &ThighSandwichController::Qualifies,
			.Start = [](RE::Actor* a_Pred, RE::Actor* a_Prey) {
				return ThighSandwichController::StartSandwiching(a_Pred, a_Prey, true);
			},
			.Subs = SandwichSubs,
		},

		AI::Adapter{
			.Name = "Stomp",
			.Stances = CanCrawl,
			.Nodes = StompNodes,
			.Enabled = &Config::AI.Stomp.bEnableAction,
			.Chance = &Config::AI.Stomp.fProbability,
			.Shape = &TargetShape::Stomp,
			.Reach = &StompReach,
			.Qualifies = &StompQualifies,
			.Starts = StompStarts,
		},

		AI::Adapter{
			.Name = "KickSwipe",
			.Stances = CanCrawl,
			.Nodes = KickNodes,
			.Enabled = &Config::AI.KickSwipe.bEnableAction,
			.Chance = &Config::AI.KickSwipe.fProbability,
			.Shape = &TargetShape::Stomp,
			.Reach = &StompReach,
			.Qualifies = &KickQualifies,
			.Starts = KickStarts,
		},

		AI::Adapter{
			.Name = "Calamity",
			.Nodes = CalamityNodes,
			.Enabled = &Config::AI.TinyCalamity.bEnableAction,
			.Chance = &Config::AI.TinyCalamity.fProbability,
			.Ready = &CalamityReady,
			.Start = [](RE::Actor* a_Actor, RE::Actor*) {
				return TinyCalamity_Shout(a_Actor);
			},
		},

		AI::Adapter{
			.Name = "WrathfulCalamity",
			.Nodes = CalamityNodes,
			.Enabled = &Config::AI.WrathfulCalamity.bEnableAction,
			.Chance = &Config::AI.WrathfulCalamity.fProbability,
			.Shape = &TargetShape::Vore,
			.Ready = &WrathReady,
			.Reach = &VoreController::Reach,
			.Qualifies = &TinyCalamity_CanErase,
			.Start = [](RE::Actor* a_Pred, RE::Actor* a_Prey) {
				return TinyCalamity_WrathfulCalamity(a_Pred, std::vector<RE::Actor*>{ a_Prey });
			},
		},

		AI::Adapter{
			.Name = "Growth",
			.Nodes = GrowthNodes,
			.Enabled = &Config::AI.Growth.bEnableAction,
			.Chance = &Config::AI.Growth.fProbability,
			.Starts = GrowthStarts,
		},

		AI::Adapter{
			.Name = "Shrink",
			.Nodes = ShrinkNodes,
			.Enabled = &Config::AI.Shrink.bEnableAction,
			.Chance = &Config::AI.Shrink.fProbability,
			.Starts = ShrinkStarts,
		},
	};
}

namespace GTS::AI {

	std::span<const Adapter> Table() {
		return Adapters;
	}

	std::string_view ExitFor(const Adapter& a_Adapter, Actions::ActionId a_Current) {

		for (const Choice& choice : a_Adapter.Subs) {
			if (choice.Exit && choice.Node == a_Current) {
				return choice.Action;
			}
		}

		return a_Adapter.Bail;
	}

	bool InBranch(RE::Actor* a_Actor, const Adapter& a_Adapter) {

		const Actions::ActorAction* state = Actions::ActionRegistry::StateOf(a_Actor);

		if (!state) {
			return false;
		}

		return std::ranges::contains(a_Adapter.Nodes, state->Current) ||
		       std::ranges::contains(a_Adapter.Nodes, state->Pending) ||
		       std::ranges::contains(a_Adapter.Nodes, state->Resumes);
	}
}
