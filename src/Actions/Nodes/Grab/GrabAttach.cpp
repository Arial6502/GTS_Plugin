#include "Actions/Nodes/Grab/GrabCommon.hpp"
#include "Actions/Nodes/Grab/GrabAttach.hpp"
#include "Actions/Core/ActionRegistry.hpp"
#include "Actions/Core/Possession.hpp"

#include "Managers/Animation/Utils/AnimationUtils.hpp"
#include "Managers/Animation/Utils/TurnTowards.hpp"
#include "Managers/Animation/AnimationManager.hpp"

#include "Magic/Effects/Common.hpp"

#include "Managers/Animation/Utils/AttachPoint.hpp"
#include "Utils/Actions/VoreUtils.hpp"

using namespace GTS;
using namespace std;


namespace {
    bool ShouldAbortGrab(Actor* giantref, Actor* tinyref, bool CanCancel, bool Dead, float sizedifference) {

        if (!CanCancel) {
            return false;
        }

        const float stamina = GetAV(giantref, ActorValue::kStamina);
        const bool tooSmall = sizedifference < Action_Grab;
        const bool tired = !IsBetweenBreasts(tinyref) && stamina < 2.0f;

        if (!Dead && !tooSmall && !tired) {
            return false;
        }

        // Says which of the three it was. This ends the grab and nothing else reports it, so a hold
        // that vanished has no other explanation to go on.
        logger::info("Grab on {:08X} cancelled: dead={} small={} (diff {:.2f} needs {:.2f}) tired={} (stamina {:.1f})",
            giantref->formID, Dead, tooSmall, sizedifference, Action_Grab, tired, stamina);

        PushActorAway(giantref, tinyref, 1.0f);
        Actions::Grabbing::DropOne(giantref, tinyref);

        return true;
    }

    bool ManageGrabPlayAttachment(Actor* giantref, Actor* tinyref) {
        auto TargetBone = Attachment_GetTargetNode(giantref);
        std::string_view node_lookup;

        

        switch (TargetBone) {
            case AttachToNode::ObjectL: {
                node_lookup = "AnimObjectL"; break;
            }
            case AttachToNode::ObjectR: {
                node_lookup = "AnimObjectR"; break;
            }
            case AttachToNode::ObjectA: {
                node_lookup = "AnimObjectA"; break;
            }
            case AttachToNode::ObjectB: {
                node_lookup = "AnimObjectB"; break;
            }
            case AttachToNode::None: {
                node_lookup = "AnimObjectL"; break;
            }
            default: return false;
        }

        NiAVObject* Object = find_node(giantref, node_lookup);

        if (Object) {
            NiPoint3 coords = Object->world.translate;
            
            if (!tinyref->IsPlayerRef()) {
                FaceSame(giantref, tinyref);
            }

            if (TransientActorData* Transient = Transient::GetActorData(giantref)) {
                if (Transient->KissVoring) {
                    const auto Offset = Config::Gameplay.ActionSettings.fGrabPlayVoreOffset_Z;
                    float offset = (0.6f + Offset) * get_visual_scale(giantref);
                    coords.z += offset;
                }
            }

            if (!AttachTo(giantref, tinyref, coords)) {
                Actions::Grabbing::DropOne(giantref, tinyref);
                return false;
            }
            return true;
        }

        return false;
    }

    void SetReattachingState(Actor* giant, bool Reattach) {
        auto data = Transient::GetActorData(giant);
        if (data) {
            data->ReattachingTiny = Reattach;
        }
    }

    void ReattachTinyTask(Actor* giant, Actor* tiny, bool Dead) {
        if (!Dead) {
            // Sometimes Tiny still exists and just unloaded, so we move Tiny to us in that case as well
            logger::trace("Moving tiny {:08X} to giant {:08X}", tiny->formID, giant->formID);
            SetReattachingState(giant, true);
            tiny->MoveTo(giant);

            double Start = Time::WorldTimeElapsed();
            std::string name = std::format("Reattach_{}", tiny->formID);
            ActorHandle gianthandle = giant->CreateRefHandle();
            ActorHandle tinyhandle = tiny->CreateRefHandle();

            TaskManager::Run(name, [=](auto& progressData) {

                auto giantPtr = gianthandle.get();
                auto tinyPtr = tinyhandle.get();

                if (!giantPtr) {
                    return false;
                }

                auto giantref = giantPtr.get();

                if (!tinyPtr) {
                    SetReattachingState(giantref, false);
                    return false;
                }

                auto tinyref = tinyPtr.get();
                double timepassed = Time::WorldTimeElapsed() - Start;

                if (timepassed > 0.25) {
                    tinyref->MoveTo(giantref);
                    DisableCollisions(tinyref, giantref);
                    // Best effort. A creature has no graph to put in a pose and refuses this, which
                    // changes nothing: the store says it is held and that is what the branch runs on.
                    if (IsBetweenBreasts(tinyref)) {
                        Actions::ActionRegistry::Notify(tinyref, IsHostile(giantref, tinyref) ? "GTSBEH_T_Storage_Enemy" : "GTSBEH_T_Storage_Ally");
                    }
                }

                // Held for the whole pull rather than just its first quarter second. The carry task
                // reads it to know the tiny is still mid transition and must not be measured yet.
                if (timepassed > 1.25) {
                    SetReattachingState(giantref, false);
                    return false;
                }

                return true;
            });
        }
    }
}

