#pragma once

namespace GTS {

	// The actors whose movement goal belongs to this plugin. The goal lives in one slot on the
	// movement arbiter with no priority, so an actor is held by refusing every other writer rather
	// than by writing more often than they do. Hooks/Movement/MovementGoal.cpp does the refusing.
	class MovementGate {

		public:
		static void Take(RE::FormID a_Actor);
		static void Give(RE::FormID a_Actor);
		static void Clear();

		// Read from the movement job thread.
		[[nodiscard]] static bool Holds(RE::FormID a_Actor);

		// Held across our own calls into the movement system so those goals are let through.
		class Issuing {

			public:
			Issuing();
			~Issuing();

			Issuing(const Issuing&) = delete;
			Issuing& operator=(const Issuing&) = delete;
		};

		[[nodiscard]] static bool IsIssuing();
	};
}
