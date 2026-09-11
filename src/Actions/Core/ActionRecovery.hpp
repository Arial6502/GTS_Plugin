#pragma once

namespace GTS::Actions {

	// Saves store animation variables, so a save taken mid grab loads with GTS_GrabbedTiny set and an
	// empty Possession store. This clears the action variables the DLL writes after a load.
	class ActionRecovery : public EventListener, public CInitSingleton<ActionRecovery> {

		public:
		void OnSKSEPostLoadGame() override;
		void OnPluginReset() override;

		private:
		static void Sweep();

		static inline bool m_Pending = false;
	};
}
