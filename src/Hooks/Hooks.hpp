#pragma once

namespace Hooks {

	class HookManager : public CInitSingleton<HookManager>, public EventListener {

		public:
		static void InstallNormal();
		static void InstallLate();

		private:
		void OnGameMainMenuFullyLoaded() override;
	};
}
