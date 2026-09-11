#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/InputScopes.hpp"
#include "Actions/Core/PlayerTarget.hpp"

#include "Config/Keybinds.hpp"
#include "Config/Util/KeybindHandler.hpp"

#include "Actions/Core/ActionCleanup.hpp"
#include "Actions/Core/ActionLock.hpp"
#include "Actions/Core/ActionLog.hpp"
#include "Actions/Core/ActionReactions.hpp"
#include "Actions/Core/MovementRegistry.hpp"
#include "Actions/Core/Possession.hpp"

#include "Actions/Reactions/FurnitureReactions.hpp"
#include "Actions/Reactions/ModSupportReactions.hpp"

#include "Actions/Nodes/Crush/BoobCrushNode.hpp"
#include "Actions/Nodes/Grab/GrabNode.hpp"
#include "Actions/Nodes/Grab/GrabPlayNode.hpp"
#include "Actions/Nodes/Grab/CleavageNode.hpp"
#include "Actions/Nodes/Crush/ButtCrushNode.hpp"
#include "Actions/Nodes/Hug/HugNode.hpp"
#include "Actions/Nodes/Calamity/CalamityNode.hpp"
#include "Actions/Nodes/Kick/KickSwipeNode.hpp"
#include "Actions/Nodes/Size/GrowthNode.hpp"
#include "Actions/Nodes/Size/ShrinkNode.hpp"
#include "Actions/Nodes/Stomp/StompNode.hpp"
#include "Actions/Nodes/Stomp/TrampleNode.hpp"

#include "Actions/Movement/CrawlMovement.hpp"
#include "Actions/Movement/ProneMovement.hpp"
#include "Actions/Nodes/ThighCrush/ThighCrushNode.hpp"
#include "Actions/Nodes/ThighSandwich/ThighSandwichButtNode.hpp"
#include "Actions/Nodes/ThighSandwich/ThighSandwichNode.hpp"
#include "Actions/Nodes/Vore/VoreNode.hpp"

#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Console/ConsoleManager.hpp"
#include "Managers/Input/InputManager.hpp"

namespace {

	using namespace GTS;
	using namespace GTS::Actions;

	std::size_t IndexOf(ActionId a_Id) {
		return std::to_underlying(a_Id);
	}

	// EnteredVia values the registry writes itself. Everything else is the name of the action that was
	// asked for.
	constexpr std::string_view kMachineVia[] = { "handoff", "restore", "resumed", "carry", "unrequested" };
}

namespace GTS::Actions {

	void ActionRegistry::Register(std::unique_ptr<IActionNode> a_Node) {

		std::lock_guard lock(ActionLock);

		if (!a_Node) {
			return;
		}

		const ActionId id = a_Node->Id();

		if (id == ActionId::kNone || id >= ActionId::kTotal) {
			ReportAndExit(std::format("Action node registered with an out of range id ({})", std::to_underlying(id)));
			return;
		}

		if (m_Slots[IndexOf(id)].Node) {
			ReportAndExit(std::format("Two action nodes both claim ActionId::{}", ActionName(id)));
			return;
		}

		m_Slots[IndexOf(id)].Node = a_Node.get();
		m_Owned.push_back(std::move(a_Node));
	}

	void ActionRegistry::AddDriver(std::unique_ptr<IActionDriver> a_Driver) {

		std::lock_guard lock(ActionLock);
		if (a_Driver) {
			m_Drivers.push_back(std::move(a_Driver));
		}
	}

	IActionNode* ActionRegistry::Find(ActionId a_Id) {

		std::lock_guard lock(ActionLock);
		return a_Id > ActionId::kNone && a_Id < ActionId::kTotal ? m_Slots[IndexOf(a_Id)].Node : nullptr;
	}

	IActionNode* ActionRegistry::EntryOwner(std::string_view a_Action) {

		std::lock_guard lock(ActionLock);

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			for (const auto& def : slot.Node->Entries()) {
				if (def.Action == a_Action) {
					return slot.Node;
				}
			}
		}

