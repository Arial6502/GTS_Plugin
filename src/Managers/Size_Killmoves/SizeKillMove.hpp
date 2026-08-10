#pragma once

#include "Managers/Size_Killmoves/KillMoveHelper.hpp"

namespace GTS {

    enum class SizeKillMoveState {
        None,
        MoveToEnemy,     
        RiseAboveEnemy,  
        LookAtFace,      
        LookAtNode,      
        DeathFlyOff,     
        ReturnCamera    
    };

    struct SizeKillMoveSettings {
        float MoveToEnemyTime      = 0.15f; 
        float MoveToEnemyDistance  = 10.0f;  
        float MoveToEnemyRiseFrac  = 0.5f;  
                                            
        float MoveToEnemyArcHeight = 10.0f;  
                                             
        float MoveFromEnemyOffset = 200.0f; 
        float MoveUpwards         = 30.0f;  

        float RiseTime   = 0.15f;  
        float RiseHeight = 40.0f;  

        float LookAtFaceTime = 0.4f; 
        float FaceToNodeBlendTime = 0.3f;  
        float NodeProximityRadius = 12.0f;  
                                          
        float ProximityBlendTime  = 0.3f;  
        float NodeLookDownHeight  = 15.0f;
                                           
        const char* DefaultLookNodeName = "NPC Head [Head]";
        float LookAtNodeMaxWait = 12.0f;  

        float DeathFlyOffTime   = 1.5f; 
        float DeathHoldDistance = 40.0f;
        float LookAtNodeDistance = 30.0f;
        bool  OrbitEnabled = true;
        float OrbitAngle   = 360.0f;     
        float OrbitTime    = 9.0f;       
        float PostDeathMaxWait = 15.0f;   

        float ReturnTime = 0.60f; // seconds

        float SlowMoTarget = 0.25f; 
        float ImpactSlowMoExtra = 0.96f; 
        float ImpactSlowMoInTime = 0.1f;
        float ImpactSlowMoTime  = 0.6f; 
    };

    inline SizeKillMoveSettings _settings{};
    inline SizeKillMoveState _state = SizeKillMoveState::None;

    inline CameraSequenceState _cam{};

    inline RE::Actor* _enemy = nullptr;
    inline std::vector<RE::NiAVObject*> _nodes = {};
    inline bool _isFoot = false; 

    inline RE::NiPoint3 _nodeEyeTarget;       
    inline bool         _nodeLookDown = false; 

    void UpdateSizeKillmove();
    void StartKillmove(RE::Actor* giant, RE::Actor* enemy, RE::NiAVObject* lookNode, DamageSource Cause, KillMoveParameters params);
    void StartKillmove(RE::Actor* giant, RE::Actor* enemy, std::vector<RE::NiAVObject*> lookNodes, KillMoveParameters params);
    RE::NiPoint3 NodeOrHeadPos();
    bool UpdateKillMove();
    void RecordStartingPosition();
}