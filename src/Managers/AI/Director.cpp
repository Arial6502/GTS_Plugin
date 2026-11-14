#include "Managers/AI/Director.hpp"
#include "Managers/AI/Adapters.hpp"
#include "Managers/AI/Blinds.hpp"
#include "Managers/AI/Eligibility.hpp"
#include "Managers/AI/Steering.hpp"
#include "Managers/AI/Posture.hpp"
#include "Managers/AI/Suppression.hpp"
#include "Managers/Animation/Utils/TurnTowards.hpp"
#include "Managers/Console/ConsoleManager.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"
#include "Systems/Movement/MovementGate.hpp"
#include "Debug/AIDebug/AIDebugger.hpp"
#include "Config/Config.hpp"

namespace {

	using namespace GTS;
	using namespace GTS::AI;

	std::uint64_t RefusalKey(RE::FormID a_Target, std::uint8_t a_Adapter) {
		return (static_cast<std::uint64_t>(a_Target) << 8) | a_Adapter;
	}

	RE::Actor* TargetOf(const Brain& a_Brain) {
		auto ptr = a_Brain.Intent.Target.get();
		return ptr ? ptr.get() : nullptr;
	}

	float Jitter() {
		return 0.75f + static_cast<float>(rand() % 50) / 100.0f;
	}

	float Value(const float* a_Value, float a_Fallback) {
		return a_Value ? *a_Value : a_Fallback;
	}

	bool Flag(const bool* a_Value, bool a_Fallback) {
		return a_Value ? *a_Value : a_Fallback;
	}

	// Percent settings are stored as 0..100 so they read naturally on a slider. Clamped here because
	// the UI only enforces its own limits when bEnforceUIClamps is on.
	float Fraction(float a_Percent) {
		return std::clamp(a_Percent, 1.0f, 100.0f) * 0.01f;
	}

	float JogBeyond() {
		return std::max(Config::AI.Movement.fJogBeyond, 0.0f);
	}

	float RunBeyond() {
		return std::max(Config::AI.Movement.fRunBeyond, JogBeyond());
	}

	bool SameName(std::string_view a_Lhs, std::string_view a_Rhs) {
		return std::ranges::equal(a_Lhs, a_Rhs, [](char a_A, char a_B) {
			return std::tolower(static_cast<unsigned char>(a_A)) == std::tolower(static_cast<unsigned char>(a_B));
		});
	}

	bool InReach(RE::Actor* a_Actor, RE::Actor* a_Target, const Adapter& a_Adapter) {
		return a_Adapter.Reach && TargetShape::InReach(a_Actor, a_Target, a_Adapter.Reach(a_Actor));
	}
}

namespace GTS::AI {

	bool Director::SuppressesAttacks(RE::Actor* a_Actor) {
		return Suppression::Holds(a_Actor);
	}

	bool Director::Force(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Adapter) {

		if (!a_Actor) {
			return false;
		}

		for (std::uint8_t index = 0; index < Table().size(); ++index) {

			const Adapter& adapter = Table()[index];

			if (!SameName(adapter.Name, a_Adapter)) {
				continue;
			}

			const double now = Time::WorldTimeElapsed();

			Brain& brain = m_Brains[a_Actor->formID];

			Undo(a_Actor, brain);
			brain.Refused.clear();
			brain.SubRefused.clear();

			brain.Intent = Plan{
				.Adapter = index,
				.Target = a_Target ? a_Target->GetHandle() : RE::ActorHandle{},
				.Forced = true,
			};

			const bool walk = a_Target && adapter.Qualifies && !InReach(a_Actor, a_Target, adapter);

			Log(a_Actor, std::format("FORCE {} on {:08X}", adapter.Name, a_Target ? a_Target->formID : 0u));
			Enter(a_Actor, brain, walk ? Phase::kApproach : Phase::kPosture, now);

			return true;
		}

		return false;
	}

	void Director::Log(RE::Actor* a_Actor, std::string_view a_Line) {

		if (Actions::ActionRegistry::Watching()) {
			logger::info("AI| {:08X} {}", a_Actor ? a_Actor->formID : 0u, a_Line);
		}

		if (a_Actor) {
			AIDebugger::Record(a_Actor->formID, a_Actor->GetDisplayFullName(), a_Line);
		}
	}

	void Director::Enter(RE::Actor* a_Actor, Brain& a_Brain, Phase a_Phase, double a_Now) {
		a_Brain.Stage = a_Phase;
		a_Brain.StageSince = a_Now;
		Log(a_Actor, std::format("PHASE {}", PhaseName(a_Phase)));
	}

	void Director::Bail(RE::Actor* a_Actor, Brain& a_Brain, std::string_view a_Why, double a_Now) {
		Log(a_Actor, std::format("BAIL {}", a_Why));
		Enter(a_Actor, a_Brain, Phase::kRecover, a_Now);
	}

