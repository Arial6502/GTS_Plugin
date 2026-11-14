#include "Managers/AI/Steering.hpp"
#include "Systems/Movement/MovementGate.hpp"

namespace {

	using namespace GTS;

	// The console PathToRef. Four arguments: actor, target, speed in XMM2, null in R9. Speed is -1
	// because Actor::PathToRef only builds its MovementParameters block for a speed in [0,1], and that
	// block pins the actor to a walk. Out of range means no override, so the actor uses its own speeds.
	using PathToRefFn = bool(*)(RE::Actor*, RE::TESObjectREFR*, float, void*);

	// IMovementSetGoal::SetGoal releases the built path and aborts the in-flight navmesh search on
	// every call, so re-issuing per frame starves the search and leaves the actor walking a straight
	// line into whatever is in the way. This is the gap that lets a path build.
	constexpr float PathGap = 0.35f;

	// A path is built to where the target stood. Once the target has walked this far off that point the
	// remaining path can point behind the actor, which reads as her backing up, so the goal is re-sent
	// early rather than waiting out the full gap.
	constexpr float GoalDrift = 96.0f;

	// Floor on an early re-issue, so a target circling the actor cannot starve the path build.
	constexpr float MinGap = 0.15f;

	constexpr float Progress = 16.0f;

	struct Walk {
		double Issued = 0.0;
		float Best = std::numeric_limits<float>::max();
		double Improved = 0.0;
		RE::NiPoint3 GoalAt;
		std::optional<Gait> Applied;
	};

	absl::flat_hash_map<RE::FormID, Walk> g_Walks;

	bool PathTo(RE::Actor* a_Actor, RE::Actor* a_Target) {
		static const REL::Relocation<PathToRefFn> Path{ REL::RelocationID(36787, 37803) };
		const MovementGate::Issuing issuing;
		return Path(a_Actor, a_Target, -1.0f, nullptr);
	}

	// The gait is recomputed from the current package on every movement update and written with
	// ActorState::SetMoveFlags, so setting the ActorState1 bits by hand is overwritten. Preferred speed
	// on AIProcess::currentPackage is the first thing that computation reads, ahead of forced run and
	// the distance fallback. ActorPackage::SetPackage rebuilds the flag whenever the package changes,
	// so this is re-asserted every update rather than set once.
	void ApplyGait(RE::Actor* a_Actor, std::optional<Gait> a_Gait) {

		constexpr auto Preferred = static_cast<std::uint32_t>(RE::PACKAGE_DATA::GeneralFlag::kPreferredSpeed);

		auto* process = a_Actor->GetActorRuntimeData().currentProcess;

		if (!process) {
			return;
		}

		auto& package = process->currentPackage;

		if (!a_Gait) {
			package.modifiedPackageFlag &= ~Preferred;
			return;
		}

		package.modifiedPackageFlag |= Preferred;
		package.preferredSpeed = static_cast<std::int8_t>(*a_Gait);
	}
}

namespace GTS {

	bool Steering::Update(RE::Actor* a_Actor, RE::Actor* a_Target, Gait a_Gait) {

		if (!a_Actor || !a_Target || !a_Actor->Is3DLoaded() || !a_Target->Is3DLoaded()) {
			return false;
		}

		const double now = Time::WorldTimeElapsed();
		const bool fresh = !g_Walks.contains(a_Actor->formID);

		Walk& walk = g_Walks[a_Actor->formID];

		if (fresh) {
			walk.Improved = now;
		}

		if (walk.Applied != std::optional<Gait>(a_Gait)) {
			walk.Applied = a_Gait;
			logger::debug("[Steering] {:08X} gait {}", a_Actor->formID, std::to_underlying(a_Gait));
		}

		ApplyGait(a_Actor, a_Gait);

		{
			const float distance = a_Actor->GetPosition().GetDistance(a_Target->GetPosition());

			if (distance < walk.Best - Progress) {
				walk.Best = distance;
				walk.Improved = now;
			}
		}

		if (!fresh) {

			const double waited = now - walk.Issued;
			const bool drifted = (a_Target->GetPosition() - walk.GoalAt).Length() > GoalDrift;

			if (waited < PathGap && !(drifted && waited >= MinGap)) {
				return true;
			}
		}

		walk.Issued = now;
		walk.GoalAt = a_Target->GetPosition();

		return PathTo(a_Actor, a_Target);
	}

	void Steering::Stop(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return;
		}

		if (g_Walks.erase(a_Actor->formID) > 0) {
			ApplyGait(a_Actor, std::nullopt);
		}
	}

	void Steering::Clear() {
		g_Walks.clear();
		MovementGate::Clear();
	}

	bool Steering::IsSteering(const RE::Actor* a_Actor) {
		return a_Actor && g_Walks.contains(a_Actor->formID);
	}

	float Steering::Stalled(const RE::Actor* a_Actor) {

		if (!a_Actor) {
			return 0.0f;
		}

		const auto found = g_Walks.find(a_Actor->formID);

		if (found == g_Walks.end()) {
			return 0.0f;
		}

		return static_cast<float>(Time::WorldTimeElapsed() - found->second.Improved);
	}
}
