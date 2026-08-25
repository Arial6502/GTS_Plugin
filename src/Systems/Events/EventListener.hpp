#pragma once

#include "Systems/Events/EventData.hpp"

namespace GTS {

	class EventListener {
		public:
		EventListener() = default;
		virtual ~EventListener() = default;
		EventListener(EventListener const&) = delete;
		EventListener& operator=(EventListener const&) = delete;

		virtual void OnMainUpdate() {}							// Called on Live (non paused) gameplay at the end of the game loop
		virtual void OnPapyrusUpdate() {}                       // Called on Papyrus Update
		virtual void OnHavokUpdate() {}                         // Called on Havok update (when processing hitjobs)
		virtual void OnPostSMPUpdate() {}                       // Called right after SMP has modified world transforms.
		virtual void OnCameraUpdate() {}                        // Called when the camera update event is fired (in the TESCameraState)

		// Fired when a foot lands
		virtual void OnImpact(const Impact& impact) {}
		virtual void OnActorLoad3D(Actor* actor) {}                      // Fired when the game requestes an actor to be 3d Loaded.
		virtual void OnActor3DUnload(Actor* actor) {}                    // Fired immediately BEFORE an actor's 3d is torn down. Actor and its current 3d are still valid, so cached node pointers must be released here.
		virtual void OnActorPerkAdded(const AddPerkEvent& evt) {}        // Fired when a perk is added
		virtual void OnActorPerkRemoved(const RemovePerkEvent& evt) {}   // Fired when a perk about to be removed
		virtual void OnActorUpdate(RE::Actor* actor) {} 		         // Called per frame for each currently loaded actor
		virtual void OnLethalHit(Actor* a_source, Actor* a_target) {}    // Hitdata based death event
		virtual void OnActorAnimationChange(Actor* actor, const std::string_view& tag, const std::string_view& payload) {} // Fired when a actor animation event occurs

		virtual void OnGameActorReset(Actor* actor) {}
		virtual void OnGameActorEquip(Actor* actor) {} 		                                   // Called when an actor has an item equipped
		virtual void OnGameDragonSoulAbsorb() {}                                               // Called when Player absorbs dragon soul
		virtual void OnGameMenuChange(const MenuOpenCloseEvent* menu_event) {}                 // Fired when a skyrim menu change event occurs
		virtual void OnGameFurnitureChange(Actor* user, TESObjectREFR* object, bool enter) {}  // Fired when actor enters/exits furniture
		virtual void OnGameDeath(Actor* a_killer, Actor* a_victim, bool a_dead) {}             // GameEvent based Actor Death
		virtual void OnGameContainerChanged(const TESContainerChangedEvent* a_evt) {}
		virtual void OnGameActorLoaded(Actor* refr) {} 
		virtual void OnGameHit(const TESHitEvent* evt) {}                                      // Called when a papyrus hit event is fired
		virtual void OnGameMainMenuFullyLoaded() {}
		virtual void OnGameFormDelete(FormID a_id) {}

		virtual void OnModConfigReset() {}   //Fires If Config settings are reset.
		virtual void OnModConfigRefresh() {} //Fires when a config refresh is requested.
		
		virtual void OnGTSLevelUp(Actor* a_actor) {}                 //Fires when a GTS gains a level.
		virtual void OnHighHeelEquiped(const HighheelEquip& evt) {}  // Fired when a highheel is (un)equiped or when an actor is loaded with HH

		virtual void OnPluginReset() {} // Called on game load started (not yet finished) and when new game is selected

		//SKSESerialization
		virtual void OnSerdePreSave() {}                                    //Fired before data is saved
		virtual void OnSerdeSave(SKSE::SerializationInterface* a_this) {}   //cosave data save callback
		virtual void OnSerdePostSave() {}                                   //Fires right after data is saved
		virtual void OnSerdePreLoad() {}                                    //fires right before save data is loaded
		virtual void OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_recordType, std::uint32_t a_recordVersion, std::uint32_t a_recordSize) {} //Save data load callback
		virtual void OnSerdePostLoad() {}                                   //Fires right after save data is loaded
		virtual void OnSerdeRevert(SKSE::SerializationInterface* a_this) {} //OnRevert callback fires when you return to the main menu or load a different save when one is already loaded.
		virtual void OnSKSEFormDelete(RE::FormID a_id) {}                   //Fired when the game instructs the VM to perform its temporary reference cleanup

