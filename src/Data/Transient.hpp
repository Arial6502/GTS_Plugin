#pragma once
#include "Data/Storage/TransientData.hpp"

namespace GTS {

	class Transient final : public EventListener, public CInitSingleton<Transient> {

		public:
		static void EraseUnloadedData();
		static TransientActorData* GetActorData(Actor* actor);

		private:
		virtual void OnPluginReset() override;
		virtual void OnGameActorReset(Actor* actor) override;


		static inline std::mutex _Lock;
		static inline std::unordered_map<FormID, TransientActorData> TempActorDataMap {};
	};
}
