#pragma once
#include "Config/Util/TomlRefl.hpp"

//-------------------------------------------------------------------------------------------------------------------
//  BASE STRUCT
//  (Directly Serialized)
//-------------------------------------------------------------------------------------------------------------------

struct SettingsKillMove_t {
    // Main settings
    bool bConfigureMode = false;
    bool bThirdPersonBreastKillMove = true;
    bool bEnableKillMoves = true;
    //-----Probability--------------------------------------------------------------
    float fKillMoveChance_Death = 5.0f;
    float fKillMoveChance_Crush = 10.0f;
    //------------------------------------------------------------------------------
    //-----Breast Settings----------------------------------------------------------
    //-----When Big (>= 8x)
    float fBreastAbsorb_ForwardFromGTS_AtLarge = 22.0f;
    float fBreastAbsorb_ForwardFromGTS_AtLarge_Min = 8.0f;              // 8 = normal, 6 = erin (Erin is race)

    float fBreastAbsorb_FocusHeightOffset_AtLarge  = 4.0f;              //4 = normal, 3 = erin
    float fBreastAbsorb_FocusHeightOffset_AtLarge_Min = 2.0f;           //2 = normal, 1 = erin
    
    //-----When Small (< 8x)
    float fBreastAbsorb_ForwardFromGTS_AtSmall = 60.0f; 
    float fBreastAbsorb_ForwardFromGTS_AtSmall_Min = 18.0f;             // 18 = normal, 24 = erin
    
    float fBreastAbsorb_FocusHeightOffset_AtSmall  = 18.0f;             // 18 = normal, 12 = erin
    float fBreastAbsorb_FocusHeightOffset_AtSmall_Min = 6.0f;          // 6 = normal, -2.5 = erin
    //-----

    float fBreastAbsorb_FocusHeightOffset_NoBone = 1.25f;

    //-----When Big (>= 8x)
    float fBreastSuffocate_ForwardFromGTS_AtLarge = 16.0f;              // 16 = normal , 10 = erin
    float fBreastSuffocate_FocusHeightOffset_AtLarge  = 8.0f;           // 8 = normal, 3 = erin
    float fBreastSuffocate_PulledOutForwardOffset_AtLarge = 0.0f;
    //-----When Small (< 8x)
    float fBreastSuffocate_ForwardFromGTS_AtSmall = 28.0f;              // 28 = normal, 36 = erin
    float fBreastSuffocate_FocusHeightOffset_AtSmall  = 8.0f;           // 8 = normal, 4 = erin
    float fBreastSuffocate_PulledOutForwardOffset_AtSmall = 20.0f;
    //-------
    
    float fBreastSuffocate_PulledOutUpOffset = 2.0f;
    
    float fBreastSuffocate_FocusHeightOffset_NoBone = 12.5f;

};
TOML_SERIALIZABLE(SettingsKillMove_t);
TOML_REGISTER_NAME(SettingsKillMove_t, "KillMoves");