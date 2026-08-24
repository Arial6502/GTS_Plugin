#pragma once

namespace GTS {

	static const std::vector<std::string_view> Butt_Zones = {"NPC R Butt", "NPC L Butt", "NPC R Thigh [RThg]", "NPC L Thigh [LThg]"};

	enum class FurnitureType
	{
		None,
		Chair,
		Bed,
		Lean
	};

	enum class FurnitureDamageSwitch
	{
		EnableDamage,
		DisableDamage,
	};

	class FurnitureManager : public EventListener, public CInitSingleton<FurnitureManager> {
		public:
        virtual void OnGameFurnitureChange(RE::Actor* activator, TESObjectREFR* object, bool enter) override;
		virtual void OnActorLoad3D(RE::Actor* actor) override;
		virtual void OnActorUpdate(RE::Actor* actor) override;

		static void RecordAndHandleFurnState(RE::Actor* activator, TESObjectREFR* object, bool enter);
		static void Furniture_EnableButtHitboxes(RE::Actor* activator, FurnitureDamageSwitch type);
		static void ResetTrackedFurniture(RE::Actor* actor);

		static bool ValidActor(Actor* a_actor);
		static bool ValidFurn(TESObjectREFR* a_obj);
	};
}