	void Director::Apply(RE::Actor* a_Actor, Brain& a_Brain, std::uint8_t a_Effects) {

		if (a_Effects & Effect::kSuppressed) {
			if (!(a_Brain.Applied & Effect::kSuppressed)) {
				Suppression::Take(a_Actor->formID);
				a_Brain.Applied |= Effect::kSuppressed;
			}
			Suppression::Enforce(a_Actor);
		}

		if (a_Effects & Effect::kCombatLever) {
			a_Brain.Applied |= Effect::kCombatLever;
			Blinds::Apply(a_Actor);
		}

		if (a_Effects & Effect::kSteering) {
			a_Brain.Applied |= Effect::kSteering;
		}

		if (a_Effects & Effect::kStance) {
			a_Brain.Applied |= Effect::kStance;
		}

		if ((a_Effects & Effect::kMovement) && !(a_Brain.Applied & Effect::kMovement)) {
			MovementGate::Take(a_Actor->formID);
			a_Brain.Applied |= Effect::kMovement;
		}

		if (a_Brain.Applied & Effect::kStance) {
			AI::Posture::Enforce(a_Actor);
		}
	}

	// Dropping the stance swaps the behaviour graph, so it is kept separate from the effects that are
	// safe to drop at any moment.
	void Director::UndoStance(RE::Actor* a_Actor, Brain& a_Brain) {

		if (a_Brain.Applied & Effect::kStance) {
			AI::Posture::Release(a_Actor);
			a_Brain.Applied &= static_cast<std::uint8_t>(~Effect::kStance);
		}
	}

	void Director::Undo(RE::Actor* a_Actor, Brain& a_Brain) {

		if (a_Brain.Applied & Effect::kSteering) {
			Steering::Stop(a_Actor);
		}

		// Released last of the movement effects: the actor is better off with no goal at all than with
		// the game handing her one back while the brain is still mid plan.
		if (a_Brain.Applied & Effect::kMovement) {
			MovementGate::Give(a_Actor->formID);
		}

		UndoStance(a_Actor, a_Brain);

		if (a_Brain.Applied & Effect::kCombatLever) {
			Blinds::Release(a_Actor);
		}

		if (a_Brain.Applied & Effect::kSuppressed) {
			Suppression::Give(a_Actor->formID);
		}

		a_Brain.Applied = 0;
	}

	bool Director::Claimed(RE::FormID a_Target, RE::FormID a_By) {

		for (const auto& [owner, brain] : m_Brains) {

			if (owner == a_By || !brain.Intent.Valid()) {
				continue;
			}

			if (auto ptr = brain.Intent.Target.get(); ptr && ptr->formID == a_Target) {
				return true;
			}
		}

		return false;
	}

	bool Director::Refused(const Brain& a_Brain, RE::FormID a_Target, std::uint8_t a_Adapter, double a_Now) {

		const std::uint64_t key = RefusalKey(a_Target, a_Adapter);

		for (const auto& [stored, until] : a_Brain.Refused) {
			if (stored == key && a_Now < until) {
				return true;
			}
		}

		return false;
	}

	void Director::Refuse(Brain& a_Brain, RE::FormID a_Target, std::uint8_t a_Adapter, double a_Until) {

		const std::uint64_t key = RefusalKey(a_Target, a_Adapter);

		for (auto& entry : a_Brain.Refused) {
			if (entry.first == key) {
				entry.second = a_Until;
				return;
			}
		}

		if (a_Brain.Refused.size() >= 4) {
			a_Brain.Refused.erase(a_Brain.Refused.begin());
		}

		a_Brain.Refused.emplace_back(key, a_Until);
	}

	bool Director::SubRefused(const Brain& a_Brain, std::string_view a_Action, double a_Now) {

		for (const auto& entry : a_Brain.SubRefused) {
			if (entry.first == a_Action.data() && a_Now < entry.second) {
				return true;
			}
		}

		return false;
	}

	void Director::RefuseSub(Brain& a_Brain, std::string_view a_Action, double a_Until) {

		for (auto& entry : a_Brain.SubRefused) {
			if (entry.first == a_Action.data()) {
				entry.second = a_Until;
				return;
			}
		}

		if (a_Brain.SubRefused.size() >= 6) {
			a_Brain.SubRefused.erase(a_Brain.SubRefused.begin());
		}

		a_Brain.SubRefused.emplace_back(a_Action.data(), a_Until);
	}

