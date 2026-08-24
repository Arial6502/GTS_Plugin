#pragma once

#include "Systems/Events/EventData.hpp"

namespace GTS {

	class EventListener;

	class EventDispatcher : 
		public CInitSingleton<EventDispatcher>, 
		public BSTEventSink<TESHitEvent>,
		public BSTEventSink<TESObjectLoadedEvent>,
		public BSTEventSink<TESResetEvent>,
		public BSTEventSink<TESEquipEvent>,
		public BSTEventSink<TESTrackedStatsEvent>,
		public BSTEventSink<TESFurnitureEvent>,
		public BSTEventSink<TESDeathEvent>,
		public BSTEventSink<TESContainerChangedEvent>,
		public BSTEventSink<MenuOpenCloseEvent>,
		public BSTEventSink<TESFormDeleteEvent>/*,
		public BSTEventSink<TESUniqueIDChangeEvent>,
		public BSTEventSink<TESResolveNPCTemplatesEvent>,
		public BSTEventSink<TESCellAttachDetachEvent>,
		public BSTEventSink<TESMoveAttachDetachEvent>*/{

		public:

		static void AddListener(EventListener* a_listener);

		template <typename T>
		static void AddListener() {
			AddListener(&T::GetSingleton());
		}

		static void Init(uint32_t a_serdeID);

		//SKSE Events
		static void SKSEDispatch(SKSE::MessagingInterface::Message* a_message);
		//static void SKSEDispatchFormDelete(RE::VMHandle a_callback);
		static void SerdeDispatchLoad(SKSE::SerializationInterface* a_this);
		static void SerdeDispatchSave(SKSE::SerializationInterface* a_this);
		static void SerdeDispatchRevert(SKSE::SerializationInterface* a_this);

		//Hook-Driven Update Events
		static void DispatchMainUpdate();
		static void DispatchActorUpdate(RE::Actor* actor);
		static void DispatchPapyrusUpdate();
		static void DispatchHavokUpdate();
		static void DispatchPostSMPUpdate();
		static void DispatchCameraUpdate();

		static void DispatchActor3DLoad(RE::Actor* actor);
		static void DispatchActor3DUnload(RE::Actor* actor);
		static void DispatchActorAddPerk(const AddPerkEvent& evt);
		static void DispatchActorRemovePerk(const RemovePerkEvent& evt);
		static void DispatchActorAnimationEvent(RE::Actor* actor, const RE::BSFixedString& a_tag, const RE::BSFixedString& a_payload);

		static void DispatchImpactEvent(const Impact& impact);

		static void DispatchHighHeelEquiped(const HighheelEquip& evt);
		static void DispachModConfigReset();
		static void DispatchModConfigRefresh();
		static void DispatchLethalHitEvent(const RE::HitData* a_data);
		static void DispatchGTSLevelUpEvent(RE::Actor* a_actor);


		//BSTEventSink-Based Events
		static void DispatchGameHitEvent(const RE::TESHitEvent* evt);
		static void DispatchGameActorLoadedEvent(Actor* refr);
		static void DispatchGameContainerChangeEvent(const TESContainerChangedEvent* a_evt);
		static void DispatchGameFormDelete(FormID a_id);
		static void DispatchGameFurnitureEvent(const TESFurnitureEvent* a_event);
		static void DispatchGameDeathEvent(const TESDeathEvent* a_event);
		static void DispatchGameMenuChangeEvent(const RE::MenuOpenCloseEvent* a_evt);
		static void DispatchGameActorResetEvent(RE::Actor* actor);
		static void DispatchGameActorEquipEvent(RE::Actor* actor);
		static void DispatchGameDragonSoulAbsorbEvent();

		private:

		//BTSEventSink Subscribers
		BSEventNotifyControl ProcessEvent(const TESHitEvent* a_evt,              BSTEventSource<TESHitEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESObjectLoadedEvent* a_evt,     BSTEventSource<TESObjectLoadedEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESResetEvent* a_evt,            BSTEventSource<TESResetEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESEquipEvent* a_evt,            BSTEventSource<TESEquipEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESTrackedStatsEvent* a_evt,     BSTEventSource<TESTrackedStatsEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent* a_evt,       BSTEventSource<MenuOpenCloseEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESFurnitureEvent* a_evt,        BSTEventSource<TESFurnitureEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESDeathEvent* a_evt,            BSTEventSource<TESDeathEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESContainerChangedEvent* a_evt, BSTEventSource<TESContainerChangedEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESFormDeleteEvent* a_evt,          BSTEventSource<TESFormDeleteEvent>* a_evtSrc) override;
	 /* BSEventNotifyControl ProcessEvent(const TESUniqueIDChangeEvent* a_evt,      BSTEventSource<TESUniqueIDChangeEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESResolveNPCTemplatesEvent* a_evt, BSTEventSource<TESResolveNPCTemplatesEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESCellAttachDetachEvent* a_evt,    BSTEventSource<TESCellAttachDetachEvent>* a_evtSrc) override;
		BSEventNotifyControl ProcessEvent(const TESMoveAttachDetachEvent* a_evt,    BSTEventSource<TESMoveAttachDetachEvent>* a_evtSrc) override;*/

		//Max allowed registered listeners, increase if needed.
        static constexpr std::size_t MaxListeners = 256;

        static inline std::mutex m_lock;
        static inline std::array<std::atomic<EventListener*>, MaxListeners> m_listeners {};
        static inline std::atomic<std::size_t> m_count {0};

        template <typename Func>
        static void ForEachListener(Func&& func) {

            const std::size_t Count = m_count.load(std::memory_order_acquire);

            for (std::size_t i = 0; i < Count; ++i) {
                func(m_listeners[i].load(std::memory_order_relaxed));
            }
        }
	};
}