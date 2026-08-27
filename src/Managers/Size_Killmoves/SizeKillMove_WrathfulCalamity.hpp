#pragma once
#include "Managers/Size_Killmoves/KillMoveHelper.hpp"

using namespace GTS;
namespace WrathfulCalamity {

    enum class WrathfulPOVState {
        None,
        FocusEnemy,       
        LookAtGiantNode,  
        ImpactHold,      
        ReturnCamera
    };

    struct WrathfulCalamitySettings {
        float FocusEnemyTime = 0.15f; 

        float FocusForwardOffset = -30.0f;  
        float FocusHeightOffset  = 35.0f;  
        float MinAnchorScale     = 0.35f; 

        float ToGiantNodeBlendTime = 0.3f; 

        float SlowMoRampTime = 12.0f;
        float SlowMoMin      = 0.25f;

        float ImpactHoldTime  = 0.75f; 
        float ImpactSlowMoCut = 0.03f;

        float ReturnTime = 1.5f; 
    };

    inline WrathfulCalamitySettings _settings{};
    inline WrathfulPOVState _state = WrathfulPOVState::None;

    inline CameraSequenceState _cam{};

    inline RE::Actor* _victim = nullptr; 
    inline RE::Actor* _giant  = nullptr; 
    inline RE::NiAVObject* _giantNode = nullptr; 

    inline float _returnFromSGTM = 1.0f;

    RE::NiPoint3 EnemyAnchorPos();      
    RE::NiPoint3 GiantNodeOrHeadPos();  
}

namespace GTS {
    void UpdateWrathfulKillmove();
    void StartWrathfulCalamityKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode = false, bool TinyCalamity = true);
    bool UpdateWrathfulCalamityKillMove();
    void RecordWrathfulCalamityStartingPosition();
    bool OverrideHeadtracking_WrathfulCalamity(NiPoint3 &target);
}