	const Choice* Director::Weighted(RE::Actor* a_Actor, std::span<const Choice> a_Rows, float a_Idle, RE::Actor* a_Partner) {

		std::vector<const Choice*> rows = {};
		float total = std::max(a_Idle, 0.0f);

		for (const Choice& choice : a_Rows) {

			if (!choice.Weight || *choice.Weight <= 0.0f) {
				continue;
			}

			if (choice.Allow && !choice.Allow(a_Actor, a_Partner)) {
				continue;
			}

			if (Actions::ActionRegistry::Probe(a_Actor, choice.Action) != Actions::RequestResult::kAccepted) {
				continue;
			}

			rows.push_back(&choice);
			total += *choice.Weight;
		}

		float roll = RandomFloat(0.0f, 1.0f) * total;

		for (const Choice* choice : rows) {
			roll -= *choice->Weight;
			if (roll <= 0.0f) {
				return choice;
			}
		}

		return nullptr;
	}

	void Director::PickSub(RE::Actor* a_Actor, Brain& a_Brain, const Adapter& a_Adapter, double a_Now) {

		if (a_Now < a_Brain.NextSub) {
			return;
		}

		// A branch with nothing to send and nothing to bail out with has no sub-action step at all.
		if (a_Adapter.Subs.empty() && a_Adapter.Bail.empty()) {
			return;
		}

		const Actions::ActorAction* state = Actions::ActionRegistry::StateOf(a_Actor);

		if (!state) {
			return;
		}

		const float interval = std::max(Value(a_Adapter.Interval, 1.0f), 0.1f);

		a_Brain.NextSub = a_Now + interval * Jitter();

		// Sub-actions only go to the node that is actually running, never to one it handed off to.
		std::vector<const Choice*> rows = {};
		float total = std::max(Value(a_Adapter.Idle, 0.0f), 0.0f);

		for (const Choice& choice : a_Adapter.Subs) {

			if (choice.Node != state->Current || !choice.Weight || *choice.Weight <= 0.0f) {
				continue;
			}

			if (SubRefused(a_Brain, choice.Action, a_Now)) {
				continue;
			}

			if (choice.Allow) {
				RE::Actor* partner = Actions::Possession::CarriedAlive(a_Actor->formID);
				if (!choice.Allow(a_Actor, partner)) {
					continue;
				}
			}

			if (Actions::ActionRegistry::Probe(a_Actor, choice.Action) != Actions::RequestResult::kAccepted) {
				continue;
			}

			rows.push_back(&choice);
			total += *choice.Weight;
		}

		if (rows.empty()) {

			// Everything is unavailable while an action is playing, so a single empty pick says nothing.
			// Only a run of them means the branch has actually run out of things to do.
			if (a_Brain.Empty < 0xFF) {
				++a_Brain.Empty;
			}

			const std::string_view exit = ExitFor(a_Adapter, state->Current);

			if (!exit.empty() && a_Brain.Empty >= EmptyPicksBeforeBail) {
				a_Brain.Empty = 0;
				const QuietFailures quiet;
				Actions::ActionRegistry::Perform(a_Actor, exit);
				Log(a_Actor, std::format("SUB bail {}", exit));
			}

			return;
		}

		a_Brain.Empty = 0;

		float roll = RandomFloat(0.0f, 1.0f) * total;

		for (const Choice* choice : rows) {

			roll -= *choice->Weight;

			if (roll > 0.0f) {
				continue;
			}

			const Actions::RequestResult result = [&] {
				const QuietFailures quiet;
				return Actions::ActionRegistry::Perform(a_Actor, choice->Action);
			}();

			Log(a_Actor, std::format("SUB {} {}", choice->Action, std::to_underlying(result)));

			switch (result) {
				case Actions::RequestResult::kAccepted:
				{
					break;
				}
				case Actions::RequestResult::kGraphRefused:
				{
					a_Brain.NextSub = a_Now + GraphRetry;
					break;
				}
				default:
				{
					RefuseSub(a_Brain, choice->Action, a_Now + interval * 2.0);
					break;
				}
			}

			return;
		}

		// The roll landed on the idle weight, so nothing is sent this interval.
	}

