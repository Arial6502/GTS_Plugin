#pragma once

namespace GTS {

	class ExplosionManager : public EventListener, public CInitSingleton <ExplosionManager> {
		public:
		virtual void OnImpact(const Impact& impact) override;
	};

}
