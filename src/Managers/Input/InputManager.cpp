#include "Managers/Input/InputManager.hpp"

#include "Config/Keybinds.hpp"
#include "Config/Settings/SettingsKeybinds.hpp"
#include "Managers/Console/ConsoleManager.hpp"

#include "Utils/Actions/ActionUtils.hpp"

#include "Actions/Core/ActionRegistry.hpp"

namespace GTS {

	std::vector<ManagedInputEvent> InputManager::LoadInputEvents() {

		std::vector<ManagedInputEvent> results;

		for (const auto& bind : Keybinds::InputEvents) {

			ManagedInputEvent newData(bind);

			if (newData.HasKeys()) {
				results.push_back(newData);
			}

		}

		// Sort longest duration first
		std::ranges::sort(results,[](ManagedInputEvent const& a, ManagedInputEvent const& b) {
			return a.MinDuration() > b.MinDuration();
		});

		return results;
	}

	void InputManager::RegisterInputEvent(std::string_view a_namesv, std::function<void(const ManagedInputEvent&)> a_funcCallback, std::function<bool(void)> a_condCallbakc, const InputScope& a_scope) {

		auto& bindings = GetSingleton().m_inputEvents[std::string(a_namesv)];

		// Appended rather than replaced. A name may carry one binding per scope, which is what lets
		// the same key mean different things inside different animation states.
		bindings.emplace_back(std::move(a_funcCallback), std::move(a_condCallbakc), a_scope);

		logger::debug("Registered input event: {} (rank {})", a_namesv, std::to_underlying(a_scope.Rank));
	}

	void InputManager::PushContext(const InputScope& a_scope) {
		GetSingleton().m_overrides.push_back(a_scope);
	}

	void InputManager::PopContext() {

		auto& overrides = GetSingleton().m_overrides;

		if (!overrides.empty()) {
			overrides.pop_back();
		}
	}

	const ManagedInputEvent* InputManager::Trigger(std::string_view a_Name) {

		const auto& triggers = GetSingleton().m_eventTriggers;

		const auto it = std::ranges::find_if(triggers, [&](const ManagedInputEvent& a_Trigger) {
			return a_Trigger.GetName() == a_Name;
		});

		return it != triggers.end() ? &*it : nullptr;
	}

	bool InputManager::BindHeld(std::string_view a_Name) {

		const ManagedInputEvent* trigger = Trigger(a_Name);

		if (!trigger || trigger->IsDisabled()) {
			return false;
		}

		return trigger->AllKeysPressed(GetSingleton().m_heldKeys);
	}

	bool InputManager::ModifierHeldFor(std::string_view a_Modifier, std::string_view a_Bind) {

		if (!BindHeld(a_Modifier)) {
			return false;
		}

		const ManagedInputEvent* modifier = Trigger(a_Modifier);
		const ManagedInputEvent* bind = Trigger(a_Bind);

		if (!modifier || !bind) {
			return true;
		}

		const auto& keys = bind->GetKeys();

		return !std::ranges::all_of(modifier->GetKeys(), [&](const InputKey& a_Key) {
			return keys.contains(a_Key);
		});
	}

	std::span<const RegisteredInputEvent> InputManager::BindingsFor(std::string_view a_name) {

		const auto& events = GetSingleton().m_inputEvents;
		const auto it = events.find(a_name);

		return it != events.end() ? std::span<const RegisteredInputEvent>(it->second) : std::span<const RegisteredInputEvent>{};
	}