	void Director::Think(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		if (a_Now < a_Brain.NextThink) {
			return;
		}

		a_Brain.NextThink = a_Now + Config::AI.fMasterTimer * Jitter();

		if (!Eligibility::Performer(a_Actor)) {
			return;
		}

		// Already inside a branch we know, from a restored carry or a player started action. Take it
		// over rather than starting something else on top of it.
		if (Actions::ActionRegistry::Busy(a_Actor)) {

			for (std::uint8_t index = 0; index < Table().size(); ++index) {

				if (!InBranch(a_Actor, Table()[index])) {
					continue;
				}

				a_Brain.Intent = Plan{ .Adapter = index, .Adopted = true };
				Log(a_Actor, std::format("ADOPT {}", Table()[index].Name));
				Enter(a_Actor, a_Brain, Phase::kEngaged, a_Now);
				return;
			}

			return;
		}

		if (RandomFloat(0.0f, 1.0f) * 100.0f > Config::AI.fActChance) {
			return;
		}

		const std::vector<RE::Actor*> candidates = Eligibility::Candidates(a_Actor);

		struct Option {
			std::uint8_t Adapter;
			RE::Actor* Target;
			float Weight;
		};

		std::vector<Option> options = {};

		for (std::uint8_t index = 0; index < Table().size(); ++index) {

			const Adapter& adapter = Table()[index];

			if (!Flag(adapter.Enabled, true) || Value(adapter.Chance, 0.0f) <= 0.0f) {
				continue;
			}

			if (adapter.Ready && !adapter.Ready(a_Actor)) {
				continue;
			}

			// Nothing to aim at: the branch acts on the performer, or finds its own target inside the
			// node. It is offered at its plain chance and starts on the spot.
			if (!adapter.Qualifies) {
				options.push_back(Option{ index, nullptr, Value(adapter.Chance, 0.0f) });
				continue;
			}

			if (candidates.empty()) {
				continue;
			}

			// Only the nearest qualifying candidate is offered. Choosing between adapters is what the
			// proximity weight below does.
			RE::Actor* best = nullptr;
			float bestDistance = 0.0f;

			for (RE::Actor* candidate : candidates) {

				if (!adapter.Qualifies(a_Actor, candidate)) {
					continue;
				}

				if (Refused(a_Brain, candidate->formID, index, a_Now)) {
					continue;
				}

				if (Claimed(candidate->formID, a_Actor->formID)) {
					continue;
				}

				const float distance = a_Actor->GetPosition().GetDistance(candidate->GetPosition());

				if (!best || distance < bestDistance) {
					best = candidate;
					bestDistance = distance;
				}
			}

			if (!best) {
				continue;
			}

			const float reach = adapter.Reach ? std::max(adapter.Reach(a_Actor), 1.0f) : 1.0f;
			const float proximity = 1.0f / (1.0f + bestDistance / (reach * 4.0f));

			options.push_back(Option{ index, best, Value(adapter.Chance, 0.0f) * proximity });
		}

		if (options.empty()) {
			return;
		}

		float total = 0.0f;

		for (const Option& option : options) {
			total += option.Weight;
		}

		float roll = RandomFloat(0.0f, 1.0f) * total;

		for (const Option& option : options) {

			roll -= option.Weight;

			if (roll > 0.0f) {
				continue;
			}

			const Adapter& adapter = Table()[option.Adapter];

			a_Brain.Intent = Plan{
				.Adapter = option.Adapter,
				.Target = option.Target ? option.Target->GetHandle() : RE::ActorHandle{},
			};

			if (!option.Target) {
				Log(a_Actor, std::format("PLAN {} on self", adapter.Name));
				Enter(a_Actor, a_Brain, Phase::kStart, a_Now);
				return;
			}

			Log(a_Actor, std::format("PLAN {} on {:08X} dist {:.0f}", adapter.Name, option.Target->formID,
				a_Actor->GetPosition().GetDistance(option.Target->GetPosition())));

			Enter(a_Actor, a_Brain, InReach(a_Actor, option.Target, adapter) ? Phase::kPosture : Phase::kApproach, a_Now);
			return;
		}
	}

	// Why walk past someone who will do just as well. Every so often during the approach, if another
	// candidate for the same branch is already in reach or much closer, the plan moves to them.
	void Director::Retarget(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		if (a_Now < a_Brain.NextRetarget || a_Brain.Intent.Forced) {
			return;
		}

		a_Brain.NextRetarget = a_Now + std::max(Config::AI.Movement.fRetargetInterval, 0.25f);

		RE::Actor* current = TargetOf(a_Brain);
		const Adapter& adapter = Table()[a_Brain.Intent.Adapter];

		if (!current || !adapter.Qualifies) {
			return;
		}

		const float distance = a_Actor->GetPosition().GetDistance(current->GetPosition());

		RE::Actor* best = nullptr;
		float bestDistance = distance;

		for (RE::Actor* candidate : Eligibility::Candidates(a_Actor)) {

			if (candidate == current || !adapter.Qualifies(a_Actor, candidate)) {
				continue;
			}

			if (Refused(a_Brain, candidate->formID, a_Brain.Intent.Adapter, a_Now)) {
				continue;
			}

			if (Claimed(candidate->formID, a_Actor->formID)) {
				continue;
			}

			const float candidateDistance = a_Actor->GetPosition().GetDistance(candidate->GetPosition());
			const bool worth = InReach(a_Actor, candidate, adapter) ||
				candidateDistance < distance * Fraction(Config::AI.Movement.fSwitchRatio);

			if (worth && candidateDistance < bestDistance) {
				best = candidate;
				bestDistance = candidateDistance;
			}
		}

		if (!best) {
			return;
		}

		a_Brain.Intent.Target = best->GetHandle();
		a_Brain.ApproachFrom = 0.0f;

		// Progress is measured against the new target, so the stall timer starts over.
		Steering::Stop(a_Actor);
		a_Brain.Applied &= static_cast<std::uint8_t>(~Effect::kSteering);

		Log(a_Actor, std::format("RETARGET {:08X} dist {:.0f}", best->formID, bestDistance));
	}

