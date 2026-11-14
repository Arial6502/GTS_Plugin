#include "Managers/AI/Posture.hpp"
#include "Managers/AI/Adapters.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Config/Config.hpp"

namespace {

	using namespace GTS;
	using Actions::Stance;

	std::shared_mutex g_Lock;
	absl::flat_hash_map<RE::FormID, Stance> g_Held;

	constexpr std::uint32_t AlwaysSneak = static_cast<std::uint32_t>(RE::PACKAGE_DATA::GeneralFlag::kAlwaysSneak);

	// Actor::Update derives the sneak state from the current package every frame and forces the actor
	// back when it disagrees, so setting ActorState1::sneaking by hand is undone the same frame. This is
	// the channel it actually reads.
	void PackageSneak(RE::Actor* a_Actor, bool a_Enable) {

		auto* process = a_Actor->GetActorRuntimeData().currentProcess;

		if (!process) {
			return;
		}

		auto& package = process->currentPackage;

		if (a_Enable) {
			package.modifiedPackageFlag |= AlwaysSneak;
		}
		else {
			package.modifiedPackageFlag &= ~AlwaysSneak;
		}
	}

	void Mark(RE::Actor* a_Actor, bool a_Held) {
		if (TransientActorData* transient = Transient::GetActorData(a_Actor)) {
			transient->AIStance = a_Held;
		}
	}
}

namespace GTS::AI {

	void Posture::Hold(RE::Actor* a_Actor, Stance a_Stance) {

		if (!a_Actor) {
			return;
		}

		{
			std::unique_lock lock(g_Lock);
			g_Held[a_Actor->formID] = a_Stance;
		}

		Mark(a_Actor, a_Stance != Stance::kStanding);

		switch (a_Stance) {
			case Stance::kCrawl:
			{
				// Crawling is a sneak underneath, so the sneak goes on first and the crawl graph
				// change follows it.
				PackageSneak(a_Actor, true);
				AnimationVars::Other::SetVanillaSneaking(a_Actor, true);

				if (SetCrawlAnimation(a_Actor, true)) {
					UpdateCrawlAnimations(a_Actor, true);
				}
				break;
			}
			case Stance::kSneak:
			{
				PackageSneak(a_Actor, true);
				AnimationVars::Other::SetVanillaSneaking(a_Actor, true);
				break;
			}
			default:
			{
				break;
			}
		}
	}

	void Posture::Release(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return;
		}

		Stance held = Stance::kStanding;

		{
			std::unique_lock lock(g_Lock);

			const auto found = g_Held.find(a_Actor->formID);

			if (found == g_Held.end()) {
				return;
			}

			held = found->second;
			g_Held.erase(found);
		}

		if (held == Stance::kCrawl && SetCrawlAnimation(a_Actor, false)) {
			UpdateCrawlAnimations(a_Actor, false);
		}

		if (held != Stance::kStanding) {
			AnimationVars::Other::SetVanillaSneaking(a_Actor, false);
			PackageSneak(a_Actor, false);
		}

		// Cleared last, so UpdateCrawlState only takes the actor back once the stance is undone.
		Mark(a_Actor, false);
	}

	void Posture::Enforce(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return;
		}

		std::shared_lock lock(g_Lock);

		const auto found = g_Held.find(a_Actor->formID);

		if (found != g_Held.end() && found->second != Stance::kStanding) {
			PackageSneak(a_Actor, true);
		}
	}

	void Posture::Clear() {
		std::unique_lock lock(g_Lock);
		g_Held.clear();
	}

	bool Posture::Settled(RE::Actor* a_Actor, Stance a_Stance) {

		if (!a_Actor) {
			return false;
		}

		switch (a_Stance) {
			case Stance::kCrawl:
			{
				return AnimationVars::Crawl::IsCrawling(a_Actor);
			}
			case Stance::kSneak:
			{
				return a_Actor->IsSneaking() && !AnimationVars::Crawl::IsCrawling(a_Actor);
			}
			default:
			{
				return !AnimationVars::Crawl::IsCrawling(a_Actor);
			}
		}
	}

	bool Posture::ForcesSneak(RE::FormID a_Actor) {
		std::shared_lock lock(g_Lock);
		const auto found = g_Held.find(a_Actor);
		return found != g_Held.end() && found->second != Stance::kStanding;
	}

	Stance Posture::Roll(RE::Actor* a_Actor, std::uint8_t a_Stances) {

		// Crawl is rolled first, so a branch with both does not need the chances to sum to 100.
		if ((a_Stances & StanceBit(Stance::kCrawl)) && RandomFloat(0.0f, 1.0f) * 100.0f < Config::AI.Movement.fCrawlChance) {
			return Stance::kCrawl;
		}

		if ((a_Stances & StanceBit(Stance::kSneak)) && RandomFloat(0.0f, 1.0f) * 100.0f < Config::AI.Movement.fSneakChance) {
			return Stance::kSneak;
		}

		return Stance::kStanding;
	}
}
