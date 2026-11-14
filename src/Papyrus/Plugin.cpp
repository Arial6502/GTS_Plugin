#include "Papyrus/Plugin.hpp"
#include "Utils/Text/Text.hpp"

#include "Config/Config.hpp"

#include "Magic/Effects/Common.hpp"
#include "Utils/Actions/VoreUtils.hpp"

#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/AI/Director.hpp"
#include "API/Devourment.hpp"


using namespace RE;
using namespace RE::BSScript;
using namespace GTS;

namespace {

	constexpr std::string_view PapyrusClass = "GTSPlugin";

	void ResetQuestProgression(StaticFunctionTag*) {
		GTS::ResetQuest();
	}

	float Quest_GetProgression(StaticFunctionTag*, int stage) {
		return GTS::GetQuestProgression(stage);
	}

	bool WasDragonEaten(StaticFunctionTag*) {
		auto pc = PlayerCharacter::GetSingleton();
		auto transient = GTS::Transient::GetActorData(pc);
		if (transient) {
			return transient->DragonWasEaten;
		}
		return false;
	}

	void CallDevourmentCompatibility(StaticFunctionTag*, Actor* Pred, Actor* Prey, bool Digested) {
		if (GTS::Config::General.bDevourmentCompat) {
			GTS::Devourment_Compatibility(Pred, Prey, Digested);
		}
	}

	void ForceStartSizeInteraction(StaticFunctionTag*, Actor* Pred, Actor* Prey, int Type) {

		// The numbers are the published contract, so they stay as they are. Everything but Devourment
		// goes through the director, which walks to a target that is out of reach.
		constexpr std::array Branches = {
			"Vore"sv, "Vore"sv, "Stomp"sv, "KickSwipe"sv, "ThighSandwich"sv,
			"ThighCrush"sv, "ButtCrush"sv, "Hug"sv, "Grab"sv,
		};

		if (!Pred) {
			Notify("Pred is null");
			return;
		}

		if (!Prey) {
			Notify("Prey is null");
			return;
		}

		if (Type < 0 || std::cmp_greater_equal(Type, Branches.size())) {
			Notify("Unknown interaction type");
			return;
		}

		// Devourment swallows outright when it is present, and falls through to vore when it is not.
		if (Type == 1 && Devourment::Enabled() && Devourment::Swallow(Pred, Prey, DevourmentLocus::kStomach)) {
			DamageAV(Pred, ActorValue::kStamina, 30.0f);
			return;
		}

		if (!AI::Director::Force(Pred, Prey, Branches.at(Type))) {
			Notify("That interaction is not available");
		}
	}
}

namespace GTS {

	bool register_papyrus_plugin(IVirtualMachine* vm) {

		//Quest
		vm->RegisterFunction("ResetQuestProgression", PapyrusClass, ResetQuestProgression);
		vm->RegisterFunction("Quest_GetProgression", PapyrusClass, Quest_GetProgression);
		vm->RegisterFunction("WasDragonEaten", PapyrusClass, WasDragonEaten);

		//Devourment
		vm->RegisterFunction("CallDevourmentCompatibility", PapyrusClass, CallDevourmentCompatibility);
		vm->RegisterFunction("ForceStartSizeInteraction", PapyrusClass, ForceStartSizeInteraction);

		return true;
	}
}
