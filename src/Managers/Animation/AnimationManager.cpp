
#include "Managers/Animation/AnimationManager.hpp"

#include "Config/Config.hpp"

#include "Managers/Perks/PerkHandler.hpp"
#include "Utils/Actions/InputFunctions.hpp"

#include "Debug/AnimDebug/AnimationDebugger.hpp"
#include "Debug/Trace/AnimationTracer.hpp"
#include "Actions/Core/ActionRegistry.hpp"

namespace GTS {

	AnimationEventData::AnimationEventData(Actor& giant, TESObjectREFR* tiny) : giant(giant), tiny(tiny) {}

	AnimationEvent::AnimationEvent(const std::function<void(AnimationEventData&)>& a_callback, const std::string& a_group) : callback(a_callback), group(a_group) {}

	TriggerData::TriggerData(const std::vector< std::string_view>& behavors,  std::string_view group) : behavors({}), group(group) {
		for (auto& sv: behavors) {
			this->behavors.emplace_back(sv);
		}
	}

	void AnimationManager::OnSKSEDataLoaded() {

		// Every animation is owned by src/Actions now: nodes for the ones with a state, stances in
		// src/Actions/Movement for crawling and prone, and tables in src/Actions/Reactions for the
		// tags that belong to no state at all. The old files are kept as .legacy next to this one,
		// out of the build, so nothing can reach them and the compiler proves it.
		//
		// What is left of this class is the parts other code still reads: the animation speed and
		// high heel aggregation below, and the two chokepoints that feed the action registry.

		InputFunctions::RegisterEvents();

	}

	void AnimationManager::OnMainUpdate() {
		auto player = PlayerCharacter::GetSingleton();
		if (player) {
			UpdateGravity(player);
		}
	}

	void AnimationManager::OnPluginReset() {
		this->data.clear();
	}

	void AnimationManager::OnGameActorReset(Actor* actor) {
		std::lock_guard lock(_lock);
		if (actor) {
			this->data.erase(actor);
		}
	}

	float AnimationManager::GetHighHeelSpeed(Actor* actor) {
		float Speed = 1.0f;
		auto& AnimMgr = AnimationManager::GetSingleton();

		try {

			if (!AnimMgr.data.empty()) {

				if (AnimMgr.data.contains(actor)) {

					for (auto& data : AnimMgr.data.at(actor) | std::views::values) {
						Speed *= data.HHspeed;
					}
				}
			}

		}
		catch (const std::out_of_range&) {}

		return Speed * Actions::ActionRegistry::HHSpeed(actor);
	}

	float AnimationManager::GetBonusAnimationSpeed(Actor* actor) {
		float totalSpeed = 1.0f;
		auto& AnimMgr = AnimationManager::GetSingleton();

		try {

			if (!AnimMgr.data.empty()) {

				if (AnimMgr.data.contains(actor)) {

					for (auto& data : AnimMgr.data.at(actor) | std::views::values) {
						totalSpeed *= data.animSpeed;
					}
				}
			}
		}
		catch (const std::out_of_range&) {}
		return totalSpeed * Actions::ActionRegistry::AnimSpeed(actor);
	}

	float AnimationManager::GetAnimSpeed(Actor* actor) {

		float speed = 1.0f;

		if (!Config::General.bDynamicAnimspeed) {
			return 1.0f;
		}

		if (actor) {

			auto saved_data = GTS::Persistent::GetActorData(actor);
			if (saved_data) {
				if (saved_data->fAnimSpeed > 0.0f) {
					speed *= saved_data->fAnimSpeed;
				}
			}

			const auto& AnimMgr = AnimationManager::GetSingleton();
			auto& AnimData = AnimMgr.data;
			try {

				float totalSpeed = 1.0f;

				if (auto it = AnimData.find(actor); it != AnimData.end()) {
					for (const auto& data : it->second | std::views::values) {
						totalSpeed *= data.animSpeed;
					}
					speed *= totalSpeed;
				}

			}
			catch (const std::out_of_range&) {}

			speed *= Actions::ActionRegistry::AnimSpeed(actor);
		}
		return speed;
	}

	void AnimationManager::RegisterEvent( std::string_view name,  std::string_view group, std::function<void(AnimationEventData&)> func) {
		AnimationManager::GetSingleton().eventCallbacks.try_emplace(std::string(name), func, std::string(group));
		//log::info("Registering Event: Name {}, Group {}", name, group);
	}

