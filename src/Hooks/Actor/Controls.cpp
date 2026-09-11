#include "Managers/Size_Killmoves/KillMoveHelper.hpp"
#include "Managers/Size_Killmoves/SizeKillMove.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Hooks/Actor/Controls.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Hooks/Util/HookUtil.hpp"

using namespace GTS;

namespace {

	// Which vanilla inputs are taken over is declared on the states that take them over, so this only
	// has to ask. One input event is offered to every handler in turn and CanProcess is hooked on six
	// of them, so this runs several times for one press: the answer has to be the same each time or a
	// later handler passes the press to the game after an earlier one consumed it.
	bool AllowVanillaInput(RE::IDEvent* a_Event) {

		if (!a_Event) {
			return true;
		}

		auto* player = PlayerCharacter::GetSingleton();

		if (!player) {
			return true;
		}

		return !Actions::ActionRegistry::HandleVanillaInput(player, a_Event->userEvent.c_str());
	}

	bool CanMove() {

		auto player = PlayerCharacter::GetSingleton();
		if (!player) {
			return true;
		}

		Actor* Controlled = GetPlayerOrControlled();
		if (!Controlled->IsPlayerRef()) {
			if (AnimationVars::Action::IsThighSandwiching(Controlled)) { // Disallow player movement if we have control over other actor and actor does thigh sandwich
				return false;
			} if (IsBetweenBreasts(Controlled)) {
				return false;
			}
		}
		if (IsFreeCameraEnabled()) {
			return true;
		}
		if (!AnimationVars::Utility::BehaviorsInstalled(player)) { // Don't mess with movement if user didn't install anims correctly
			return true;
		}
		if (AnimationVars::General::IsTransitioning(player)) { // Disallow to move during transition
			return false;
		}
		if (AnimationVars::Grab::IsGrabAttacking(player)) { // Allow movement for Grab Attacking
			return true;
		}
		return !AnimationVars::General::IsBusy(player); // Else return GTS Busy
	}
}

namespace Hooks {

	struct CanProcess {

		static constexpr size_t funcIndex = 0x01;

		template<int ID>
		//void* is the class instance, can't use PlayerInputHandler as its abstract.
		static bool thunk(void* a_this, RE::InputEvent* a_event) {

			{
				GTS_PROFILE_ENTRYPOINT_UNIQUE("ActorControls::CanProcess", ID);

				if (a_event->GetEventType() != INPUT_EVENT_TYPE::kMouseMove) {

					if (!CanMove()) {
						return false;
					}

					auto EvtID = a_event->AsIDEvent();
					if (!AllowVanillaInput(EvtID)) {
						return false;
					}
				}
			}

			return func<ID>(a_this, a_event);
		}

		template<int ID>
		FUNCTYPE_VFUNC_UNIQUE func;
	};

	struct ProcessMouseMove {
		static constexpr size_t funcIndex = 0x01;

		template<int ID>
		static bool thunk(void* a_this, RE::InputEvent* a_event) {
			if (a_event->GetEventType() == INPUT_EVENT_TYPE::kMouseMove || a_event->GetEventType() == INPUT_EVENT_TYPE::kThumbstick) {
				if (IsInAnyGTSKillMove()) {
					return false;
				}
			}
			return func<ID>(a_this, a_event);
		}

		template<int ID>
		FUNCTYPE_VFUNC_UNIQUE func;
	};


	void Hook_Controls::Install() {

		logger::info("Installing Control VTABLE Hooks...");

		stl::write_vfunc_unique<CanProcess, 0>(VTABLE_ActivateHandler[0]);
		stl::write_vfunc_unique<CanProcess, 1>(VTABLE_JumpHandler[0]);
		stl::write_vfunc_unique<CanProcess, 2>(VTABLE_ShoutHandler[0]);
		stl::write_vfunc_unique<CanProcess, 3>(VTABLE_SneakHandler[0]);
		stl::write_vfunc_unique<CanProcess, 4>(VTABLE_ToggleRunHandler[0]);
		stl::write_vfunc_unique<CanProcess, 5>(VTABLE_AttackBlockHandler[0]);

		stl::write_vfunc_unique<ProcessMouseMove, 6>(VTABLE_LookHandler[0]);

		//stl::write_vfunc_unique<CanProcess, 6>(VTABLE_AutoMoveHandler[0]);
		//stl::write_vfunc_unique<CanProcess, 7>(VTABLE_MovementHandler[0]);
		//stl::write_vfunc_unique<CanProcess, 8>(VTABLE_ReadyWeaponHandler[0]);
		//stl::write_vfunc_unique<CanProcess, 9>(VTABLE_RunHandler[0]);
		//stl::write_vfunc_unique<CanProcess, 10>(VTABLE_ThirdPersonState[0]);
		//stl::write_vfunc_unique<CanProcess, 11>(VTABLE_SprintHandler[0]);

	}

}