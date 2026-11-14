#pragma once

namespace GTS {

	class InitUtils : public CInitSingleton<InitUtils>, public EventListener {
		
		private:
		void OnSKSEDataLoaded() override;
		void OnSKSEPostLoad() override;

		static void CPrintPluginInfo();
		static void LogPrintPluginInfo();

		public:
		static void VersionCheck();
	};
}