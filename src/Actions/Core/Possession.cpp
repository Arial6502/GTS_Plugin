#include "Actions/Core/Possession.hpp"

#include "Actions/Core/ActionCleanup.hpp"
#include "Actions/Core/ActionLock.hpp"

#include "Utils/Actions/VoreUtils.hpp"
#include "Utils/Actor/RefPin.hpp"

namespace {

	using namespace GTS::Actions;

	RE::FormID IdOf(RE::ActorHandle a_Handle) {
		auto ptr = a_Handle.get();
		return ptr ? ptr->formID : 0;
	}

	bool IsDeadActor(RE::Actor* a_Actor) {
		return a_Actor->IsDead() || GTS::GetAV(a_Actor, RE::ActorValue::kHealth) <= 0.0f;
	}
}

namespace GTS::Actions {

	bool Possession::Take(RE::FormID a_Giant, PossessionSlot a_Slot, RE::ActorHandle a_Tiny) {

		std::lock_guard lock(ActionLock);

		const RE::FormID tiny = IdOf(a_Tiny);
		if (!a_Giant || !tiny || a_Slot >= PossessionSlot::kTotal) {
			return false;
		}

		if (auto it = m_Holder.find(tiny); it != m_Holder.end()) {
			if (it->second != a_Giant) {
				return false;
			}
		}

		auto& list = m_Held[a_Giant].Slots[std::to_underlying(a_Slot)];

		for (const auto& existing : list) {
			if (IdOf(existing) == tiny) {
				return true;
			}
		}

		// One actor per carried slot. Each has a single attach point.
		if (IsCarriedSlot(a_Slot) && !list.empty()) {
			return false;
		}

		list.push_back(a_Tiny);
		m_Holder[tiny] = a_Giant;
		m_Death.erase(tiny);

		AfterChange(a_Giant);
		return true;
	}

