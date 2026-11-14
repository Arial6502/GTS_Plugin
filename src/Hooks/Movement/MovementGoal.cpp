#include "Hooks/Movement/MovementGoal.hpp"
#include "Hooks/Util/HookUtil.hpp"
#include "Systems/Movement/MovementGate.hpp"

using namespace GTS;

namespace {

	// MovementPathManagerArbiter holds its IMovementSetGoal at +0x18 and the controller that
	// registered it at +0x40. IMovementControllerRegisterInterface is MovementControllerAI's first
	// base, so that pointer is the controller, and its formID names the actor it drives. Reading the
	// FormID rather than the Actor* keeps this safe on the movement job thread.
	RE::FormID GoalOwner(void* a_SetGoal) {
		auto* arbiter = static_cast<std::byte*>(a_SetGoal) - 0x18;
		auto* controller = *reinterpret_cast<RE::MovementControllerAI**>(arbiter + 0x40);
		return controller ? controller->formID : 0;
	}

	bool Refuse(void* a_SetGoal) {
		return !MovementGate::IsIssuing() && MovementGate::Holds(GoalOwner(a_SetGoal));
	}
}

namespace Hooks {

	// Takes a request for the pathing manager to solve. Packages and our own calls use this one.
	struct Hook_SetGoal {

		static constexpr std::size_t funcIndex = 1;

		static bool thunk(void* a_This, void* a_Request) {
			return Refuse(a_This) ? true : func(a_This, a_Request);
		}

		FUNCTYPE_VFUNC func;
	};

	// Takes a path the caller has already built. Combat does its own pathfinding and hands the
	// finished result straight in, so it never reaches the request form above.
	struct Hook_SetGoalWithPath {

		static constexpr std::size_t funcIndex = 2;

		static void thunk(void* a_This, void* a_Path, void* a_Request) {

			if (Refuse(a_This)) {
				return;
			}

			func(a_This, a_Path, a_Request);
		}

		FUNCTYPE_VFUNC func;
	};

	void Hook_MovementGoal::Install() {
		stl::write_vfunc<Hook_SetGoal>(RE::VTABLE_MovementPathManagerArbiter[1]);
		stl::write_vfunc<Hook_SetGoalWithPath>(RE::VTABLE_MovementPathManagerArbiter[1]);
	}
}
