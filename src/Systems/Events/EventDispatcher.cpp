#include "Systems/Events/EventDispatcher.hpp"
#include "Systems/Events/EventListener.hpp"
#include "Systems/Events/EventData.hpp"

namespace {

	template <std::size_t N>
	struct FixedString {
		char Data[N] {};
		consteval FixedString(const char (&a_str)[N]) {
			std::copy_n(a_str, N, Data);
		}
		[[nodiscard]] constexpr std::string_view View() const {
			return {Data, N - 1};
		}
	};

	[[nodiscard]] constexpr std::string_view StripQualification(const std::string_view a_raw) {

		if (const std::size_t Qualifier = a_raw.rfind("::"); Qualifier != std::string_view::npos) {
			return a_raw.substr(Qualifier + 2);
		}

		if (const std::size_t Keyword = a_raw.rfind(' '); Keyword != std::string_view::npos) {
			return a_raw.substr(Keyword + 1);
		}

		return a_raw;
	}

	[[nodiscard]] std::string_view GetTypeName(const std::type_info& ti) {
		return StripQualification(ti.name());
	}

	template <FixedString Function>
	[[nodiscard]] std::string_view ListenerScopeName(const GTS::EventListener* a_listener) {

		static tbb::concurrent_unordered_map<const std::type_info*, std::string> Cache;
		const std::type_info* Type = &typeid(*a_listener);

		if (const auto it = Cache.find(Type); it != Cache.end()) {
			return it->second;
		}

		std::string Name;
		Name.reserve(4 + GetTypeName(*Type).size() + Function.View().size());
		Name += "::";
		Name += GetTypeName(*Type);
		Name += "::";
		Name += Function.View();

		return Cache.emplace(Type, std::move(Name)).first->second;
	}

	std::atomic_bool FirstMenuLoadDone {false};
}

namespace GTS {

	//-------------------
	// Event Dispatcher
	//-------------------

	void EventDispatcher::AddListener(EventListener* a_listener) {
		if (!a_listener) return;

		{
			std::lock_guard lock(m_lock);

			const std::size_t Slot = m_count.load(std::memory_order_relaxed);

			if (Slot >= MaxListeners) {
				logger::critical("Listener limit of {} reached, with: {}. Raise EventDispatcher::MaxListeners.", MaxListeners, GetTypeName(typeid(*a_listener)));
				ReportAndExit(
					fmt::format(
						"Event registry limit of {} reached\n"
						"Failed on: {}. Raise EventDispatcher::MaxListeners.", 
						MaxListeners, 
						GetTypeName(typeid(*a_listener))).c_str()
				);
				return;
			}

			logger::trace("Registering Listener: {}", GetTypeName(typeid(*a_listener)));

			m_listeners[Slot].store(a_listener, std::memory_order_relaxed);

			// Publishes the slot write above to any thread already dispatching.
			// Must stay the last statement: it is what makes the entry visible.
			m_count.store(Slot + 1, std::memory_order_release);
		}
	}

	void EventDispatcher::Init(uint32_t a_serdeID) {

		//Register SKSE Eventlistener
		if (const SKSE::MessagingInterface* mi = SKSE::GetMessagingInterface()) {
			logger::trace("Registering SKSE Messaging Interface Listener");
			mi->RegisterListener(SKSEDispatch);
		}

		//Setup SKSE Serialization callbacks
		if (const SKSE::SerializationInterface* serde = SKSE::GetSerializationInterface()) {
			logger::trace("Registering SKSE Serialization Interface Callbacks with ID: {}", a_serdeID);

			serde->SetUniqueID(a_serdeID);

			serde->SetLoadCallback(SerdeDispatchLoad);
			serde->SetSaveCallback(SerdeDispatchSave);
			serde->SetRevertCallback(SerdeDispatchRevert);
			//serde->SetFormDeleteCallback(SKSEDispatchFormDelete);

		}

		//Setup scripted game event callbacks
		if (ScriptEventSourceHolder* evtSrcHolder = ScriptEventSourceHolder::GetSingleton()) {

			logger::trace("Registering GameEvents");

			evtSrcHolder->AddEventSink<TESResetEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESContainerChangedEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESDeathEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESHitEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESObjectLoadedEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESEquipEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESTrackedStatsEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESFurnitureEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESFormDeleteEvent>(&GetSingleton());

			/*
			evtSrcHolder->AddEventSink<TESUniqueIDChangeEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESResolveNPCTemplatesEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESCellAttachDetachEvent>(&GetSingleton());
			evtSrcHolder->AddEventSink<TESMoveAttachDetachEvent>(&GetSingleton());
			*/
		}

		//Setup UI game event callbacks
		if (UI* ui = UI::GetSingleton()) {
			ui->AddEventSink<MenuOpenCloseEvent>(&GetSingleton());
			logger::info("Successfully registered MenuOpenCloseEventHandler");
		}
	}

