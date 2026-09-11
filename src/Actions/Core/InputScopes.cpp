#include "Actions/Core/InputScopes.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/MovementRegistry.hpp"

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	bool AtRoot(RE::Actor* a_Actor, std::uint32_t) {
		return a_Actor && !ActionRegistry::Busy(a_Actor);
	}

	// Idle, or the running node steps aside for this entry, as Perform does through Permits.
	bool AtEntry(RE::Actor* a_Actor, std::uint32_t a_Entry) {

		if (!a_Actor) {
			return false;
		}

		if (!ActionRegistry::Busy(a_Actor)) {
			return true;
		}

		const auto* running = ActionRegistry::Find(ActionRegistry::Current(a_Actor));
		return running && running->Permits(static_cast<ActionId>(a_Entry), a_Actor->formID);
	}

	bool InStance(RE::Actor* a_Actor, std::uint32_t a_Id) {
		return a_Actor && MovementRegistry::In(a_Actor, static_cast<MovementId>(a_Id));
	}

	bool InNode(RE::Actor* a_Actor, std::uint32_t a_Id) {
		return a_Actor && ActionRegistry::Current(a_Actor) == static_cast<ActionId>(a_Id);
	}
}

namespace GTS::Actions {

	InputScope RootScope() {
		return { .InScope = AtRoot, .Param = 0, .Rank = ScopeRank::kRoot };
	}

	InputScope EntryScope(ActionId a_Entry) {
		return { .InScope = AtEntry, .Param = std::to_underlying(a_Entry), .Rank = ScopeRank::kRoot };
	}

	InputScope StanceScope(MovementId a_Id) {
		return { .InScope = InStance, .Param = std::to_underlying(a_Id), .Rank = ScopeRank::kStance };
	}

	InputScope NodeScope(ActionId a_Id) {
		return { .InScope = InNode, .Param = std::to_underlying(a_Id), .Rank = ScopeRank::kNode };
	}
}