	void Director::Approach(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		RE::Actor* target = TargetOf(a_Brain);
		const Adapter& adapter = Table()[a_Brain.Intent.Adapter];

		if (!target || !target->Is3DLoaded()) {
			Bail(a_Actor, a_Brain, "target lost", a_Now);
			return;
		}

		// Death does not disqualify a target on its own: a corpse is still worth stomping while it can
		// be crushed. The adapter is the one that knows.
		if (adapter.Qualifies && !adapter.Qualifies(a_Actor, target)) {
			Bail(a_Actor, a_Brain, "target no longer qualifies", a_Now);
			return;
		}

		Retarget(a_Actor, a_Brain, a_Now);

		target = TargetOf(a_Brain);

		if (!target) {
			Bail(a_Actor, a_Brain, "target lost", a_Now);
			return;
		}

		// Reach is scaled by the actor's size, so the destination is where she can act from rather than
		// the target itself, and everything below is measured in reaches.
		const float reach = std::max(adapter.Reach ? adapter.Reach(a_Actor) : 0.0f, 1.0f);
		const float remaining = std::max(a_Actor->GetPosition().GetDistance(target->GetPosition()) - reach, 0.0f);

		if (a_Brain.ApproachFrom <= 0.0f) {
			a_Brain.ApproachFrom = remaining;
		}

		// Once most of the walk is behind her the stance change starts, so she arrives already in it
		// instead of stopping to change. A short approach has no room for that fraction to mean
		// anything, hence the floor.
		if (!a_Brain.Intent.Rolled &&
			(remaining <= a_Brain.ApproachFrom * Fraction(Config::AI.Movement.fStanceAt) ||
			 remaining <= reach * std::max(Config::AI.Movement.fStanceFloor, 0.0f))) {
			TakeStance(a_Actor, a_Brain, adapter);
		}

		// Ground still to cover sets the pace, so she closes a long gap at a run and arrives walking
		// rather than barrelling into her target.
		const Gait gait = remaining > reach * RunBeyond() ? Gait::kRun
			: remaining > reach * JogBeyond() ? Gait::kJog
			: Gait::kWalk;

		Apply(a_Actor, a_Brain, Effect::kSteering | Effect::kSuppressed | Effect::kCombatLever);

		if (!Steering::Update(a_Actor, target, gait)) {
			Bail(a_Actor, a_Brain, "unreachable", a_Now);
			return;
		}

		if (InReach(a_Actor, target, adapter)) {
			Enter(a_Actor, a_Brain, Phase::kPosture, a_Now);
			return;
		}

		if (Steering::Stalled(a_Actor) > Config::AI.Movement.fApproachStall) {
			Bail(a_Actor, a_Brain, "stall", a_Now);
			return;
		}

		if (a_Now - a_Brain.StageSince > Config::AI.Movement.fApproachTimeout) {
			Bail(a_Actor, a_Brain, "timeout", a_Now);
		}
	}

	// Rolls the stance once per plan and holds it if the roll wants one. Standing counts as rolled, so
	// the roll is not repeated when Posture is reached.
	bool Director::TakeStance(RE::Actor* a_Actor, Brain& a_Brain, const Adapter& a_Adapter) {

		a_Brain.Intent.Want = AI::Posture::Roll(a_Actor, a_Adapter.Stances);
		a_Brain.Intent.Rolled = true;

		if (a_Brain.Intent.Want == Actions::Stance::kStanding) {
			return false;
		}

		AI::Posture::Hold(a_Actor, a_Brain.Intent.Want);
		Apply(a_Actor, a_Brain, Effect::kStance);

		Log(a_Actor, std::format("STANCE {}", Actions::StanceName(a_Brain.Intent.Want)));
		return true;
	}

