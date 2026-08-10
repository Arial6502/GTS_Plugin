#include "Managers/Size_Killmoves/KillMoveHelper.hpp"
#pragma once

using namespace GTS;
namespace BreastAbsorbKillMove {

    enum class BreastAbsorbPOVState {
        None,
        EnterEnemyPOV,       
        LookAtGiantNode,
        AbsorbSlowMo,  
        PostDeathRecovery,      
        ReturnCamera
    };

    struct BreastCameraSettings {
        float EnterEnemyPOVTime = 0.325f; 

        float MinAnchorScale     = 0.05f; 

        float ToGiantNodeBlendTime = 0.3f; 

        float PositionSmoothHalflife = 0.08f;

        float SlowMoRampTime = 7.25f;
        float SlowMoMin      = 0.45f;

        float postDeathRecoveryTime = 7.5f;
        float postDeathTransitionTime = 1.0f;
        float ReturnTime = 1.75f; 

        float ImpactSlowMoExtra = 0.90f; 
        float ImpactSlowMoInTime = 0.1f;
        float ImpactSlowMoTime  = 0.35f;

        float ImpactShakeTime      = 0.35f; 
        float ImpactShakeMagnitude = 4.0f;  
        float ImpactShakeFrequency = 80.0f;
    };

    inline BreastCameraSettings _settings{};
    inline BreastAbsorbPOVState _state = BreastAbsorbPOVState::None;

    inline CameraSequenceState _cam{};

    inline RE::Actor* _victim = nullptr; 
    inline RE::Actor* _giant  = nullptr; 
    inline RE::NiAVObject* _giantNode = nullptr; 

    inline float _returnFromSGTM = 1.0f;

    RE::NiPoint3 EnemyAnchorPos();      
    RE::NiPoint3 GiantNodeOrHeadPos();  
}

namespace GTS {
    void UpdateBreastAbsorbState();
    void StartBreastAbsorbKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode = false, bool TinyCalamity = true);
    bool UpdateBreastAbsorbKillMove();
    void RecordBreastAbsorbStartingPosition();
    bool OverrideHeadtracking_BreastAbsorb(NiPoint3 &target);
}