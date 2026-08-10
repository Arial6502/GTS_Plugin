#pragma once

namespace GTS {
    constexpr float configureModeTimeSlowdown = 0.025f;

    float VictimScale(Actor* a_victim);
    float GiantScale(Actor* a_giant);

    float PredictDamage(Actor* giant, Actor* enemy, DamageSource Cause, float base);
    bool CanTriggerKillMove(Actor* giant, Actor* enemy, DamageSource Cause, float baseDamage, float crushModifier, float killMoveChance_Crush = 0.0f);
    float ApplyHeelOffset(bool foot);

    RE::NiPoint3 VictimHeadPos(Actor* victim, const RE::NiPoint3& fallback, float noBoneOffsetZ = 0.0f);
    RE::NiMatrix3 VictimHeadRot(Actor* victim);
    RE::NiPoint3 GiantHeadPos(Actor* giant, const RE::NiPoint3& fallback);
    RE::NiPoint3 GiantNodeOrHeadPos(RE::NiAVObject* node, Actor* giant, const RE::NiPoint3& fallback, bool applyHeels = false);

    float Clamp01(float x);
    float Ease(float x);
    RE::NiPoint3 Lerp(const RE::NiPoint3& a, const RE::NiPoint3& b, float t);
    RE::NiPoint3 RotateAroundAxis(const RE::NiPoint3& v, const RE::NiPoint3& axis, float angle);
    RE::NiMatrix3 BuildLookAt(const RE::NiPoint3& from, const RE::NiPoint3& to);

    struct Quat { float w, x, y, z; };
    Quat MatrixToQuat(const RE::NiMatrix3& m);
    RE::NiMatrix3 QuatToMatrix(const Quat& q);
    Quat SlerpQuat(const Quat& a, Quat b, float t);
    RE::NiMatrix3 SlerpMatrix(const RE::NiMatrix3& a, const RE::NiMatrix3& b, float t);


    struct CameraSequenceState {
        bool  active = false;
        float timer  = 0.0f;

        RE::NiMatrix3 cameraRot;
        RE::NiMatrix3 startRot;
        RE::NiPoint3  cameraPos;
        RE::NiPoint3  startPos;

        RE::NiPoint3  stageFromPos; 
        RE::NiPoint3  stageToPos;   
        RE::NiMatrix3 stageFromRot; 

        RE::NiMatrix3 rotBlendFrom;
        float blendTimer    = 0.0f;
        float blendDuration = 0.0f;
        bool  impactActive = false;
        float impactTimer  = 0.0f;
    };

    float AdvanceStageTimer(CameraSequenceState& state, float dt, float duration);
    void AdvanceCustomTimer(float dt, float duration, float& timer);
    float AdvanceStageTimerSafe(CameraSequenceState& state, float dt, float duration);

    template <typename StateEnum>
    void EnterStage(CameraSequenceState& state, StateEnum& stateVar, StateEnum newStage) {
        stateVar = newStage;
        state.timer = 0.0f;
        state.stageFromPos = state.cameraPos;
        state.stageFromRot = state.cameraRot;
    }

    void BeginBlend(CameraSequenceState& state, float duration);
    float AdvanceBlend(CameraSequenceState& state, float dt);

    void TriggerImpactSlowMo(CameraSequenceState& state);

    bool ApplyImpactSlowMo(CameraSequenceState& state, float dt, float slowMoTarget, float impactExtra, float impactInTime, float impactOutTime);

    bool IsPlayerAnimBusy();

    bool DriveCameraWithCollision(const CameraSequenceState& state, const RE::NiPoint3& collisionRayStart);

    RE::NiPoint3 SmoothTowards(const RE::NiPoint3& current, const RE::NiPoint3& target, float dt, float halflife);
    RE::NiPoint3 ComputeImpactShakeOffset(const CameraSequenceState& state, float magnitude, float frequency, float scale, float decay);
    void EaseCameraToStart(CameraSequenceState& state, float easedT);

    void TryKillMove(Actor* giant, const AimOutcome& aim, KillMoveParameters params);
    void RecordKillMoveCameraPositions();
    void ResetKillMoveCameraTracking();
    bool UpdatingAnyKillMove();

    bool IsInAnyGTSKillMove();
    bool isInConfiguringMode();

    template <typename StateEnum>
    void RecordStartingPosition(CameraSequenceState& state, StateEnum currentStage, StateEnum noneStage) {
        if (currentStage == noneStage) {
            auto camera = RE::PlayerCamera::GetSingleton();
            auto root = camera->cameraRoot.get();
            state.cameraPos = root->world.translate;
            state.cameraRot = root->world.rotate;
        }
    }
}