		return nullptr;
	}

	const ActorAction* ActionRegistry::StateOf(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return nullptr;
		}

		auto it = m_State.find(a_Actor->formID);
		return it == m_State.end() ? nullptr : &it->second;
	}

	void ActionRegistry::Log(std::string_view a_Line) {

		std::lock_guard lock(ActionLock);
		if (m_Watching) {
			ActionLog::Write(a_Line);
		}
	}

	std::string ActionRegistry::Describe(RE::FormID a_Owner, const ActorAction& a_State) {

		std::lock_guard lock(ActionLock);

		if (!a_State.Active()) {
			return a_State.Waiting() ? std::format("pending:{}", ActionName(a_State.Pending)) : "idle";
		}

		auto* node = Find(a_State.Current);
		if (!node) {
			return "?";
		}

		const std::string_view inner = node->StateName(a_Owner);
		return inner.empty() ? std::string(ActionName(a_State.Current)) : std::format("{}({})", ActionName(a_State.Current), inner);
	}

	bool ActionRegistry::Notify(RE::Actor* a_Actor, std::string_view a_Behaviour, bool a_Force) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Behaviour.empty()) {
			return false;
		}

		if (!a_Force && AnimationVars::General::IsTransitioning(a_Actor)) {
			Log(std::format("SEND|{:08X}|{}|refused, transitioning", a_Actor->formID, a_Behaviour));
			return false;
		}

		if (!a_Actor->NotifyAnimationGraph(a_Behaviour)) {
			if (!m_Live.contains(a_Behaviour) && m_Dead.emplace(a_Behaviour).second) {
				++m_Counters.DeadBehaviours;
				logger::warn("ActionRegistry: the graph has no behaviour named '{}'", a_Behaviour);
			}

			Log(std::format("SEND|{:08X}|{}|refused by the graph", a_Actor->formID, a_Behaviour));
			return false;
		}

		if (m_Live.emplace(a_Behaviour).second) {
			m_Dead.erase(std::string(a_Behaviour));
		}

		Log(std::format("SEND|{:08X}|{}|sent", a_Actor->formID, a_Behaviour));
		return true;
	}

	bool ActionRegistry::Followed(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		return m_Watching && a_Actor && (m_VarTarget == 0 || m_VarTarget == a_Actor->formID);
	}

	bool ActionRegistry::ClaimsAction(std::string_view a_Action) {

		std::lock_guard lock(ActionLock);

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			for (const auto& def : slot.Node->Entries()) {
				if (def.Action == a_Action) {
					return true;
				}
			}

			for (const auto& def : slot.Node->Actions()) {
				if (def.Action == a_Action) {
					return true;
				}
			}
		}

		return false;
	}

	void ActionRegistry::Enter(RE::Actor* a_Actor, ActorAction& a_State, const Slot& a_Slot) {

		std::lock_guard lock(ActionLock);

		const double now = Time::WorldTimeElapsed();
		std::string via = std::move(a_State.EnteredVia);

		const std::uint8_t entry = a_State.PendingEntry;

		a_State.Owner = a_Actor->GetHandle();
		a_State.Current = a_Slot.Node->Id();
		a_State.Pending = ActionId::kNone;
		a_State.Signature = entry < a_Slot.EntrySignatures.size() && !a_Slot.EntrySignatures[entry].Empty() ? a_Slot.EntrySignatures[entry] : a_Slot.Signature;
		a_State.HandOffTo = ActionId::kNone;
		a_State.HandOffAt = -1.0;
		a_State.JustLeft = ActionId::kNone;
		a_State.PartnersReleased = false;
		a_State.PartnersReleasedAt = -1.0;
		a_State.PartnerDied = false;
		a_State.DeathIntended = false;

		// A node the registry puts back never plays the animation that sends its Confirm.
		const bool handedBack = std::ranges::contains(kMachineVia, via);

		a_State.AwaitingConfirm = !handedBack && entry < a_Slot.Node->Entries().size() ? a_Slot.Node->Entries()[entry].Confirm : std::string_view{};
		a_State.AwaitingSince = now;
		a_State.EnteredAt = now;
		a_State.PendingSince = -1.0;
		a_State.ExitRequestedAt = -1.0;
		a_State.MismatchSince = -1.0;
		a_State.AnimSpeed = 1.0f;
		a_State.HHSpeed = 1.0f;
		a_State.CanEditAnimSpeed = false;
		a_State.HHDisabled = false;
		a_State.EnteredVia = std::move(via);
		a_State.Recent.clear();

		++m_Counters.Enters;

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		const ActionContext ctx(a_Actor, a_Actor->formID, a_State, snapshot);
		a_Slot.Node->OnEnter(ctx);

		Log(std::format("ENTER|{:08X}|{}|via={}", a_Actor->formID, ActionName(a_State.Current), a_State.EnteredVia.empty() ? "graph" : a_State.EnteredVia));
	}

	bool ActionRegistry::Resume(RE::Actor* a_Actor, ActionId a_Id, std::string_view a_Via) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Id == ActionId::kNone || a_Id >= ActionId::kTotal) {
			return false;
		}

		const Slot& slot = m_Slots[IndexOf(a_Id)];

		if (!slot.Node) {
			return false;
		}

		ActorAction& state = m_State[a_Actor->formID];

		if (!state.Idle()) {
			return false;
		}

		state.Owner = a_Actor->GetHandle();
		state.PendingEntry = 0;
		state.EnteredVia = a_Via;

		Enter(a_Actor, state, slot);
		return true;
	}

	double ActionRegistry::Grace(RE::Actor* a_Actor, double a_Seconds) {

		std::lock_guard lock(ActionLock);

		const double speed = std::clamp(static_cast<double>(AnimationManager::GetAnimSpeed(a_Actor)), 0.05, 4.0);
		return a_Seconds / speed;
	}

	void ActionRegistry::SendAbortSignal(const IActionNode* a_Node, RE::FormID a_Owner, RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (!a_Node || !a_Actor) {
			return;
		}

		const std::string_view signal = a_Node->AbortSignal();

		if (signal.empty()) {
			return;
		}

		for (const PossessionSlot slot : a_Node->OwnedSlots()) {

			for (const auto& handle : Possession::All(a_Owner, slot)) {

				auto ptr = handle.get();

				if (ptr && ptr->Is3DLoaded()) {
					Notify(ptr.get(), signal, true);
				}
			}
		}

		// Leave reaches here after the giant's 3d is gone.
		if (a_Actor->Is3DLoaded()) {
			Notify(a_Actor, signal, true);
		}
	}

	void ActionRegistry::Leave(RE::FormID a_Owner, RE::Actor* a_Actor, ActorAction& a_State, ExitReason a_Reason, std::uint32_t a_Offender) {

		std::lock_guard lock(ActionLock);

		if (!a_State.Active()) {
			return;
		}

		auto* node = Find(a_State.Current);
		const std::string path = Describe(a_Owner, a_State);
		const double now = Time::WorldTimeElapsed();

		if (a_Reason == ExitReason::kDesync) {

			++m_Counters.Desyncs;

			std::string recent;
			for (const auto& [tag, at] : a_State.Recent) {
				if (!recent.empty()) {
					recent += ",";
				}
				recent += std::format("{}@-{:.2f}s", tag, now - at);
			}

			Log(std::format("DESYNC|{:08X}|{}|{}|recent={}", a_Owner, path, GraphVars::Name(a_Offender), recent.empty() ? "none" : recent));
		}

		if (node && a_Reason != ExitReason::kCompleted && a_Reason != ExitReason::kSuspended) {
			SendAbortSignal(node, a_Owner, a_Actor);
		}

		// A handoff only survives a completed exit. After an abort there is no state left to enter the target.
		const bool orderly = a_Reason == ExitReason::kCompleted || a_Reason == ExitReason::kHandOff;
		const ActionId handOff = orderly ? a_State.HandOffTo : ActionId::kNone;
		const ExitReason reason = handOff != ActionId::kNone && Find(handOff) ? ExitReason::kHandOff : a_Reason;

		const ActionId left = a_State.Current;
		const ActionId resumes = a_State.Resumes;
		const RE::ActorHandle owner = a_State.Owner;

		if (node) {
			const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
			const ActionContext ctx(a_Actor, a_Owner, a_State, snapshot);
			node->OnExit(ctx, reason);
		}

		if (!KeepsState(reason)) {

			// Slots the resumed node owns are kept for it.
			auto* back = resumes != ActionId::kNone ? Find(resumes) : nullptr;
			Cleanup::ReleaseOwnedExcept(a_Owner, node ? node->OwnedSlots() : kAllSlots, back ? back->OwnedSlots() : std::span<const PossessionSlot>{});
			Cleanup::ResetCamera(a_Actor);

			// The kill intent ends with the node that set it, for this node's slots only.
			Possession::ClearIntent(a_Owner, node ? node->PartnerSlots() : kAllSlots);
		}

		a_State = ActorAction{};
		a_State.Owner = owner;
		a_State.JustLeft = left;
		a_State.IdleSince = now;

		++m_Counters.Exits;
		Log(std::format("EXIT|{:08X}|{}|{}", a_Owner, path, ExitReasonName(reason)));

		if (reason == ExitReason::kHandOff) {
			a_State.Pending = handOff;
			a_State.PendingSince = now;
			a_State.EnteredVia = "handoff";
			++m_Counters.HandOffs;
			Log(std::format("HANDOFF|{:08X}|{} -> {}", a_Owner, path, ActionName(handOff)));
			return;
		}

		// Stepping aside, not ending. The node that is taking over enters next, and the hold stays
		// exactly as it is until it gives the slot back.
		if (reason == ExitReason::kSuspended) {
			return;
		}

		// Whatever stepped aside for this one comes back, unless it has nothing left to run on. A grab
		// whose tiny died while the stomp played is over, and Alive is what says so.
		if (resumes != ActionId::kNone && a_Actor) {
			if (auto* back = Find(resumes); back && back->Alive(a_Owner, a_Actor) != Liveness::kOver) {
				Resume(a_Actor, resumes, "resumed");
				return;
			}
		}

		RestoreCarry(a_Owner, a_Actor);
	}

	// Puts the carry back on when a branch ends without handing over and someone alive is still carried.
	void ActionRegistry::RestoreCarry(RE::FormID a_Owner, RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		// No 3d, no graph to run on. OnActorLoad3D calls this again once it is back.
		if (!a_Actor || !a_Actor->Is3DLoaded() || IsActorLost(a_Actor)) {
			return;
		}

		// A body left in a slot is released by the branch that killed it. Restoring for one would loop.
		bool carrying = false;

		for (const PossessionSlot slot : kAllSlots) {
			if (IsCarriedSlot(slot) && Possession::FirstAlive(a_Owner, slot)) {
				carrying = true;
				break;
			}
		}

		if (!carrying) {
			return;
		}

		for (const auto& slot : m_Slots) {

			if (!slot.Node || !slot.Node->HoldsCarried()) {
				continue;
			}

			if (Resume(a_Actor, slot.Node->Id(), "carry")) {
				Log(std::format("CARRY|{:08X}|{} taken back over, still holding {}", a_Owner, ActionName(slot.Node->Id()), Possession::Describe(a_Owner)));
			}

			return;
		}
	}

	RequestResult ActionRegistry::Perform(RE::Actor* a_Actor, std::string_view a_Action) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Action.empty()) {
			return RequestResult::kUnknownAction;
		}

		if (const RequestResult movement = MovementRegistry::Perform(a_Actor, a_Action); movement != RequestResult::kUnknownAction) {
			return movement;
		}

		ActorAction& state = m_State[a_Actor->formID];
		state.Owner = a_Actor->GetHandle();

		if (state.Waiting()) {
			++m_Counters.Refusals;
			Log(std::format("REFUSED|{:08X}|{}|{}|believed={}", a_Actor->formID, a_Action,
				RequestResultName(RequestResult::kAlreadyPending), Describe(a_Actor->formID, state)));
			return RequestResult::kAlreadyPending;
		}

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		const double now = Time::WorldTimeElapsed();
		RequestResult result = RequestResult::kUnknownAction;

		if (state.Active()) {

			auto* node = Find(state.Current);

			if (node) {
				for (const auto& def : node->Actions()) {

					if (def.Action != a_Action) {
						continue;
					}

					const ActionContext ctx(a_Actor, a_Actor->formID, state, snapshot);

					{
						const Slot& owner = m_Slots[IndexOf(state.Current)];
						const std::size_t index = static_cast<std::size_t>(&def - node->Actions().data());

						if (index < owner.ActionAsserts.size() && !owner.ActionAsserts[index].Empty()
							&& snapshot.Matches(owner.ActionAsserts[index])) {
							result = RequestResult::kAlreadyRunning;
							continue;
						}
					}

					if (def.Cooldown > 0.0f) {
						const auto key = std::pair{a_Actor->formID, def.Action.data()};
						if (auto it = m_Cooldowns.find(key); it != m_Cooldowns.end() && (now - it->second) < def.Cooldown) {
							result = RequestResult::kOnCooldown;
							continue;
						}
					}

					if (def.Guard && !def.Guard(ctx)) {
						result = RequestResult::kGuardFailed;
						continue;
					}

					if (def.Verify) {

						const auto key = std::pair{a_Actor->formID, def.Action.data()};
						auto it = m_Refused.find(key);

						if (it != m_Refused.end() && (now - it->second) < VerifyGrace) {
							result = RequestResult::kVerifyFailed;
							continue;
						}

						if (!def.Verify(ctx)) {
							m_Refused[key] = now;
							result = RequestResult::kVerifyFailed;
							continue;
						}

						m_Refused.erase(key);
					}

					if (!def.Behaviour.empty() && !Notify(a_Actor, def.Behaviour)) {
						++m_Counters.Refusals;
						Log(std::format("REFUSED|{:08X}|{}|{}|believed={}", a_Actor->formID, a_Action,
							RequestResultName(RequestResult::kGraphRefused), Describe(a_Actor->formID, state)));
						return RequestResult::kGraphRefused;
					}

					if (def.Cooldown > 0.0f) {
						m_Cooldowns[std::pair{a_Actor->formID, def.Action.data()}] = now;
					}

					state.ExitRequestedAt = -1.0;

					// An action with a Confirm fills its slot before the animation plays, so it is watched like an entry.
					if (!def.Confirm.empty()) {
						state.AwaitingConfirm = def.Confirm;
						state.AwaitingSince = now;
					}

					if (def.Kills) {
						Possession::Intend(a_Actor->formID, state.Current, def.Action);
					}
					else {
						Possession::ClearIntent(a_Actor->formID);
					}

					if (!def.Input.empty()) {
						state.SpeedInput = def.Input;
					}

					if (def.OnTaken) {
						def.OnTaken(ctx);
					}

					if (def.HandOff != ActionId::kNone) {
						ctx.HandOff(def.HandOff);
					}
					else if (def.Aborts) {
						Log(std::format("TAKE|{:08X}|{}|{}", a_Actor->formID, ActionName(state.Current), a_Action));
						Leave(a_Actor->formID, a_Actor, state, ExitReason::kAborted, 0);
						return RequestResult::kAccepted;
					}
					else if (def.Exits) {
						ctx.RequestExit();
					}

					Log(std::format("TAKE|{:08X}|{}|{}", a_Actor->formID, ActionName(state.Current), a_Action));
					return RequestResult::kAccepted;
				}
			}

			// Not one of this node's actions, but a node it lets run anyway. The grab steps aside, the
			// stomp starts from idle as it normally would, and the grab comes back when it ends. The
			// hold is untouched throughout: it belongs to the store, not to the node.
			if (result == RequestResult::kUnknownAction && node) {

				if (auto* wanted = EntryOwner(a_Action); wanted && node->Permits(wanted->Id(), a_Actor->formID)) {

					const ActionId suspended = state.Current;

					Leave(a_Actor->formID, a_Actor, state, ExitReason::kSuspended, GraphVars::kNoBit);

					const RequestResult taken = Perform(a_Actor, a_Action);

					if (taken == RequestResult::kAccepted) {
						m_State[a_Actor->formID].Resumes = suspended;
					}
					else {
						Resume(a_Actor, suspended, "resumed");
					}

					return taken;
				}
			}

			if (result == RequestResult::kUnknownAction && ClaimsAction(a_Action)) {
				result = RequestResult::kNotCurrent;
			}
		}
		else {

			for (const auto& slot : m_Slots) {

				if (!slot.Node) {
					continue;
				}

				for (const auto& def : slot.Node->Entries()) {

					if (def.Action != a_Action) {
						continue;
					}

					const EntryContext ctx(a_Actor, snapshot);

					if (std::ranges::any_of(def.BlockedBy, [&](PossessionSlot a_Slot) { return ctx.Occupied(a_Slot); })) {
						result = RequestResult::kGuardFailed;
						continue;
					}

					if (!slot.Node->CanEnter(ctx) || (def.Guard && !def.Guard(ctx))) {
						result = RequestResult::kGuardFailed;
						continue;
					}

					if (def.Verify) {

						const auto key = std::pair{a_Actor->formID, def.Action.data()};
						auto it = m_Refused.find(key);

						if (it != m_Refused.end() && (now - it->second) < VerifyGrace) {
							result = RequestResult::kVerifyFailed;
							continue;
						}

						if (!def.Verify(ctx)) {
							m_Refused[key] = now;
							result = RequestResult::kVerifyFailed;
							continue;
						}

						m_Refused.erase(key);
					}

					const std::string_view behaviour = def.Resolve ? def.Resolve(ctx) : def.Behaviour;

					if (def.Resolve && behaviour.empty()) {
						result = RequestResult::kGuardFailed;
						continue;
					}

					if (!behaviour.empty() && !Notify(a_Actor, behaviour)) {
						++m_Counters.Refusals;
						Log(std::format("REFUSED|{:08X}|{}|{}|believed={}", a_Actor->formID, a_Action,
							RequestResultName(RequestResult::kGraphRefused), Describe(a_Actor->formID, state)));
						return RequestResult::kGraphRefused;
					}

					if (def.OnTaken) {
						def.OnTaken(ctx);
					}

					state.Pending = slot.Node->Id();
					state.PendingEntry = static_cast<std::uint8_t>(&def - slot.Node->Entries().data());
					state.PendingSince = now;
					state.EnteredVia = a_Action;
					state.SpeedInput = def.Input;

					Log(std::format("REQUEST|{:08X}|{}|{}", a_Actor->formID, ActionName(state.Pending), a_Action));
					return RequestResult::kAccepted;
				}
			}
		}

		++m_Counters.Refusals;
		Log(std::format("REFUSED|{:08X}|{}|{}|believed={}", a_Actor->formID, a_Action, RequestResultName(result), Describe(a_Actor->formID, state)));
		return result;
	}

	void ActionRegistry::Abort(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return;
		}

		if (auto it = m_State.find(a_Actor->formID); it != m_State.end()) {
			Leave(a_Actor->formID, a_Actor, it->second, ExitReason::kAborted, GraphVars::kNoBit);
		}
	}

	void ActionRegistry::OnAnnotation(RE::Actor* a_Actor, std::string_view a_Tag) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Tag.empty()) {
			return;
		}

		auto it = m_State.find(a_Actor->formID);

		if (it != m_State.end() && it->second.Waiting() && !it->second.Active()) {

			const Slot& slot = m_Slots[IndexOf(it->second.Pending)];

			if (slot.Node && slot.Signature.Empty() == false) {

				for (const auto& def : slot.Node->Annotations()) {

					if (def.Tag != a_Tag || def.Shared) {
						continue;
					}

					Log(std::format("CONFIRM_ANNO|{:08X}|{}|{} arrived with no signature", a_Actor->formID, ActionName(it->second.Pending), a_Tag));
					Enter(a_Actor, it->second, slot);
					break;
				}
			}
		}

		const bool active = it != m_State.end() && it->second.Active();

		if (Followed(a_Actor)) {
			ActionLog::Write(std::format("RAW|{:08X}|{}|{}", a_Actor->formID, a_Tag,
				active ? Describe(a_Actor->formID, it->second) : "idle"));
		}

		if (Reactions::Dispatch(a_Actor, a_Tag) && Followed(a_Actor)) {
			ActionLog::Write(std::format("REACT|{:08X}|{}|{}", a_Actor->formID, a_Tag, Reactions::GroupOf(a_Tag)));
		}

		MovementRegistry::Dispatch(a_Actor, a_Tag);

		if (!active) {
			OnPartnerAnnotation(a_Actor, a_Tag);
			return;
		}

		ActorAction& state = it->second;
		auto* node = Find(state.Current);
		if (!node) {
			return;
		}

		const double now = Time::WorldTimeElapsed();

		{
			state.Recent.emplace_back(std::string(a_Tag), now);
			if (state.Recent.size() > 4) {
				state.Recent.erase(state.Recent.begin());
			}
		}

		for (const auto& signal : node->ExitSignals()) {
			if (signal == a_Tag && state.ExitRequestedAt < 0.0) {
				state.ExitRequestedAt = now;
				Log(std::format("EXIT_REQ|{:08X}|{}|{}", a_Actor->formID, ActionName(state.Current), a_Tag));
			}
		}

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		const ActionContext ctx(a_Actor, a_Actor->formID, state, snapshot);

		for (const auto& def : node->Annotations()) {

			if (def.Tag != a_Tag) {
				continue;
			}

			if (state.AwaitingConfirm == a_Tag) {
				state.AwaitingConfirm = {};
				state.AwaitingSince = -1.0;
				Log(std::format("CONFIRM|{:08X}|{}|{}", a_Actor->formID, ActionName(state.Current), a_Tag));
			}

			if (def.Handler) {
				def.Handler(ctx);
			}

			if (def.ReleasesPartners) {
				state.PartnersReleased = true;
				state.PartnersReleasedAt = now;
				Cleanup::ReleaseOwned(a_Actor->formID, node->PartnerSlots());
			}

			const bool endsHere = def.EndsIfPartnerGone || (state.PartnerDied && !state.DeathIntended);

			if (endsHere && !Possession::AnyIn(a_Actor->formID, node->PartnerSlots())) {

				Log(std::format("PARTNER_GONE|{:08X}|{}|{} ends the branch{}", a_Actor->formID, Describe(a_Actor->formID, state), a_Tag, state.PartnerDied ? ", partner died" : ""));
				SendAbortSignal(node, a_Actor->formID, a_Actor);

				if (state.ExitRequestedAt < 0.0) {
					state.ExitRequestedAt = now;
				}
			}

			Log(std::format("ANNO|{:08X}|{}|{}|{}", a_Actor->formID, ActionName(state.Current), a_Tag, "handled"));
			return;
		}

		if (m_Claimed.contains(a_Tag)) {
			++m_Counters.OrphanAnnotations;
			Log(std::format("ORPHAN_ANNO|{:08X}|{}|active={}", a_Actor->formID, a_Tag, Describe(a_Actor->formID, state)));
		}
	}

	void ActionRegistry::OnPartnerAnnotation(RE::Actor* a_Partner, std::string_view a_Tag) {

		std::lock_guard lock(ActionLock);

		const RE::FormID holder = Possession::HolderOf(a_Partner->formID);
		if (!holder) {
			return;
		}

		auto it = m_State.find(holder);
		if (it == m_State.end() || !it->second.Active()) {
			return;
		}

		auto* node = Find(it->second.Current);
		if (!node) {
			return;
		}

		auto owner = it->second.Owner.get();
		RE::Actor* giant = owner ? owner.get() : nullptr;

		if (!giant) {
			return;
		}

		const GraphSnapshot snapshot = GraphVars::Read(giant);
		const ActionContext ctx(giant, holder, it->second, snapshot, false, a_Partner);

		for (const auto& def : node->PartnerAnnotations()) {

			if (def.Tag != a_Tag) {
				continue;
			}

			if (def.Handler) {
				def.Handler(ctx);
			}

			if (def.ReleasesPartners) {
				it->second.PartnersReleased = true;
				Cleanup::ReleaseOwned(holder, node->PartnerSlots());
			}

			Log(std::format("PARTNER|{:08X}|{}|{}|from {:08X}", holder, ActionName(it->second.Current), a_Tag, a_Partner->formID));
			return;
		}
	}

	void ActionRegistry::OnLegacyTrigger(RE::Actor* a_Actor, std::string_view a_Trigger, std::string_view a_Detail, bool a_Sent) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return;
		}

		if (Followed(a_Actor)) {
			ActionLog::Write(std::format("LEGACY|{:08X}|{}|{}|{}", a_Actor->formID, a_Trigger, a_Detail, a_Sent ? "sent" : "refused"));
		}

		if (!a_Sent || a_Detail.empty()) {
			return;
		}

		const std::string_view a_Behaviour = a_Detail;

		auto existing = m_State.find(a_Actor->formID);
		const bool active = existing != m_State.end() && existing->second.Active();

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			for (const auto& def : slot.Node->Entries()) {

				if (def.Behaviour != a_Behaviour || active) {
					continue;
				}

				ActorAction& state = m_State[a_Actor->formID];
				state.Owner = a_Actor->GetHandle();
				state.Pending = slot.Node->Id();
				state.PendingSince = Time::WorldTimeElapsed();
				state.EnteredVia = def.Action.empty() ? std::string(a_Behaviour) : std::string(def.Action);
				return;
			}

			for (const auto& def : slot.Node->Actions()) {

				if (def.Behaviour != a_Behaviour) {
					continue;
				}

				if (!active || existing->second.Current != slot.Node->Id()) {
					++m_Counters.OrphanTriggers;
					Log(std::format("ORPHAN_TRIG|{:08X}|{}|wants={}|believed={}", a_Actor->formID, a_Behaviour,
						ActionName(slot.Node->Id()),
						active ? Describe(a_Actor->formID, existing->second) : "idle"));
				}

				return;
			}
		}
	}

	void ActionRegistry::Tick(RE::Actor* a_Actor, ActorAction& a_State, float a_Delta) {

		std::lock_guard lock(ActionLock);

		const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
		const double now = Time::WorldTimeElapsed();

		if (a_State.Active()) {

			const Slot& slot = m_Slots[IndexOf(a_State.Current)];
			if (!slot.Node) {
				a_State = ActorAction{};
				return;
			}

			const bool draining = a_State.ExitRequestedAt >= 0.0 && (now - a_State.ExitRequestedAt) <= Grace(a_Actor, DrainGrace);

			// Every frame, since a slot can hold more than one actor. Only the state below latches once.
			if (const DeathOutcome death = Possession::DropDead(a_Actor->formID, slot.Node->PartnerSlots()); death != DeathOutcome::kNone && !a_State.PartnersReleased) {

				a_State.PartnersReleased = true;
				a_State.PartnersReleasedAt = now;
				a_State.PartnerDied = true;
				a_State.DeathIntended = death == DeathOutcome::kIntended;

				Log(std::format("PARTNER_DIED|{:08X}|{}|{}, waiting for the animation to finish", a_Actor->formID,
					Describe(a_Actor->formID, a_State), DeathOutcomeName(death)));
			}

			const bool handingOff = a_State.HandOffTo != ActionId::kNone;

			if (handingOff) {

				if (a_State.HandOffAt < 0.0) {
					a_State.HandOffAt = now;
				}

				const Slot& target = m_Slots[IndexOf(a_State.HandOffTo)];

				if (target.Node && !target.Signature.Empty() && snapshot.Matches(target.Signature)) {
					Log(std::format("HANDOFF|{:08X}|{}|{} asserted", a_Actor->formID, Describe(a_Actor->formID, a_State), ActionName(a_State.HandOffTo)));
					Leave(a_Actor->formID, a_Actor, a_State, ExitReason::kCompleted, 0);
					return;
				}

				if ((now - a_State.HandOffAt) > Grace(a_Actor, PendingGrace)) {
					Log(std::format("LOST|{:08X}|{}|{} never asserted, handoff dropped", a_Actor->formID, Describe(a_Actor->formID, a_State), ActionName(a_State.HandOffTo)));
					a_State.HandOffTo = ActionId::kNone;
					a_State.HandOffAt = -1.0;
				}
			}

			const double finishWindow = Grace(a_Actor, a_State.PartnerDied && !a_State.DeathIntended ? DeathGrace : DrainGrace);

			const bool finishing = a_State.PartnersReleased && !draining && !handingOff
				&& a_State.PartnersReleasedAt >= 0.0 && (now - a_State.PartnersReleasedAt) <= finishWindow;

			if (a_State.PartnerDied && !finishing && !draining && !handingOff) {
				Log(std::format("LOST|{:08X}|{}|the partner died and the animation never ended", a_Actor->formID, Describe(a_Actor->formID, a_State)));
				Leave(a_Actor->formID, a_Actor, a_State, ExitReason::kActorLost, 0);
				return;
			}

			const Liveness liveness = slot.Node->Alive(a_Actor->formID, a_Actor);

			if (liveness == Liveness::kOver && !finishing) {
				Log(std::format("OVER|{:08X}|{}|the node says it is done", a_Actor->formID, Describe(a_Actor->formID, a_State)));
				Leave(a_Actor->formID, a_Actor, a_State, draining || handingOff ? ExitReason::kCompleted : ExitReason::kDesync, 0);
				return;
			}

			const bool leaving = draining || handingOff;

			if (a_State.Signature.Empty() || ((liveness == Liveness::kAlive || finishing) && !leaving) || snapshot.Matches(a_State.Signature)) {
				a_State.MismatchSince = -1.0;
			}
			else if (a_State.MismatchSince < 0.0) {
				a_State.MismatchSince = now;
			}
			else if ((now - a_State.MismatchSince) >= MismatchGrace) {
				const bool expected = draining || slot.Node->Exit() == ExitPolicy::kUnannounced;
				Leave(a_Actor->formID, a_Actor, a_State, expected ? ExitReason::kCompleted : ExitReason::kDesync, snapshot.FirstMismatch(a_State.Signature));
				return;
			}

			if (!a_State.AwaitingConfirm.empty() && (now - a_State.AwaitingSince) > Grace(a_Actor, ConfirmGrace)) {
				Log(std::format("LOST|{:08X}|{}|{} never arrived", a_Actor->formID, Describe(a_Actor->formID, a_State), a_State.AwaitingConfirm));
				Leave(a_Actor->formID, a_Actor, a_State, ExitReason::kUnconfirmed, 0);
				return;
			}

			for (const PossessionSlot required : slot.Node->RequiredSlots()) {

				if (a_State.PartnersReleased) {
					break;
				}

				if (!Possession::Occupied(a_Actor->formID, required)) {
					Log(std::format("LOST|{:08X}|{}|the {} slot is empty", a_Actor->formID, Describe(a_Actor->formID, a_State), SlotName(required)));
					Leave(a_Actor->formID, a_Actor, a_State, ExitReason::kActorLost, 0);
					return;
				}
			}

			DriveAnimSpeed(a_Actor, a_State, a_Delta);

			const ActionContext ctx(a_Actor, a_Actor->formID, a_State, snapshot, true);
			slot.Node->OnUpdate(ctx, a_Delta);

			return;
		}

		if (a_State.Waiting()) {

			const Slot& slot = m_Slots[IndexOf(a_State.Pending)];

			const GraphSignature& wanted = a_State.PendingEntry < slot.EntrySignatures.size() && !slot.EntrySignatures[a_State.PendingEntry].Empty()
				? slot.EntrySignatures[a_State.PendingEntry]
				: slot.Signature;

			const bool handedOver = a_State.EnteredVia == "handoff";

			if (slot.Node && ((!wanted.Empty() && snapshot.Matches(wanted))
				|| (handedOver && slot.Node->Alive(a_Actor->formID, a_Actor) == Liveness::kAlive))) {
				Enter(a_Actor, a_State, slot);
				return;
			}

			if (a_State.PendingSince >= 0.0 && (now - a_State.PendingSince) > Grace(a_Actor, PendingGrace)) {

				Log(std::format("LOST|{:08X}|{}|the graph never asserted the signature", a_Actor->formID, ActionName(a_State.Pending)));
				a_State.Pending = ActionId::kNone;
				a_State.PendingSince = -1.0;
				a_State.EnteredVia.clear();

				// The node that handed over kept its actors for the one that never arrived.
				RestoreCarry(a_Actor->formID, a_Actor);
			}

			return;
		}

		if (Possession::IsHeld(a_Actor->formID)) {
			return;
		}

		if (a_Actor->IsDead() || GetAV(a_Actor, RE::ActorValue::kHealth) <= 0.0f) {
			return;
		}

		if (a_State.JustLeft != ActionId::kNone) {

			const Slot& previous = m_Slots[IndexOf(a_State.JustLeft)];

			if (!previous.Node || previous.Signature.Empty() || !snapshot.Matches(previous.Signature)) {
				a_State.JustLeft = ActionId::kNone;
			}
		}

		for (const auto& slot : m_Slots) {

			if (!slot.Node || slot.Signature.Empty() || !snapshot.Matches(slot.Signature)) {
				continue;
			}

			if (slot.Node->Id() == a_State.JustLeft) {
				continue;
			}

			a_State.PendingEntry = 0;
			a_State.EnteredVia = "unrequested";
			Enter(a_Actor, a_State, slot);
			return;
		}
	}

	void ActionRegistry::OnMainUpdate() {

		std::lock_guard lock(ActionLock);

		++m_Frame;
		ActionLog::SetFrame(m_Frame);

		const float delta = Time::WorldTimeDelta();

		if (m_Watching) {
			for (auto* actor : find_actors()) {
				if (actor && actor->Is3DLoaded()) {
					auto& state = m_State[actor->formID];
					if (!state.Owner) {
						state.Owner = actor->GetHandle();
					}
				}
			}
		}

		absl::InlinedVector<RE::FormID, 16> keys;
		keys.reserve(m_State.size());

		for (const auto& id : m_State | std::views::keys) {
			keys.push_back(id);
		}

		{
			absl::InlinedVector<RE::FormID, 16> stances;
			MovementRegistry::Tracked(stances);

			for (const RE::FormID id : stances) {
				if (!m_State.contains(id)) {
					keys.push_back(id);
				}
			}
		}

		RE::FormID swept = 0;

		if (auto* controlled = GetPlayerOrControlled(); controlled && controlled->Is3DLoaded()) {
			swept = controlled->formID;
			MovementRegistry::Tick(controlled, delta);
		}

		absl::InlinedVector<std::pair<RE::FormID, bool>, 8> drop;

		for (const RE::FormID id : keys) {

			auto it = m_State.find(id);

			if (it == m_State.end()) {

				auto* held = RE::TESForm::LookupByID<RE::Actor>(id);

				if (held && held->Is3DLoaded()) {
					if (id != swept) {
						MovementRegistry::Tick(held, delta);
					}
				}
				else {
					MovementRegistry::Forget(id);
				}

				continue;
			}

			auto ptr = it->second.Owner.get();
			RE::Actor* actor = ptr ? ptr.get() : nullptr;

			if (!actor || !actor->Is3DLoaded()) {

				if (it->second.Active()) {
					Leave(id, actor, it->second, ExitReason::kActorLost, GraphVars::kNoBit);
				}

				drop.emplace_back(id, true);
				continue;
			}

			if (id != swept) {
				MovementRegistry::Tick(actor, delta);
			}

			Tick(actor, it->second, delta);

			if (auto after = m_State.find(id); after != m_State.end()) {

				for (const auto& driver : m_Drivers) {
					driver->Poll(actor, after->second.Current);
				}

				// Kept for SettleGrace after going idle, so the idle branch can adopt a signature the graph still asserts.
				const bool settled = after->second.IdleSince < 0.0 || (Time::WorldTimeElapsed() - after->second.IdleSince) > SettleGrace;

				if (after->second.Idle() && settled && !m_Watching && MovementRegistry::Current(actor) == MovementId::kNone) {
					drop.emplace_back(id, false);
				}
			}
		}

		for (const auto& [id, forget] : drop) {

			m_State.erase(id);

			if (forget) {
				Forget(id);
			}
		}

		if (m_Watching) {
			PollVars();
		}
	}

	bool ActionRegistry::CanPerform(RE::Actor* a_Actor, std::string_view a_Action) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_Action.empty()) {
			return false;
		}

		const auto* state = StateOf(a_Actor);

		if (state && state->Active()) {

			auto* node = Find(state->Current);
			if (!node) {
				return false;
			}

			for (const auto& def : node->Actions()) {

				if (def.Action != a_Action) {
					continue;
				}

				if (!def.Guard) {
					return true;
				}

				const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
				ActorAction probe = *state;
				const ActionContext ctx(a_Actor, a_Actor->formID, probe, snapshot, true);

				return def.Guard(ctx);
			}

			auto* wanted = EntryOwner(a_Action);

			if (!wanted || !node->Permits(wanted->Id(), a_Actor->formID)) {
				return false;
			}
		}

		if (state && state->Waiting()) {
			return false;
		}

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			for (const auto& def : slot.Node->Entries()) {

				if (def.Action != a_Action) {
					continue;
				}

				const GraphSnapshot snapshot = GraphVars::Read(a_Actor);
				const EntryContext ctx(a_Actor, snapshot, true);

				return slot.Node->CanEnter(ctx) && (!def.Guard || def.Guard(ctx));
			}
		}

		return false;
	}

	void ActionRegistry::ValidateInputs() {

		std::lock_guard lock(ActionLock);

		absl::flat_hash_map<std::pair<std::string_view, ActionId>, std::string_view> claimed;

		auto check = [&](std::string_view a_Input, std::string_view a_Action, ActionId a_Scope, const IActionNode* a_Node) {

			if (a_Input.empty()) {
				return;
			}

			auto [it, fresh] = claimed.try_emplace(std::pair(a_Input, a_Scope), a_Action);

			if (!fresh && it->second != a_Action) {
				ReportAndExit(std::format("Input '{}' is bound to '{}' and to '{}' in the same scope ({}).",
					a_Input, it->second, a_Action, ActionName(a_Node->Id())));
			}
		};

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			for (const auto& def : slot.Node->Entries()) {
				check(def.Input, def.Action, ActionId::kNone, slot.Node);
			}

			for (const auto& def : slot.Node->Actions()) {
				check(def.Input, def.Action, slot.Node->Id(), slot.Node);
			}
		}
	}

	void ActionRegistry::BindInput() {

		std::lock_guard lock(ActionLock);

		ValidateInputs();

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			auto bind = [](std::string_view a_Input, std::string_view a_Action, const InputScope& a_Scope) {

				if (a_Input.empty()) {
					return;
				}

				const bool entry = a_Scope.Rank == ScopeRank::kRoot;

				InputManager::RegisterInputEvent(a_Input,
					[a_Input, a_Action, entry](const ManagedInputEvent&) {

						if (entry && PlayerTarget::Engaged(a_Input)) {
							PlayerTarget::Perform(a_Action);
							return;
						}

						Perform(GetPlayerOrControlled(), a_Action);
					},
					[a_Input, a_Action, entry] {

						if (entry && PlayerTarget::Engaged(a_Input)) {
							return PlayerTarget::CanPerform(a_Action);
						}

						return CanPerform(GetPlayerOrControlled(), a_Action);
					},
					a_Scope
				);
			};

			for (const auto& def : slot.Node->Entries()) {
				Keybinds::NoteBindKind(def.Input, true);
				bind(def.Input, def.Action, EntryScope(slot.Node->Id()));
			}

			for (const auto& def : slot.Node->Actions()) {
				Keybinds::NoteBindKind(def.Input, false);
				bind(def.Input, def.Action, NodeScope(slot.Node->Id()));
			}

			slot.Node->RegisterInput();
		}
	}

	std::vector<AvailableAction> ActionRegistry::Available(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		std::vector<AvailableAction> out;

		if (!a_Actor) {
			return out;
		}

		const auto add = [&](std::string_view a_Input, std::string_view a_Action) {

			if (a_Input.empty()) {
				return;
			}

			const InputDef* def = KeybindHandler::Find(a_Input);

			out.emplace_back(AvailableAction{
				.Action = a_Action,
				.Input = a_Input,
				.UIName = def && !def->UIName.empty() ? def->UIName : a_Input,
				.UIDescription = def ? def->UIDescription : std::string_view{},
				.Icon = def ? def->Icon : std::string_view{},
				.Ready = CanPerform(a_Actor, a_Action),
			});
		};

		const IActionNode* node = Find(Current(a_Actor));

		if (node) {
			for (const auto& def : node->Actions()) {
				add(def.Input, def.Action);
			}
		}

		const bool busy = Busy(a_Actor);

		for (const auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			// Idle offers every entry. While a node is running, the ones it steps aside for as well,
			// which is the same set Perform accepts.
			if (busy && !(node && node->Permits(slot.Node->Id(), a_Actor->formID))) {
				continue;
			}

			for (const auto& def : slot.Node->Entries()) {
				add(def.Input, def.Action);
			}
		}

		if (!busy) {
			MovementRegistry::AppendAvailable(a_Actor, out);
		}

		return out;
	}

	bool ActionRegistry::HandleVanillaInput(RE::Actor* a_Actor, std::string_view a_UserEvent) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || a_UserEvent.empty()) {
			return false;
		}

		const auto offer = [&](std::span<const VanillaBlock> a_Blocks) {

			for (const auto& block : a_Blocks) {

				if (block.UserEvent != a_UserEvent || !block.Handle) {
					continue;
				}

				if (block.Handle(a_Actor)) {
					return true;
				}
			}

			return false;
		};

		for (const auto& slot : m_Slots) {

			if (slot.Node && offer(slot.Node->Blocks())) {
				return true;
			}
		}

		return MovementRegistry::HandleVanillaInput(a_Actor, a_UserEvent);
	}

	ActionId ActionRegistry::Current(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		const auto* state = StateOf(a_Actor);
		return state ? state->Current : ActionId::kNone;
	}

	bool ActionRegistry::Busy(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		const auto* state = StateOf(a_Actor);
		return state && !state->Idle();
	}

	float ActionRegistry::AnimSpeed(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		const auto* state = StateOf(a_Actor);
		return state && state->Active() ? state->AnimSpeed : 1.0f;
	}

	float ActionRegistry::HHSpeed(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		const auto* state = StateOf(a_Actor);
		return state && state->Active() ? state->HHSpeed : 1.0f;
	}

	bool ActionRegistry::HHDisabled(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		const auto* state = StateOf(a_Actor);
		return state && state->Active() && state->HHDisabled;
	}

	void ActionRegistry::DriveAnimSpeed(RE::Actor* a_Actor, ActorAction& a_State, float a_Delta) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor || !a_State.CanEditAnimSpeed) {
			return;
		}

		if (a_Actor != GetPlayerOrControlled()) {
			return;
		}

		const bool strangling = AnimationVars::Cleavage::IsBoobsDoting(a_Actor);
		const float min = strangling ? 0.50f : 0.33f;
		const float max = strangling ? 1.75f : 3.0f;

		if (!a_State.SpeedInput.empty() && InputManager::BindHeld(a_State.SpeedInput)) {

			const bool slower = InputManager::ModifierHeldFor(Keybinds::SlowDownBind, a_State.SpeedInput);
			const float rate = (slower ? -RampPerSecond : RampPerSecond) * a_Delta;

			AdjustAnimSpeed(a_Actor, rate * GetAnimationSlowdown(a_Actor), min, max);
			SyncHeldAnimSpeed(a_Actor);
			return;
		}

		// Back to what the node authored, not to 1.0 - that would undo its own 1.35 or 1.66.
		const float target = std::clamp(a_State.SpeedBase, min, max);

		if (a_State.AnimSpeed != target) {

			const float step = DecayPerSecond * a_Delta;

			a_State.AnimSpeed = a_State.AnimSpeed > target ? std::max(target, a_State.AnimSpeed - step)
			                                               : std::min(target, a_State.AnimSpeed + step);
			SyncHeldAnimSpeed(a_Actor);
		}
	}

	void ActionRegistry::SyncHeldAnimSpeed(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		const float speed = AnimationManager::GetAnimSpeed(a_Actor);

		for (std::size_t i = 0; i < kSlotCount; ++i) {

			for (const auto& handle : Possession::All(a_Actor->formID, static_cast<PossessionSlot>(i))) {

				RE::Actor* tiny = handle.get().get();

				if (!tiny || tiny == a_Actor) {
					continue;
				}

				if (auto* transient = Transient::GetActorData(tiny)) {
					transient->HugAnimationSpeed = speed;
				}
			}
		}
	}

	void ActionRegistry::AdjustAnimSpeed(RE::Actor* a_Actor, float a_Bonus, float a_Min, float a_Max) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return;
		}

		auto it = m_State.find(a_Actor->formID);
		if (it == m_State.end() || !it->second.Active() || !it->second.CanEditAnimSpeed) {
			return;
		}

		it->second.AnimSpeed = std::clamp(it->second.AnimSpeed + a_Bonus, a_Min, a_Max);
	}

	void ActionRegistry::Validate() {

		std::lock_guard lock(ActionLock);

		std::size_t registered = 0;

		for (auto& slot : m_Slots) {

			if (!slot.Node) {
				continue;
			}

			++registered;
			slot.Signature = GraphVars::Compile(slot.Node->Signature());

			for (const auto& def : slot.Node->Entries()) {
				slot.EntrySignatures.push_back(def.Signature.empty() ? GraphSignature{} : GraphVars::Compile(def.Signature));
			}

			for (const auto& def : slot.Node->Actions()) {
				slot.ActionAsserts.push_back(def.Asserts.empty() ? GraphSignature{} : GraphVars::Compile(def.Asserts));
			}

			if (slot.Signature.Empty()) {
				logger::warn("ActionRegistry: {} declares no signature and can never be entered or exited", ActionName(slot.Node->Id()));
			}

			{
				absl::flat_hash_set<std::string_view> seen;
				for (const auto& def : slot.Node->Annotations()) {

					if (!seen.emplace(def.Tag).second) {
						ReportAndExit(std::format("Action node {} registers annotation '{}' twice", ActionName(slot.Node->Id()), def.Tag));
					}

					if (!def.Shared) {
						m_Claimed.emplace(def.Tag);
					}
				}

				for (const auto& def : slot.Node->PartnerAnnotations()) {

					if (!seen.emplace(def.Tag).second) {
						ReportAndExit(std::format("Action node {} registers annotation '{}' twice", ActionName(slot.Node->Id()), def.Tag));
					}

					if (!def.Shared) {
						m_Claimed.emplace(def.Tag);
					}
				}
			}

			// One set each. Perform reads the entry table when the actor is idle and the action table
			// when this node is already running, so a name in both is well defined and is how a node
			// offers the same thing from either side: Grab.Enter starts the grab from idle and catches
			// a second actor while one is already stored.
			{
				absl::flat_hash_set<std::string_view> seen;
				for (const auto& def : slot.Node->Entries()) {
					if (!seen.emplace(def.Action).second) {
						ReportAndExit(std::format("Action node {} registers entry '{}' twice", ActionName(slot.Node->Id()), def.Action));
					}
				}
			}

			{
				absl::flat_hash_set<std::string_view> seen;
				for (const auto& def : slot.Node->Actions()) {
					if (!seen.emplace(def.Action).second) {
						ReportAndExit(std::format("Action node {} registers action '{}' twice", ActionName(slot.Node->Id()), def.Action));
					}
				}
			}
		}

		// An action name has to resolve to one node, or Perform cannot route it from idle.
		{
			absl::flat_hash_map<std::string_view, ActionId> owners;

			for (const auto& slot : m_Slots) {

				if (!slot.Node) {
					continue;
				}

				for (const auto& def : slot.Node->Entries()) {
					if (auto [it, fresh] = owners.emplace(def.Action, slot.Node->Id()); !fresh) {
						ReportAndExit(std::format("Entry action '{}' is claimed by both {} and {}", def.Action, ActionName(it->second), ActionName(slot.Node->Id())));
					}
				}
			}
		}

		logger::info("ActionRegistry: {} node(s), {} graph variable(s), {} driver(s)", registered, GraphVars::Count(), m_Drivers.size());
	}

	bool ActionRegistry::Watching() {

		std::lock_guard lock(ActionLock);
		return m_Watching;
	}

	void ActionRegistry::PollVars() {

		std::lock_guard lock(ActionLock);

		if (!m_VarTarget) {
			return;
		}

		auto* actor = RE::TESForm::LookupByID<RE::Actor>(m_VarTarget);

		if (!actor || !actor->Is3DLoaded()) {
			ActionLog::Write(std::format("VAR_LOST|{:08X}", m_VarTarget));
			m_VarTarget = 0;
			m_VarSlots.clear();
			return;
		}

		GraphIntrospect::Poll(actor, m_VarSlots, Time::WorldTimeElapsed(),
			[&](const GraphVarSlot& a_Slot, std::uint32_t a_Old, std::uint32_t a_New) {
				ActionLog::Write(std::format("VAR|{:08X}|{}|{}|{}|{}", m_VarTarget, a_Slot.Display,
					GraphIntrospect::KindName(a_Slot.Kind),
					GraphIntrospect::Describe(a_Slot, a_Old),
					GraphIntrospect::Describe(a_Slot, a_New)));
			});
	}

	bool ActionRegistry::StartWatch(RE::Actor* a_Target) {

		std::lock_guard lock(ActionLock);

		ActionLog::Close();

		if (!ActionLog::Open()) {
			return false;
		}

		m_Counters = {};
		m_Watching = true;
		m_VarTarget = 0;
		m_VarSlots.clear();

		std::size_t total = 0;

		ActionLog::Write(std::format("WATCH|start|nodes={}|declared={}", total, GraphVars::Count()));

		if (a_Target && GraphIntrospect::BuildSlots(a_Target, m_VarSlots)) {

			std::ranges::sort(m_VarSlots, [](const GraphVarSlot& a_L, const GraphVarSlot& a_R) {
				return a_L.Display < a_R.Display;
			});

			m_VarTarget = a_Target->formID;

			ActionLog::Write(std::format("VAR_BEGIN|{:08X}|{}|count={}", m_VarTarget, a_Target->GetDisplayFullName(), m_VarSlots.size()));

			for (const auto& slot : m_VarSlots) {
				ActionLog::Write(std::format("VAR0|{:08X}|{}|{}|{}", m_VarTarget, slot.Display,
					GraphIntrospect::KindName(slot.Kind), GraphIntrospect::Describe(slot, slot.Raw)));
			}

			ActionLog::Write("VAR_END");
		}
		else if (a_Target) {
			ActionLog::Write(std::format("VAR_BEGIN|{:08X}|no readable behaviour graph", a_Target->formID));
		}

		return true;
	}

	void ActionRegistry::StopWatch() {

		std::lock_guard lock(ActionLock);

		if (m_Watching) {
			const auto [stanceIn, stanceOut] = MovementRegistry::Traffic();

			ActionLog::Write(std::format("WATCH|stop|enters={}|exits={}|handoffs={}|desync={}|refused={}|orphan_trig={}|orphan_anno={}|dead_beh={}|stance_in={}|stance_out={}",
				m_Counters.Enters, m_Counters.Exits, m_Counters.HandOffs, m_Counters.Desyncs,
				m_Counters.Refusals, m_Counters.OrphanTriggers, m_Counters.OrphanAnnotations, m_Counters.DeadBehaviours,
				stanceIn, stanceOut));
		}

		m_Watching = false;
		m_VarTarget = 0;
		m_VarSlots.clear();
	}

	void ActionRegistry::PrintStatus() {

		std::lock_guard lock(ActionLock);

		std::size_t total = 0;


		Cprint("ActionRegistry: {} node(s), {} stance(s), {} driver(s), {} reaction(s), {} graph var(s). Watching: {}",
			total, MovementRegistry::Count(), m_Drivers.size(), Reactions::Count(), GraphVars::Count(), m_Watching ? "yes" : "no"
		);

		Cprint("  enters {}  exits {}  handoffs {}  desync {}  refused {}  orphan trig {}  orphan anno {}",
			m_Counters.Enters, m_Counters.Exits, m_Counters.HandOffs, m_Counters.Desyncs,
			m_Counters.Refusals, m_Counters.OrphanTriggers, m_Counters.OrphanAnnotations
		);

		for (auto& [id, state] : m_State) {

			if (state.Idle()) {
				continue;
			}

			auto ptr = state.Owner.get();

			Cprint("  {:08X}: {} (via {}) holding {}", id, Describe(id, state), state.EnteredVia.empty() ? "?" : state.EnteredVia, Possession::Describe(id));
		}

	}

	void ActionRegistry::Forget(RE::FormID a_Owner) {

		std::lock_guard lock(ActionLock);

		for (const auto& slot : m_Slots) {
			if (slot.Node) {
				slot.Node->OnForget(a_Owner);
			}
		}

		for (const auto& driver : m_Drivers) {
			driver->OnForget(a_Owner);
		}

		MovementRegistry::Forget(a_Owner);

		absl::erase_if(m_Cooldowns, [&](const auto& a_Entry) { return a_Entry.first.first == a_Owner; });
		absl::erase_if(m_Refused, [&](const auto& a_Entry) { return a_Entry.first.first == a_Owner; });
	}

	void ActionRegistry::OnSKSEDataLoaded() {

		std::lock_guard lock(ActionLock);

		//Register Animation Nodes
		Register(std::make_unique<ThighCrushNode>());
		Register(std::make_unique<ThighSandwichNode>());
		Register(std::make_unique<ThighSandwichButtNode>());
		Register(std::make_unique<ButtCrushNode>());
		Register(std::make_unique<BoobCrushNode>());
		Register(std::make_unique<VoreNode>());
		Register(std::make_unique<HugNode>());
		Register(std::make_unique<GrabNode>());
		Register(std::make_unique<GrabPlayNode>());
		Register(std::make_unique<CleavageNode>());
		Register(std::make_unique<GrowthNode>());
		Register(std::make_unique<ShrinkNode>());
		Register(std::make_unique<CalamityNode>());
		Register(std::make_unique<StompNode>());
		Register(std::make_unique<TrampleNode>());
		Register(std::make_unique<KickSwipeNode>());

		//Register Movement Types
		MovementRegistry::Register(std::make_unique<CrawlMovement>());
		MovementRegistry::Register(std::make_unique<ProneMovement>());

		RegisterFurnitureReactions();
		RegisterModSupportReactions();

		Validate();
		MovementRegistry::Validate();
		GraphVars::Freeze();

		BindInput();
		MovementRegistry::BindInput();
		Keybinds::ApplyStoredOverrides();
		InputManager::GetSingleton().Init();

		ConsoleManager::RegisterCommand({
			.Name = "action",
			.Desc = "Inspect the action node registry",
			.Usage = "<status|watch|stop> [player|target|formid]",
			.MinArgs = 1,
			.MaxArgs = 2,
			.Callback = [](const ConsoleArgs& a_Args) {

				const std::string action = a_Args.Lower(0);

				if (action == "watch") {

					RE::Actor* target = a_Args.ResolveActor(1);

					if (!StartWatch(target)) {
						Cprint("Could not open the log.");
						return;
					}

					Cprint("Watching. Log: GTSPluginActions.log");
					Cprint(target ? std::format("Variables followed on {}", target->GetDisplayFullName()) : "No actor resolved, node events only.");
					return;
				}

				if (action == "stop") {
					StopWatch();
					Cprint("Stopped watching.");
					return;
				}

				if (action == "status") {
					PrintStatus();
					return;
				}

				ConsoleManager::PrintUsage("action");
			},
		});
	}

	// Every lifecycle event ends an actor's state here, so OnExit always runs.
	void ActionRegistry::Teardown(RE::FormID a_Owner, RE::Actor* a_Actor, ExitReason a_Reason) {

		std::lock_guard lock(ActionLock);

		if (auto it = m_State.find(a_Owner); it != m_State.end()) {

			if (it->second.Active()) {
				Leave(a_Owner, a_Actor, it->second, a_Reason, GraphVars::kNoBit);
			}

			m_State.erase(a_Owner);
		}

		Forget(a_Owner);
	}

	void ActionRegistry::OnActor3DUnload(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (a_Actor) {
			Teardown(a_Actor->formID, a_Actor, ExitReason::kActorLost);
		}
	}

	// Possession runs first and keeps the carried slots across a cell change. The carry node is put back here.
	void ActionRegistry::OnActorLoad3D(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (a_Actor) {
			RestoreCarry(a_Actor->formID, a_Actor);
		}
	}

	void ActionRegistry::OnGameActorReset(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (a_Actor) {
			Teardown(a_Actor->formID, a_Actor, ExitReason::kAborted);
		}
	}

	void ActionRegistry::OnPluginReset() {

		std::lock_guard lock(ActionLock);

		m_State.clear();
		m_Cooldowns.clear();
		m_Refused.clear();
		m_Dead.clear();
		m_Live.clear();
		m_Counters = {};

		for (const auto& slot : m_Slots) {
			if (slot.Node) {
				slot.Node->OnReset();
			}
		}

		for (const auto& driver : m_Drivers) {
			driver->OnReset();
		}

		MovementRegistry::Reset();
	}
}
