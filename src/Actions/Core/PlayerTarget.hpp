#pragma once

namespace GTS::Actions::PlayerTarget {

	[[nodiscard]] bool Engaged(std::string_view a_Bind);
	[[nodiscard]] bool CanPerform(std::string_view a_Action);
	bool Perform(std::string_view a_Action);
	bool StartAimedOn(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Action);
}
