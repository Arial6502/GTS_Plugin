#include "Actions/Reactions/FurnitureReactions.hpp"

#include "Actions/Core/ActionReactions.hpp"
#include "Managers/FurnitureManager.hpp"

namespace {
	using namespace GTS;
	using namespace GTS::Actions;

	void OnChairIdle(RE::Actor* a_Actor) {

		if (IsFemale(a_Actor)) {
			FurnitureManager::Furniture_EnableButtHitboxes(a_Actor, FurnitureDamageSwitch::EnableDamage);
		}
	}

	void OnChairGetUp(RE::Actor* a_Actor) {

		if (IsFemale(a_Actor)) {
			FurnitureManager::Furniture_EnableButtHitboxes(a_Actor, FurnitureDamageSwitch::DisableDamage);
		}
	}

	constexpr ReactionDef Table[] = {
		{ .Tag = "ChairIdle",       .Handler = OnChairIdle },
		{ .Tag = "idleChairGetUp",  .Handler = OnChairGetUp },
	};
}

namespace GTS::Actions {

	void RegisterFurnitureReactions() {
		Reactions::Register(Table, "Furniture");
	}
}
