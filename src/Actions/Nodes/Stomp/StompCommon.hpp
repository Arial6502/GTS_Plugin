#pragma once

namespace GTS::Actions::Stomping {

	inline constexpr std::string_view NODE_FOOT_R = "NPC R Foot [Rft ]";
	inline constexpr std::string_view NODE_FOOT_L = "NPC L Foot [Lft ]";

	// One frame late, so the grind check that runs on the same annotation wins over the launch.
	void DelayedLaunch(RE::Actor* a_Giant, std::string_view a_Task, float a_Radius, float a_Power, FootEvent a_Event);

	// Which foot the aim picked, and whether the understomp variant should play. Both entries and the
	// AI go through this, so the aim runs in exactly one place.
	struct Aim {
		bool Left = false;
		bool Under = false;
	};

	[[nodiscard]] Aim Resolve(RE::Actor* a_Giant, StompAimType a_Type, bool a_Strong, bool a_Trample);
}
