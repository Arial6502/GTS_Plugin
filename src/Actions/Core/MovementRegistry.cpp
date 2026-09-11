#include "Actions/Core/MovementRegistry.hpp"
#include "Actions/Core/InputScopes.hpp"

#include "Config/Keybinds.hpp"
#include "Config/Util/KeybindHandler.hpp"

#include "Actions/Core/ActionLog.hpp"
#include "Actions/Core/ActionRegistry.hpp"

#include "Managers/Input/InputManager.hpp"
#include "Actions/Core/ActionLock.hpp"

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	std::size_t IndexOf(MovementId a_Id) {
		return std::to_underlying(a_Id);
	}
}

namespace GTS::Actions {

	void MovementRegistry::Register(std::unique_ptr<IMovementState> a_State) {

		std::lock_guard lock(ActionLock);

		if (!a_State) {
			return;
		}

		const std::size_t index = IndexOf(a_State->Id());

		if (index >= m_Slots.size() || m_Slots[index].State) {
			ReportAndExit(std::format("Movement state {} is registered twice or has no id", MovementName(a_State->Id())));
			return;
		}

		m_Slots[index].State = a_State.get();
		m_Owned.push_back(std::move(a_State));
	}

	IMovementState* MovementRegistry::Find(MovementId a_Id) {

		std::lock_guard lock(ActionLock);
		const std::size_t index = IndexOf(a_Id);
		return index < m_Slots.size() ? m_Slots[index].State : nullptr;
	}

	void MovementRegistry::Log(std::string_view a_Line) {

		std::lock_guard lock(ActionLock);

		if (ActionRegistry::Watching()) {
			ActionLog::Write(a_Line);
		}
	}

	std::size_t MovementRegistry::Count() {

		std::lock_guard lock(ActionLock);

		std::size_t total = 0;

		for (const auto& slot : m_Slots) {
			if (slot.State) {
				++total;
			}
		}

		return total;
	}

	std::string MovementRegistry::Describe(RE::FormID a_Owner) {

		std::lock_guard lock(ActionLock);

		auto it = m_State.find(a_Owner);

		if (it == m_State.end() || !it->second.Active()) {
			return std::string(MovementName(MovementId::kNone));
		}

		return std::string(MovementName(it->second.Current));
	}

	void MovementRegistry::Validate() {

		std::lock_guard lock(ActionLock);

		absl::flat_hash_map<std::string_view, MovementId> owners;

		for (auto& slot : m_Slots) {

			if (!slot.State) {
				continue;
			}

			slot.Signature = GraphVars::Compile(slot.State->Signature());

			for (const auto& def : slot.State->Entries()) {
				slot.EntrySignatures.push_back(def.Signature.empty() ? GraphSignature{} : GraphVars::Compile(def.Signature));
			}

			if (slot.Signature.Empty()) {
				logger::warn("MovementRegistry: {} declares no signature and can never be entered or exited", MovementName(slot.State->Id()));
			}

			for (const auto& def : slot.State->Entries()) {
				if (auto [it, fresh] = owners.emplace(def.Action, slot.State->Id()); !fresh) {
					ReportAndExit(std::format("Movement action '{}' is claimed by both {} and {}", def.Action, MovementName(it->second), MovementName(slot.State->Id())));
				}
			}
		}

		logger::info("MovementRegistry: {} stance(s)", Count());
	}

	void MovementRegistry::BindInput() {

		std::lock_guard lock(ActionLock);

		for (const auto& slot : m_Slots) {

			if (!slot.State) {
				continue;
			}

			for (const auto& def : slot.State->Entries()) {

				if (def.Input.empty()) {
					continue;
				}

				Keybinds::NoteBindKind(def.Input, true);

				const std::string_view action = def.Action;

				InputManager::RegisterInputEvent(def.Input,
					[action](const ManagedInputEvent&) { Perform(GetPlayerOrControlled(), action); },
					[action] { return CanPerform(GetPlayerOrControlled(), action); },
					RootScope());
			}
		}
	}

