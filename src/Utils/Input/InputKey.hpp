#pragma once

namespace GTS {

	// A physical key position, not a character. Keyboard codes are DirectInput scancodes, which is
	// what ButtonEvent::GetIDCode already carries, so a binding means the same key whatever layout
	// Windows is set to.
	//
	// The device is part of the identity. Without it a mouse button and a gamepad button share a
	// number: gamepad kLeftShoulder is 0x100, which is what the old mouse offset produced for LMB.
	struct InputKey {

		RE::INPUT_DEVICE Device = RE::INPUT_DEVICE::kKeyboard;
		std::uint32_t Code = 0;

		[[nodiscard]] friend bool operator==(const InputKey&, const InputKey&) = default;

		template <typename H>
		friend H AbslHashValue(H a_State, const InputKey& a_Key) {
			return H::combine(std::move(a_State), std::to_underlying(a_Key.Device), a_Key.Code);
		}
	};

	using InputKeySet = absl::flat_hash_set<InputKey>;

	// Reads a name from the keybind file. Accepts the DIK_ prefixed and bare forms, and the mouse
	// names, which the file still carries with the 0x100 offset baked into NAMED_KEYS.
	[[nodiscard]] std::optional<InputKey> InputKeyFromName(std::string_view a_Name);

	// The name the key is stored under. Names a position, so a keybind file is portable between
	// users on different layouts.
	[[nodiscard]] std::string_view InputKeyStorageName(const InputKey& a_Key);

	// Windows only tells a window its input language changed, and only the thread owning that window
	// tracks it. The WndProc hook hands the layout here so the UI does not have to guess which thread
	// to ask. Null until the first change message arrives, which is why there is still a fallback.
	void SetActiveKeyboardLayout(HKL a_Layout);

	// What the settings page shows. Localized asks the active keyboard layout, so a Cyrillic, AZERTY
	// or JIS user sees their own label; otherwise the key keeps the US name it is stored under, which
	// is what guides and screenshots use.
	[[nodiscard]] std::string InputKeyDisplayName(const InputKey& a_Key, bool a_Localized);

	// Every layout Windows will admit to, and what a few sample keys are called under each. Backs
	// the "gts layout" command, which exists because there is no other way to see which of the
	// several answers Windows gives is the one the user actually switched to.
	[[nodiscard]] std::vector<std::string> DescribeKeyboardLayouts();
}
