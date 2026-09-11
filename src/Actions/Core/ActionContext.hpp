#pragma once

#include "Actions/Core/ActorAction.hpp"
#include "Actions/Core/GraphSnapshot.hpp"
#include "Actions/Core/Possession.hpp"

namespace GTS::Actions {

	class ActionContext {

		public:
		ActionContext(RE::Actor* a_Actor, RE::FormID a_Owner, ActorAction& a_State, const GraphSnapshot& a_Snapshot, bool a_Probing = false, RE::Actor* a_Partner = nullptr)
			: m_Actor(a_Actor), m_Owner(a_Owner), m_State(a_State), m_Snapshot(a_Snapshot), m_Probing(a_Probing), m_Partner(a_Partner) {}

		[[nodiscard]] RE::Actor* Actor() const { return m_Actor; }
		[[nodiscard]] RE::FormID Owner() const { return m_Owner; }
		[[nodiscard]] const GraphSnapshot& Graph() const { return m_Snapshot; }
		[[nodiscard]] bool Probing() const { return m_Probing; }
		[[nodiscard]] bool Deferred(bool a_Established) const { return m_Probing || a_Established; }
		[[nodiscard]] Stance Stance() const;
		[[nodiscard]] RE::Actor* Partner() const { return m_Partner; }

		[[nodiscard]] ActionId Id() const { return m_State.Current; }
		[[nodiscard]] std::string_view EnteredVia() const { return m_State.EnteredVia; }
		[[nodiscard]] double TimeActive() const;
		[[nodiscard]] bool Exiting() const { return m_State.ExitRequestedAt >= 0.0; }
		bool Send(std::string_view a_Behaviour, bool a_Force = false) const;
		static bool SendTo(RE::Actor* a_Target, std::string_view a_Behaviour, bool a_Force = false);
		std::size_t SendToHeld(PossessionSlot a_Slot, std::string_view a_Behaviour) const;
		void HandOff(ActionId a_Target) const;
		void RequestExit() const;
		void Abort() const;
		bool Take(PossessionSlot a_Slot, RE::ActorHandle a_Tiny) const;
		bool Take(PossessionSlot a_Slot, RE::Actor* a_Tiny) const;
		void Release(PossessionSlot a_Slot) const;
		void ReleaseAll() const;
		[[nodiscard]] bool Occupied(PossessionSlot a_Slot) const;
		[[nodiscard]] RE::Actor* Held(PossessionSlot a_Slot) const;
		[[nodiscard]] Possession::SlotList AllHeld(PossessionSlot a_Slot) const;

		void SetAnimSpeed(float a_Speed) const;
		void SetCanEditAnimSpeed(bool a_Allow) const;
		void SetHHDisabled(bool a_Disabled, float a_Speed = 1.0f) const;
		[[nodiscard]] float AnimSpeed() const { return m_State.AnimSpeed; }

		static void Log(std::string_view a_Line);

		private:
		RE::Actor* m_Actor;
		RE::FormID m_Owner;
		ActorAction& m_State;
		const GraphSnapshot& m_Snapshot;
		bool m_Probing;
		RE::Actor* m_Partner;
	};

	class EntryContext {

		public:
		EntryContext(RE::Actor* a_Actor, const GraphSnapshot& a_Snapshot, bool a_Probing = false) 
			: m_Actor(a_Actor), m_Snapshot(a_Snapshot), m_Probing(a_Probing) {}

		[[nodiscard]] RE::Actor* Actor() const { return m_Actor; }
		[[nodiscard]] RE::FormID Owner() const { return m_Actor ? m_Actor->formID : 0; }
		[[nodiscard]] const GraphSnapshot& Graph() const { return m_Snapshot; }
		[[nodiscard]] bool Probing() const { return m_Probing; }
		[[nodiscard]] bool Deferred(bool a_Established) const { return m_Probing || a_Established; }
		[[nodiscard]] Stance Stance() const;

		[[nodiscard]] bool Occupied(PossessionSlot a_Slot) const;
		[[nodiscard]] std::size_t Count(PossessionSlot a_Slot) const;
		[[nodiscard]] RE::Actor* Held(PossessionSlot a_Slot) const;

		private:
		RE::Actor* m_Actor;
		const GraphSnapshot& m_Snapshot;
		bool m_Probing;
	};
}
