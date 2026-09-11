#pragma once

#include "Actions/Core/ActionContext.hpp"

namespace GTS::SandwichingData_fwd {}

namespace GTS::Actions::Sandwich {

	// Whoever the sandwich is holding. Both branches use the same slot, so the handoff between them
	// moves nothing: the actors belong to the sandwich, not to the branch that happens to be running.
	constexpr PossessionSlot Slot = PossessionSlot::kThighs;

	// Leaves the root Giantess_State from any point inside it. Both branches run in that state.
	constexpr std::string_view BEH_ABORT = "GTSBeh_ExitEvents";

	// SandwichingData stays the authority for who is in the sandwich, the way VoreData does for vore.
	// The possession slot mirrors it so the registry can see the pair.
	void Claim(const ActionContext& a_Ctx);

	[[nodiscard]] std::vector<RE::Actor*> Tinies(RE::Actor* a_Giant);
	[[nodiscard]] bool Empty(RE::Actor* a_Giant);

	void Suffocate(RE::Actor* a_Giant, bool a_Enable, float a_Mult = 1.0f);
	void ResetData(RE::Actor* a_Giant);

	[[nodiscard]] float StaminaCost(RE::Actor* a_Giant, float a_Base);

	// Thigh damage, from the branch one impacts.
	void ThighDamage(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Damage);

	// Butt damage, from the branch two landings. Returns false when a tiny is low enough that the
	// finisher should take over instead, which only the grind asks about.
	bool ButtDamage(RE::Actor* a_Giant, float a_Damage, bool a_AllowFinisher, float a_Mult = 1.0f);

	void ButtDamageOverTime(RE::Actor* a_Giant);
	void AbsorbTinies(RE::Actor* a_Giant);

	void StartLegRumble(std::string_view a_Tag, RE::Actor* a_Giant, float a_Power, float a_HalfLife);
	void StopLegRumble(std::string_view a_Tag, RE::Actor* a_Giant);

	void DisableRune(RE::Actor* a_Giant);

	// The unbirth is reachable from the thigh loop and from the butt loop both, so its behaviour is
	// shared rather than living in whichever branch happens to be running.
	constexpr std::string_view BEH_UNBIRTH = "GTSBEH_Sandwich_UB";
	constexpr std::string_view BEH_UNBIRTH_T = "GTSBEH_T_Sandwich_UB";

	void TakeUnbirth(const ActionContext& a_Ctx);
	void OnUnbirthInserted(const ActionContext& a_Ctx);
	void OnUnbirthKill(const ActionContext& a_Ctx);
	void OnResetData(const ActionContext& a_Ctx);
}
