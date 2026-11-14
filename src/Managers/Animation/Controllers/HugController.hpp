#pragma once

namespace GTS {

	class HugAnimationController : public CInitSingleton <HugAnimationController> {
		public:
		static void Hugs_OnCooldownMessage(Actor* giant);
		std::vector<Actor*> GetHugTargetsInFront(Actor* pred, std::size_t numberOfPrey);
		bool CanHug(Actor* pred, Actor* prey);
		static bool StartHug(Actor* pred, Actor* prey);
		void AllowMessage(bool allow);

		// CanHug split in two, without its messages: how far the hug reaches, and whether prey can be
		// hugged at all.
		[[nodiscard]] static float Reach(Actor* pred);
		[[nodiscard]] static bool Qualifies(Actor* pred, Actor* prey);

		private:
		bool allow_message = false;	
	};
}