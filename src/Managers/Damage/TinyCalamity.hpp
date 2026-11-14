#pragma once

namespace GTS {
    struct WeightedNode {
        const char* name;
        float weight;
    };
    
    bool TinyCalamity_WrathfulCalamity(Actor* giant, std::vector<Actor*> preys);

    // Whether this tiny could be erased right now, with none of the messaging or the cooldown stamp.
    // The AI asks before it plans a kill. Must agree with the checks inside WrathfulCalamity.
    bool TinyCalamity_CanErase(Actor* giant, Actor* tiny);

    // Speaks the Tiny Calamity shout, which is what applies the effect the other two nodes need.
    bool TinyCalamity_Shout(Actor* giant);
    
    void TinyCalamity_ShrinkActor(Actor* giant, Actor* tiny, float shrink);
    void TinyCalamity_ExplodeActor(Actor* giant, Actor* tiny);
    void TinyCalamity_StaggerActor(Actor* giant, Actor* tiny, float giantHp);
     
    void TinyCalamity_SeekActors(Actor* giant);
    void TinyCalamity_CrushCheck(Actor* giant, Actor* tiny);
    void TinyCalamity_BonusSpeed(Actor* giant);
}