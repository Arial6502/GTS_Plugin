#pragma once

#include "Actions/Core/ActionTypes.hpp"
#include "Utils/Actions/TargetShape.hpp"

namespace GTS::AI {

	constexpr std::uint8_t StanceBit(Actions::Stance a_Stance) {
		return static_cast<std::uint8_t>(1 << std::to_underlying(a_Stance));
	}

	// One thing the AI can send while it is inside a branch. Node is which of the branch's nodes has to
	// be current for the row to apply, so Grab, GrabPlay and Cleavage can share one list.
	struct Choice {
		std::string_view Action;
		Actions::ActionId Node = Actions::ActionId::kNone;
		float* Weight = nullptr;

		// AI policy only, such as healing only teammates. Legality stays in the node.
		bool (*Allow)(RE::Actor* a_Performer, RE::Actor* a_Partner) = nullptr;

		// The way out of this node. A branch whose nodes each have their own exit marks one row per
		// node rather than naming a single action for the whole branch.
		bool Exit = false;

		// Sent after this one, once the node accepts it.
		std::string_view Then = {};
	};

	// One branch of the Actions system the AI knows how to drive. Data, in the same spirit as EntryDef
	// and ActionDef: the Director reads the table and never names an action itself.
	struct Adapter {

		std::string_view Name;

		// The stances this branch has animations for. Standing is always available.
		std::uint8_t Stances = StanceBit(Actions::Stance::kStanding);

		// Every node the branch can be in. Grab hands off to GrabPlay and Cleavage without the brain
		// being asked, so all three count as still engaged.
		std::span<const Actions::ActionId> Nodes;

		bool* Enabled = nullptr;
		float* Chance = nullptr;
		float* Interval = nullptr;

		// Weight of doing nothing when a sub-action is picked.
		float* Idle = nullptr;

		const TargetShape::Cone* Shape = nullptr;

		// An extra gate on the performer alone, for a branch that is only available in some state.
		bool (*Ready)(RE::Actor*) = nullptr;

		// True once the branch has nothing left to do with the actor it is acting on, which sends the
		// exit without waiting for the picks to dry up.
		bool (*Finished)(RE::Actor* a_Performer, RE::Actor* a_Partner) = nullptr;

		// Size and legality, with no distance in it. The player's own path keeps the messages. A null
		// Qualifies means the branch needs no target: it acts on the performer, or on whatever the node
		// finds for itself.
		float (*Reach)(RE::Actor*) = nullptr;
		bool (*Qualifies)(RE::Actor*, RE::Actor*) = nullptr;
		bool (*Start)(RE::Actor*, RE::Actor*) = nullptr;

		// Weighted pick of what to send to begin with, for branches whose start is one of several
		// actions. Takes precedence over Start.
		std::span<const Choice> Starts = {};

		std::span<const Choice> Subs = {};

		// Sent when nothing else qualifies, for branches that should not be held forever. Used only when
		// no Subs row is marked Exit for the node that is running.
		std::string_view Bail = {};
	};

	[[nodiscard]] std::span<const Adapter> Table();

	// True when the actor is somewhere inside this adapter's branch, including a node it handed off to
	// and one it was suspended for.
	[[nodiscard]] bool InBranch(RE::Actor* a_Actor, const Adapter& a_Adapter);

	// How this branch gets out of the node that is currently running.
	[[nodiscard]] std::string_view ExitFor(const Adapter& a_Adapter, Actions::ActionId a_Current);
}
