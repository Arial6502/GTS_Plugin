#include "Actions/Nodes/Stomp/StompCommon.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"

#include "Utils/Actions/AutoAim/AimAssist.hpp"
#include "Utils/Actions/AutoAim/AutoAimUtils.hpp"

namespace GTS::Actions::Stomping {

	void DelayedLaunch(RE::Actor* a_Giant, std::string_view a_Task, float a_Radius, float a_Power, FootEvent a_Event) {

		const double start = Time::WorldTimeElapsed();
		ActorHandle handle = a_Giant->CreateRefHandle();

		TaskManager::Run(std::format("{}_{}", a_Task, a_Giant->formID), [=](auto&) {

			auto handleRef = handle.get();

			if (!handleRef) {
				return false;
			}

			if (Time::WorldTimeElapsed() - start > 0.03) {
				LaunchTask(handleRef.get(), a_Radius, a_Power, a_Event);
				return false;
			}

			return true;
		});
	}

	Aim Resolve(RE::Actor* a_Giant, StompAimType a_Type, bool a_Strong, bool a_Trample) {

		Aim aim{};
		aim.Left = AutoAim_Miss_GetNextStompSide(a_Giant, a_Type);
		aim.Under = AutoAim_And_DetermineStompType(a_Giant, aim.Left, a_Strong, a_Trample);

		return aim;
	}
}
