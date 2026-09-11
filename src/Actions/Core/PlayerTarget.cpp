#include "Actions/Core/PlayerTarget.hpp"

#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/IActionNode.hpp"
#include "Config/Keybinds.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Utils/Actions/AutoAim/AutoAimUtils_Calculation.hpp"

namespace {

	constexpr std::string_view ModifierBind = GTS::Keybinds::TargetSwitchBind;

	std::vector<RE::Actor*> Candidates(RE::Actor* a_Player) {

		std::vector<RE::Actor*> found;

		for (auto* teammate : GTS::FindTeammates()) {

			if (!teammate || teammate == a_Player || teammate->IsDead()) {
				continue;
			}

			if (GTS::Actions::ActionRegistry::Busy(teammate)) {
				continue;
			}

			found.push_back(teammate);
		}

		const RE::NiPoint3 origin = a_Player->GetPosition();

		std::ranges::sort(found, {}, [&](RE::Actor* a_Actor) {
			return (a_Actor->GetPosition() - origin).SqrLength();
		});

		return found;
	}
}

namespace GTS::Actions::PlayerTarget {

	bool Engaged(std::string_view a_Bind) {
		return InputManager::ModifierHeldFor(ModifierBind, a_Bind);
	}

	bool CanPerform(std::string_view a_Action) {

		RE::Actor* player = RE::PlayerCharacter::GetSingleton();

		if (!player || !ActionRegistry::EntryOwner(a_Action)) {
			return false;
		}

		return !Candidates(player).empty();
	}

	bool StartAimedOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action) {
		const AutoAim::PreferTarget prefer(a_Target);
		return ActionRegistry::Perform(a_Actor, a_Action) == RequestResult::kAccepted;
	}

	bool Perform(std::string_view a_Action) {

		RE::Actor* player = RE::PlayerCharacter::GetSingleton();

		if (!player) {
			return false;
		}

		IActionNode* node = ActionRegistry::EntryOwner(a_Action);

		if (!node) {
			return false;
		}

		const auto followers = Candidates(player);

		if (followers.empty()) {
			Notify("No follower is free to do that.");
			return false;
		}

		bool explain = true;

		for (auto* follower : followers) {

			if (node->StartOn(follower, player, a_Action, explain)) {
				ControlAnother(follower, false);
				return true;
			}

			explain = false;
		}

		return false;
	}
}
