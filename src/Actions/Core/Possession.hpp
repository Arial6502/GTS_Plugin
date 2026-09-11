#pragma once

#include "Actions/Core/ActionTypes.hpp"

namespace GTS::Actions {

	class Possession : public EventListener, public CInitSingleton<Possession> {

		public:
		using SlotList = absl::InlinedVector<RE::ActorHandle, 2>;

		void OnActor3DUnload(RE::Actor* a_Actor) override;
		void OnActorLoad3D(RE::Actor* a_Actor) override;
		void OnGameActorReset(RE::Actor* a_Actor) override;
		void OnPluginReset() override;
		static bool Take(RE::FormID a_Giant, PossessionSlot a_Slot, RE::ActorHandle a_Tiny);

		static void Release(RE::FormID a_Giant, PossessionSlot a_Slot);
		static void ReleaseOne(RE::FormID a_Giant, PossessionSlot a_Slot, RE::ActorHandle a_Tiny);
		static void ReleaseAll(RE::FormID a_Giant);
		// Moves one actor from one slot to another. The hold does not end, so nothing is released and
		// no cleanup runs: it is the same giant carrying the same actor somewhere else.
		//
		// This is what a slot change needs once two slots can be occupied at once. Move takes the whole
		// slot with it, which is only right while the destination is empty.
		static bool Transfer(RE::FormID a_Giant, PossessionSlot a_From, PossessionSlot a_To, RE::ActorHandle a_Tiny);

		static void Move(RE::FormID a_Giant, PossessionSlot a_From, PossessionSlot a_To);

		[[nodiscard]] static bool Occupied(RE::FormID a_Giant, PossessionSlot a_Slot);
		[[nodiscard]] static bool AnyIn(RE::FormID a_Giant, std::span<const PossessionSlot> a_Slots);
		[[nodiscard]] static std::size_t Count(RE::FormID a_Giant, PossessionSlot a_Slot);
		[[nodiscard]] static RE::ActorHandle First(RE::FormID a_Giant, PossessionSlot a_Slot);
		[[nodiscard]] static RE::Actor* FirstActor(RE::FormID a_Giant, PossessionSlot a_Slot);
		[[nodiscard]] static SlotList All(RE::FormID a_Giant, PossessionSlot a_Slot);
		[[nodiscard]] static RE::Actor* Carried(RE::FormID a_Giant);
		[[nodiscard]] static RE::Actor* CarriedAlive(RE::FormID a_Giant);
		[[nodiscard]] static RE::Actor* FirstAlive(RE::FormID a_Giant, PossessionSlot a_Slot);

	
		struct Death {
			DeathIntent Intent = DeathIntent::kUnexpected;
			ActionId Action = ActionId::kNone;
			std::string_view Cause;
			double IntendedAt = -1.0;
			bool Reported = false;
		};

		// A kill that failed leaves its intent set. Past this it no longer counts. Longer than any kill
		// animation, since the calamity erase still fires footsteps 11s in.
		static constexpr double IntentGrace = 60.0;

		static void Intend(RE::FormID a_Giant, ActionId a_Action, std::string_view a_Cause);
		static void ClearIntent(RE::FormID a_Giant);

		// Per slot, so a failed kill in one branch leaves another branch's intent alone.
		static void ClearIntent(RE::FormID a_Giant, PossessionSlot a_Slot);
		static void ClearIntent(RE::FormID a_Giant, std::span<const PossessionSlot> a_Slots);

		[[nodiscard]] static std::optional<Death> DeathOf(RE::FormID a_Tiny);
		[[nodiscard]] static bool Intended(RE::FormID a_Tiny);
		[[nodiscard]] static bool Killed(RE::FormID a_Giant);
		// Drops the dead from every slot. Only deaths in a_Report are returned.
		static DeathOutcome DropDead(RE::FormID a_Giant, std::span<const PossessionSlot> a_Report);

		[[nodiscard]] static RE::FormID HolderOf(RE::FormID a_Tiny);
		[[nodiscard]] static bool IsHeld(RE::FormID a_Tiny);
		[[nodiscard]] static bool IsHeldIn(RE::FormID a_Tiny, PossessionSlot a_Slot);
		[[nodiscard]] static std::string Describe(RE::FormID a_Giant);

		struct Carry {
			RE::FormID Giant = 0;
			RE::FormID Tiny = 0;
			PossessionSlot Slot = PossessionSlot::kTotal;
		};

		[[nodiscard]] static std::vector<Carry> Snapshot();

		private:

		struct Record {
			std::array<SlotList, kSlotCount> Slots;

			[[nodiscard]] bool Empty() const {
				return std::ranges::all_of(Slots, [](const SlotList& a_List) { return a_List.empty(); });
			}
		};

		[[nodiscard]] static bool IntentExpired(const Death& a_Death);
		[[nodiscard]] static bool KeptForAnimation(RE::FormID a_Tiny);
		static void ForgetAnimated(RE::FormID a_Actor);
		static void Forget(RE::FormID a_Actor);
		static void Restore(RE::FormID a_Giant);
		static void SyncPin(RE::FormID a_Giant);
		static void AfterChange(RE::FormID a_Giant);
		static void ReleaseHandle(const RE::ActorHandle& a_Handle, PossessionSlot a_Slot);

		static inline absl::flat_hash_map<RE::FormID, Death> m_Death = {};
		static inline absl::flat_hash_map<RE::FormID, Record> m_Held = {};
		static inline absl::flat_hash_map<RE::FormID, RE::FormID> m_Holder = {};
	};
}
