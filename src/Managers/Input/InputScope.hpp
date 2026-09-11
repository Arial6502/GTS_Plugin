#pragma once

namespace GTS {

	// How specific a scope is. When one key carries several bindings the most specific live one wins,
	// so an action node's key beats a stance's, which beats one that is live whenever nothing is
	// running. Override sits above all of them for a UI that takes the input over, such as a wheel.
	enum class ScopeRank : std::uint8_t {
		kRoot = 0,
		kStance = 1,
		kNode = 2,
		kOverride = 3,
	};

	// Whether a binding is live for this actor right now.
	//
	// The predicate and its parameter are supplied by whoever registers the binding, so the action
	// system is not the only thing that can scope one. That is also why this carries a plain function
	// pointer and an integer rather than naming ActionId: InputManager must not depend on the action
	// layer, since the action layer already depends on it.
	struct InputScope {

		bool (*InScope)(RE::Actor*, std::uint32_t) = nullptr;   // null: live whatever is running
		std::uint32_t Param = 0;
		ScopeRank Rank = ScopeRank::kRoot;

		[[nodiscard]] bool Live(RE::Actor* a_Actor) const {
			return !InScope || InScope(a_Actor, Param);
		}
	};
}
