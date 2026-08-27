#include "Managers/AI/CombatSteering.hpp"

namespace {

	using namespace GTS;

	// Actor::SetKeepOffsetFromActor and Actor::ClearKeepOffsetFromActor.
	// Both go through the MovementControllerNPC at Actor+0x150: the first enables the keep offset planner agent and hands it the target and
	// radii, the second turns it back off.

	//SetKeepOffsetFromActor
	//SE ID: 36870 
	//AE ID: 37894

	//ClearKeepOffsetFromActor
	//SE ID: 36871 
	//AE ID: 37895

	using SetKeepOffsetFn = void(*)(RE::Actor*, const RE::ActorHandle*, const RE::NiPoint3*, const RE::NiPoint3*, float, float);
	using ClearKeepOffsetFn = void(*)(RE::Actor*);

	constexpr float kFollowRadius = 64.0f;
	constexpr float kCatchUpRadius = 512.0f;

	// Reissuing is idempotent, so this only exists to keep the call off every single frame.
	constexpr double kRefreshSeconds = 1.0;

	struct Steered {
		RE::ActorHandle Target{};
		double          LastIssued = 0.0;
	};

	std::mutex                                  g_Lock{};
	std::unordered_map<std::uint32_t, Steered>  g_Steered{};

	void ActorSetKeepDistance(RE::Actor* a_Actor, RE::Actor* a_Target) {

		static const REL::Relocation<SetKeepOffsetFn> Func{ REL::RelocationID(36870, 37894, NULL) };

		const RE::ActorHandle Handle = a_Target->GetHandle();
		constexpr RE::NiPoint3 Zero{};

		Func(a_Actor, &Handle, &Zero, &Zero, kCatchUpRadius, kFollowRadius);
	}

	void ActorClearKeepDistance(RE::Actor* a_Actor) {
		static const REL::Relocation<ClearKeepOffsetFn> Func{ REL::RelocationID(36871, 37895, NULL) };
		Func(a_Actor);
	}
}

namespace GTS {

	void CombatSteering::Apply(RE::Actor* a_Actor, RE::Actor* a_Target) {

		if (!a_Actor || !a_Target || a_Actor == a_Target) {
			return;
		}

		if (!a_Actor->Is3DLoaded() || a_Target->Is3DLoaded()) {
			return;
		}

		const std::uint32_t   Self   = a_Actor->GetHandle().native_handle();
		const RE::ActorHandle Target = a_Target->GetHandle();
		const double          Now    = Time::WorldTimeElapsed();

		{
			std::scoped_lock Lock(g_Lock);

			auto& Entry = g_Steered[Self];

			if (Entry.Target == Target && (Now - Entry.LastIssued) < kRefreshSeconds) {
				return;
			}

			Entry.Target     = Target;
			Entry.LastIssued = Now;
		}

		ActorSetKeepDistance(a_Actor, a_Target);
	}

	void CombatSteering::Release(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return;
		}

		{
			std::scoped_lock Lock(g_Lock);

			if (g_Steered.erase(a_Actor->GetHandle().native_handle()) == 0) {
				return;
			}
		}

		ActorClearKeepDistance(a_Actor);
	}

	void CombatSteering::Clear() {
		std::scoped_lock Lock(g_Lock);
		g_Steered.clear();
	}

	bool CombatSteering::IsSteering(const RE::Actor* a_Actor) {

		if (!a_Actor) {
			return false;
		}

		std::scoped_lock Lock(g_Lock);
		return g_Steered.contains(const_cast<RE::Actor*>(a_Actor)->GetHandle().native_handle());
	}
}
