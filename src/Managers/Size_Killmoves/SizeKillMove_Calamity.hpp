#include "Managers/Size_Killmoves/KillMoveHelper.hpp"
#pragma once

using namespace GTS;
namespace Calamity {

    enum class TinyPOVState {
        None,
        DiveToEye,       
        LookUpAtGiant,   
        LookAtGiantNode, 
        ImpactShake,     
        ReturnCamera     
    };

    struct TinyPOVSettings {
        float DiveToEyeTime = 0.15f;

        float EyeForwardOffset = 5.0f; 
        float EyeHeightOffset  = 40.0f; 
        float LookUpAtGiantTime = 1.2f; 

        float FaceToNodeBlendTime      = 0.3f; 
        float GiantNodeProximityRadius = 30.0f; 
        float ProximityBlendTime = 0.3f; 

        float LookAtGiantNodeMaxWait = 12.0f; 
        bool  GiantNodeIsFoot        = false; 

        float ImpactShakeTime      = 0.35f; 
        float ImpactShakeMagnitude = 4.0f;  
        float ImpactShakeFrequency = 40.0f; 

        float ReturnTime = 0.5f; 

        float SlowMoTarget = 0.8f; 

        float ImpactSlowMoExtra = 0.9f; 
        float ImpactSlowMoInTime = 0.3f;
        float ImpactSlowMoTime  = 1.0f; 
    };

    inline TinyPOVSettings _settings{};
    inline TinyPOVState _state = TinyPOVState::None;

    inline CameraSequenceState _cam{};

    inline RE::Actor* _victim = nullptr; 
    inline RE::Actor* _giant  = nullptr; 
    inline RE::NiAVObject* _giantNode = nullptr; 

    

    RE::NiPoint3 VictimEyePos();     
    RE::NiPoint3 GiantNodeOrHeadPos(); 
}

namespace GTS {
    void UpdateFakeCalamityKillmove();
    void StartCalamityKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode = false, bool TinyCalamity = true);
    bool UpdateCalamityKillMove();
    void RecordCalamityStartingPosition();
    bool OverrideHeadtracking_TinyCalamity(NiPoint3 &target);
}