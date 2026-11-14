#pragma once

namespace GTS {

	class GrabAnimationController : public CInitSingleton <GrabAnimationController> {
		public:
		std::vector<Actor*> GetGrabTargetsInFront(Actor* pred, std::size_t numberOfPrey);
		static bool StartGrab(Actor* pred, Actor* prey);
		bool CanGrab(Actor* pred, Actor* prey) const;
		void AllowMessage(bool allow);

		// CanGrab split in two, without its messages: how far the grab reaches, and whether prey can be
		// grabbed at all.
		[[nodiscard]] static float Reach(Actor* pred);
		[[nodiscard]] static bool Qualifies(Actor* pred, Actor* prey);

		private:
		bool allow_message = false;
	};
}