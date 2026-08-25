#include "Data/Persistent.hpp"

namespace GTS {

	//----------------------
	// Overriden Virtuals
	//----------------------

	void Persistent::OnPluginReset() {

		{
			std::unique_lock lock(_Lock);

			ClearData();

			// Ensure we reset them back to inital scales
			// if they are loaded into game memory
			// since skyrim only lazy loads actors
			// that are already in memory it won't reload
			// their nif scales otherwise
			for (const auto& actor : find_actors()) {
				ResetToInitScale(actor);
			}
		}

		logger::info("Persistent: Reset Event");
	}

	void Persistent::OnGameActorReset(Actor* actor) {
		// Fired after a TESReset event
		// This event should be when the game attempts to reset their
		// actor values etc when the cell resets

		//Actor can somehow be null here.
		//This will mean that said actor wont be reset

		logger::trace("Persistent::OnGameActorReset");

		if (!actor) {
			logger::warn("Persistent::ResetActor: Tried to reset null actor");
			return;
		}

		auto key = actor->formID;

		auto it = this->ActorMap.value.find(key);
		if (it != this->ActorMap.value.end()) {
			it->second = {};
			ResetToInitScale(actor);
		}
	}

	//---------------------------
	// Getters
	//---------------------------

	PersistentActorData* Persistent::GetActorData(Actor* actor) {
		if (!actor) {
			return nullptr;
		}

		std::unique_lock lock(_Lock);
		auto key = actor->formID;

		// Lambda to add new ActorData if conditions are met
		auto addActorData = [&]() -> PersistentActorData* {
			if (!actor->Is3DLoaded()) {
				return nullptr;
			}
			if (get_scale(actor) < 0.0f) {
				return nullptr;
			}
			auto [iter, inserted] = ActorMap.value.try_emplace(key);
			return &(iter->second);
		};

		// Attempt to find the actor's data in the map
		auto it = ActorMap.value.find(key);
		if (it != ActorMap.value.end()) {
			return &(it->second);
		}

		// ActorData not found; attempt to add it
		return addActorData();
	}

	PersistentKillCountData* Persistent::GetKillCountData(Actor* actor) {
		if (!actor) {
			return nullptr;
		}

		{
			std::unique_lock lock(_Lock);

			auto key = actor->formID;
			auto it = KillCountMap.value.find(key);

			if (it != KillCountMap.value.end()) {
				return &it->second;
			}

			// Key not found, add new entry
			if (!actor->Is3DLoaded()) {
				return nullptr;
			}
			auto [newIt, inserted] = KillCountMap.value.try_emplace(key);
			return &newIt->second;
		}
	}

	//---------------------------
	// Data Management
	//---------------------------

	void Persistent::ClearData() {

		ActorMap.value.clear();
		KillCountMap.value.clear();

		TrackedCameraState.value       = 0;
		EnableCrawlPlayer.value        = false;
		EnableCrawlFollower.value      = false;
		HugStealCount.value            = 0.0f;
		StolenSize.value               = 0.0f;
		CrushCount.value               = 0.0f;
		STNCount.value                 = 0.0f;
		HandCrushed.value              = 0.0f;
		VoreCount.value                = 0.0f;
		GiantCount.value               = 0.0f;
		MSGSeenTinyCamity.value        = false;
		MSGSeenGrowthSpurt.value       = false;
		MSGSeenAspectOfGTS.value       = false;
		UnlockMaxSizeSliders.value     = false;
	}

	void Persistent::OnSerdeSave(SKSE::SerializationInterface* a_this) {
		logger::info("Serializing Persistent...");

		std::unique_lock lock(_Lock);

		// ---- Mod Settings
		ModSettings.Save(a_this);

		//----- Camera
		TrackedCameraState.Save(a_this);

		//----- Crawk/Sneak State
		EnableCrawlPlayer.Save(a_this);
		EnableCrawlFollower.Save(a_this);

		// ---- Quest Progression
		HugStealCount.Save(a_this);
		StolenSize.Save(a_this);
		CrushCount.Save(a_this);
		STNCount.Save(a_this);
		HandCrushed.Save(a_this);
		VoreCount.Save(a_this);
		GiantCount.Save(a_this);

		// ---- Ability Info
		MSGSeenTinyCamity.Save(a_this);
		MSGSeenGrowthSpurt.Save(a_this);
		MSGSeenAspectOfGTS.Save(a_this);

		// ---- Unlimited Size slider unlocker
		UnlockMaxSizeSliders.Save(a_this);

		//----- Actor Data Structs
		ActorMap.Save(a_this);
		KillCountMap.Save(a_this);
	}

	void Persistent::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {

		std::unique_lock lock(_Lock);

		logger::debug("Persistent OnSerdeLoad Start");

		//----- Camera
		TrackedCameraState.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		//----- Crawk/Sneak State
		EnableCrawlPlayer.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		EnableCrawlFollower.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		// ---- Quest Progression
		HugStealCount.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		StolenSize.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		CrushCount.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		STNCount.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		HandCrushed.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		VoreCount.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		GiantCount.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		// ---- Ability Info
		MSGSeenTinyCamity.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		MSGSeenGrowthSpurt.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		MSGSeenAspectOfGTS.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		// ---- Unlimited Size slider unlocker
		UnlockMaxSizeSliders.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		// ---- Mod Settings
		ModSettings.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		//----- Actor Data structs
		ActorMap.Load(a_this, a_recordType, a_recordVersion, a_recordSize);
		KillCountMap.Load(a_this, a_recordType, a_recordVersion, a_recordSize);

		logger::info("Persistent OnSerdeLoad OK");
	}


	void Persistent::EraseUnloadedData() {
		std::unique_lock lock(_Lock);
		// Create a set to hold the whitelisted FormIDs.
		std::unordered_set<FormID> allowedFormIDs;

		// Always keep FormID 0x14 (Player).
		allowedFormIDs.insert(0x14);

		// Get preserve all currently loaded actors
		for (const Actor* ActorToNotDelete : find_actors()) {
			if (ActorToNotDelete) {
				allowedFormIDs.insert(ActorToNotDelete->formID);
			}
		}

		// Iterate through ActorDataMap and remove entries whose key is not in allowedFormIDs.
		absl::erase_if(ActorMap.value,[&](const auto& entry) {
			return !allowedFormIDs.contains(entry.first);
		});

		// Iterate through KillCountMap and remove entries whose key is not in allowedFormIDs.
		absl::erase_if(KillCountMap.value,[&](const auto& entry) {
			return !allowedFormIDs.contains(entry.first);
		});

		logger::critical("All Unloaded actors have beeen purged from persistent.");
	}
}
