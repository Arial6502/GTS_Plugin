#pragma once

namespace GTS {

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