		//SKSE Events
		virtual void OnSKSEPostLoad() {}
		virtual void OnSKSEPostPostLoad() {}
		virtual void OnSKSEInputLoaded() {}
		virtual void OnSKSEDataLoaded() {}
		virtual void OnSKSEPostLoadGame() {}
		virtual void OnSKSENewGame() {}
		virtual void OnSKSEPreLoadGame() {}
		virtual void OnSKSESaveGame() {}
		virtual void OnSKSEDeleteGame() {}

		// Dense registration index, assigned by EventDispatcher::AddListener. The profiler
		// uses it to reach a dispatch site's slot with an array index instead of a lookup.
		[[nodiscard]] std::uint32_t GetProfilerIndex() const noexcept { return m_profilerIndex; }
		void SetProfilerIndex(std::uint32_t a_index) noexcept { m_profilerIndex = a_index; }

		private:
		std::uint32_t m_profilerIndex = 0xFFFFFFFFu;
	};
}

// Every event, listed once. The dispatcher builds its per event subscriber lists, its EventId
// enum and its override traits from this, so adding a virtual above means a new line here as well.
#define GTS_EVENT_LIST(X)       \
    X(OnMainUpdate)             \
    X(OnPapyrusUpdate)          \
    X(OnHavokUpdate)            \
    X(OnPostSMPUpdate)          \
    X(OnCameraUpdate)           \
    X(OnImpact)                 \
    X(OnActorLoad3D)            \
    X(OnActor3DUnload)          \
    X(OnActorPerkAdded)         \
    X(OnActorPerkRemoved)       \
    X(OnActorUpdate)            \
    X(OnLethalHit)              \
    X(OnActorAnimationChange)   \
    X(OnGameActorReset)         \
    X(OnGameActorEquip)         \
    X(OnGameDragonSoulAbsorb)   \
    X(OnGameMenuChange)         \
    X(OnGameFurnitureChange)    \
    X(OnGameDeath)              \
    X(OnGameContainerChanged)   \
    X(OnGameActorLoaded)        \
    X(OnGameHit)                \
    X(OnGameMainMenuFullyLoaded)\
    X(OnGameFormDelete)         \
    X(OnModConfigReset)         \
    X(OnModConfigRefresh)       \
    X(OnGTSLevelUp)             \
    X(OnHighHeelEquiped)        \
    X(OnPluginReset)            \
    X(OnSerdePreSave)           \
    X(OnSerdeSave)              \
    X(OnSerdePostSave)          \
    X(OnSerdePreLoad)           \
    X(OnSerdeLoad)              \
    X(OnSerdePostLoad)          \
    X(OnSerdeRevert)            \
    X(OnSKSEFormDelete)         \
    X(OnSKSEPostLoad)           \
    X(OnSKSEPostPostLoad)       \
    X(OnSKSEInputLoaded)        \
    X(OnSKSEDataLoaded)         \
    X(OnSKSEPostLoadGame)       \
    X(OnSKSENewGame)            \
    X(OnSKSEPreLoadGame)        \
    X(OnSKSESaveGame)           \
    X(OnSKSEDeleteGame)         

namespace GTS {

    enum class EventId : std::uint8_t {
        #define GTS_EVENT_ENUM(a_name) a_name,
        GTS_EVENT_LIST(GTS_EVENT_ENUM)
        #undef GTS_EVENT_ENUM
        kTotal
    };

    inline constexpr std::size_t kEventCount = static_cast<std::size_t>(EventId::kTotal);

    namespace EventOverride {

        #define GTS_EVENT_TRAIT(a_name)                                                      \
            template <typename T, bool Visible = requires { &T::a_name; }>                   \
            struct Probe_##a_name : std::true_type {};                                       \
                                                                                             \
            template <typename T>                                                            \
            struct Probe_##a_name<T, true> : std::bool_constant<                             \
                !std::is_same_v<decltype(&T::a_name), decltype(&EventListener::a_name)>> {}; \
                                                                                             \
            template <typename T>                                                            \
            inline constexpr bool a_name = Probe_##a_name<T>::value;

        GTS_EVENT_LIST(GTS_EVENT_TRAIT)
        #undef GTS_EVENT_TRAIT
    }
}