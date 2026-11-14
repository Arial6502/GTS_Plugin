#pragma once

namespace GTS::TargetShape {

	// A cone in front of the actor with its apex pushed behind them, so actors right at the feet count
	// as in front. Width is the actor's width at scale 1.
	struct Cone {
		float AngleDeg;
		float Width = 70.0f;
	};

	inline constexpr Cone Grab     { 70.0f };
	inline constexpr Cone Hug      { 70.0f };
	inline constexpr Cone ButtCrush{ 70.0f };
	inline constexpr Cone Vore     { 75.0f };
	inline constexpr Cone ThighCrush{ 75.0f };
	inline constexpr Cone Sandwich { 60.0f };
	inline constexpr Cone Stomp    { 50.0f };

	[[nodiscard]] bool InReach(RE::Actor* a_Pred, RE::Actor* a_Prey, float a_Reach);
	[[nodiscard]] bool InCone(RE::Actor* a_Pred, RE::Actor* a_Prey, const Cone& a_Cone);

	// The candidates inside the cone, nearest first.
	[[nodiscard]] std::vector<RE::Actor*> InFront(RE::Actor* a_Pred, std::vector<RE::Actor*> a_Candidates, const Cone& a_Cone);
}