	void MovementRegistry::Enter(RE::Actor* a_Actor, ActorMovement& a_State, const Slot& a_Slot) {

		std::lock_guard lock(ActionLock);

		a_State.Current = a_Slot.State->Id();
		a_State.Pending = MovementId::kNone;
		a_State.PendingSince = -1.0;
		a_State.MismatchSince = -1.0;
		a_State.LeavingAt = -1.0;
		a_State.EnteredAt = Time::WorldTimeElapsed();
		a_State.Signature = a_Slot.Signature;

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		ActorAction scratch = {};
		const ActionContext ctx(a_Actor, a_Actor->formID, scratch, snapshot);

		++m_Enters;
		a_Slot.State->OnEnter(ctx);

		Log(std::format("MOVE_ENTER|{:08X}|{}|via={}", a_Actor->formID, MovementName(a_State.Current), a_State.EnteredVia.empty() ? "sweep" : a_State.EnteredVia));
	}

	void MovementRegistry::Leave(RE::FormID a_Owner, RE::Actor* a_Actor, ActorMovement& a_State, ExitReason a_Reason) {

		std::lock_guard lock(ActionLock);

		auto* state = Find(a_State.Current);

		if (state && a_Actor) {
			const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
			ActorAction scratch = {};
			const ActionContext ctx(a_Actor, a_Owner, scratch, snapshot);
			state->OnExit(ctx, a_Reason);
		}

		++m_Exits;
		Log(std::format("MOVE_EXIT|{:08X}|{}|{}", a_Owner, MovementName(a_State.Current), ExitReasonName(a_Reason)));

		a_State.Current = MovementId::kNone;
		a_State.Pending = MovementId::kNone;
		a_State.PendingSince = -1.0;
		a_State.MismatchSince = -1.0;
		a_State.LeavingAt = -1.0;
		a_State.Signature = {};
		a_State.EnteredVia.clear();
	}

