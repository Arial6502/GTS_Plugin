#pragma once

namespace GTS::RefPin {

	// Keeps a reference alive while the mod is holding it. The game deletes a temporary reference when
	// its cell detaches and hands the FormID to something else, which is what makes a carried actor
	// vanish across a loading screen. Promoting marks the reference persistent for as long as the
	// GTS quest is listed as one of its owners, the same way a filled quest alias does.
	//
	// Every Pin needs its Unpin. A reference left promoted is written to the save and stays there.
	bool Pin(RE::TESObjectREFR* a_Ref);
	void Unpin(RE::TESObjectREFR* a_Ref);

	// Whether this mod is one of the reference's promotion owners. Reads the reference's own extra
	// data, so it answers correctly for a save made before this session.
	[[nodiscard]] bool Pinned(const RE::TESObjectREFR* a_Ref);
}
