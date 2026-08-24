#pragma once

// Module that handles changing spectator targets

namespace GTS {

	class SpectatorManager : public EventListener, public CInitSingleton <SpectatorManager> {
		public:
		virtual void OnMainUpdate() override;
		virtual void OnPluginReset() override;
		virtual void OnGameActorReset(Actor* actor) override;

		static Actor* GetCameraTarget();
		static void SetCameraTarget(RE::Actor* a_Actor, bool aTemporary);
		static bool IsCameraTargetPlayer();
		static void ResetTarget(bool aDoFullReset);

		private:
		static inline FormID Target = 0x14;
		static inline volatile bool TrackedActorLost = false;

	};
}
