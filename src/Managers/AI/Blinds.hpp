#pragma once

namespace GTS::AI {

	// Takes the actor out of the fight while the Director drives her. CombatController::inactive is a
	// held flag, so it is applied every tick of a phase and cleared when the plan ends.
	class Blinds {

		public:
		static void Apply(RE::Actor* a_Actor);
		static void Release(RE::Actor* a_Actor);
	};
}