	RequestResult MovementRegistry::Perform(RE::Actor* a_Actor, std::string_view a_Action) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Action.empty()) {
			return RequestResult::kUnknownAction;
		}

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		const double now = Time::WorldTimeElapsed();
		RequestResult result = RequestResult::kUnknownAction;

		for (const auto& slot : m_Slots) {

			if (!slot.State) {
				continue;
			}

			for (const auto& def : slot.State->Entries()) {

				if (def.Action != a_Action) {
					continue;
				}

				ActorMovement& state = m_State[a_Actor->formID];
				state.Owner = a_Actor->GetHandle();

				// Leaving is always allowed through. A stance that is only pending still has to be
				// escapable, or a prone entered from a crawl cannot be stood up out of until the crawl
				// ends on its own.
				if (state.Waiting() && !def.Exits) {
					return RequestResult::kAlreadyPending;
				}

				// Already in it. Sending the entry behaviour again does nothing but make the graph
				// refuse it, once per frame for as long as the key is held.
				if (!def.Exits && state.Current == slot.State->Id()) {
					return RequestResult::kAlreadyRunning;
				}

				// Already leaving. Retried rather than refused outright, in case the first send did not
				// take, but not once a frame.
				if (def.Exits && state.LeavingAt >= 0.0 && (now - state.LeavingAt) < PendingGrace) {
					return RequestResult::kAlreadyPending;
				}

				const EntryContext ctx(a_Actor, snapshot);

				if (!slot.State->CanEnter(ctx) || (def.Guard && !def.Guard(ctx))) {
					result = RequestResult::kGuardFailed;
					continue;
				}

				if (def.Verify && !def.Verify(ctx)) {
					result = RequestResult::kVerifyFailed;
					continue;
				}

				const std::string_view behaviour = def.Resolve ? def.Resolve(ctx) : def.Behaviour;

				if (!behaviour.empty() && !ActionRegistry::Notify(a_Actor, behaviour)) {
					return RequestResult::kGraphRefused;
				}

				if (def.OnTaken) {
					def.OnTaken(ctx);
				}

				if (def.Exits) {
					state.LeavingAt = now;
				}

				// Only a request that turns the stance on waits for a signature. One that turns it off
				// is done: the stance ends when the graph drops the variable, which Tick sees.
				if (!def.Exits && state.Current != slot.State->Id()) {
					state.Pending = slot.State->Id();
					state.PendingSince = now;
					state.EnteredVia = a_Action;
				}

				Log(std::format("MOVE_REQUEST|{:08X}|{}|{}", a_Actor->formID, MovementName(slot.State->Id()), a_Action));
				return RequestResult::kAccepted;
			}
		}

		return result;
	}

	bool MovementRegistry::CanPerform(RE::Actor* a_Actor, std::string_view a_Action) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Action.empty()) {
			return false;
		}

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);

		for (const auto& slot : m_Slots) {

			if (!slot.State) {
				continue;
			}

			for (const auto& def : slot.State->Entries()) {

				if (def.Action != a_Action) {
					continue;
				}

				const EntryContext ctx(a_Actor, snapshot, true);
				return slot.State->CanEnter(ctx) && (!def.Guard || def.Guard(ctx));
			}
		}

		return false;
	}

	bool MovementRegistry::Dispatch(RE::Actor* a_Actor, std::string_view a_Tag) {

		std::lock_guard lock(ActionLock);

		auto it = m_State.find(a_Actor->formID);

		// A stance is usually started by the graph rather than by a request: pressing sneak while
		// crawling is enabled puts the actor down without GTSBeh_Crawl_On ever being sent. Finding
		// that would otherwise need every loaded actor polled every frame, so the first annotation of
		// a stance whose signature already holds is what starts it instead.
		if (it == m_State.end() || !it->second.Active()) {

			const GraphSnapshot snapshot = GraphVars::Read(a_Actor);

			// Backwards, for the same reason the sweep is: the highest priority stance whose signature
			// holds is the one that owns the actor.
			for (auto slot = m_Slots.rbegin(); slot != m_Slots.rend(); ++slot) {

				if (!slot->State || slot->Signature.Empty() || !snapshot.Matches(slot->Signature)) {
					continue;
				}

				const bool claims = std::ranges::any_of(slot->State->Annotations(),
					[&](const AnnotationDef& a_Def) { return a_Def.Tag == a_Tag; });

				if (!claims) {
					continue;
				}

				ActorMovement& fresh = m_State[a_Actor->formID];
				fresh.Owner = a_Actor->GetHandle();
				Enter(a_Actor, fresh, *slot);

				it = m_State.find(a_Actor->formID);
				break;
			}
		}

		if (it == m_State.end() || !it->second.Active()) {
			return false;
		}

		auto* state = Find(it->second.Current);

		if (!state) {
			return false;
		}

		for (const auto& def : state->Annotations()) {

			if (def.Tag != a_Tag) {
				continue;
			}

			if (def.Handler) {
				const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
				ActorAction scratch = {};
				const ActionContext ctx(a_Actor, a_Actor->formID, scratch, snapshot);
				def.Handler(ctx);
			}

			Log(std::format("MOVE_ANNO|{:08X}|{}|{}", a_Actor->formID, MovementName(it->second.Current), a_Tag));
			return true;
		}

		return false;
	}

	void MovementRegistry::Tick(RE::Actor* a_Actor, float a_Delta) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return;
		}

		auto it = m_State.find(a_Actor->formID);
		const bool tracked = it != m_State.end();

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		const double now = Time::WorldTimeElapsed();

		// Checked before the active branch. A stance asked for while another one holds is a change of
		// stance, not a queue: prone is entered from a crawl and the crawl variable is still set for a
		// moment afterwards, so looking at the current one first means the request is never seen.
		if (tracked && it->second.Waiting()) {

			ActorMovement& state = it->second;
			const std::size_t index = IndexOf(state.Pending);

			// Only if it outranks what is running, or what is running has stopped. Turning crawl on
			// while prone asserts GTS_IsCrawling straight away, and the giant is still lying down: the
			// request is for the mode underneath the prone, not for a change of stance.
			const bool asserted = index < m_Slots.size() && m_Slots[index].State && snapshot.Matches(m_Slots[index].Signature);
			const bool outranks = std::to_underlying(state.Pending) >= std::to_underlying(state.Current);
			const bool currentOver = !state.Active() || !snapshot.Matches(state.Signature);

			if (asserted && (outranks || currentOver)) {

				// Leave clears both, so they are kept over the swap.
				const MovementId pending = state.Pending;
				const std::string via = state.EnteredVia;

				if (state.Active()) {
					Leave(a_Actor->formID, a_Actor, state, ExitReason::kCompleted);
				}

				state.Pending = pending;
				state.EnteredVia = via;

				Enter(a_Actor, state, m_Slots[index]);
				return;
			}

			// Outranked. The behaviour has already gone to the graph and the stance underneath is set;
			// the sweep picks it up when the one on top ends.
			if (asserted && !outranks) {
				state.Pending = MovementId::kNone;
				state.PendingSince = -1.0;
				state.EnteredVia.clear();
				return;
			}

			if (state.PendingSince >= 0.0 && (now - state.PendingSince) > PendingGrace) {
				Log(std::format("MOVE_LOST|{:08X}|{}|never asserted", a_Actor->formID, MovementName(state.Pending)));
				state.Pending = MovementId::kNone;
				state.PendingSince = -1.0;
				state.EnteredVia.clear();
			}
		}

		if (tracked && it->second.Active()) {

			ActorMovement& state = it->second;

			if (snapshot.Matches(state.Signature)) {
				state.MismatchSince = -1.0;
			}
			else if (state.MismatchSince < 0.0) {
				state.MismatchSince = now;
			}
			else if ((now - state.MismatchSince) >= MismatchGrace) {
				Leave(a_Actor->formID, a_Actor, state, ExitReason::kCompleted);
				return;
			}

			if (auto* current = Find(state.Current)) {
				ActorAction scratch = {};
				const ActionContext ctx(a_Actor, a_Actor->formID, scratch, snapshot);
				current->OnUpdate(ctx, a_Delta);
			}

			return;
		}

		// The sweep. A stance can start without anyone asking: the player crouches, another mod puts
		// the actor down. Whoever's signature holds owns the actor.
		//
		// Walked backwards because the signatures overlap. GTS_IsCrawling is crawl mode rather than
		// the crawl animation, so it stays set through a prone entered out of a crawl and both match
		// at once. The stances themselves are exclusive, so the highest priority match wins and the
		// rest are the mode underneath it.
		for (auto slot = m_Slots.rbegin(); slot != m_Slots.rend(); ++slot) {

			if (!slot->State || slot->Signature.Empty() || !snapshot.Matches(slot->Signature)) {
				continue;
			}

			ActorMovement& state = m_State[a_Actor->formID];
			state.Owner = a_Actor->GetHandle();
			Enter(a_Actor, state, *slot);
			return;
		}
	}

	void MovementRegistry::Tracked(absl::InlinedVector<RE::FormID, 16>& a_Out) {

		std::lock_guard lock(ActionLock);

		for (const auto& id : m_State | std::views::keys) {
			a_Out.push_back(id);
		}
	}

	std::pair<std::uint32_t, std::uint32_t> MovementRegistry::Traffic() {

		std::lock_guard lock(ActionLock);
		return { m_Enters, m_Exits };
	}

	void MovementRegistry::AppendAvailable(RE::Actor* a_Actor, std::vector<AvailableAction>& a_Out) {

		std::lock_guard lock(ActionLock);

		for (const auto& slot : m_Slots) {

			if (!slot.State) {
				continue;
			}

			for (const auto& def : slot.State->Entries()) {

				if (def.Input.empty()) {
					continue;
				}

				const InputDef* bind = KeybindHandler::Find(def.Input);

				a_Out.emplace_back(AvailableAction{
					.Action = def.Action,
					.Input = def.Input,
					.UIName = bind && !bind->UIName.empty() ? bind->UIName : def.Input,
					.UIDescription = bind ? bind->UIDescription : std::string_view{},
					.Icon = bind ? bind->Icon : std::string_view{},
					.Ready = CanPerform(a_Actor, def.Action),
				});
			}
		}
	}

	bool MovementRegistry::HandleVanillaInput(RE::Actor* a_Actor, std::string_view a_UserEvent) {

		std::lock_guard lock(ActionLock);

		for (const auto& slot : m_Slots) {

			if (!slot.State) {
				continue;
			}

			for (const auto& block : slot.State->Blocks()) {

				if (block.UserEvent == a_UserEvent && block.Handle && block.Handle(a_Actor)) {
					return true;
				}
			}
		}

		return false;
	}

	MovementId MovementRegistry::Current(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return MovementId::kNone;
		}

		auto it = m_State.find(a_Actor->formID);
		return it != m_State.end() ? it->second.Current : MovementId::kNone;
	}

	bool MovementRegistry::In(RE::Actor* a_Actor, MovementId a_Id) {

		std::lock_guard lock(ActionLock);
		return Current(a_Actor) == a_Id;
	}

	void MovementRegistry::Forget(RE::FormID a_Owner) {

		std::lock_guard lock(ActionLock);

		m_State.erase(a_Owner);

		for (const auto& slot : m_Slots) {
			if (slot.State) {
				slot.State->OnForget(a_Owner);
			}
		}
	}

	void MovementRegistry::Reset() {

		std::lock_guard lock(ActionLock);

		m_State.clear();
		m_Enters = 0;
		m_Exits = 0;

		for (const auto& slot : m_Slots) {
			if (slot.State) {
				slot.State->OnReset();
			}
		}
	}
}
