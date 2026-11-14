#pragma once

namespace GTS {

	enum class Gait : std::uint8_t {
		kWalk = 0,
		kJog = 1,
		kRun = 2,
		kFastWalk = 3,
	};

	// Walks an actor to a target across the navmesh. Owning the movement goal is the Director's job and
	// lasts the whole plan, not just the walk: see MovementGate. Replaces CombatSteering, which used the
	// keep offset planner and ignored the navmesh.
	class Steering {

		public:
		// Call every frame while approaching. Returns false when the engine refused the goal, which
		// means the target cannot be pathed to right now.
		static bool Update(RE::Actor* a_Actor, RE::Actor* a_Target, Gait a_Gait);

		static void Stop(RE::Actor* a_Actor);
		static void Clear();

		[[nodiscard]] static bool IsSteering(const RE::Actor* a_Actor);

		// Seconds since the actor last got meaningfully closer to its target, 0 while it is closing.
		[[nodiscard]] static float Stalled(const RE::Actor* a_Actor);
	};
}
