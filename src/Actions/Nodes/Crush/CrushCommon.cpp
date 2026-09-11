#include "Actions/Nodes/Crush/CrushCommon.hpp"
#include "Actions/Core/Possession.hpp"
#include "Managers/Rumble.hpp"
#include "Managers/Animation/Controllers/ButtCrushController.hpp"
#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Utils/Actions/ButtCrushUtils.hpp"

namespace {

	using namespace GTS;

	constexpr std::array BODY_NODES = {
		"NPC COM [COM ]"sv,
		"NPC L Foot [Lft ]"sv,
		"NPC R Foot [Rft ]"sv,
		"NPC L Toe0 [LToe]"sv,
		"NPC R Toe0 [RToe]"sv,
		"NPC L Calf [LClf]"sv,
		"NPC R Calf [RClf]"sv,
		"NPC L PreRearCalf"sv,
		"NPC R PreRearCalf"sv,
		"NPC L FrontThigh"sv,
		"NPC R FrontThigh"sv,
		"NPC R RearCalf [RrClf]"sv,
		"NPC L RearCalf [RrClf]"sv,
		"NPC L UpperarmTwist1 [LUt1]"sv,
		"NPC L UpperarmTwist2 [LUt2]"sv,
		"NPC L Forearm [LLar]"sv,
		"NPC L ForearmTwist2 [LLt2]"sv,
		"NPC L ForearmTwist1 [LLt1]"sv,
		"NPC L Hand [LHnd]"sv,
		"NPC R UpperarmTwist1 [RUt1]"sv,
		"NPC R UpperarmTwist2 [RUt2]"sv,
		"NPC R Forearm [RLar]"sv,
		"NPC R ForearmTwist2 [RLt2]"sv,
		"NPC R ForearmTwist1 [RLt1]"sv,
		"NPC R Hand [RHnd]"sv,
	};

	constexpr std::array BREAST_NODES = {
		"NPC L Breast"sv,
		"NPC R Breast"sv,
		"L Breast02"sv,
		"R Breast02"sv,
	};

	constexpr std::array IMPACT_NODES = {
		"NPC R Thigh [RThg]"sv,
		"NPC L Thigh [LThg]"sv,
		"NPC R Butt"sv,
		"NPC L Butt"sv,
		"NPC Spine [Spn0]"sv,
		"NPC Spine1 [Spn1]"sv,
		"NPC Spine2 [Spn2]"sv,
	};

	// BodyRumbleNodes followed by BreastRumbleNodes, so the breast set can be handed out as one span.
	const std::vector<std::string_view> ALL_NODES = [] {
		std::vector<std::string_view> out(BODY_NODES.begin(), BODY_NODES.end());
		out.insert(out.end(), BREAST_NODES.begin(), BREAST_NODES.end());
		return out;
	}();

	constexpr std::string_view NODE_PELVIS = "NPC Pelvis [Pelv]";
}

namespace GTS::Actions::Crush {

	std::span<const std::string_view> BodyRumbleNodes() {
		return {ALL_NODES.data(), BODY_NODES.size()};
	}

	std::span<const std::string_view> BreastRumbleNodes() {
		return ALL_NODES;
	}

	std::span<const std::string_view> ImpactBodyNodes() {
		return IMPACT_NODES;
	}

	void StartRumble(std::string_view a_Tag, RE::Actor& a_Actor, float a_Power, float a_Halflife, std::span<const std::string_view> a_Nodes) {
		for (const auto& node : a_Nodes) {
			Rumbling::Start(std::format("{}_{}", a_Tag, node), &a_Actor, a_Power, a_Halflife, node);
		}
	}

	void StopRumble(std::string_view a_Tag, RE::Actor& a_Actor, std::span<const std::string_view> a_Nodes) {
		for (const auto& node : a_Nodes) {
			Rumbling::Stop(std::format("{}_{}", a_Tag, node), &a_Actor);
		}
	}

	void RecordStart(RE::Actor* a_Giant) {
		if (a_Giant) {
			RecordStartButtCrushSize(a_Giant);
		}
	}

