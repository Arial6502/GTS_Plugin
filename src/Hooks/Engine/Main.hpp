#pragma once

namespace Hooks {

	void InstallSMPBridge();

	//False when the bridge could not be installed, or when it installed on the wrong side of SMP.
	[[nodiscard]] bool SMPBridgeActive();
	//True if SMP dll is installed and we hooked after it
	[[nodiscard]] bool SMPInstalled();

	//Diagnostic string for the UI: what happened at install time.
	[[nodiscard]] std::string_view SMPBridgeStatus();

	class Hook_MainUpdate {
		public:
		static void Install();
		static void InstallLate();
	};
}