	void Director::Posture(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		RE::Actor* target = TargetOf(a_Brain);
		const Adapter& adapter = Table()[a_Brain.Intent.Adapter];

		// Steering owns the facing while it runs, so it stops before the actor is turned by hand.
		if (a_Brain.Applied & Effect::kSteering) {
			Steering::Stop(a_Actor);
			a_Brain.Applied &= static_cast<std::uint8_t>(~Effect::kSteering);
		}

		if (target && adapter.Shape && !TargetShape::InCone(a_Actor, target, *adapter.Shape)) {
			FaceTowardsPoint(a_Actor, target->GetPosition());
		}

		// Plans that began in reach never walked, so the roll happens here instead.
		if (!a_Brain.Intent.Rolled && !TakeStance(a_Actor, a_Brain, adapter)) {
			Enter(a_Actor, a_Brain, Phase::kStart, a_Now);
			return;
		}

		if (a_Brain.Intent.Want == Actions::Stance::kStanding) {
			Enter(a_Actor, a_Brain, Phase::kStart, a_Now);
			return;
		}

		// A stance that will not settle is not worth losing the plan over, so the start goes ahead
		// either way once the window is up.
		if (AI::Posture::Settled(a_Actor, a_Brain.Intent.Want) || a_Now - a_Brain.StageSince > StanceWindow) {
			Enter(a_Actor, a_Brain, Phase::kStart, a_Now);
		}
	}

	void Director::Start(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		RE::Actor* target = TargetOf(a_Brain);
		const Adapter& adapter = Table()[a_Brain.Intent.Adapter];

		if (a_Brain.Applied & Effect::kSteering) {
			Steering::Stop(a_Actor);
			a_Brain.Applied &= static_cast<std::uint8_t>(~Effect::kSteering);
		}

		const bool needsTarget = adapter.Qualifies != nullptr;

		if (needsTarget) {

			if (!target || !target->Is3DLoaded()) {
				Bail(a_Actor, a_Brain, "target lost", a_Now);
				return;
			}

			if (adapter.Qualifies && !adapter.Qualifies(a_Actor, target)) {
				Refuse(a_Brain, target->formID, a_Brain.Intent.Adapter, a_Now + 3.0);
				Bail(a_Actor, a_Brain, "target no longer qualifies", a_Now);
				return;
			}

			if (!InReach(a_Actor, target, adapter)) {
				Bail(a_Actor, a_Brain, std::format("out of reach at {:.0f}",
					a_Actor->GetPosition().GetDistance(target->GetPosition())), a_Now);
				return;
			}

			if (adapter.Shape && !TargetShape::InCone(a_Actor, target, *adapter.Shape)) {
				FaceTowardsPoint(a_Actor, target->GetPosition());
			}
		}

		bool started = false;

		{
			const QuietFailures quiet;

			if (!adapter.Starts.empty()) {
				if (const Choice* choice = Weighted(a_Actor, adapter.Starts, 0.0f, target)) {
					started = Actions::ActionRegistry::Perform(a_Actor, choice->Action) == Actions::RequestResult::kAccepted;
					Log(a_Actor, std::format("START pick {}", choice->Action));
				}
			}
			else if (adapter.Start) {
				started = adapter.Start(a_Actor, target);
			}
		}

		if (!started) {
			if (target) {
				Refuse(a_Brain, target->formID, a_Brain.Intent.Adapter, a_Now + 3.0);
			}
			Bail(a_Actor, a_Brain, "start refused", a_Now);
			return;
		}

		Log(a_Actor, std::format("START {} on {:08X}", adapter.Name, target ? target->formID : 0u));

		a_Brain.SubRefused.clear();
		a_Brain.Empty = 0;
		a_Brain.NextSub = a_Now + Value(adapter.Interval, 1.0f);

		Enter(a_Actor, a_Brain, Phase::kEngaged, a_Now);
	}

	void Director::Engaged(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		const Adapter& adapter = Table()[a_Brain.Intent.Adapter];

		Apply(a_Actor, a_Brain, Effect::kSuppressed | Effect::kCombatLever);

		if (InBranch(a_Actor, adapter)) {

			// A branch that acts on someone has nothing left to do once they are dead or gone. The node
			// keeps running either way, so the exit has to be sent rather than waited for.
			if (!a_Brain.Intent.Ending && adapter.Qualifies && a_Now - a_Brain.StageSince > ConfirmWindow) {

				RE::Actor* target = TargetOf(a_Brain);
				const bool finished = adapter.Finished && adapter.Finished(a_Actor, target);

				if (!target || target->IsDead() || finished) {

					// Which way out depends on which node is running: a sandwich in butt mode cannot
					// take the base node's exit.
					const Actions::ActorAction* state = Actions::ActionRegistry::StateOf(a_Actor);
					const std::string_view exit = ExitFor(adapter, state ? state->Current : Actions::ActionId::kNone);

					if (!exit.empty()) {
						a_Brain.Intent.Ending = true;
						const QuietFailures quiet;
						Actions::ActionRegistry::Perform(a_Actor, exit);
						Log(a_Actor, std::format("BAIL {}, sent {}", finished ? "nothing left to do" : "partner gone", exit));
						return;
					}
				}
			}

			// Nothing more is sent once the exit is on its way, or it plays over the top of it.
			if (!a_Brain.Intent.Ending) {
				PickSub(a_Actor, a_Brain, adapter, a_Now);
			}

			return;
		}

		// The graph needs a moment to reach the node after a start, so an empty branch only counts once
		// that window has passed.
		if (a_Now - a_Brain.StageSince < ConfirmWindow) {
			return;
		}

		if (!a_Brain.Intent.Adopted) {
			if (RE::Actor* target = TargetOf(a_Brain)) {
				Refuse(a_Brain, target->formID, a_Brain.Intent.Adapter, a_Now + 3.0);
			}
		}

		Bail(a_Actor, a_Brain, "branch ended", a_Now);
	}

