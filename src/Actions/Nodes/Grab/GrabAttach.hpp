#pragma once

namespace GTS::Actions::Grabbing {
    bool IsCurrentlyReattaching(Actor* giant);
    bool HandleGrabLogic(Actor* giantref, Actor* tinyref, ActorHandle gianthandle, ActorHandle tinyhandle);
    void ReattachTiny(Actor* giant, Actor* tiny);
    bool FailSafeAbort(Actor* giantref, Actor* tinyref);

    // Keeps a stored actor turned to sit between the breasts rather than facing out of them.
    void Task_RotateActorToBreastX(Actor* giant, Actor* tiny);
}