	std::vector<std::string> InputManager::DescribeBindings(RE::Actor* a_actor) {

		const auto& self = GetSingleton();

		constexpr std::array<std::string_view, 4> rankNames{ "root", "stance", "node", "override" };

		std::vector<std::string> names;
		names.reserve(self.m_inputEvents.size());

		for (const auto& [name, bindings] : self.m_inputEvents) {
			names.emplace_back(name);
		}

		std::ranges::sort(names);

		std::size_t unscoped = 0;

		for (const auto& [name, bindings] : self.m_inputEvents) {
			unscoped += std::ranges::count_if(bindings, [](const RegisteredInputEvent& a_b) { return !a_b.scope.InScope; });
		}

		std::vector<std::string> out;
		out.emplace_back(std::format("{} names, {} unscoped, {} overrides pushed", names.size(), unscoped, self.m_overrides.size()));

		for (const auto& name : names) {

			const auto& bindings = self.m_inputEvents.find(name)->second;
			const RegisteredInputEvent* live = self.Resolve(name, a_actor);

			std::string ranks;

			for (const auto& binding : bindings) {

				if (!ranks.empty()) {
					ranks += ',';
				}

				// A binding with no predicate is live everywhere, which is not the same as being
				// scoped to root even though both sit at that rank.
				ranks += binding.scope.InScope ? rankNames[std::to_underlying(binding.scope.Rank)] : std::string_view("any");

				if (&binding == live) {
					ranks += '*';
				}
			}

			out.emplace_back(std::format("{:<28} {}{}", name, ranks, live ? "" : "  (none live)"));
		}

		return out;
	}

	const RegisteredInputEvent* InputManager::Resolve(std::string_view a_name, RE::Actor* a_actor) const {

		const auto it = m_inputEvents.find(a_name);

		if (it == m_inputEvents.end()) {
			return nullptr;
		}

		// While a UI holds the input nothing underneath it answers, and while none does the bindings
		// belonging to one are inert.
		const bool overridden = !m_overrides.empty();

		const RegisteredInputEvent* best = nullptr;

		for (const auto& binding : it->second) {

			if (overridden != (binding.scope.Rank == ScopeRank::kOverride)) {
				continue;
			}

			if (!binding.scope.Live(a_actor)) {
				continue;
			}

			if (!best || binding.scope.Rank > best->scope.Rank) {
				best = &binding;
			}
		}

		return best;
	}

	void InputManager::Init() {

		m_ready.store(false);

		try {
			m_eventTriggers = LoadInputEvents();
		} 
		catch (std::exception e) {
			logger::error("Error Creating ManagedInputEvents: {}", e.what());
			return;
		} 

		logger::info("Loaded {} key bindings", m_eventTriggers.size());
		
		m_ready.store(true);
	}