	void Director::Recover(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		// Everything but the stance comes off straight away. Changing the behaviour graph while the
		// actor is still mid action swaps the skeleton out from under whatever is running, so the
		// stance waits for the action to end.
		if ((a_Brain.Applied & Effect::kStance) && Actions::ActionRegistry::Busy(a_Actor) &&
			a_Now - a_Brain.StageSince < RecoverWait) {

			const std::uint8_t stance = static_cast<std::uint8_t>(a_Brain.Applied & Effect::kStance);

			a_Brain.Applied &= static_cast<std::uint8_t>(~Effect::kStance);
			Undo(a_Actor, a_Brain);
			a_Brain.Applied = stance;

			return;
		}

		Undo(a_Actor, a_Brain);
		a_Brain.Intent = {};
		a_Brain.NextThink = a_Now + RecoverCooldown;
		Enter(a_Actor, a_Brain, Phase::kIdle, a_Now);
	}

	void Director::Tick(RE::Actor* a_Actor, Brain& a_Brain, double a_Now) {

		// The player taking control mid plan, or the actor going down, ends it wherever it was.
		if (a_Brain.Stage != Phase::kIdle && a_Brain.Stage != Phase::kRecover && !Eligibility::Performer(a_Actor)) {
			Bail(a_Actor, a_Brain, "performer ineligible", a_Now);
		}

		// Whoever else wants to move this actor is refused for the whole plan, not only while a path is
		// live. Handing movement back between phases let the game walk her away from the target the
		// brain had just chosen.
		if (a_Brain.Stage != Phase::kIdle && a_Brain.Stage != Phase::kRecover) {
			Apply(a_Actor, a_Brain, Effect::kMovement);
		}

		switch (a_Brain.Stage) {
			case Phase::kIdle:
			{
				Think(a_Actor, a_Brain, a_Now);
				break;
			}
			case Phase::kApproach:
			{
				Approach(a_Actor, a_Brain, a_Now);
				break;
			}
			case Phase::kPosture:
			{
				Posture(a_Actor, a_Brain, a_Now);
				break;
			}
			case Phase::kStart:
			{
				Start(a_Actor, a_Brain, a_Now);
				break;
			}
			case Phase::kEngaged:
			{
				Engaged(a_Actor, a_Brain, a_Now);
				break;
			}
			case Phase::kRecover:
			{
				Recover(a_Actor, a_Brain, a_Now);
				break;
			}
		}
	}

	void Director::OnMainUpdate() {

		if (!Config::AI.bEnableActionAI) {

			if (!m_Brains.empty()) {

				for (auto& [owner, brain] : m_Brains) {
					if (RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(owner)) {
						Undo(actor, brain);
					}
				}

				m_Brains.clear();
				Suppression::Clear();
				Steering::Clear();
			}

			return;
		}

		const double now = Time::WorldTimeElapsed();

		for (RE::Actor* actor : find_actors()) {

			if (!actor || !actor->Is3DLoaded()) {
				continue;
			}

			if (!m_Brains.contains(actor->formID) && !Eligibility::Performer(actor)) {
				continue;
			}

			Tick(actor, m_Brains[actor->formID], now);
		}
	}

	void Director::Forget(RE::FormID a_Actor) {

		const auto found = m_Brains.find(a_Actor);

		if (found == m_Brains.end()) {
			return;
		}

		if (RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(a_Actor)) {
			Undo(actor, found->second);
		}

		// Unconditional: an actor whose form no longer resolves never reached Undo, and a leaked entry
		// would leave the game unable to move it for the rest of the session.
		Suppression::Give(a_Actor);
		MovementGate::Give(a_Actor);

		m_Brains.erase(found);
	}

	void Director::OnActor3DUnload(RE::Actor* a_Actor) {
		if (a_Actor) {
			Forget(a_Actor->formID);
		}
	}

	void Director::OnGameActorReset(RE::Actor* a_Actor) {
		if (a_Actor) {
			Forget(a_Actor->formID);
		}
	}

	void Director::OnPluginReset() {
		m_Brains.clear();
		Suppression::Clear();
		Steering::Clear();
		AI::Posture::Clear();
	}

