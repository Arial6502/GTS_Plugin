#pragma once

namespace GTS::AI {

	// Who may act, and who may be acted on. Lifted out of AIManager so the Director and the adapters
	// share one answer.
	class Eligibility {

		public:
		[[nodiscard]] static bool Performer(RE::Actor* a_Actor);
		[[nodiscard]] static bool Prey(RE::Actor* a_Performer, RE::Actor* a_Prey);

		// Everything the performer could act on this think, combat target first.
		[[nodiscard]] static std::vector<RE::Actor*> Candidates(RE::Actor* a_Performer);
	};
}
