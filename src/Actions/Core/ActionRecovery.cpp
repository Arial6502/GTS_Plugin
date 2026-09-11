#include "Actions/Core/ActionRecovery.hpp"

#include "Actions/Core/ActionLock.hpp"
#include "Actions/Core/Possession.hpp"

namespace {

	using namespace GTS;

	// DLL-written action variables only. Movement state such as crawl and sneak is left alone.
	void ClearActionVars(RE::Actor* a_Actor) {

		AnimationVars::Grab::SetHasGrabbedTiny(a_Actor, false);
		AnimationVars::Grab::SetGrabState(a_Actor, false);
		AnimationVars::Action::SetIsStoringTiny(a_Actor, false);
		AnimationVars::Action::SetIsCleavageZOverrideEnabled(a_Actor, false);
		AnimationVars::Action::SetIsFootGrinding(a_Actor, false);
		AnimationVars::Hug::SetIsHuggingTeammate(a_Actor, false);
		AnimationVars::Tiny::SetIsBeingSneakHugged(a_Actor, false);
		AnimationVars::Tiny::SetIsBeingCrawlHugged(a_Actor, false);
	}
}

namespace GTS::Actions {

	void ActionRecovery::Sweep() {

		std::lock_guard lock(ActionLock);

		std::size_t cleared = 0;

		for (auto* actor : find_actors()) {

			if (!actor || !actor->Is3DLoaded()) {
				continue;
			}

			ClearActionVars(actor);
			++cleared;
		}

		logger::info("ActionRecovery: cleared action variables on {} actor(s) after load", cleared);
	}

	void ActionRecovery::OnSKSEPostLoadGame() {

		std::lock_guard lock(ActionLock);

		if (m_Pending) {
			return;
		}

		m_Pending = true;

		// Waits for the fader. Actors load their 3d behind it, and a graph variable written before that is lost.
		TaskManager::Run("ActionRecovery", [](const TaskUpdate&) {

			auto* ui = RE::UI::GetSingleton();

			if (ui && ui->IsMenuOpen(RE::FaderMenu::MENU_NAME)) {
				return true;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();

			if (!player || !player->Is3DLoaded()) {
				return true;
			}

			Sweep();
			m_Pending = false;
			return false;
		});
	}

	void ActionRecovery::OnPluginReset() {

		std::lock_guard lock(ActionLock);

		TaskManager::Cancel("ActionRecovery");
		m_Pending = false;
	}
}
