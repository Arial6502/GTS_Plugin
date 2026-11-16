#pragma once

#include "Managers/AI/Brain.hpp"

namespace GTS::AI {

	// One state machine per actor, over the Actions system. Picks a target, walks to it with attacks
	// held back, starts through the adapter's node, and undoes exactly what it applied when the plan
	// ends. Replaces AIManager, CombatSteering and AttackManager.
	class Director : public EventListener, public CInitSingleton<Director> {

		public:
		void OnSKSEDataLoaded() override;
		void OnMainUpdate() override;
		void OnActor3DUnload(RE::Actor* a_Actor) override;
		void OnGameActorReset(RE::Actor* a_Actor) override;
		void OnPluginReset() override;

		// What the attack hook asks. Either a plan is running or the size policy says so.
		[[nodiscard]] static bool SuppressesAttacks(RE::Actor* a_Actor);

		// One entry per live brain, for the debug window.
		[[nodiscard]] static std::vector<BrainView> Inspect();

		// Start a named branch on demand, from Papyrus or the console. Skips the act chance and the
		// adapter's enabled flag, and walks to a target that is out of reach.
		static bool Force(RE::Actor* a_Actor, RE::Actor* a_Target, std::string_view a_Adapter);

		private:
		static void Tick(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static void Think(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static void Approach(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static void Retarget(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static void Posture(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static bool TakeStance(RE::Actor* a_Actor, Brain& a_Brain, const struct Adapter& a_Adapter);
		static void Start(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static void Engaged(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);
		static void PickSub(RE::Actor* a_Actor, Brain& a_Brain, const struct Adapter& a_Adapter, double a_Now);
		[[nodiscard]] static const struct Choice* Weighted(RE::Actor* a_Actor, std::span<const struct Choice> a_Rows, float a_Idle, RE::Actor* a_Partner = nullptr);
		static void Recover(RE::Actor* a_Actor, Brain& a_Brain, double a_Now);

		static void Enter(RE::Actor* a_Actor, Brain& a_Brain, Phase a_Phase, double a_Now);
		static void Bail(RE::Actor* a_Actor, Brain& a_Brain, std::string_view a_Why, double a_Now);
		static void Apply(RE::Actor* a_Actor, Brain& a_Brain, std::uint8_t a_Effects);
		static void Undo(RE::Actor* a_Actor, Brain& a_Brain);
		static void UndoStance(RE::Actor* a_Actor, Brain& a_Brain);
		static void Forget(RE::FormID a_Actor);

		[[nodiscard]] static bool Claimed(RE::FormID a_Target, RE::FormID a_By);
		[[nodiscard]] static bool Refused(const Brain& a_Brain, RE::FormID a_Target, std::uint8_t a_Adapter, double a_Now);
		static void Refuse(Brain& a_Brain, RE::FormID a_Target, std::uint8_t a_Adapter, double a_Until);
		[[nodiscard]] static bool SubRefused(const Brain& a_Brain, std::string_view a_Action, double a_Now);
		static void RefuseSub(Brain& a_Brain, std::string_view a_Action, double a_Until);

		static void Log(RE::Actor* a_Actor, std::string_view a_Line);

		// The three windows below are seconds of animation played, read from the clips, not real time.

		// How long the registry has to show the branch before a start counts as refused.
		static constexpr double ConfirmWindow = 2.0;

		// How long after a plan ends before the same brain thinks again.
		static constexpr double RecoverCooldown = 2.0;

		// How many picks in a row may find nothing before the branch is told to wrap up. One empty pick
		// only means an action is playing.
		static constexpr std::uint8_t EmptyPicksBeforeBail = 3;

		// How long Recover waits for a running action to end before dropping the stance anyway.
		static constexpr double RecoverWait = 10.0;

		// How long a forced stance gets to settle before the start goes ahead regardless.
		static constexpr double StanceWindow = 6.0;

		// Animation seconds a queued second step may wait for its first step to finish.
		static constexpr double QueueWindow = 12.0;


		static inline absl::flat_hash_map<RE::FormID, Brain> m_Brains = {};
	};
}
