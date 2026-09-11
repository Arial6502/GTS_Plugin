#include "Actions/Core/ActionContext.hpp"

#include "Actions/Core/ActionLog.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"

namespace GTS::Actions {

	static Stance StanceOf(RE::Actor* a_Actor) {

		if (!a_Actor) {
			return Stance::kStanding;
		}

		if (AnimationVars::Crawl::IsCrawling(a_Actor)) {
			return Stance::kCrawl;
		}

		return a_Actor->IsSneaking() ? Stance::kSneak : GTS::Actions::Stance::kStanding;
	}

	bool EntryContext::Occupied(PossessionSlot a_Slot) const {
		return Possession::Occupied(Owner(), a_Slot);
	}

	std::size_t EntryContext::Count(PossessionSlot a_Slot) const {
		return Possession::Count(Owner(), a_Slot);
	}

	RE::Actor* EntryContext::Held(PossessionSlot a_Slot) const {
		return Possession::FirstActor(Owner(), a_Slot);
	}

	Stance EntryContext::Stance() const {
		return StanceOf(m_Actor);
	}

	double ActionContext::TimeActive() const {
		return Time::WorldTimeElapsed() - m_State.EnteredAt;
	}

	Stance ActionContext::Stance() const {
		return StanceOf(m_Actor);
	}

	bool ActionContext::Send(std::string_view a_Behaviour, bool a_Force) const {
		return SendTo(m_Actor, a_Behaviour, a_Force);
	}

	bool ActionContext::SendTo(RE::Actor* a_Target, std::string_view a_Behaviour, bool a_Force) {

		if (!a_Target || a_Behaviour.empty()) {
			return false;
		}

		return ActionRegistry::Notify(a_Target, a_Behaviour, a_Force);
	}

	std::size_t ActionContext::SendToHeld(PossessionSlot a_Slot, std::string_view a_Behaviour) const {

		std::size_t sent = 0;

		for (const auto& handle : AllHeld(a_Slot)) {

			auto ptr = handle.get();
			if (ptr && SendTo(ptr.get(), a_Behaviour)) {
				++sent;
			}
		}

		return sent;
	}

	void ActionContext::HandOff(ActionId a_Target) const {
		m_State.HandOffTo = a_Target;
		m_State.ExitRequestedAt = Time::WorldTimeElapsed();
	}

	void ActionContext::RequestExit() const {
		if (m_State.ExitRequestedAt < 0.0) {
			m_State.ExitRequestedAt = Time::WorldTimeElapsed();
		}
	}

	void ActionContext::Abort() const {
		ActionRegistry::Abort(m_Actor);
	}

	bool ActionContext::Take(PossessionSlot a_Slot, RE::ActorHandle a_Tiny) const {
		return Possession::Take(m_Owner, a_Slot, a_Tiny);
	}

	bool ActionContext::Take(PossessionSlot a_Slot, RE::Actor* a_Tiny) const {
		return a_Tiny ? Take(a_Slot, a_Tiny->GetHandle()) : false;
	}

	void ActionContext::Release(PossessionSlot a_Slot) const {
		Possession::Release(m_Owner, a_Slot);
	}

	void ActionContext::ReleaseAll() const {
		Possession::ReleaseAll(m_Owner);
	}

	bool ActionContext::Occupied(PossessionSlot a_Slot) const {
		return Possession::Occupied(m_Owner, a_Slot);
	}

	RE::Actor* ActionContext::Held(PossessionSlot a_Slot) const {
		return Possession::FirstActor(m_Owner, a_Slot);
	}

	Possession::SlotList ActionContext::AllHeld(PossessionSlot a_Slot) const {
		return Possession::All(m_Owner, a_Slot);
	}

	void ActionContext::SetAnimSpeed(float a_Speed) const {
		m_State.AnimSpeed = a_Speed;
		m_State.SpeedBase = a_Speed;
	}

	void ActionContext::SetCanEditAnimSpeed(bool a_Allow) const {
		m_State.CanEditAnimSpeed = a_Allow;
	}

	void ActionContext::SetHHDisabled(bool a_Disabled, float a_Speed) const {
		m_State.HHDisabled = a_Disabled;
		m_State.HHSpeed = a_Speed;
	}

	void ActionContext::Log(std::string_view a_Line) {
		ActionLog::Write(a_Line);
	}
}
