#pragma once

#include "Actions/Core/IMovementState.hpp"

namespace GTS::Actions {

	class MovementRegistry {

		public:
		static void Register(std::unique_ptr<IMovementState> a_State);
		static void Validate();
		static void BindInput();
		static RequestResult Perform(RE::Actor* a_Actor, std::string_view a_Action);
		[[nodiscard]] static bool CanPerform(RE::Actor* a_Actor, std::string_view a_Action);
		static bool Dispatch(RE::Actor* a_Actor, std::string_view a_Tag);
		static void Tick(RE::Actor* a_Actor, float a_Delta);
		static void Tracked(absl::InlinedVector<RE::FormID, 16>& a_Out);
		[[nodiscard]] static bool HandleVanillaInput(RE::Actor* a_Actor, std::string_view a_UserEvent);
		static void AppendAvailable(RE::Actor* a_Actor, std::vector<AvailableAction>& a_Out);
		[[nodiscard]] static MovementId Current(RE::Actor* a_Actor);
		[[nodiscard]] static bool In(RE::Actor* a_Actor, MovementId a_Id);
		static void Forget(RE::FormID a_Owner);
		static void Reset();
		[[nodiscard]] static std::size_t Count();
		[[nodiscard]] static std::pair<std::uint32_t, std::uint32_t> Traffic();
		[[nodiscard]] static std::string Describe(RE::FormID a_Owner);

		private:
		struct Slot {
			IMovementState* State = nullptr;
			GraphSignature Signature = {};
			absl::InlinedVector<GraphSignature, 2> EntrySignatures = {};
		};

		static void Enter(RE::Actor* a_Actor, ActorMovement& a_State, const Slot& a_Slot);
		static void Leave(RE::FormID a_Owner, RE::Actor* a_Actor, ActorMovement& a_State, ExitReason a_Reason);

		[[nodiscard]] static IMovementState* Find(MovementId a_Id);
		static void Log(std::string_view a_Line);

		static constexpr double PendingGrace = 2.0;
		static constexpr double MismatchGrace = 0.25;

		static inline std::vector<std::unique_ptr<IMovementState>> m_Owned = {};
		static inline std::array<Slot, kMovementCount> m_Slots = {};
		static inline absl::flat_hash_map<RE::FormID, ActorMovement> m_State = {};

		static inline std::uint32_t m_Enters = 0;
		static inline std::uint32_t m_Exits = 0;
	};
}
