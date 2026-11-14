#pragma once

#include "UI/Controls/Button.hpp"

namespace ImGuiEx {

	// Right aligned button that restores a settings struct to its defaults. Every settings struct
	// carries its own default member initialisers, so a fresh instance is the default.
	// The label and tooltip are passed in rather than baked in, so they can be translated later.
	template <typename T>
	bool ResetSettings(const char* a_label, const char* a_tooltip, T& a_settings, bool a_disabled = false) {

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(a_label).x);

		if (Button(a_label, a_tooltip, a_disabled)) {
			a_settings = T{};
			return true;
		}

		return false;
	}
}