namespace GTS::Actions::Grabbing {
    bool IsCurrentlyReattaching(Actor* giant) { // Sometimes Tiny is still grabbed and we need to update Tiny pos so Tiny becomes visible
        // Works in such cases like Changing locations/going between loading screens with Tiny grabbed
        bool Attaching = false;
        auto data = Transient::GetActorData(giant);
        if (data) {
            Attaching = data->ReattachingTiny;
        }
        return Attaching;
    }

    bool HandleGrabLogic(Actor* giantref, Actor* tinyref, ActorHandle gianthandle, ActorHandle tinyhandle) {
        float sizedifference = get_scale_difference(giantref, tinyref, SizeType::VisualScale, true, false);

        ForceRagdoll(tinyref, false); 

        for (auto muted: {giantref, tinyref}) { // Try to stfu actors
            ShutUp(muted);
        }
        bool Escaped = IsEscapingInteraction(tinyref);
        bool Devourment = IsInvisible_Devourment(tinyref);
        bool Attacking = AnimationVars::Grab::IsGrabAttacking(giantref);

        bool TinyDead = tinyref->IsDead() || GetAV(tinyref, ActorValue::kHealth) <= 0.0f;
        bool Dead = (giantref->IsDead() || Escaped || Devourment || TinyDead);
        bool CanCancel = (Dead || !AnimationVars::Action::IsVoring(giantref)) && (!Attacking || IsBeingEaten(tinyref));

        // The tiny dying is the action layer's business: Possession drops them and the node plays out
        // its own ending. Aborting from here as well killed the animation half way through and left
        // the graph running with nothing tracking it.
        const bool abortable = giantref->IsDead() || Escaped || Devourment;

        if (ShouldAbortGrab(giantref, tinyref, CanCancel, abortable, sizedifference)) {
            Anims_FixAnimationDesync(giantref, tinyref, true); // Reset anim speed override on Tiny
            return false;
        }
        // Switch to always using Grab Play logic in that case, we need it since it doesn't cancel properly without it
        // Runs once per carried actor. The grab play and cleavage targets are set on the giant, so
        // only the actor in the running branch's slot follows them.
        const bool inHand = Actions::Possession::IsHeldIn(tinyref->formID, Actions::PossessionSlot::kHand);
        const bool cleaving = Actions::ActionRegistry::Current(giantref) == Actions::ActionId::kCleavage;

        if (inHand && AnimationVars::Action::IsInGrabPlayState(giantref)) {
            return ManageGrabPlayAttachment(giantref, tinyref);
        }

        if (IsBeingEaten(tinyref) && !IsBetweenBreasts(tinyref) && !AnimationVars::Action::IsInCleavageState(giantref)) {
            if (!AttachToObjectA(gianthandle, tinyhandle)) {
                // Unable to attach
                logger::info("Can't attach to ObjectA");
                Actions::Grabbing::DropOne(giantref, tinyref);
                return false;
            }
        } else if (IsBetweenBreasts(tinyref)) {
            bool hostile = IsHostile(giantref, tinyref);
            float restore = 0.04f * TimeScale();
            if (!hostile) {
                tinyref->AsActorValueOwner()->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, restore);
                tinyref->AsActorValueOwner()->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kStamina, restore);
            }

            RestoreBreastAttachmentState(giantref, tinyref); // If someone suddenly ragdolls us during breast anims
            Anims_FixAnimationDesync(giantref, tinyref, false);

            float AnimSpeed_GTS = AnimationManager::GetAnimSpeed(giantref);
            float AnimSpeed_Tiny = AnimationManager::GetAnimSpeed(tinyref);

            if (cleaving && Attachment_GetTargetNode(giantref) == AttachToNode::ObjectL) {
                auto ObjectL = find_node(giantref, "AnimObjectL");
                if (ObjectL) {
                    NiPoint3 coords = ObjectL->world.translate;

                    if (!AttachTo(giantref, tinyref, coords)) {
                        Actions::Grabbing::DropOne(giantref, tinyref);
                        return false;
                    }
                }
                return true;
            } else if (cleaving && Attachment_GetTargetNode(giantref) == AttachToNode::ObjectB) {
                if (DebugDraw::Wants()) {
                    auto node = find_node(tinyref, "NPC Root [Root]");
                    if (node) {
                        NiPoint3 point = node->world.translate;
                        
                        DebugDraw::Sphere(point, 6.0f, { .Color = IM_COL32(0, 255, 0, 255), .Thickness = 1.0f, .LifetimeMs = 40 });
                    }
                }

                if (AnimationVars::Cleavage::IsBoobsDoting(giantref) && 
                    (sizedifference < Action_Grab || Dead) && 
                    !AnimationVars::Cleavage::IsExitingStrangle(giantref)) {// If size is too small 
                    Actions::ActionRegistry::Notify(giantref, "GTSBEH_Boobs_Crush_Dot_Stop");
                }

                if (!AttachToObjectB(gianthandle, tinyhandle)) { // Attach to ObjectB non stop
                    Actions::Grabbing::DropOne(giantref, tinyref);
                    return false;
                }
                return true;
            }
            