	void RestoreSize(RE::Actor* a_Giant) {

		if (!a_Giant) {
			return;
		}

		if (auto* transient = Transient::GetActorData(a_Giant); transient && transient->ButtCrushStartScale > 0.0f) {
			SetButtCrushSize(a_Giant, 0.0f, true);
		}

		ModGrowthCount(a_Giant, 0, true);
	}

	void CancelHeldTasks(RE::FormID a_Owner, PossessionSlot a_Slot) {
		for (const auto& handle : Possession::All(a_Owner, a_Slot)) {
			if (NiPointer<Actor> ptr = handle.get()) {
				TaskManager::Cancel(std::format(TaskAttachFmt, ptr->formID));
			}
		}
	}

	std::string EntryAction(RE::Actor* a_Actor, std::string_view a_Suffix) {
		const bool crawling = a_Actor && AnimationVars::Crawl::IsCrawling(a_Actor);
		return std::format("{}.{}", crawling ? "BoobCrush" : "ButtCrush", a_Suffix);
	}

	bool CanEnter(const EntryContext& a_Ctx, bool a_WantCrawling) {

		auto* actor = a_Ctx.Actor();
		if (!actor) {
			return false;
		}

		if (!CanDoActionBasedOnQuestProgress(actor, QuestAnimationType::kGrabAndSandwich)) {
			return false;
		}

		if (IsPlayerFirstPerson(actor) || AnimationVars::Growth::IsChangingSize(actor)) {
			return false;
		}

		if (AnimationVars::General::IsGTSBusy(actor)) {
			return false;
		}

		// A tiny in the hand only allows the crawling variant.
		if (Actions::Possession::Carried(actor->formID) && !AnimationVars::Crawl::IsCrawling(actor)) {
			return false;
		}

		if (!actor->IsPlayerRef() && IsBeingHeld(actor, RE::PlayerCharacter::GetSingleton())) {
			return false;
		}

		return AnimationVars::Crawl::IsCrawling(actor) == a_WantCrawling;
	}

	bool CanStart(const EntryContext& a_Ctx) {
		return a_Ctx.Actor() != nullptr;
	}

	// CanDoButtCrush claims the cooldown as well as reading it, so it only runs on a real request.
	bool VerifyStart(const EntryContext& a_Ctx) {

		auto* actor = a_Ctx.Actor();

		if (!actor) {
			return false;
		}

		if (!CanDoButtCrush(actor, true)) {
			ButtCrushController::ButtCrush_OnCooldownMessage(actor);
			return false;
		}

		return true;
	}

	bool CanGrow(const ActionContext& a_Ctx, bool a_Blocked) {

		auto* actor = a_Ctx.Actor();

		if (!actor || a_Blocked) {
			return false;
		}

		if (!Runtime::HasPerkTeam(actor, Runtime::PERK.GTSPerkButtCrushAug2) || AnimationVars::Growth::IsChangingSize(actor)) {
			return false;
		}

		return true;
	}

	bool VerifyGrow(const ActionContext& a_Ctx) {

		auto* actor = a_Ctx.Actor();

		if (!actor) {
			return false;
		}

		if (!ButtCrush_IsAbleToGrow(actor, GetGrowthLimit(actor))) {

			if (actor->IsPlayerRef()) {
				NotifyWithSound(actor, "Your body can't grow any further");
			}

			return false;
		}

		return true;
	}

	void ApplyGrowth(const ActionContext& a_Ctx, std::string_view a_SpringName, float a_NpcStaminaScale) {

		auto* giant = a_Ctx.Actor();
		if (!giant) {
			return;
		}

		const float bonus = GetButtCrushGrowthAmount(giant, 0.24f);

		ModGrowthCount(giant, 1.0f, false);
		SetButtCrushSize(giant, bonus, false);
		SpringGrow(giant, bonus, 0.3f / GetAnimationSlowdown(giant), std::string(a_SpringName), false);

		float stamina = 100.0f * GetButtCrushCost(giant, false);
		if (!giant->IsPlayerRef()) {
			stamina *= a_NpcStaminaScale;
		}

		DamageAV(giant, ActorValue::kStamina, stamina);
		Runtime::PlaySoundAtNode(Runtime::SNDR.GTSSoundGrowth, giant, 1.0f, NODE_PELVIS);
	}
}
