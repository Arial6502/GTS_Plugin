#pragma once

// Module that handles footsteps

namespace GTS {

	class TremorManager : public EventListener, public CInitSingleton <TremorManager> {
		public:
		virtual void OnImpact(const Impact& impact) override;
	};
}
