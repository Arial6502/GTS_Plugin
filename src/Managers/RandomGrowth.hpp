#pragma once

namespace GTS {

	class RandomGrowth : public EventListener, public CInitSingleton <RandomGrowth> {
		public:
		virtual void OnMainUpdate() override;

		static void RestoreStats(Actor* actor, float multiplier);
	};
}