	//-----------------
	// SKSE Callbacks
	//-----------------

	void EventDispatcher::SKSEDispatch(SKSE::MessagingInterface::Message* a_message) {

		switch (a_message->type) {

			// Called after all plugins have finished running SKSEPluginLoad.
			case SKSE::MessagingInterface::kPostLoad:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEPostLoad);
					a_lst->OnSKSEPostLoad();
				});
				break;
			}

			// Called after all kPostLoad message handlers have run.
			case SKSE::MessagingInterface::kPostPostLoad:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEPostPostLoad);
					a_lst->OnSKSEPostPostLoad();
				});
				break;
			}

			// Called when input data has been found.
			case SKSE::MessagingInterface::kInputLoaded:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEInputLoaded);
					a_lst->OnSKSEInputLoaded();
				});
				break;
			}

			// All ESM/ESL/ESP plugins have loaded, main menu is now active.
			case SKSE::MessagingInterface::kDataLoaded:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEDataLoaded);
					a_lst->OnSKSEDataLoaded();
				});
				break;
			}


			// Player's selected save game has finished loading.
			case SKSE::MessagingInterface::kPostLoadGame:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEPostLoadGame);
					a_lst->OnSKSEPostLoadGame();
				});
				break;
			}

			// Player starts a new game from main menu.
			case SKSE::MessagingInterface::kNewGame:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSENewGame);
					a_lst->OnSKSENewGame();
				});

				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnPluginReset);
					a_lst->OnPluginReset();
				});

				break;
			}

			// Player selected a game to load, but it hasn't loaded yet, data will be the name of the loaded save.
			case SKSE::MessagingInterface::kPreLoadGame:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEPreLoadGame);
					a_lst->OnSKSEPreLoadGame();
				});

				break;
			}

			// The player has saved a game.
			case SKSE::MessagingInterface::kSaveGame:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSESaveGame);
					a_lst->OnSKSESaveGame();
				});
				break;
			}

			// The player deleted a saved game from within the load menu, data will be the save name.
			case SKSE::MessagingInterface::kDeleteGame:
			{
				ForEachListener([](EventListener* a_lst) {
					GTS_PROFILE_LISTENER(a_lst, OnSKSEDeleteGame);
					a_lst->OnSKSEDeleteGame();
				});
				break;
			}

			default:
			{
				logger::trace("Received unhandled SKSE message: {}", a_message->type);
				break;
			}
		}
	}

	/*
	void EventDispatcher::SKSEDispatchFormDelete(VMHandle a_callback) {

		//Lower 32 bits are the FormID of the deleted reference.
		const FormID a_id = static_cast<FormID>(a_callback & 0xFFFFFFFF);

		
		//Test
		{
			static std::atomic<std::uint64_t> Seq {0};

			const TESForm* Probe = a_id ? TESForm::LookupByID(a_id) : nullptr;

			logger::info(
				"FormDelete #{} handle=0x{:016X} upper=0x{:08X} formID=0x{:08X} resolved={} type={} deleted={}",
				Seq.fetch_add(1, std::memory_order_relaxed),
				static_cast<std::uint64_t>(a_callback),
				static_cast<std::uint32_t>(a_callback >> 32),
				a_id,
				Probe != nullptr,
				Probe ? RE::FormTypeToString(Probe->GetFormType()) : "n/a",
				Probe ? Probe->IsDeleted() : false
			);
		}

		// SKSE hooks this off the VM's handle cleanup, so it does NOT only fire for
		// deleted forms. Use form lookup/IsDeleted to verify if the form is actually deleted.
		if (!a_id) {
			return;
		}

		if (TESForm* ref = TESForm::LookupByID(a_id)){
			if (ref->formType == FormType::ActorCharacter) {
				//Form is valid.
				if (ref->IsDeleted()) {
					ForEachListener([a_id](EventListener* a_lst) {
						GTS_PROFILE_LISTENER(a_lst, OnSerdeFormDelete);
						a_lst->OnSKSEFormDelete(a_id);
					});
				}
			}
		}
		else {
			ForEachListener([a_id](EventListener* a_lst) {
				GTS_PROFILE_LISTENER(a_lst, OnSerdeFormDelete);
				a_lst->OnSKSEFormDelete(a_id);
			});
		}
	}*/

	void EventDispatcher::SerdeDispatchLoad(SKSE::SerializationInterface* a_this) {

		ForEachListener([](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdePreLoad);
			a_lst->OnSerdePreLoad();
		});

		std::uint32_t type, version, size;

		while (a_this->GetNextRecordInfo(type, version, size)) {
			ForEachListener([&](EventListener* a_lst) {
				GTS_PROFILE_LISTENER(a_lst, OnSerdeLoad);
				a_lst->OnSerdeLoad(a_this, type, version, size);
			});
		}

		ForEachListener([](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdePostLoad);
			a_lst->OnSerdePostLoad();
		});
	}

	void EventDispatcher::SerdeDispatchSave(SKSE::SerializationInterface* a_this) {

		ForEachListener([](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdePreSave);
			a_lst->OnSerdePreSave();
		});

		ForEachListener([a_this](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdeSave);
			a_lst->OnSerdeSave(a_this);
		});

		ForEachListener([](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdePostSave);
			a_lst->OnSerdePostSave();
		});
	}

	void EventDispatcher::SerdeDispatchRevert(SKSE::SerializationInterface* a_this) {

		ForEachListener([a_this](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdeRevert);
			a_lst->OnSerdeRevert(a_this);
		});

		ForEachListener([a_this](EventListener* a_lst) {
			GTS_PROFILE_LISTENER(a_lst, OnSerdeRevert);
			a_lst->OnPluginReset();
		});

	}

	//*********************************************
	// HOOK DRIVEN
	//*********************************************

	//-----------------------------
	// Periodic / OnFrame Updates
	//-----------------------------

	void EventDispatcher::DispatchMainUpdate() {

		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnMainUpdate);
			listener->OnMainUpdate();
		});
	}

	void EventDispatcher::DispatchActorUpdate(Actor* actor) {

		ForEachListener([actor](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnActorUpdate);
			listener->OnActorUpdate(actor);
		});
	}

	void EventDispatcher::DispatchPapyrusUpdate() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnPapyrusUpdate);
			listener->OnPapyrusUpdate();
		});
	}

	void EventDispatcher::DispatchHavokUpdate() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnHavokUpdate);
			listener->OnHavokUpdate();
		});
	}

	void EventDispatcher::DispatchPostSMPUpdate() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnPostSMPUpdate);
			listener->OnPostSMPUpdate();
		});
	}

	void EventDispatcher::DispatchCameraUpdate() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnCameraUpdate);
			listener->OnCameraUpdate();
		});
	}


	//-----------------------------
	// Actor Related
	//-----------------------------

	void EventDispatcher::DispatchActor3DLoad(Actor* actor) {
		ForEachListener([actor](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, ActorLoaded);
			listener->OnActorLoad3D(actor);
		});
	}

	// Fired from the Actor::Set3D hook when the game clears an actor's 3d. Runs
	// BEFORE the teardown, so listeners still see a valid actor and valid 3d and can
	// release anything they cached from it.
	void EventDispatcher::DispatchActor3DUnload(Actor* actor) {
		ForEachListener([actor](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnActor3DUnload);
			listener->OnActor3DUnload(actor);
		});
	}

	void EventDispatcher::DispatchActorAddPerk(const AddPerkEvent& evt) {
		ForEachListener([evt](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnAddPerk);
			listener->OnActorPerkAdded(evt);
		});
	}

	void EventDispatcher::DispatchActorRemovePerk(const RemovePerkEvent& evt) {
		ForEachListener([evt](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnRemovePerk);
			listener->OnActorPerkRemoved(evt);
		});
	}

	void EventDispatcher::DispatchActorAnimationEvent(Actor* actor, const BSFixedString& a_tag, const BSFixedString& a_payload) {
		const std::string tag = a_tag.c_str();
		const std::string payload = a_payload.c_str();
		ForEachListener([actor, tag, payload](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnActorAnimationChange);
			listener->OnActorAnimationChange(actor, tag, payload);
		});
	}

	void EventDispatcher::DispatchLethalHitEvent(const HitData* a_data) {

		Actor* Agressor = a_data->aggressor.get().get();
		Actor* Victim = a_data->target.get().get();

		if (Victim) {
			//Don't fire on already dead actors
			if (!Victim->IsDead()) {
				ForEachListener([&](EventListener* listener) {
					GTS_PROFILE_LISTENER(listener, OnLethalHit);
					listener->OnLethalHit(Agressor, Victim);
				});
			}
		}
	}

	//-----------------------------
	// Other Hook Based Events
	//-----------------------------

	void EventDispatcher::DispatchImpactEvent(const Impact& impact) {
		ForEachListener([impact](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnImpact);
			listener->OnImpact(impact);
		});
	}

	//-----------------------------------------------
	// BSTEventSink (Game Event) Driven Callbacks
	//-----------------------------------------------

	void EventDispatcher::DispatchGameActorResetEvent(Actor* actor) {
		ForEachListener([actor](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameActorReset);
			listener->OnGameActorReset(actor);
		});
	}

	void EventDispatcher::DispatchGameActorEquipEvent(Actor* actor) {
		ForEachListener([actor](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameActorEquip);
			listener->OnGameActorEquip(actor);
		});
	}

	void EventDispatcher::DispatchGameDragonSoulAbsorbEvent() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameDragonSoulAbsorb);
			listener->OnGameDragonSoulAbsorb();
		});
	}



	void EventDispatcher::DispatchGameHitEvent(const TESHitEvent* evt) {
		ForEachListener([evt](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameHit);
			listener->OnGameHit(evt);
		});
	}

	void EventDispatcher::DispatchGameActorLoadedEvent(Actor* refr) {
		ForEachListener([refr](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameActorLoaded);
			listener->OnGameActorLoaded(refr);
		});
	}

	void EventDispatcher::DispatchGameContainerChangeEvent(const TESContainerChangedEvent* a_evt) {
		ForEachListener([a_evt](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameContainerChanged);
			listener->OnGameContainerChanged(a_evt);
		});
	}

	void EventDispatcher::DispatchGameFormDelete(FormID a_id) {
		if (a_id) {
			ForEachListener([a_id](EventListener* listener) {
				GTS_PROFILE_LISTENER(listener, OnGameFormDelete);
				listener->OnGameFormDelete(a_id);
			});
		}
	}

	void EventDispatcher::DispatchGameFurnitureEvent(const TESFurnitureEvent* a_event) {
		if (a_event) {
			Actor* const actor = skyrim_cast<Actor*>(a_event->actor.get());
			TESObjectREFR* const object = a_event->targetFurniture.get();
			if (actor && object) {
				ForEachListener([actor, object, a_event](EventListener* listener) {
					GTS_PROFILE_LISTENER(listener, OnGameFurnitureChange);
					listener->OnGameFurnitureChange(actor, object, a_event->type == TESFurnitureEvent::FurnitureEventType::kEnter);
				});
			}
		}
	}

	void EventDispatcher::DispatchGameDeathEvent(const TESDeathEvent* a_event) {

		Actor* Killer = skyrim_cast<Actor*>(a_event->actorKiller.get());
		Actor* Victim = skyrim_cast<Actor*>(a_event->actorDying.get());
		const bool Dead = a_event->dead;

		ForEachListener([Killer, Victim, Dead](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGameDeath);
			listener->OnGameDeath(Killer, Victim, Dead);
		});
	}

	void EventDispatcher::DispatchGameMenuChangeEvent(const MenuOpenCloseEvent* a_evt) {

		if (a_evt) {

			ForEachListener([a_evt](EventListener* listener) {
				GTS_PROFILE_LISTENER(listener, OnGameMenuChange);
				listener->OnGameMenuChange(a_evt);
			});

			if (a_evt->menuName == RE::MainMenu::MENU_NAME && a_evt->opening && !FirstMenuLoadDone.exchange(true)) {
				ForEachListener([](EventListener* a_lst) {
					a_lst->OnGameMainMenuFullyLoaded();
				});
			}
		}
	}

	//*********************************************
	// Plugin-Local Events
	//*********************************************

	//-----------------------------
	// Actor Related
	//-----------------------------

	void EventDispatcher::DispatchHighHeelEquiped(const HighheelEquip& evt) {
		ForEachListener([evt](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnHighheelEquip);
			listener->OnHighHeelEquiped(evt);
		});
	}

	void EventDispatcher::DispatchGTSLevelUpEvent(Actor* a_actor) {
		ForEachListener([a_actor](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnGTSLevelUp);
			listener->OnGTSLevelUp(a_actor);
		});
	}

	//-----------------------------
	// Config Related
	//-----------------------------

	void EventDispatcher::DispachModConfigReset() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnModConfigReset);
			listener->OnModConfigReset();
		});
	}

	void EventDispatcher::DispatchModConfigRefresh() {
		ForEachListener([](EventListener* listener) {
			GTS_PROFILE_LISTENER(listener, OnModConfigRefresh);
			listener->OnModConfigRefresh();
		});
	}

	//--------------------------------
	// BSTEvent Game Event Wrappers
	//--------------------------------


	//---- TESHitEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESHitEvent* a_evt, BSTEventSource<TESHitEvent>* a_evtSrc) {
		if (a_evt) DispatchGameHitEvent(a_evt);
		return BSEventNotifyControl::kContinue;
	}

	//---- TESObjectLoadedEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESObjectLoadedEvent* a_evt, BSTEventSource<TESObjectLoadedEvent>* a_evtSrc) {
		if (a_evt && a_evt->loaded) {
			if (Actor* const refr = TESForm::LookupByID<Actor>(a_evt->formID)) {
				DispatchGameActorLoadedEvent(refr);
			}
		}
		return BSEventNotifyControl::kContinue;
	}

	//---- TESResetEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESResetEvent* a_evt, BSTEventSource<TESResetEvent>* a_evtSrc) {
		if (a_evt) {
			if (TESObjectREFR* object = a_evt->object.get()) {
				if (Actor* const actor = TESForm::LookupByID<Actor>(object->formID)) {
					DispatchGameActorResetEvent(actor);
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}

	//---- TESEquipEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESEquipEvent* a_evt, BSTEventSource<TESEquipEvent>* a_evtSrc) {
		if (a_evt && a_evt->actor) {
			if (Actor* actor = TESForm::LookupByID<Actor>(a_evt->actor->formID)) {
				DispatchGameActorEquipEvent(actor);
			}
		}
		return BSEventNotifyControl::kContinue;
	}

	//---- TESTrackedStatsEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESTrackedStatsEvent* a_evt, BSTEventSource<TESTrackedStatsEvent>* a_evtSrc) {
		static constexpr std::string_view _DragonSpulsStr = "Dragon Souls Collected";
		if (a_evt) {
			if (a_evt->stat == _DragonSpulsStr) {
				DispatchGameDragonSoulAbsorbEvent();
			}
		}
		return BSEventNotifyControl::kContinue;
	}

	//---- MenuOpenCloseEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const MenuOpenCloseEvent* a_evt, BSTEventSource<MenuOpenCloseEvent>* a_evtSrc) {
		if (a_evt) DispatchGameMenuChangeEvent(a_evt);
		return RE::BSEventNotifyControl::kContinue;
	}

	//---- TESFurnitureEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESFurnitureEvent* a_evt, BSTEventSource<TESFurnitureEvent>* a_evtSrc) {
		if (a_evt) DispatchGameFurnitureEvent(a_evt);
		return RE::BSEventNotifyControl::kContinue;
	}

	//---- TESDeathEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESDeathEvent* a_evt, BSTEventSource<TESDeathEvent>* a_evtSrc) {
		if (a_evt) DispatchGameDeathEvent(a_evt);
		return RE::BSEventNotifyControl::kContinue;
	}

	//---- TESContainerChangedEvent
	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESContainerChangedEvent* a_evt, BSTEventSource<TESContainerChangedEvent>* a_evtSrc) {
		if (a_evt) DispatchGameContainerChangeEvent(a_evt);
		return RE::BSEventNotifyControl::kContinue;
	}

	BSEventNotifyControl EventDispatcher::ProcessEvent(const TESFormDeleteEvent* a_evt, BSTEventSource<TESFormDeleteEvent>* a_evtSrc) {
		if (a_evt) DispatchGameFormDelete(a_evt->formID);
		return RE::BSEventNotifyControl::kContinue;
	}
}