	void Possession::Intend(RE::FormID a_Giant, ActionId a_Action, std::string_view a_Cause) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);

		if (it == m_Held.end()) {
			return;
		}

		const double now = Time::WorldTimeElapsed();

		for (const auto& list : it->second.Slots) {
			for (const auto& handle : list) {

				const RE::FormID tiny = IdOf(handle);

				if (!tiny) {
					continue;
				}

				Death& death = m_Death[tiny];

				if (death.Intent != DeathIntent::kIntended) {
					death.IntendedAt = now;
					death.Reported = false;
				}

				death.Intent = DeathIntent::kIntended;
				death.Action = a_Action;
				death.Cause = a_Cause;
			}
		}
	}

	void Possession::ClearIntent(RE::FormID a_Giant) {
		ClearIntent(a_Giant, kAllSlots);
	}

	void Possession::ClearIntent(RE::FormID a_Giant, PossessionSlot a_Slot) {
		ClearIntent(a_Giant, std::span(&a_Slot, 1));
	}

	void Possession::ClearIntent(RE::FormID a_Giant, std::span<const PossessionSlot> a_Slots) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);

		if (it == m_Held.end()) {
			return;
		}

		for (const PossessionSlot slot : a_Slots) {

			if (slot >= PossessionSlot::kTotal) {
				continue;
			}

			for (const auto& handle : it->second.Slots[std::to_underlying(slot)]) {
				if (const RE::FormID tiny = IdOf(handle)) {
					m_Death.erase(tiny);
				}
			}
		}
	}

	// By value, because the map can rehash once the lock is released.
	std::optional<Possession::Death> Possession::DeathOf(RE::FormID a_Tiny) {

		std::lock_guard lock(ActionLock);
		auto it = m_Death.find(a_Tiny);
		return it == m_Death.end() ? std::nullopt : std::optional(it->second);
	}

	bool Possession::Intended(RE::FormID a_Tiny) {

		std::lock_guard lock(ActionLock);
		const auto death = DeathOf(a_Tiny);
		return death && death->Intent == DeathIntent::kIntended && !IntentExpired(*death);
	}

	bool Possession::IntentExpired(const Death& a_Death) {
		return a_Death.IntendedAt < 0.0 || (Time::WorldTimeElapsed() - a_Death.IntendedAt) > IntentGrace;
	}

	bool Possession::Killed(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);

		if (it == m_Held.end()) {
			return false;
		}

		for (const auto& list : it->second.Slots) {
			for (const auto& handle : list) {

				auto ptr = handle.get();

				if (ptr && Intended(ptr->formID) && IsDeadActor(ptr.get())) {
					return true;
				}
			}
		}

		return false;
	}

	void Possession::ReleaseHandle(const RE::ActorHandle& a_Handle, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);

		NiPointer<RE::Actor> ptr = a_Handle.get();

		if (ptr) {

			Cleanup::ReleaseTiny(ptr.get(), a_Slot);

			// Next frame. Demoting calls into the engine's reference bookkeeping, and a release can
			// land between the hand letting go and the throw applying its impulse.
			const RE::ActorHandle handle = a_Handle;

			TaskManager::RunOnce([handle](const OneshotUpdate&) {
				if (auto later = handle.get()) {
					RefPin::Unpin(later.get());
				}
			});
		}
	}

	void Possession::Release(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		if (it == m_Held.end() || a_Slot >= PossessionSlot::kTotal) {
			return;
		}

		auto& list = it->second.Slots[std::to_underlying(a_Slot)];

		for (const auto& handle : list) {
			ReleaseHandle(handle, a_Slot);
			if (const RE::FormID tiny = IdOf(handle)) {
				m_Holder.erase(tiny);
				m_Death.erase(tiny);
			}
		}

		list.clear();

		if (it->second.Empty()) {
			m_Held.erase(it);
		}

		AfterChange(a_Giant);
	}

	void Possession::ReleaseOne(RE::FormID a_Giant, PossessionSlot a_Slot, RE::ActorHandle a_Tiny) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		if (it == m_Held.end() || a_Slot >= PossessionSlot::kTotal) {
			return;
		}

		const RE::FormID tiny = IdOf(a_Tiny);
		auto& list = it->second.Slots[std::to_underlying(a_Slot)];

		ReleaseHandle(a_Tiny, a_Slot);

		list.erase(std::ranges::remove_if(list, [&](const RE::ActorHandle& a_Handle) { return IdOf(a_Handle) == tiny; }).begin(), list.end());
		m_Holder.erase(tiny);
		m_Death.erase(tiny);

		if (it->second.Empty()) {
			m_Held.erase(it);
		}

		AfterChange(a_Giant);
	}

	void Possession::ReleaseAll(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		if (it == m_Held.end()) {
			return;
		}

		for (std::size_t i = 0; i < kSlotCount; ++i) {
			for (const auto& handle : it->second.Slots[i]) {
				ReleaseHandle(handle, static_cast<PossessionSlot>(i));
				if (const RE::FormID tiny = IdOf(handle)) {
					m_Holder.erase(tiny);
					m_Death.erase(tiny);
				}
			}
		}

		m_Held.erase(it);

		AfterChange(a_Giant);
	}

	bool Possession::Transfer(RE::FormID a_Giant, PossessionSlot a_From, PossessionSlot a_To, RE::ActorHandle a_Tiny) {

		std::lock_guard lock(ActionLock);

		const RE::FormID tiny = IdOf(a_Tiny);
		auto it = m_Held.find(a_Giant);

		if (!tiny || it == m_Held.end() || a_From >= PossessionSlot::kTotal || a_To >= PossessionSlot::kTotal) {
			return false;
		}

		auto& from = it->second.Slots[std::to_underlying(a_From)];
		const auto found = std::ranges::find_if(from, [&](const RE::ActorHandle& a_Handle) { return IdOf(a_Handle) == tiny; });

		if (found == from.end()) {
			return false;
		}

		auto& to = it->second.Slots[std::to_underlying(a_To)];

		for (const auto& existing : to) {
			if (IdOf(existing) == tiny) {
				return false;
			}
		}

		to.push_back(*found);
		from.erase(found);

		AfterChange(a_Giant);
		return true;
	}

	void Possession::Move(RE::FormID a_Giant, PossessionSlot a_From, PossessionSlot a_To) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		if (it == m_Held.end() || a_From >= PossessionSlot::kTotal || a_To >= PossessionSlot::kTotal) {
			return;
		}

		auto& from = it->second.Slots[std::to_underlying(a_From)];
		auto& to = it->second.Slots[std::to_underlying(a_To)];

		for (const auto& handle : from) {
			to.push_back(handle);
		}

		from.clear();

		AfterChange(a_Giant);
	}

	bool Possession::AnyIn(RE::FormID a_Giant, std::span<const PossessionSlot> a_Slots) {

		std::lock_guard lock(ActionLock);
		return std::ranges::any_of(a_Slots, [&](PossessionSlot a_Slot) { return Occupied(a_Giant, a_Slot); });
	}

	bool Possession::Occupied(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);
		return Count(a_Giant, a_Slot) > 0;
	}

	std::size_t Possession::Count(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);
		return All(a_Giant, a_Slot).size();
	}

	RE::ActorHandle Possession::First(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);
		const auto all = All(a_Giant, a_Slot);
		return all.empty() ? RE::ActorHandle{} : all.front();
	}

	RE::Actor* Possession::FirstActor(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);
		auto ptr = First(a_Giant, a_Slot).get();
		return ptr ? ptr.get() : nullptr;
	}

	Possession::SlotList Possession::All(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		if (it == m_Held.end() || a_Slot >= PossessionSlot::kTotal) {
			return {};
		}

		return it->second.Slots[std::to_underlying(a_Slot)];
	}

	RE::Actor* Possession::Carried(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		if (auto* inHand = FirstActor(a_Giant, PossessionSlot::kHand)) {
			return inHand;
		}

		return FirstActor(a_Giant, PossessionSlot::kBreasts);
	}

	RE::Actor* Possession::CarriedAlive(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		auto* tiny = Carried(a_Giant);
		return tiny && !IsDeadActor(tiny) ? tiny : nullptr;
	}

	RE::Actor* Possession::FirstAlive(RE::FormID a_Giant, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);

		auto* tiny = FirstActor(a_Giant, a_Slot);
		return tiny && !IsDeadActor(tiny) ? tiny : nullptr;
	}

	DeathOutcome Possession::DropDead(RE::FormID a_Giant, std::span<const PossessionSlot> a_Report) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);

		if (it == m_Held.end()) {
			return DeathOutcome::kNone;
		}

		bool changed = false;
		bool dropped = false;
		bool intended = false;

		for (std::size_t i = 0; i < kSlotCount; ++i) {

			const auto slot = static_cast<PossessionSlot>(i);
			const bool reported = std::ranges::contains(a_Report, slot);

			const auto gone = [&](const RE::ActorHandle& a_Handle) {

				auto ptr = a_Handle.get();

				if (!ptr) {
					changed = true;
					dropped |= reported;
					return true;
				}

				auto* tiny = ptr.get();

				if (IsActorLost(tiny)) {
					ReleaseHandle(a_Handle, slot);
					m_Holder.erase(tiny->formID);
					m_Death.erase(tiny->formID);
					changed = true;
					dropped |= reported;
					return true;
				}

				if (!IsDeadActor(tiny)) {
					return false;
				}

				auto death = m_Death.find(tiny->formID);

				// A failed kill leaves its intent behind. Once the death has been reported the node is
				// playing that kill's ending, so a reported intent never expires.
				if (death != m_Death.end() && !death->second.Reported && IntentExpired(death->second)) {
					m_Death.erase(death);
					death = m_Death.end();
				}

				const bool meant = (death != m_Death.end() && death->second.Intent == DeathIntent::kIntended) || IsBeingEaten(tiny);

				if (meant) {

					if (death == m_Death.end()) {
						death = m_Death.emplace(tiny->formID, Death{ .Intent = DeathIntent::kIntended, .IntendedAt = Time::WorldTimeElapsed() }).first;
					}

					if (reported && !death->second.Reported) {
						death->second.Reported = true;
						intended = true;
					}

					return false;
				}

				ReleaseHandle(a_Handle, slot);
				m_Holder.erase(tiny->formID);
				m_Death.erase(tiny->formID);
				changed = true;
				dropped |= reported;
				return true;
			};

			auto& list = it->second.Slots[i];
			list.erase(std::ranges::remove_if(list, gone).begin(), list.end());
		}

		if (changed) {

			if (it->second.Empty()) {
				m_Held.erase(it);
			}

			AfterChange(a_Giant);
		}

		if (dropped) {
			return DeathOutcome::kUnexpected;
		}

		return intended ? DeathOutcome::kIntended : DeathOutcome::kNone;
	}

	RE::FormID Possession::HolderOf(RE::FormID a_Tiny) {

		std::lock_guard lock(ActionLock);
		auto it = m_Holder.find(a_Tiny);
		return it == m_Holder.end() ? 0 : it->second;
	}

	bool Possession::IsHeldIn(RE::FormID a_Tiny, PossessionSlot a_Slot) {

		std::lock_guard lock(ActionLock);

		const RE::FormID giant = HolderOf(a_Tiny);

		if (!giant) {
			return false;
		}

		for (const auto& handle : All(giant, a_Slot)) {
			if (IdOf(handle) == a_Tiny) {
				return true;
			}
		}

		return false;
	}

	bool Possession::IsHeld(RE::FormID a_Tiny) {

		std::lock_guard lock(ActionLock);
		return m_Holder.contains(a_Tiny);
	}

	std::string Possession::Describe(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		if (it == m_Held.end()) {
			return "nothing";
		}

		std::string out;

		for (std::size_t i = 0; i < kSlotCount; ++i) {

			const auto& list = it->second.Slots[i];
			if (list.empty()) {
				continue;
			}

			if (!out.empty()) {
				out += " ";
			}

			out += std::format("{}={}", SlotName(static_cast<PossessionSlot>(i)), list.size());

			for (const auto& handle : list) {
				if (auto ptr = handle.get(); ptr && Intended(ptr->formID)) {
					out += IsDeadActor(ptr.get()) ? "(killed)" : "(dying)";
				}
			}
		}

		return out.empty() ? "nothing" : out;
	}

	void Possession::Forget(RE::FormID a_Actor) {

		std::lock_guard lock(ActionLock);

		ReleaseAll(a_Actor);

		if (auto it = m_Holder.find(a_Actor); it != m_Holder.end()) {
			const RE::FormID giant = it->second;
			m_Holder.erase(it);
			m_Death.erase(a_Actor);

			if (auto held = m_Held.find(giant); held != m_Held.end()) {

				for (std::size_t i = 0; i < kSlotCount; ++i) {

					auto& list = held->second.Slots[i];

					for (const auto& handle : list) {
						if (IdOf(handle) == a_Actor) {
							ReleaseHandle(handle, static_cast<PossessionSlot>(i));
						}
					}

					list.erase(std::ranges::remove_if(list, [&](const RE::ActorHandle& a_Handle) { return IdOf(a_Handle) == a_Actor; }).begin(), list.end());
				}

				if (held->second.Empty()) {
					m_Held.erase(held);
				}
			}

			AfterChange(giant);
		}
	}

	// Disintegrating a killed actor unloads their 3d while the kill animation is still playing, so a
	// dead actor with a live kill intent stays in their slot here. DropDead does the same for the rest.
	bool Possession::KeptForAnimation(RE::FormID a_Tiny) {

		auto it = m_Death.find(a_Tiny);

		if (it == m_Death.end() || it->second.Intent != DeathIntent::kIntended || IntentExpired(it->second)) {
			return false;
		}

		auto* tiny = RE::TESForm::LookupByID<RE::Actor>(a_Tiny);
		return tiny && IsDeadActor(tiny);
	}

	void Possession::ForgetAnimated(RE::FormID a_Actor) {

		std::lock_guard lock(ActionLock);

		if (auto it = m_Held.find(a_Actor); it != m_Held.end()) {

			for (std::size_t i = 0; i < kSlotCount; ++i) {
				if (!IsCarriedSlot(static_cast<PossessionSlot>(i))) {
					Release(a_Actor, static_cast<PossessionSlot>(i));
				}
			}
		}

		if (KeptForAnimation(a_Actor)) {
			return;
		}

		if (auto it = m_Holder.find(a_Actor); it != m_Holder.end()) {

			const RE::FormID giant = it->second;

			if (auto held = m_Held.find(giant); held != m_Held.end()) {

				for (std::size_t i = 0; i < kSlotCount; ++i) {

					if (IsCarriedSlot(static_cast<PossessionSlot>(i))) {
						continue;
					}

					auto& list = held->second.Slots[i];

					for (const auto& handle : list) {
						if (IdOf(handle) == a_Actor) {
							ReleaseHandle(handle, static_cast<PossessionSlot>(i));
						}
					}

					const std::size_t before = list.size();
					list.erase(std::ranges::remove_if(list, [&](const RE::ActorHandle& a_Handle) { return IdOf(a_Handle) == a_Actor; }).begin(), list.end());

					if (list.size() != before) {
						m_Holder.erase(a_Actor);
						m_Death.erase(a_Actor);
					}
				}

				if (held->second.Empty()) {
					m_Held.erase(held);
				}
			}
		}
	}

	void Possession::OnActor3DUnload(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return;
		}

		if (IsActorLost(a_Actor)) {
			Forget(a_Actor->formID);
			return;
		}

		ForgetAnimated(a_Actor->formID);
	}

	void Possession::OnActorLoad3D(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);

		if (!a_Actor) {
			return;
		}

		AfterChange(a_Actor->formID);

		if (const RE::FormID giant = HolderOf(a_Actor->formID)) {
			AfterChange(giant);
		}
	}

	void Possession::Restore(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		auto* giant = RE::TESForm::LookupByID<RE::Actor>(a_Giant);

		if (!giant || !giant->Is3DLoaded()) {
			return;
		}

		bool inHand = false;
		bool inBreasts = false;

		if (auto it = m_Held.find(a_Giant); it != m_Held.end()) {
			inHand = !it->second.Slots[std::to_underlying(PossessionSlot::kHand)].empty();
			inBreasts = !it->second.Slots[std::to_underlying(PossessionSlot::kBreasts)].empty();
		}

		AnimationVars::Grab::SetHasGrabbedTiny(giant, inHand);
		AnimationVars::Action::SetIsStoringTiny(giant, inBreasts);
	}

	void Possession::SyncPin(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);

		auto it = m_Held.find(a_Giant);
		bool carrying = false;

		if (it != m_Held.end()) {
			for (std::size_t i = 0; i < kSlotCount; ++i) {

				if (!IsCarriedSlot(static_cast<PossessionSlot>(i))) {
					continue;
				}

				for (const auto& handle : it->second.Slots[i]) {
					if (auto ptr = handle.get()) {
						carrying = true;
						RefPin::Pin(ptr.get());
					}
				}
			}
		}

		auto* giant = RE::TESForm::LookupByID<RE::Actor>(a_Giant);

		if (!giant) {
			return;
		}

		if (carrying) {
			RefPin::Pin(giant);
		}
		else {
			RefPin::Unpin(giant);
		}
	}

	void Possession::AfterChange(RE::FormID a_Giant) {

		std::lock_guard lock(ActionLock);
		Restore(a_Giant);
		SyncPin(a_Giant);
	}

	std::vector<Possession::Carry> Possession::Snapshot() {

		std::lock_guard lock(ActionLock);

		std::vector<Carry> out;

		for (const auto& [giant, record] : m_Held) {
			for (std::size_t i = 0; i < kSlotCount; ++i) {

				const auto slot = static_cast<PossessionSlot>(i);

				if (!IsCarriedSlot(slot)) {
					continue;
				}

				for (const auto& handle : record.Slots[i]) {
					if (auto ptr = handle.get()) {
						out.emplace_back(giant, ptr->formID, slot);
					}
				}
			}
		}

		return out;
	}

	void Possession::OnGameActorReset(RE::Actor* a_Actor) {

		std::lock_guard lock(ActionLock);
		if (a_Actor) {
			Forget(a_Actor->formID);
		}
	}

	// A load or a return to the main menu ends every hold. Nothing about an animation survives a save,
	// so the pins go with the store: left behind, a promoted reference stays promoted in the save with
	// nothing holding it.
	void Possession::OnPluginReset() {

		std::lock_guard lock(ActionLock);

		for (const auto& [giant, record] : m_Held) {

			if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(giant)) {
				RefPin::Unpin(actor);
			}

			for (const auto& list : record.Slots) {
				for (const auto& handle : list) {
					if (auto ptr = handle.get()) {
						RefPin::Unpin(ptr.get());
					}
				}
			}
		}

		m_Held.clear();
		m_Holder.clear();
		m_Death.clear();
	}

}