	void AnimationManager::RegisterTrigger( std::string_view trigger,  std::string_view group,  std::string_view behavior) {
		AnimationManager::RegisterTriggerWithStages(trigger, group, {behavior});
		//log::info("Registering Trigger: {}, Group {}, Behavior {}", trigger, group, behavior);
	}

	void AnimationManager::RegisterTriggerWithStages( std::string_view trigger,  std::string_view group,  std::vector< std::string_view> behaviors) {
		if (!behaviors.empty()) {
			AnimationManager::GetSingleton().triggers.try_emplace(std::string(trigger), behaviors, group);
			//log::info("Registering Trigger With Stages: {}, Group {}", trigger, group);
		}
	}

	void AnimationManager::StartAnim( std::string_view trigger, Actor& giant) {
		AnimationManager::StartAnim(trigger, giant, nullptr);
	}

	void AnimationManager::StartAnim( std::string_view trigger, Actor* giant) {
		if (giant) {
			AnimationManager::StartAnim(trigger, *giant);
			//log::info("Starting Trigger {} for {}", trigger, giant->GetDisplayFullName());
		}
	}

	void AnimationManager::StartAnim(std::string_view trigger, Actor& giant, TESObjectREFR* tiny) {

		if (AnimationVars::General::IsTransitioning(&giant)) {
			AnimationTracer::Trigger(&giant, trigger, "transitioning", false);
			Actions::ActionRegistry::OnLegacyTrigger(&giant, trigger, "transitioning", false);
			AnimationDebugger::Trigger(&giant, trigger, "refused: transitioning", false);
			return;
		}

		if (giant.IsPlayerRef()) {
			if (IsFirstPerson() || State::IsInRaceMenu()) {
				//Time::WorldTimeElapsed() > 1.0
				//ForceThirdPerson(&giant);
				// It kinda works in fp that way, but it introduces some issues with animations such as Hugs and Butt Crush.
				// Better to wait for full support someday
				AnimationTracer::Trigger(&giant, trigger, "first person", false);
				Actions::ActionRegistry::OnLegacyTrigger(&giant, trigger, "first person", false);
				AnimationDebugger::Trigger(&giant, trigger, "refused: first person", false);
				return; // Don't start animations in FP, it's not supported.
			}
		}

		try {
			auto& me = AnimationManager::GetSingleton();
			// Find the behavior for this trigger exit on catch if not

			auto& behavorToPlay = me.triggers.at(std::string(trigger));
			auto& group = behavorToPlay.group;
			// Try to create anim data for actor
			me.data.try_emplace(&giant);
			auto& actorData = me.data.at(&giant); // Must exists now
			// Create the anim data for this group if not present
			actorData.try_emplace(group, giant, tiny);
			// Run the anim
			//log::info("Playing Trigger {} for {}", trigger, giant.GetDisplayFullName());
			//log::info("Playing {}", behavorToPlay.behavors[0]);
			giant.NotifyAnimationGraph(behavorToPlay.behavors[0]);
			AnimationTracer::Trigger(&giant, trigger, behavorToPlay.behavors[0], true);
			AnimationDebugger::Trigger(&giant, trigger, behavorToPlay.behavors[0], true);
			Actions::ActionRegistry::OnLegacyTrigger(&giant, trigger, behavorToPlay.behavors[0], true);

			PerkHandler::UpdatePerkValues(&giant, PerkUpdate::Perk_Acceleration); // Currently used for Anim Speed buff only
		}
		catch (const std::out_of_range&) {
			logger::error("Requested play of unknown animation named: {}", trigger);
			AnimationTracer::Trigger(&giant, trigger, "unknown trigger", false);
			Actions::ActionRegistry::OnLegacyTrigger(&giant, trigger, "unknown trigger", false);
			AnimationDebugger::Trigger(&giant, trigger, "refused: unknown trigger", false);
			return;
		}
	}

	void AnimationManager::ResetAnimationSpeedData(Actor* actor) {
		try {

			auto& me = AnimationManager::GetSingleton();

			if (!me.data.empty()) {

				if (me.data.contains(actor)) {

					for (auto& data : me.data.at(actor) | std::views::values) {
						data.animSpeed = 1.0f;
						data.canEditAnimSpeed = false;
						data.stage = 0;
					}
				}
			}
		}
		catch (std::out_of_range&) {}
	}

