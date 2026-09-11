#include "Utils/Input/InputKey.hpp"

namespace {

	using namespace GTS;

	constexpr std::string_view UNKNOWN_KEY = "UNKNOWN";

	// DIK codes at or above 0x80 sit behind the E0 prefix on a real keyboard: RCONTROL 0x9D,
	// RALT 0xB8, NUMPADENTER 0x9C and the arrow cluster. GetKeyNameText needs that told to it
	// separately or it names the numpad key sharing the low 7 bits.
	[[nodiscard]] constexpr bool IsExtended(std::uint32_t a_Code) {
		return a_Code >= 0x80;
	}

	// One name per code, chosen shortest first so a hand-edited file reads as "LMB" rather than
	// "LMOUSEBUTTON". NAMED_KEYS carries several aliases per code and its iteration order is not
	// stable, so the tie-break has to be total.
	[[nodiscard]] const absl::flat_hash_map<std::uint32_t, std::string>& StorageNames() {

		static const absl::flat_hash_map<std::uint32_t, std::string> Names = [] {

			absl::flat_hash_map<std::uint32_t, std::string> out;

			for (const auto& [name, code] : NAMED_KEYS) {

				if (name.starts_with("DIK_")) {
					continue;
				}

				auto [it, fresh] = out.try_emplace(code, name);

				if (!fresh && std::pair(name.size(), std::string_view(name)) < std::pair(it->second.size(), std::string_view(it->second))) {
					it->second = name;
				}
			}

			return out;
		}();

		return Names;
	}

	// GetKeyNameText and the MapVirtualKey family read the *calling thread's* keyboard layout, and
	// the thread that draws the UI is not the one Windows tracks the input locale on. Asking the
	// foreground window's thread is what makes a Cyrillic or AZERTY layout show its own labels.
	HKL g_ReportedLayout = nullptr;

	[[nodiscard]] HKL ActiveLayout() {

		if (g_ReportedLayout) {
			return g_ReportedLayout;
		}

		const HWND foreground = GetForegroundWindow();
		const DWORD thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;

		return GetKeyboardLayout(thread);
	}

	// MAPVK_VSC_TO_VK_EX wants the E0 prefix spelled out for the extended half of the table.
	[[nodiscard]] constexpr UINT ExtendedScanCode(std::uint32_t a_Code) {
		return IsExtended(a_Code) ? (0xE000u | (a_Code & 0x7F)) : (a_Code & 0x7F);
	}

	// MAPVK_VK_TO_CHAR answers with the virtual key's own Latin letter for VK_A through VK_Z whatever
	// the layout is, and only consults the layout for OEM keys. That is why the punctuation keys
	// localised correctly and every letter came out as its US name. ToUnicodeEx runs the layout's
	// real character table.
	[[nodiscard]] std::string CharacterFor(UINT a_Vk, std::uint32_t a_ScanCode, HKL a_Layout) {

		std::array<BYTE, 256> keyState{};   // nothing held, so this is the unshifted character
		std::array<wchar_t, 8> buffer{};

		const auto translate = [&] {
			return ToUnicodeEx(a_Vk, a_ScanCode & 0x7F, keyState.data(), buffer.data(), static_cast<int>(buffer.size()), 0, a_Layout);
		};

		int written = translate();

		// Negative is a dead key. The diacritic is in the buffer but the layout is left holding it,
		// so it has to be run again or the next real keystroke composes against it.
		if (written < 0) {
			translate();
			written = 1;
		}

		if (written <= 0 || buffer[0] <= L' ') {
			return {};
		}

		CharUpperBuffW(buffer.data(), static_cast<DWORD>(written));

		return Utf16ToUtf8(std::wstring_view(buffer.data(), static_cast<std::size_t>(written)));
	}

	[[nodiscard]] std::string_view MouseName(std::uint32_t a_Code) {

		switch (a_Code) {
			case 0: { return "Left Mouse"; }
			case 1: { return "Right Mouse"; }
			case 2: { return "Middle Mouse"; }
			case 3: { return "Mouse 4"; }
			case 4: { return "Mouse 5"; }
			default: { return "Mouse"; }
		}
	}
}

namespace GTS {

	void SetActiveKeyboardLayout(HKL a_Layout) {
		g_ReportedLayout = a_Layout;
		logger::debug("Keyboard layout is now {:#x}", reinterpret_cast<std::uintptr_t>(a_Layout));
	}

	std::optional<InputKey> InputKeyFromName(std::string_view a_Name) {

		std::string key = str_toupper(remove_whitespace(std::string(a_Name)));

		// Turns LEFTALT into LALT without turning LEFT into L.
		if (key != "LEFT" && key != "DIK_LEFT") {
			replace_first(key, "LEFT", "L");
		}

		if (key != "RIGHT" && key != "DIK_RIGHT") {
			replace_first(key, "RIGHT", "R");
		}

		const auto it = NAMED_KEYS.find(key);

		if (it == NAMED_KEYS.end()) {
			return std::nullopt;
		}

		// NAMED_KEYS still bakes the mouse offset into its values. Split it back out here so the
		// device is explicit everywhere past this point.
		if (it->second >= MOUSE_OFFSET) {
			return InputKey{ RE::INPUT_DEVICE::kMouse, it->second - MOUSE_OFFSET };
		}

		return InputKey{ RE::INPUT_DEVICE::kKeyboard, it->second };
	}

