#pragma once

#include "Config/Settings/SettingsKeybinds.hpp"
#include "Utils/Input/InputKey.hpp"

namespace GTS {

	enum class LInputEventState_t : std::uint8_t {
		Idle,
		Held,
	};

	class ManagedInputEvent {
		public:

		explicit ManagedInputEvent(const InputBind& a_event);

		// Return time since it was first pressed
		[[nodiscard]] float Duration() const;

		// Will take a key list and process if the event should fire.
		// will return true if the events conditions are met
		[[nodiscard]] bool ShouldFire(const InputKeySet& keys);

		// Returns true if all keys are pressed this frame
		// Not taking into account things like duration
		[[nodiscard]] bool AllKeysPressed(const InputKeySet& keys) const;

		// Returns true if ONLY the specicified keys are pressed this frame
		// Not taking into account things like duration
		[[nodiscard]] bool OnlyKeysPressed(const InputKeySet& keys) const;

		// Resets the timer and all appropiate state variables
		void Reset();

		// Returns the duration required for the event to fire
		[[nodiscard]] float MinDuration() const;

		// Returns if the event is a onup event
		[[nodiscard]] bool IsOnUp() const;

		[[nodiscard]] std::string_view GetName() const;

		// Check if this is an On key up event
		//bool IsOnUp();
		[[nodiscard]] bool HasKeys() const;

		// Checks if this key is the same as another in terms
		// of mutaally exclusive triggers
		[[nodiscard]] bool SameGroup(const ManagedInputEvent& other) const;

		[[nodiscard]] const InputKeySet& GetKeys() const;

		[[nodiscard]] BindBlockType ShouldBlock() const;

		[[nodiscard]] bool IsDisabled() const;

		// A release trigger fires on the frame its keys come up, by which time the guard can no
		// longer be asked about a key that is no longer held. The answer from while it was held is
		// kept here so the release still respects it.
		void SetAllowed(bool a_allowed);
		[[nodiscard]] bool Allowed() const;

		private:

		double startTime = 0.0;
		bool primed = false; // Used for release events. Once primed, when keys are not pressed we fire
		bool allowed = true;

		std::string name;
		InputKeySet keys = {};
		float minDuration = 0.0f;

		// If true this event won't fire unless ONLY the keys are pressed for the entire duration
		bool exclusive = false;
		bool Disabled = false;
		BindTriggerType trigger = BindTriggerType::Once;
		LInputEventState_t state = LInputEventState_t::Idle;
		BindBlockType blockinput = BindBlockType::Automatic;
	};
}