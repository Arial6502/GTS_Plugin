#pragma once

namespace GTS::AI {

	// Holds back an actor's regular attacks. Two sources: the size policy, which is the old
	// AttackManager behaviour, and a plan, which suppresses for as long as the Director is driving.
	class Suppression {

		public:
		// Read by Hooks/Actor/Actor.cpp on the hit path, so it may be called off the main thread.
		[[nodiscard]] static bool Holds(RE::Actor* a_Actor);

		[[nodiscard]] static bool SizePolicy(RE::Actor* a_Actor);
		[[nodiscard]] static RE::Actor* Target(RE::Actor* a_Actor);

		static void Take(RE::FormID a_Actor);
		static void Give(RE::FormID a_Actor);
		static void Clear();

		// Sheathes and stops casting. Called every tick while an actor is suppressed.
		static void Enforce(RE::Actor* a_Actor);
	};
}
