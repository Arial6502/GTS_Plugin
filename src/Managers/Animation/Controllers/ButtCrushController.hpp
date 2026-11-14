#pragma once

namespace GTS {

	class ButtCrushController : public EventListener, public CInitSingleton <ButtCrushController> {

		public:
		static void ButtCrush_OnCooldownMessage(Actor* giant);
		std::vector<Actor*> GetButtCrushTargets(Actor* pred, std::size_t numberOfPrey);
		bool CanButtCrush(Actor* pred, Actor* prey) const;

		// CanButtCrush split in two, without its messages.
		[[nodiscard]] static float Reach(Actor* pred);
		[[nodiscard]] static bool Qualifies(Actor* pred, Actor* prey);
		// a_Suffix picks between the targeted entry and the fast one. The stance still decides whether
		// that resolves to a butt crush or a breast crush.
		static bool StartButtCrush(Actor* pred, Actor* prey, bool dochecks = true, std::string_view a_Suffix = "Enter");
		void AllowMessage(bool allow);

		private:
		bool allow_message = false;
	};
}