	void AnimationManager::StartAnim(std::string_view trigger, Actor* giant, TESObjectREFR* tiny) {
		if (giant) {
			AnimationManager::StartAnim(trigger, *giant, tiny);
		}
	}

	void AnimationManager::NextAnim(std::string_view trigger, Actor& giant) {
		try {
			auto& me = AnimationManager::GetSingleton();
			// Find the behavior for this trigger exit on catch if not
			auto& behavorToPlay = me.triggers.at(std::string(trigger));
			auto& group = behavorToPlay.group;
			// Get the actor data OR exit on catch
			auto& actorData = me.data.at(&giant);
			// Get the event data of exit on catch
			auto& eventData = actorData.at(group);
			std::size_t currentTrigger = eventData.currentTrigger;
			// Run the anim
			if (behavorToPlay.behavors.size() < currentTrigger) {
				giant.NotifyAnimationGraph(behavorToPlay.behavors[currentTrigger]);
			}
		}
		catch (const std::out_of_range&) {}
	}
	void AnimationManager::NextAnim(std::string_view trigger, Actor* giant) {
		if (giant) {
			AnimationManager::NextAnim(trigger, *giant);
		}
	}

	void AnimationManager::OnActorAnimationChange(Actor* actor, const std::string_view& tag, const std::string_view& payload) {
		try {
			if (actor) {

				if (!this->eventCallbacks.contains(std::string(tag))){
					AnimationTracer::Annotation(actor, tag, false);
					Actions::ActionRegistry::OnAnnotation(actor, tag);
					return;
				}

				AnimationTracer::Annotation(actor, tag, true);
				Actions::ActionRegistry::OnAnnotation(actor, tag);

				// Try to get the registerd anim for this tag
				auto& animToPlay = this->eventCallbacks.at(std::string(tag));

				// If data doesn't exist then insert with default
				this->data.try_emplace(actor);
				auto& actorData = this->data.at(actor);
				auto group = animToPlay.group;
				// If data doesn't exist this will insert it with default
				actorData.try_emplace(group, *actor, nullptr);
				// Get the data or the newly inserted data
				auto& actdata = actorData.at(group);
				// Call the anims function
				animToPlay.callback(actdata);
				// If the stage is 0 after an anim has been played then
				//   delete this data so that we can reset for the next anim
				if (actdata.stage == 0) {
					actorData.erase(group);
				}
			}
		}
		catch (const std::out_of_range&) {}
	}

	// Get the current stage of an animation group
	std::size_t AnimationManager::GetStage(Actor& actor,  std::string_view group) {
		try {
			auto& me = AnimationManager::GetSingleton();

			if (me.data.empty()) {
				return 0;
			}

			if (!me.data.contains(&actor)) {
				return 0;
			}

			return me.data.at(&actor).at(std::string(group)).stage;

		}
		catch (const std::out_of_range&) {
			return 0;
		}
	}

	std::size_t AnimationManager::GetStage(Actor* actor,  std::string_view group) {
		if (actor) {
			return AnimationManager::GetStage(*actor, group);
		}
		else {
			return 0;
		}
	}

	bool AnimationManager::HHDisabled(Actor& actor) {
		auto& me = AnimationManager::GetSingleton();

		if (!IsHumanoid(&actor)) {
			return false;
		}

		if (Actions::ActionRegistry::HHDisabled(&actor)) {
			return true;
		}

		auto it = me.data.find(&actor);
		if (it == me.data.end()) {
			return false;
		}

		return std::ranges::any_of(it->second | std::views::values, [](const auto& data) {
			return data.disableHH;
		});
	}

	bool AnimationManager::HHDisabled(Actor* actor) {
		if (actor) {
			return AnimationManager::HHDisabled(*actor);
		}
		else {
			return false;
		}
	}

	void AnimationManager::UpdateGravity(Actor* actor) {
		static Timer GravityTimer = Timer(0.33);
		if (GravityTimer.ShouldRunFrame()) {
			auto Controller = actor->GetCharController();
			if (Controller) {
				bool Enabled = Config::General.bAlterPlayerGravity;
				float size = get_visual_scale(actor);

				float new_gravity = Enabled ? 1.0f * std::sqrt(size) : 1.0f; 
				Controller->gravity = new_gravity;
			}
		}
	}
}
