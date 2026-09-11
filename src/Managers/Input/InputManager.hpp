#pragma once

#include "Managers/Input/ManagedInputEvent.hpp"
#include "Managers/Input/InputScope.hpp"

namespace GTS {

	using EventResult = RE::BSEventNotifyControl;

	struct RegisteredInputEvent {

		std::function<void(const ManagedInputEvent&)> callback = nullptr;
		std::function<bool(void)> condition = nullptr;
		InputScope scope = {};

		RegisteredInputEvent(std::function<void(const ManagedInputEvent&)> a_funcCallback, std::function<bool(void)> a_condCallback, const InputScope& a_scope) :
			callback(std::move(a_funcCallback)), condition(std::move(a_condCallback)), scope(a_scope) {
		}
	};

	class InputManager : public EventListener, public CInitSingleton<InputManager> {

		public:
		void ProcessAndFilterEvents(InputEvent** a_event);
		void OnSKSEDataLoaded() override;
		void Init();

		// One name may carry several bindings so long as their scopes do not overlap, which is how the
		// same key means different things inside different animation states.
		static void RegisterInputEvent(std::string_view a_namesv, std::function<void(const ManagedInputEvent&)> a_funcCallback, std::function<bool(void)> a_condCallbakc = nullptr, const InputScope& a_scope = {});

		// Takes input over until popped. A wheel or any other modal UI pushes one so every binding
		// below it goes inert without that UI needing an input path of its own.
		static void PushContext(const InputScope& a_scope);
		static void PopContext();

		// Every binding registered under a name, in registration order.
		[[nodiscard]] static std::span<const RegisteredInputEvent> BindingsFor(std::string_view a_name);

		// Every registered name with its bindings' ranks, and which one would answer for the actor
		// right now. This is the only way to see a scope decision from outside.
		[[nodiscard]] static std::vector<std::string> DescribeBindings(RE::Actor* a_actor);

		// Whether every key of a bind is down right now, ignoring its trigger type and whether
		// anything is registered under its name. This is what a modifier key needs: it has no
		// callback of its own, it only changes what another bind does.
		// Empty on any frame the manager skipped, so a menu opening cannot leave one stuck down.
		[[nodiscard]] static bool BindHeld(std::string_view a_Name);

		// Whether a_Modifier is held as an addition to a_Bind. A bind whose own keys already cover
		// every modifier key is not being modified by it - ALT is part of an ALT+E bind, not a
		// modifier on it - so that bind keeps its plain meaning and reports false here.
		[[nodiscard]] static bool ModifierHeldFor(std::string_view a_Modifier, std::string_view a_Bind);

		private:
		static std::vector<ManagedInputEvent> LoadInputEvents();

		// The key state machine registered under this name, or null.
		[[nodiscard]] static const ManagedInputEvent* Trigger(std::string_view a_Name);

		// The binding that should answer for this name right now, or null when none is live.
		[[nodiscard]] const RegisteredInputEvent* Resolve(std::string_view a_name, RE::Actor* a_actor) const;

		std::atomic_bool m_ready = false;
		absl::flat_hash_map<std::string, absl::InlinedVector<RegisteredInputEvent, 2>> m_inputEvents;
		std::vector<ManagedInputEvent> m_eventTriggers;
		std::vector<InputScope> m_overrides;
		InputKeySet m_heldKeys;
	};
}
