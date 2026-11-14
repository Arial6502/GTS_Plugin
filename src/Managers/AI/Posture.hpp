#pragma once

#include "Actions/Core/ActionTypes.hpp"

namespace GTS::AI {

	// Puts an actor into the stance a branch wants before it starts, and puts it back afterwards.
	// While a stance is held, TransientActorData::AIStance keeps UpdateCrawlState from resetting a
	// teammate's crawl back to the setting every frame.
	class Posture {

		public:
		static void Hold(RE::Actor* a_Actor, Actions::Stance a_Stance);

		// Re-asserted every tick while a stance is held. Actor::Update recomputes the sneak state from
		// the current package each frame, and ActorPackage::SetPackage rebuilds the flag on a package
		// change, so this is not a set once and forget it.
		static void Enforce(RE::Actor* a_Actor);
		static void Release(RE::Actor* a_Actor);
		static void Clear();

		// True once the actor is actually in the stance, not just asked to be.
		[[nodiscard]] static bool Settled(RE::Actor* a_Actor, Actions::Stance a_Stance);

		// Read by the sneak hook, which runs on the animation path.
		[[nodiscard]] static bool ForcesSneak(RE::FormID a_Actor);

		// Which stance a branch should use, given what it has animations for.
		[[nodiscard]] static Actions::Stance Roll(RE::Actor* a_Actor, std::uint8_t a_Stances);
	};
}
