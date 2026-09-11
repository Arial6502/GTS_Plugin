#pragma once

#include "Actions/Core/ActionTypes.hpp"

namespace GTS::Actions::Cleanup {

	void Scare(RE::Actor* a_Tiny);
	void ScareSlot(RE::FormID a_Owner, PossessionSlot a_Slot);
	void ReleaseTiny(RE::Actor* a_Tiny, PossessionSlot a_Slot);
	void ReleaseSlot(RE::FormID a_Owner, PossessionSlot a_Slot);
	void ReleaseAll(RE::FormID a_Owner);
	void ReleaseOwned(RE::FormID a_Owner, std::span<const PossessionSlot> a_Slots);

	// Keeps a_Keep, which is how a node that stepped aside keeps its actors.
	void ReleaseOwnedExcept(RE::FormID a_Owner, std::span<const PossessionSlot> a_Slots, std::span<const PossessionSlot> a_Keep);
	void ResetCamera(RE::Actor* a_Giant);
}