            if (hostile) {
                DamageAV(tinyref, ActorValue::kStamina, restore * 2);
            }
            if (!AttachToCleavage(gianthandle, tinyhandle)) {
                // Unable to attach
                Actions::Grabbing::DropOne(giantref, tinyref);
                logger::info("Can't attach to Cleavage");
                return false;
            }
        } else if (AttachToHand(gianthandle, tinyhandle)) {
            GrabStaminaDrain(giantref, tinyref, sizedifference);
            return true;
        } else {
            if (!AttachToHand(gianthandle, tinyhandle)) {
                // Unable to attach
                Actions::Grabbing::DropOne(giantref, tinyref);
                logger::info("Can't attach to hand");
                return false;
            }
        }
        return true;
    }

    void ReattachTiny(Actor* giant, Actor* tiny) {

        // One pull at a time. Starting a new one every frame while the cells still disagree resets
        // its clock, so it never reaches the end and the giant never leaves the transition.
        if (IsCurrentlyReattaching(giant)) {
            return;
        }

		auto HandNode = find_node(giant, "NPC L Hand [LHnd]");
		if (HandNode) {
            bool Dead = (tiny->IsDead() || GetAV(tiny, ActorValue::kHealth) <= 0.0f);

            // Cells first. An interior has no worldspace, so the worldspace compare below cannot see
            // an interior to exterior transition at all, and that is the one this has to catch. A
            // parent cell is always there to compare.
            if (giant->GetParentCell() != tiny->GetParentCell()) {
                ReattachTinyTask(giant, tiny, Dead);
                return;
            }

			NiPoint3 GiantDist = HandNode->world.translate;
			NiPoint3 TinyDist = tiny->GetPosition();
			float distance = (GiantDist - TinyDist).Length();
			float reattach_dist = std::clamp(512.0f * get_visual_scale(giant), 512.0f, 4096.0f);

            if (distance > reattach_dist) {
                ReattachTinyTask(giant, tiny, Dead);
                return;
            }

            TESWorldSpace* gts_space = giant->GetWorldspace();
            TESWorldSpace* tiny_space = tiny->GetWorldspace();

            if (gts_space && tiny_space && gts_space->formID != tiny_space->formID) {
                ReattachTinyTask(giant, tiny, Dead);
            }
        }
    }

    bool FailSafeAbort(Actor* giantref, Actor* tinyref) {
        if (AnimationVars::Grab::IsGrabAttacking(giantref)) { // Breast state also counts as attacking, so we reset only when NOT grab attacking/in breast state
            if (AnimationVars::Cleavage::IsBoobsDoting(giantref) && 
				!AnimationVars::Cleavage::IsExitingStrangle(giantref)) { // These checks are important so we don't spam StartAnim
                Actions::ActionRegistry::Notify(giantref, "GTSBEH_Boobs_Crush_Dot_Stop");
                return true; // True = try again, do not abort
            }
            return true; // Try again
        }
        Actions::Grabbing::DropOne(giantref, tinyref);
        return false; // End grab.cpp task
    }


	// Sits inside the existing namespace: the moved task is the only caller, and the roll twin it was
	// paired with in Grab.cpp was already unused.
	static void ApplyPitchRotation(Actor* a_Actor, float a_Pitch) {
		if (auto* controller = a_Actor->GetCharController()) {
			controller->pitchAngle = a_Pitch;
		}
	}

	void Task_RotateActorToBreastX(Actor* giant, Actor* tiny) {

		const std::string name = std::format("RotateActor_{}", giant->formID);
		const ActorHandle gianthandle = giant->CreateRefHandle();
		const ActorHandle tinyhandle = tiny->CreateRefHandle();

		TaskManager::Run(name, [=](auto&) {

			auto giantPtr = gianthandle.get();
			auto tinyPtr = tinyhandle.get();

			if (!giantPtr || !tinyPtr) {
				return false;
			}

			auto* tinyref = tinyPtr.get();

			if (!IsBetweenBreasts(tinyref)) {
				ApplyPitchRotation(tinyref, 0.0f);
				return false;
			}

			if (ChestFrame chest; GetChestFrame(giantPtr.get(), chest)) {
				ApplyPitchRotation(tinyref, ChestPitch(chest));
			}

			return true;
		});

		TaskManager::ChangeUpdate(name, UpdateKind::PostPhysics);
	}
}
