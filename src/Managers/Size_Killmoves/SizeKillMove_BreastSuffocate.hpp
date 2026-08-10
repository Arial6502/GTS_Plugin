#include "Managers/Size_Killmoves/KillMoveHelper.hpp"
#pragma once

using namespace GTS;
namespace BreastSuffocateKillMove {

    enum class BreastSuffocatePOVState {
        None,
        EnterEnemyPOV,       
        LookAtGiantNode,  
        PostDeathRecovery,      
        ReturnCamera
    };

    struct BreastCameraSettings {
        float EnterEnemyPOVTime = 0.6f; 
        float MinAnchorScale     = 1.0f; 

        float ToGiantNodeBlendTime = 0.3f; 

        float PositionSmoothHalflife = 0.025f;

        float PullOutTransitionTime = 0.5f;

        float SlowMoRampTime = 10.0f;
        float SlowMoMin      = 0.65f;

        float postDeathRecoveryTime = 3.5f;
        float postDeathTimePassed = 0.0f;
        float ReturnTime = 2.25f; 
        bool PulledOut = false;
    };

    inline BreastCameraSettings _settings{};
    inline BreastSuffocatePOVState _state = BreastSuffocatePOVState::None;

    inline CameraSequenceState _cam{};

    inline RE::Actor* _victim = nullptr; 
    inline RE::Actor* _giant  = nullptr; 
    inline RE::NiAVObject* _giantNode = nullptr; 

    inline float _returnFromSGTM = 1.0f;

    RE::NiPoint3 EnemyAnchorPos();      
    RE::NiPoint3 GiantNodeOrHeadPos();  
}

namespace GTS {
    void UpdateBreastSuffocateState();
    void StartBreastSuffocateKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode = false, bool TinyCalamity = true);
    bool UpdateBreastSuffocateKillMove();
    void RecordBreastSuffocateStartingPosition();
    bool OverrideHeadtracking_BreastSuffocate(NiPoint3 &target);
}