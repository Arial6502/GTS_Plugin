#pragma once

namespace GTS::Actions::Calamity {

	// Who the giant is about to shrink or erase. Not a Possession: these are targets at arm's length,
	// not someone the giant is holding, so nothing else may treat the slot as occupied. Handles rather
	// than pointers, because the list outlives a frame.
	void AddTarget(RE::FormID a_Giant, RE::Actor* a_Tiny, float a_Until);
	void ClearTargets(RE::FormID a_Giant);

	[[nodiscard]] std::vector<RE::Actor*> Targets(RE::FormID a_Giant);

	// Read instead of the tiny's animation variables, which creatures do not have.
	[[nodiscard]] bool IsTarget(RE::FormID a_Tiny);

	// The scale the tiny is being taken down to. Set per target when it is added.
	[[nodiscard]] float ShrinkUntil(RE::Actor* a_Tiny);

	// The two ways in. Both bind the target and then ask the node, so a caller needs one line.
	bool Shrink(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Until);
	bool Erase(RE::Actor* a_Giant, RE::Actor* a_Tiny);
}
