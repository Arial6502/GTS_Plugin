#pragma once

namespace GTS {

	// Attack suppression is derived, never stored. Nothing here writes a flag onto the actor,
	// so there is no state that can be left set or cleared at the wrong moment - the answer is
	// recomputed from live game state every time it is asked for.
	class AttackManager : public EventListener, public CInitSingleton<AttackManager> {

		public:
		void OnActorUpdate(RE::Actor* a_Actor) override;
		void OnActor3DUnload(RE::Actor* a_Actor) override;
		void OnSerdeRevert(SKSE::SerializationInterface* a_Interface) override;
		[[nodiscard]] static bool ShouldSuppressAttacks(RE::Actor* a_Actor);

		//Who the actor is measured against. Exposed so diagnostics can explain a refusal.
		[[nodiscard]] static RE::Actor* SuppressionTarget(RE::Actor* a_Actor);
	};
}