	void InputManager::ProcessAndFilterEvents(InputEvent** a_event) {

		if (!a_event) {
			return;
		}

		// Cleared before the guards below, so a frame this function skips reports nothing held
		// rather than whatever was down when a menu opened.
		m_heldKeys.clear();

		InputKeySet KeysToBlock = {};
		RE::InputEvent* event = *a_event;
		RE::InputEvent* prev = nullptr;

		if (auto Player = PlayerCharacter::GetSingleton()) { // Disallow to hug and etc in ragdoll state
			if (IsinRagdollState(Player)) {
				return;
			}
		}
		if (State::IsInBlockingMenu() || !State::Live()) {
			return;
		}

		if (!m_ready.load()) {
			return;
		}

		GTS_PROFILE_SCOPE("InputManager: ProcessEvents");

		//Get Current InputKeys
		for (auto eventIt = *a_event; eventIt; eventIt = eventIt->next) {
			//If the event is not a button, ignore.
			if (eventIt->GetEventType() != INPUT_EVENT_TYPE::kButton) {
				continue;
			}

			//If the event is not a ButtonEvent or it is one but the event is "empty", ignore.
			ButtonEvent* buttonEvent = eventIt->AsButtonEvent();
			if (!buttonEvent || (!buttonEvent->IsPressed() && !buttonEvent->IsUp())) {
				continue;
			}

			//If it is a ButtonEvent add it to to the list of pressed keys
			const INPUT_DEVICE device = buttonEvent->device.get();

			if (device == INPUT_DEVICE::kKeyboard || device == INPUT_DEVICE::kMouse) {
				m_heldKeys.emplace(InputKey{ device, buttonEvent->GetIDCode() });
			}
		}

		// The context an actor is in changes at most once a frame, so it is resolved here rather than
		// per trigger.
		RE::Actor* const actor = GetPlayerOrControlled();

		// Two release triggers sharing a key set would otherwise both fire. Declared out here because
		// living inside the loop left it empty on every iteration, which is why that never worked.
		absl::InlinedVector<const ManagedInputEvent*, 4> firedTriggers;

		for (auto& trigger : this->m_eventTriggers) {

			if (trigger.IsDisabled()) {
				continue;
			}

			const RegisteredInputEvent* binding = Resolve(trigger.GetName(), actor);

			// Nothing is listening for this name in the current context. The key is not ours, so it
			// is neither blocked nor allowed to accumulate hold time.
			if (!binding) {
				trigger.Reset();
				continue;
			}

			const bool allDown = trigger.AllKeysPressed(m_heldKeys);
			const BindBlockType blockInput = trigger.ShouldBlock();

			if (allDown) {
				trigger.SetAllowed(!binding->condition || binding->condition());
			}

			const bool allowed = trigger.Allowed();

			if (allDown && (blockInput == BindBlockType::Always || (allowed && blockInput != BindBlockType::Never))) {
				const auto& keys = trigger.GetKeys();
				KeysToBlock.insert(keys.begin(), keys.end());
			}

			// ShouldFire drives a hold timer and a latch, so it has to run every frame or the state
			// drifts. The guard gates the callback instead of the call.
			const bool fires = trigger.ShouldFire(m_heldKeys);

			if (!fires || !allowed) {
				continue;
			}

			const bool groupAlreadyFired = std::ranges::any_of(firedTriggers, [&trigger](const ManagedInputEvent* a_fired) {
				return trigger.SameGroup(*a_fired);
			});

			if (groupAlreadyFired) {
				trigger.Reset();
				continue;
			}

			logger::debug("Running event {}", trigger.GetName());

			firedTriggers.push_back(&trigger);
			binding->callback(trigger);
		}

		while (event != nullptr) {
			bool shouldDispatch = true;
			if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
				const auto button = skyrim_cast<RE::ButtonEvent*>(event);
				if (button) {
					if (KeysToBlock.contains(InputKey{ button->device.get(), button->GetIDCode() })) {
						shouldDispatch = false;
					}
				}
			}

			RE::InputEvent* nextEvent = event->next;
			if (!shouldDispatch) {
				if (prev != nullptr) {
					prev->next = nextEvent;
				}
				else {
					*a_event = nextEvent;
				}
			}
			else {
				prev = event;
			}
			event = nextEvent;
		}
	}

	void InputManager::OnSKSEDataLoaded() {

		Init();

		ConsoleManager::RegisterCommand("available", [] {

			auto* actor = GetPlayerOrControlled();

			for (const auto& entry : Actions::ActionRegistry::Available(actor)) {

				const std::string shortcut = Keybinds::ShortcutFor(entry.Input);

				Cprint("{:<26} {:<20} {}", entry.UIName, shortcut, entry.Ready ? "" : "(not ready)");
				logger::info("available: {} -> {} [{}]{}", entry.UIName, entry.Action, shortcut, entry.Ready ? "" : " not ready");
			}
		}, "Show every action reachable in the current context, as a wheel would see it");

		ConsoleManager::RegisterCommand("input", [] {
			for (const auto& line : DescribeBindings(GetPlayerOrControlled())) {
				Cprint("{}", line);
				logger::info("input: {}", line);
			}
		}, "Show every input binding, its scope, and which one is live right now");

		ConsoleManager::RegisterCommand("layout", [] {
			for (const auto& line : DescribeKeyboardLayouts()) {
				Cprint("{}", line);
				logger::info("layout: {}", line);
			}
		}, "Show every keyboard layout Windows reports and what it calls a few keys");
	}
}
