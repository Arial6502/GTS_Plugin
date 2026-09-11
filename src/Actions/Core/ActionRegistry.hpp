#pragma once

#include "Actions/Core/IActionDriver.hpp"
#include "Actions/Core/IActionNode.hpp"
#include "Debug/Trace/GraphIntrospect.hpp"

namespace GTS::Actions {

	class ActionRegistry : public EventListener, public CInitSingleton<ActionRegistry> {

		public:
		void OnSKSEDataLoaded() override;
		void OnMainUpdate() override;
		void OnPluginReset() override;
		void OnActor3DUnload(RE::Actor* a_Actor) override;
		void OnActorLoad3D(RE::Actor* a_Actor) override;
		void OnGameActorReset(RE::Actor* a_Actor) override;

		static void Register(std::unique_ptr<IActionNode> a_Node);
		static void AddDriver(std::unique_ptr<IActionDriver> a_Driver);

		static RequestResult Perform(RE::Actor* a_Actor, std::string_view a_Action);
		static void Abort(RE::Actor* a_Actor);
		static bool Resume(RE::Actor* a_Actor, ActionId a_Id, std::string_view a_Via = "restore");
		static void OnAnnotation(RE::Actor* a_Actor, std::string_view a_Tag);
		static void OnPartnerAnnotation(RE::Actor* a_Partner, std::string_view a_Tag);
		static void OnLegacyTrigger(RE::Actor* a_Actor, std::string_view a_Trigger, std::string_view a_Detail, bool a_Sent);
		[[nodiscard]] static bool CanPerform(RE::Actor* a_Actor, std::string_view a_Action);
		[[nodiscard]] static std::vector<AvailableAction> Available(RE::Actor* a_Actor);
		[[nodiscard]] static bool HandleVanillaInput(RE::Actor* a_Actor, std::string_view a_UserEvent);

		[[nodiscard]] static ActionId Current(RE::Actor* a_Actor);
		[[nodiscard]] static bool Busy(RE::Actor* a_Actor);
		[[nodiscard]] static float AnimSpeed(RE::Actor* a_Actor);
		[[nodiscard]] static float HHSpeed(RE::Actor* a_Actor);
		[[nodiscard]] static bool HHDisabled(RE::Actor* a_Actor);
		static void AdjustAnimSpeed(RE::Actor* a_Actor, float a_Bonus, float a_Min, float a_Max);
		static void DriveAnimSpeed(RE::Actor* a_Actor, ActorAction& a_State, float a_Delta);
		static void SyncHeldAnimSpeed(RE::Actor* a_Actor);

		[[nodiscard]] static IActionNode* Find(ActionId a_Id);
		[[nodiscard]] static IActionNode* EntryOwner(std::string_view a_Action);
		[[nodiscard]] static const ActorAction* StateOf(RE::Actor* a_Actor);
		static bool Notify(RE::Actor* a_Actor, std::string_view a_Behaviour, bool a_Force = false);

		[[nodiscard]] static bool Watching();
		static bool StartWatch(RE::Actor* a_Target);
		static void StopWatch();
		static void PrintStatus();

		private:
		struct Slot {
			IActionNode* Node = nullptr;
			GraphSignature Signature = {};
			absl::InlinedVector<GraphSignature, 2> EntrySignatures = {};
			absl::InlinedVector<GraphSignature, 6> ActionAsserts = {};
		};

		struct Counters {
			std::uint32_t Enters = 0;
			std::uint32_t Exits = 0;
			std::uint32_t HandOffs = 0;
			std::uint32_t Desyncs = 0;
			std::uint32_t Refusals = 0;
			std::uint32_t OrphanTriggers = 0;
			std::uint32_t OrphanAnnotations = 0;
			std::uint32_t DeadBehaviours = 0;
		};

		static void Validate();
		static void BindInput();
		static void ValidateInputs();
		static void Tick(RE::Actor* a_Actor, ActorAction& a_State, float a_Delta);
		static void Enter(RE::Actor* a_Actor, ActorAction& a_State, const Slot& a_Slot);
		static void Leave(RE::FormID a_Owner, RE::Actor* a_Actor, ActorAction& a_State, ExitReason a_Reason, std::uint32_t a_Offender);
		static void RestoreCarry(RE::FormID a_Owner, RE::Actor* a_Actor);
		static void Teardown(RE::FormID a_Owner, RE::Actor* a_Actor, ExitReason a_Reason);
		static void SendAbortSignal(const IActionNode* a_Node, RE::FormID a_Owner, RE::Actor* a_Actor);
		[[nodiscard]] static double Grace(RE::Actor* a_Actor, double a_Seconds);
		static void Forget(RE::FormID a_Owner);
		static void Log(std::string_view a_Line);

		static void PollVars();

		[[nodiscard]] static bool Followed(RE::Actor* a_Actor);
		[[nodiscard]] static bool ClaimsAction(std::string_view a_Action);
		[[nodiscard]] static std::string Describe(RE::FormID a_Owner, const ActorAction& a_State);


		static constexpr float RampPerSecond = 2.7f;
		static constexpr float DecayPerSecond = 4.0f;

		static constexpr double MismatchGrace = 0.25;
		static constexpr double DrainGrace = 30.0;
		static constexpr double PendingGrace = 2.0;
		static constexpr double DeathGrace = 4.0;
		static constexpr double VerifyGrace = 1.0;
		static constexpr double ConfirmGrace = 3.0;

		// How long an idle actor's state is kept before it is forgotten. Long enough for the graph to
		// settle after an exit and short enough that the map does not grow.
		static constexpr double SettleGrace = 0.5;

		static inline std::vector<std::unique_ptr<IActionNode>> m_Owned = {};
		static inline std::array<Slot, kActionCount> m_Slots = {};
		static inline std::vector<std::unique_ptr<IActionDriver>> m_Drivers = {};
		// Node hashed: ActionContext holds an ActorAction& across callbacks that may insert another actor.
		static inline absl::node_hash_map<RE::FormID, ActorAction> m_State = {};
		static inline absl::flat_hash_map<std::pair<RE::FormID, const char*>, double> m_Cooldowns = {};
		static inline absl::flat_hash_map<std::pair<RE::FormID, const char*>, double> m_Refused = {};
		static inline absl::flat_hash_set<std::string> m_Dead = {};
		static inline absl::flat_hash_set<std::string> m_Live = {};
		static inline absl::flat_hash_set<std::string> m_Claimed = {};

		static inline Counters m_Counters = {};
		static inline std::uint64_t m_Frame = 0;
		static inline bool m_Watching = false;

		static inline RE::FormID m_VarTarget = 0;
		static inline std::vector<GraphVarSlot> m_VarSlots = {};
	};
}