	std::vector<BrainView> Director::Inspect() {

		std::vector<BrainView> views = {};
		views.reserve(m_Brains.size());

		const double now = Time::WorldTimeElapsed();

		for (const auto& [owner, brain] : m_Brains) {

			RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(owner);

			BrainView view = {};

			view.Actor = owner;
			view.Name = actor ? actor->GetDisplayFullName() : "<gone>";
			view.Stage = brain.Stage;
			view.StageFor = static_cast<float>(now - brain.StageSince);
			view.Applied = brain.Applied;
			view.ApproachFrom = brain.ApproachFrom;
			view.Empty = brain.Empty;
			view.Rolled = brain.Intent.Rolled;
			view.Want = brain.Intent.Want;
			view.Forced = brain.Intent.Forced;
			view.Adopted = brain.Intent.Adopted;

			view.NextThink = static_cast<float>(brain.NextThink - now);
			view.NextSub = static_cast<float>(brain.NextSub - now);
			view.NextRetarget = static_cast<float>(brain.NextRetarget - now);

			const Adapter* adapter = brain.Intent.Valid() ? &Table()[brain.Intent.Adapter] : nullptr;

			if (adapter) {
				view.Adapter = std::string(adapter->Name);
			}

			if (auto ptr = brain.Intent.Target.get()) {
				view.Target = ptr->formID;
				view.TargetName = ptr->GetDisplayFullName();
			}

			if (actor) {

				view.Eligible = Eligibility::Performer(actor);
				view.Busy = Actions::ActionRegistry::Busy(actor);
				view.Gated = MovementGate::Holds(owner);
				view.Suppressed = Suppression::Holds(actor);
				view.Stalled = Steering::Stalled(actor);
				view.Current = std::string(Actions::ActionName(Actions::ActionRegistry::Current(actor)));

				if (adapter && adapter->Reach) {
					view.Reach = adapter->Reach(actor);
				}

				if (auto ptr = brain.Intent.Target.get()) {
					view.Distance = actor->GetPosition().GetDistance(ptr->GetPosition());
					view.Remaining = std::max(view.Distance - view.Reach, 0.0f);
				}
			}

			for (const auto& [key, until] : brain.Refused) {

				const RE::FormID target = static_cast<RE::FormID>(key >> 8);
				const std::uint8_t index = static_cast<std::uint8_t>(key & 0xFF);
				RE::Actor* refused = RE::TESForm::LookupByID<RE::Actor>(target);

				view.Refused.emplace_back(
					std::format("{} on {}", index < Table().size() ? Table()[index].Name : "?",
						refused ? refused->GetDisplayFullName() : "?"),
					static_cast<float>(until - now));
			}

			for (const auto& [action, until] : brain.SubRefused) {
				view.SubRefused.emplace_back(action ? action : "?", static_cast<float>(until - now));
			}

			views.push_back(std::move(view));
		}

		std::ranges::sort(views, [](const BrainView& a_L, const BrainView& a_R) {
			return a_L.Name < a_R.Name;
		});

		return views;
	}

	void Director::OnSKSEDataLoaded() {

		ConsoleManager::RegisterCommand({
			.Name = "ai",
			.Desc = "AI director state",
			.Usage = "status | force <adapter> [actor] [target]",
			.MinArgs = 1,
			.MaxArgs = 4,
			.Callback = [](const ConsoleArgs& a_Args) {

				if (a_Args.Lower(0) == "force") {

					RE::Actor* actor = a_Args.ResolveActor(2);
					RE::Actor* target = a_Args.Count() > 3 ? a_Args.ResolveActor(3) : RE::PlayerCharacter::GetSingleton();

					if (!actor) {
						Cprint("Select an actor to drive.");
						return;
					}

					if (Force(actor, target, a_Args.Raw(1))) {
						Cprint("{} is now doing {}.", actor->GetDisplayFullName(), a_Args.Raw(1));
						return;
					}

					std::string names = {};

					for (const Adapter& adapter : Table()) {
						names += std::format("{} ", adapter.Name);
					}

					Cprint("Unknown branch. Try: {}", names);
					return;
				}

				if (a_Args.Lower(0) != "status") {
					ConsoleManager::PrintUsage("ai");
					return;
				}

				if (m_Brains.empty()) {
					Cprint("No brains running.");
					return;
				}

				for (const auto& [owner, brain] : m_Brains) {

					RE::Actor* actor = RE::TESForm::LookupByID<RE::Actor>(owner);
					RE::Actor* target = TargetOf(brain);

					Cprint("{} {} {} -> {} applied {:#x}",
						actor ? actor->GetDisplayFullName() : "?",
						PhaseName(brain.Stage),
						brain.Intent.Valid() ? Table()[brain.Intent.Adapter].Name : "-",
						target ? target->GetDisplayFullName() : "-",
						brain.Applied);
				}
			},
		});
	}
}
