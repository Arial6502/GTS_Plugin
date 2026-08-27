#pragma once

namespace GTS {

	// Walks a suppressed actor at its combat target using the engine's own keep offset movement
	// planner, which paths on the navmesh and works no matter what the combat behaviour tree is doing.
	class CombatSteering {

		public:

		static void Apply(RE::Actor* a_Actor, RE::Actor* a_Target);
		static void Release(RE::Actor* a_Actor);
		static void Clear();

		//True if we are currently steering the actor.
		[[nodiscard]] static bool IsSteering(const RE::Actor* a_Actor);
	};
}