	std::string_view InputKeyStorageName(const InputKey& a_Key) {

		const std::uint32_t code = a_Key.Device == RE::INPUT_DEVICE::kMouse
			? a_Key.Code + MOUSE_OFFSET
			: a_Key.Code;

		const auto& names = StorageNames();
		const auto it = names.find(code);

		return it != names.end() ? std::string_view(it->second) : UNKNOWN_KEY;
	}

	std::vector<std::string> DescribeKeyboardLayouts() {

		std::vector<std::string> out;

		const auto describe = [](HKL a_Layout) {

			// The A, Q and semicolon positions on a US board. All three move or change character on
			// the layouts this is here to diagnose.
			constexpr std::array<std::uint32_t, 3> samples{ 0x1E, 0x10, 0x27 };

			std::string chars;

			for (const std::uint32_t code : samples) {

				const UINT vk = MapVirtualKeyExW(code, MAPVK_VSC_TO_VK_EX, a_Layout);
				const std::string ch = CharacterFor(vk, code, a_Layout);

				if (!chars.empty()) {
					chars += ' ';
				}

				chars += std::format("{:#04x}->vk{:#04x}->{}", code, vk, ch.empty() ? std::string("-") : ch);
			}

			return chars;
		};

		out.emplace_back(std::format("reported by WM_INPUTLANGCHANGE: {}",
			g_ReportedLayout ? std::format("{:#x}", reinterpret_cast<std::uintptr_t>(g_ReportedLayout)) : "never seen"));

		{
			const HWND foreground = GetForegroundWindow();
			const DWORD thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
			out.emplace_back(std::format("foreground thread {}: {:#x}", thread, reinterpret_cast<std::uintptr_t>(GetKeyboardLayout(thread))));
		}

		out.emplace_back(std::format("calling thread: {:#x}", reinterpret_cast<std::uintptr_t>(GetKeyboardLayout(0))));

		{
			std::array<HKL, 16> loaded{};
			const int count = GetKeyboardLayoutList(static_cast<int>(loaded.size()), loaded.data());

			for (int i = 0; i < count; ++i) {
				out.emplace_back(std::format("loaded {:#x}: {}", reinterpret_cast<std::uintptr_t>(loaded[i]), describe(loaded[i])));
			}
		}

		out.emplace_back(std::format("in use {:#x}: {}", reinterpret_cast<std::uintptr_t>(ActiveLayout()), describe(ActiveLayout())));

		return out;
	}

	std::string InputKeyDisplayName(const InputKey& a_Key, bool a_Localized) {

		if (a_Key.Device == RE::INPUT_DEVICE::kMouse) {
			return std::string(MouseName(a_Key.Code));
		}

		if (a_Key.Device != RE::INPUT_DEVICE::kKeyboard || !a_Localized) {
			return std::string(InputKeyStorageName(a_Key));
		}

		// Only the settings page calls this, so the cache needs no lock. It is keyed on the layout
		// so switching language refreshes every label.
		static HKL cachedLayout = nullptr;
		static absl::flat_hash_map<InputKey, std::string> cache;

		const HKL layout = ActiveLayout();

		if (layout != cachedLayout) {
			cachedLayout = layout;
			cache.clear();
		}

		if (const auto it = cache.find(a_Key); it != cache.end()) {
			return it->second;
		}

		std::string name;

		// A key that prints something is named by what it prints in this layout.
		if (layout) {

			const UINT vk = MapVirtualKeyExW(ExtendedScanCode(a_Key.Code), MAPVK_VSC_TO_VK_EX, layout);

			if (vk != 0) {
				name = CharacterFor(vk, a_Key.Code, layout);
			}
		}

		// Shift, Enter and the function row print nothing, so they keep a spelled out name.
		if (name.empty()) {

			const LONG lparam = static_cast<LONG>(((a_Key.Code & 0x7F) << 16) | (IsExtended(a_Key.Code) ? (1 << 24) : 0));

			std::array<wchar_t, 64> buffer{};
			const int written = GetKeyNameTextW(lparam, buffer.data(), static_cast<int>(buffer.size()));

			if (written > 0) {
				name = Utf16ToUtf8(std::wstring_view(buffer.data(), static_cast<std::size_t>(written)));
			}
		}

		if (name.empty()) {
			name = std::string(InputKeyStorageName(a_Key));
		}

		logger::debug("Key {:#04x} named '{}' under layout {:#x}", a_Key.Code, name, reinterpret_cast<std::uintptr_t>(layout));

		return cache.emplace(a_Key, std::move(name)).first->second;
	}
}
