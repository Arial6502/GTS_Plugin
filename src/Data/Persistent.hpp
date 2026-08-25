#pragma once

#include "Data/Util/BasicRecord.hpp"
#include "Data/Util/CompressedRecord.hpp"
#include "Data/Util/MapRecord.hpp"
#include "Data/Storage/PersistentData.hpp"

namespace GTS {

	class Persistent : public EventListener, public CInitSingleton<Persistent> {

		public:
		static void EraseUnloadedData();
		static PersistentKillCountData* GetKillCountData(Actor* actor);
		static PersistentActorData* GetActorData(Actor* actor);


		//----- Actor Data
		static inline Serialization::MapRecord<PersistentActorData, 'ACT_'> ActorMap {};
		static inline Serialization::MapRecord<PersistentKillCountData, 'ACTK'> KillCountMap {};

		//----- Camera
		static inline Serialization::BasicRecord<int, 'TCST'> TrackedCameraState {0};

		//----- Crawl/Sneak State
		static inline Serialization::BasicRecord<bool, 'ECPL'> EnableCrawlPlayer   {false};
		static inline Serialization::BasicRecord<bool, 'ECFL'> EnableCrawlFollower {false};

		// ---- Quest Progression
		static inline Serialization::BasicRecord<float, 'QHSR'> HugStealCount {0.0f};
		static inline Serialization::BasicRecord<float, 'QSSR'> StolenSize    {0.0f};
		static inline Serialization::BasicRecord<float, 'QCCR'> CrushCount    {0.0f};
		static inline Serialization::BasicRecord<float, 'QSTR'> STNCount      {0.0f};
		static inline Serialization::BasicRecord<float, 'QHCR'> HandCrushed   {0.0f};
		static inline Serialization::BasicRecord<float, 'QVRR'> VoreCount     {0.0f};
		static inline Serialization::BasicRecord<float, 'QGCR'> GiantCount    {0.0f};

		// ---- Guide Messages Seen
		static inline Serialization::BasicRecord<bool, 'MSTC'> MSGSeenTinyCamity  {false};
		static inline Serialization::BasicRecord<bool, 'MSGS'> MSGSeenGrowthSpurt {false};
		static inline Serialization::BasicRecord<bool, 'MSAG'> MSGSeenAspectOfGTS {false};

		// ---- Unlimited Size slider unlocker
		static inline Serialization::BasicRecord<bool, 'USSD'> UnlockMaxSizeSliders {false};

		static inline Serialization::CompressedStringRecord<'CONF'> ModSettings {""};

		private:
		static inline std::mutex _Lock;

		static void ClearData();
		void OnPluginReset() override;
		void OnGameActorReset(Actor* actor) override;

		//SKSE Callbacks
		void OnSerdeSave(SKSE::SerializationInterface* a_this) override;
		void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) override;

	};
}
