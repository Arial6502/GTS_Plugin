#pragma once

namespace GTS {

	class State : public CInitSingleton<State>, public EventListener {

		public:
		static bool IsInRaceMenu();
		static bool Enabled();
		static bool InGame();
		static bool Ready();
		static bool Live();
		static bool IsInBlockingMenu();
		static void SetInGame(bool value);
		static bool OnMainThread();
		static void SetOnMainThread(bool value);

		private:
		void OnSKSEPostLoadGame() override;
		void OnSKSENewGame() override;
		void OnSKSEPreLoadGame() override;
		void OnGameMenuChange(const MenuOpenCloseEvent* menu_event) override;

		static inline std::atomic_bool m_enabled = std::atomic_bool(true);
		static inline std::atomic_bool m_inGame = std::atomic_bool(false);
		static inline std::atomic_bool m_onMainThread = std::atomic_bool(false);
	};
}
