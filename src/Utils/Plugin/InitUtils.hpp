#pragma once

namespace GTS {

	class InitUtils : public CInitSingleton<InitUtils>, public EventListener {

		void OnSKSEDataLoaded() override;
		void OnSKSEPostLoad() override;

		static void VersionCheck();
		static void CPrintPluginInfo();
		static void LogPrintPluginInfo();
	};
}