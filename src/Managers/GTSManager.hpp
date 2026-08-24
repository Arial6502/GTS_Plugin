#pragma once

namespace GTS {

	class GTSManager : public EventListener, public CInitSingleton <GTSManager> {

		public:
		virtual void OnGameDragonSoulAbsorb() override;
		virtual void OnSKSEDataLoaded() override;
		virtual void OnMainUpdate() override;
		virtual void OnSerdePostLoad() override;


		//Used for profiling
		static inline uint32_t LoadedActorCount = 0;

		// Reapply changes (used after reload events)
		static void reapply(bool force = true);
		static void reapply_actor(Actor* actor, bool force = true);
	};
}
