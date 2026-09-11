#include "Actions/Core/ActionCleanup.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"

namespace GTS::Actions::Cleanup {

	void Scare(RE::Actor* a_Tiny) {

		if (!a_Tiny || !a_Tiny->Is3DLoaded()) {
			return;
		}

		//Can't scare NPC's in combat otherwise.
		ActionRegistry::Notify(a_Tiny, "IdleForceDefaultState", true);

		const RE::ActorHandle handle = a_Tiny->GetHandle();

		TaskManager::RunOnce([handle](const OneshotUpdate&) {
			if (auto ptr = handle.get(); ptr && ptr->Is3DLoaded()) {
				ActionRegistry::Notify(ptr.get(), "GTS_EnterFear");
			}
		});
	}

	void ScareSlot(RE::FormID a_Owner, PossessionSlot a_Slot) {
		for (const auto& handle : Possession::All(a_Owner, a_Slot)) {
			NiPointer<Actor> ptr = handle.get();
			Scare(ptr.get());
		}
	}

	void ReleaseTiny(RE::Actor* a_Tiny, PossessionSlot a_Slot) {

		if (!a_Tiny) {
			return;
		}

		SetBeingEaten(a_Tiny, false);
		SetBeingHeld(a_Tiny, false);
		SetBetweenBreasts(a_Tiny, false);
		AllowToBeCrushed(a_Tiny, true);
		Anims_FixAnimationDesync(nullptr, a_Tiny, true);

		if (a_Tiny->Is3DLoaded()) {

			// Out of the storage pose. OnBreastTake sends the same when the tiny is taken into the hand.
			if (a_Slot == PossessionSlot::kBreasts && !a_Tiny->IsDead()) {
				ActionRegistry::Notify(a_Tiny, "GTSBEH_T_Remove", true);
			}

			ActionRegistry::Notify(a_Tiny, "GTS_ExitFear", true);
			EnableCollisions(a_Tiny);
		}

		if (a_Tiny->IsPlayerRef()) {
			if (auto* camera = RE::PlayerCamera::GetSingleton()) {
				camera->cameraTarget = a_Tiny->GetHandle();
			}
		}
	}

	void ReleaseSlot(RE::FormID a_Owner, PossessionSlot a_Slot) {
		Possession::Release(a_Owner, a_Slot);
	}

	void ReleaseAll(RE::FormID a_Owner) {
		Possession::ReleaseAll(a_Owner);
	}

	void ReleaseOwned(RE::FormID a_Owner, std::span<const PossessionSlot> a_Slots) {
		ReleaseOwnedExcept(a_Owner, a_Slots, {});
	}

	void ReleaseOwnedExcept(RE::FormID a_Owner, std::span<const PossessionSlot> a_Slots, std::span<const PossessionSlot> a_Keep) {

		for (const PossessionSlot slot : a_Slots) {

			if (std::ranges::contains(a_Keep, slot)) {
				continue;
			}

			Possession::Release(a_Owner, slot);
		}
	}

	void ResetCamera(RE::Actor* a_Giant) {
		if (a_Giant) {
			ManageCamera(a_Giant, false, CameraTracking::None);
		}
	}
}
