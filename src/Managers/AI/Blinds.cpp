#include "Managers/AI/Blinds.hpp"

namespace GTS::AI {

	void Blinds::Apply(RE::Actor* a_Actor) {
		if (a_Actor) {
			if (RE::CombatController* combat = a_Actor->GetActorRuntimeData().combatController) {
				combat->inactive = true;
			}
		}
	}

	void Blinds::Release(RE::Actor* a_Actor) {
		if (a_Actor) {
			if (RE::CombatController* combat = a_Actor->GetActorRuntimeData().combatController) {
				combat->inactive = false;
			}
		}
	}
}
