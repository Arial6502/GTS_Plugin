#pragma once

namespace GTS {

	class ConfigModHandler : public EventListener, public CInitSingleton<ConfigModHandler> {
		public:
		static void DoCameraStateReset();
		static void DoHighHeelStateReset();
		static void HandleRaceMenuDataUpdate();
		void OnPluginReset() override;
		void OnSerdePostLoad() override;
		void OnModConfigReset() override;
		void OnModConfigRefresh() override;

	};

}