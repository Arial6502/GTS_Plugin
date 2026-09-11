#pragma once

namespace GTS {

	#define IM_VK_KEYPAD_ENTER (VK_RETURN + 256)

    class ImInput {

        private:
        class CharEvent final : public RE::InputEvent {
            public:
            uint32_t m_keyCode;
        };

        struct KeyEvent final {
            explicit KeyEvent(const RE::ButtonEvent* a_event) : m_keyCode(a_event->GetIDCode()), m_device(a_event->GetDevice()), m_eventType(a_event->GetEventType()), m_value(a_event->Value()), m_heldDownSecs(a_event->HeldDuration()) {}
            explicit KeyEvent(const CharEvent* a_event) : m_keyCode(a_event->m_keyCode), m_device(a_event->GetDevice()), m_eventType(a_event->GetEventType()) {}
            [[nodiscard]] constexpr bool IsPressed() const noexcept;
            [[nodiscard]] constexpr bool IsRepeating() const noexcept;
            [[nodiscard]] constexpr bool IsDown() const noexcept;
            [[nodiscard]] constexpr bool IsHeld() const noexcept;
            [[nodiscard]] constexpr bool IsUp() const noexcept;
            uint32_t m_keyCode;
            RE::INPUT_DEVICE m_device;
            RE::INPUT_EVENT_TYPE m_eventType;
            float m_value = 0;
            float m_heldDownSecs = 0;
        };

        // Input handling members
        std::shared_mutex m_inputMutex;
        std::vector<KeyEvent> m_keyEventQueue{};

        static inline bool m_capturing = false;
        static inline bool m_captureCancelled = false;
        static inline std::vector<InputKey> m_capturedKeys{};

        // Escape cancels a rebind on the press, and capture has ended by the time the release
        // arrives. Left to fall through, that release closes the menu a moment later.
        static inline bool m_swallowEscapeUp = false;

        // Set by the keybind row while the cursor is over the button that confirms a rebind. Only
        // then does a left click go to the UI instead of into the binding.
        static inline bool m_rebindConfirmHovered = false;

        // What is held right now, in the same scancodes a keybind is stored under. The game never
        // sees these presses while the menu is up, so the menu's own keybind is matched here.
        static inline InputKeySet m_heldKeys{};
        static inline bool m_settingsComboHeld = false;

        static void TrackHeld(const KeyEvent& a_event);
        [[nodiscard]] static bool SettingsComboPressed();

        public:
        static void SetRebindConfirmHovered(bool a_hovered) { m_rebindConfirmHovered = a_hovered; }

        private:

        public:
        static void UnstickKeys();
        void ProcessInputEventQueue();
        void ResetImGuiInputState();
        void ProcessInputEvents(RE::InputEvent* const* a_events);
        static void RemoveAllKeyEvents(RE::InputEvent** a_event);
        static ImGuiKey VirtualKeyToImGuiKey(WPARAM a_wParam);
        static std::string ImGuiKeyToDIKString(ImGuiKey a_key);
        static uint32_t DIKToVK(uint32_t a_DIK);
        static void UpdateMousePos();

        // Rebinding asks here for the keys the user pressed. The scancode only survives this far:
        // everything past this point is mapped through the active keyboard layout, which is what made
        // a bind captured on AZERTY or Dvorak point at a different physical key than the one pressed.
        static void BeginKeyCapture();
        static void EndKeyCapture();
        [[nodiscard]] static bool IsCapturingKeys();
        [[nodiscard]] static bool KeyCaptureCancelled();
        [[nodiscard]] static const std::vector<InputKey>& CapturedKeys();

    };
}