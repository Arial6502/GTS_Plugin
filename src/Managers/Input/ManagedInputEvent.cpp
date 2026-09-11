#include "Managers/Input/ManagedInputEvent.hpp"

namespace GTS {

	ManagedInputEvent::ManagedInputEvent(const InputBind& a_event) {

		this->Disabled = a_event.Disabled;
		this->name = a_event.Name;
		this->exclusive = a_event.Exclusive;
		this->trigger = a_event.Trigger;
		this->blockinput = a_event.Block;
		this->minDuration = a_event.Duration;
		this->startTime = 0.0;
		this->keys = {};

		for (const auto& key : a_event.Keys) {

			const auto parsed = InputKeyFromName(key);

			if (!parsed) {
				logger::warn("Key named {} in {} was unrecognized.", key, this->name);
				this->keys.clear();
				return; // Drop every key so this becomes an invalid entry and never fires
			}

			this->keys.emplace(*parsed);
		}
	}

	bool ManagedInputEvent::IsDisabled() const {
		return this->Disabled;
	}

	float ManagedInputEvent::Duration() const {
		return static_cast<float>(Time::WorldTimeElapsed() - this->startTime);
	}

	float ManagedInputEvent::MinDuration() const {
		return this->minDuration;
	}

	void ManagedInputEvent::Reset() {
		this->startTime = Time::WorldTimeElapsed();
		this->state = LInputEventState_t::Idle;
		this->primed = false;
	}

	bool ManagedInputEvent::IsOnUp() const {
		return this->trigger == BindTriggerType::Release;
	}

	bool ManagedInputEvent::SameGroup(const ManagedInputEvent& other) const {
		if (this->IsOnUp() && other.IsOnUp()) {
			return this->keys == other.keys;
		}
		return false;
	}

	bool ManagedInputEvent::AllKeysPressed(const InputKeySet& keys) const {

		if (this->keys.empty()) {
			return false;
		}

		for (const auto& key : this->keys) {
			if (!keys.contains(key)) {
				// Key not found
				return false;
			}
		}
		return true;
	}

	bool ManagedInputEvent::OnlyKeysPressed(const InputKeySet& keys_in) const {
		for (const auto& key : keys_in) {
			if (!this->keys.contains(key)) {
				return false;
			}
		}
		return true;
	}

	bool ManagedInputEvent::ShouldFire(const InputKeySet& a_gameInputKeys) {
		bool shouldFire = false;
		// Check based on keys and duration
		if (this->AllKeysPressed(a_gameInputKeys) && (!this->exclusive || this->OnlyKeysPressed(a_gameInputKeys))) {
			shouldFire = true;
		}
		else {
			// Keys aren't held reset the start time of the button hold
			this->startTime = Time::WorldTimeElapsed();
			// and reset the state to idle
			this->state = LInputEventState_t::Idle;
		}
		// Check based on duration
		if (shouldFire) {
			if (this->minDuration > 0.0) {
				// Turn it off if duration is not met
				shouldFire = this->Duration() > this->minDuration;
			}
		}

		// Check based on held and trigger state
		if (shouldFire) {
			this->primed = true;
			switch (this->state) {
				case LInputEventState_t::Idle: {
					this->state = LInputEventState_t::Held;
					switch (this->trigger) {
						// If once or continius start firing now
	
						case BindTriggerType::Once:
						case BindTriggerType::Continuous: {
							return true;
						}
						case BindTriggerType::Release: {
							return false;
						}
						default:{
							logger::warn("Unexpected TriggerMode.");
							return false; // Catch if something goes weird
						}
					}
				}
				case LInputEventState_t::Held: {
					switch (this->trigger) {
						// If once stop firing

						case BindTriggerType::Once:
						case BindTriggerType::Release:{
							// For release still do nothing
							return false;
						}
						case BindTriggerType::Continuous:{
							// if continous keep firing
							return true;
						}

						default:{
							logger::error("Unexpected TriggerMode.");
							return false; // Catch if something goes weird
						}
					}
				}
				default:{
					logger::error("Unexpected InputEventState.");
					return false; // Catch if something goes weird
				}
			}
		}
		
		if (this->primed) {
			this->primed = false;
			switch (this->trigger) {
				case BindTriggerType::Release:{
					// For release fire now that we have stopped pressing
					return true;
				}
				default:{
					return false;
				}
			}
		}
		return false;
	}

	bool ManagedInputEvent::HasKeys() const {
		return !this->keys.empty();
	}

	std::string_view ManagedInputEvent::GetName() const {
		return this->name;
	}

	void ManagedInputEvent::SetAllowed(bool a_allowed) {
		this->allowed = a_allowed;
	}

	bool ManagedInputEvent::Allowed() const {
		return this->allowed;
	}

	const InputKeySet& ManagedInputEvent::GetKeys() const {
		return this->keys;
	}

	BindBlockType ManagedInputEvent::ShouldBlock() const {
		return this->blockinput;
	}
}