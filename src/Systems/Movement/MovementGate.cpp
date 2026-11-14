#include "Systems/Movement/MovementGate.hpp"

namespace {

	std::shared_mutex g_Lock;
	absl::flat_hash_set<RE::FormID> g_Held;
	thread_local bool t_Issuing = false;
}

namespace GTS {

	void MovementGate::Take(RE::FormID a_Actor) {
		std::unique_lock lock(g_Lock);
		g_Held.insert(a_Actor);
	}

	void MovementGate::Give(RE::FormID a_Actor) {
		std::unique_lock lock(g_Lock);
		g_Held.erase(a_Actor);
	}

	void MovementGate::Clear() {
		std::unique_lock lock(g_Lock);
		g_Held.clear();
	}

	bool MovementGate::Holds(RE::FormID a_Actor) {
		std::shared_lock lock(g_Lock);
		return g_Held.contains(a_Actor);
	}

	MovementGate::Issuing::Issuing() {
		t_Issuing = true;
	}

	MovementGate::Issuing::~Issuing() {
		t_Issuing = false;
	}

	bool MovementGate::IsIssuing() {
		return t_Issuing;
	}
}
