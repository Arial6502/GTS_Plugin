#include "Managers/Size_Killmoves/SizeKillMove_BreastAbsorb.hpp"
#include "Managers/Size_Killmoves/KillMoveHelper.hpp"
#include "Managers/Damage/Utils/SizeDamageUtils.hpp"
#include "Systems/Rays/Camera/CameraCollision.hpp"
#include "Managers/Damage/CollisionDamage.hpp"
#include "Managers/Cameras/CamUtil.hpp"
#include "Managers/GTSSizeManager.hpp"
#include "Utils/DifficultyUtils.hpp"
#include "Managers/HighHeel.hpp"
#include "Config/Config.hpp"

using namespace GTS;
using namespace BreastAbsorbKillMove;
namespace { 
    constexpr float kDegToRad = 3.14159265f / 180.0f;

    bool IsAlternativeKillMove() {
        return Config::KillMove.bThirdPersonBreastKillMove;
    }
}
namespace BreastAbsorbKillMove {
    RE::NiPoint3 AltLookTarget() {
        return GTS::VictimHeadPos(_victim, _cam.cameraPos, Config::KillMove.fBreastAbsorb_FocusHeightOffset_NoBone * GiantScale(_giant));
    }

    RE::NiPoint3 EnemyAnchorPos() {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return GTS::VictimHeadPos(_victim, _cam.cameraPos, Config::KillMove.fBreastAbsorb_FocusHeightOffset_NoBone);
        }
        const auto S = _settings;
        const auto &SC = Config::KillMove;
        const float pScale = GiantScale(_giant);
        const float angleZ = player->GetAngleZ();
        const float eScale = std::max(VictimScale(_victim), S.MinAnchorScale);
        const RE::NiPoint3 headPos = AltLookTarget();

        if (IsAlternativeKillMove()) {
            
            const float t = std::clamp(_TimePassed / std::max(S.AltDuration, 0.001f), 0.0f, 1.0f);
            const float zoomIn = t * t * (3.0f - 2.0f * t);
            const float radius = std::lerp(S.AltOrbitDistance, S.AltOrbitRadius_Min, zoomIn) * pScale;
            const float elevation = S.AltElevationAngle * kDegToRad;

            const float orbitT = Clamp01(_TimePassed / std::max(S.AltOrbitTime, 0.01f));
            const float orbitAngle = S.AltOrbitAngle * kDegToRad * Ease(orbitT);

            RE::NiPoint3 horizontal(std::sin(angleZ), std::cos(angleZ), 0.0f);
            RE::NiPoint3 baseOffset = horizontal * (radius * std::cos(elevation));
            baseOffset.z = radius * std::sin(elevation);

            return headPos + RotateAroundAxis(baseOffset, RE::NiPoint3(0.f, 0.f, 1.f), orbitAngle);
        }
        
        // Player facing direction in world space.
        RE::NiPoint3 forward(std::sin(angleZ),std::cos(angleZ),0.0f);

        const RE::NiPoint3 up(0.0f, 0.0f, 1.0f);
        //-----------------Blend between Small and Big offsets. Issue: Camera clips at small sizes.
        const float focusForward_big = std::max(SC.fBreastAbsorb_ForwardFromGTS_AtLarge * eScale, SC.fBreastAbsorb_ForwardFromGTS_AtLarge_Min) * pScale;
        const float focusForward_small = std::max(SC.fBreastAbsorb_ForwardFromGTS_AtSmall * eScale, SC.fBreastAbsorb_ForwardFromGTS_AtSmall_Min) * pScale;

        const float focusUp_big = std::max(SC.fBreastAbsorb_FocusHeightOffset_AtLarge * eScale, SC.fBreastAbsorb_FocusHeightOffset_AtLarge_Min) * pScale;
        const float focusUp_small = std::max(SC.fBreastAbsorb_FocusHeightOffset_AtSmall * eScale, SC.fBreastAbsorb_FocusHeightOffset_AtSmall_Min) * pScale;
        //----------------

        const float scaleCompensation = std::min(pScale / 8.0f, 1.0f);
        //-----------------Blend between them
        const float focusForward = std::lerp(focusForward_small, focusForward_big, scaleCompensation);
        const float focusUp = std::lerp(focusUp_small, focusUp_big, scaleCompensation);
        //-----------------

        return headPos + forward * (focusForward) + up * (focusUp);
    }

    RE::NiPoint3 GiantNodeOrHeadPos() {
        return GTS::GiantNodeOrHeadPos(_giantNode, _giant, _cam.cameraPos);
    }

    void UpdateEnemyPOV(float dt) {
        float t = AdvanceStageTimer(_cam, dt, _settings.EnterEnemyPOVTime);
        float eased = Ease(t);

        RE::NiPoint3 anchorTarget = EnemyAnchorPos();
        _cam.cameraPos = Lerp(_cam.stageFromPos, anchorTarget, eased);
        _cam.cameraRot = BuildLookAt(_cam.cameraPos, VictimHeadPos(_victim, anchorTarget));

        Time::SGTM(1.0f);

        if (t >= 1.0f) {
            EnterStage(_cam, _state, BreastAbsorbPOVState::LookAtGiantNode);
            _cam.rotBlendFrom = _cam.cameraRot;
            BeginBlend(_cam, _settings.ToGiantNodeBlendTime);
        }
    }

    void UpdateLookAtGiantNode(float dt) {
        float rampT = AdvanceStageTimer(_cam, dt, _settings.SlowMoRampTime);

        _cam.cameraPos = GTS::SmoothTowards(_cam.cameraPos, EnemyAnchorPos(), dt, _settings.PositionSmoothHalflife); // camera stays glued to the same anchor near the enemy, smoothed to avoid jitter (see PositionSmoothHalflife)

        RE::NiPoint3 node;
        if (IsAlternativeKillMove()) {
            node = AltLookTarget(); // watch the victim
        } else {
            node = GiantNodeOrHeadPos();
        }

        if (_cam.timer >= 0.25f && !IsAlternativeKillMove()) {
            if (_victim && _victim->GetAlpha() > 0.0f) {
                _victim->SetAlpha(0.0f); // Hide vicitm
            }
        }
        float bt = AdvanceBlend(_cam, dt);
        RE::NiMatrix3 liveTarget = BuildLookAt(_cam.cameraPos, node);
        _cam.cameraRot = SlerpMatrix(_cam.rotBlendFrom, liveTarget, bt);

        float slowMo = 1.0f - (1.0f - _settings.SlowMoMin) * Ease(rampT);
        Time::SGTM(isInConfiguringMode() ? configureModeTimeSlowdown : slowMo);

        if (_victim && _victim->IsDead()) {
            _cam.stageFromPos = _cam.cameraPos;
            _cam.stageFromRot = _cam.cameraRot;
            _returnFromSGTM = slowMo;
            TriggerImpactSlowMo(_cam);
            EnterStage(_cam, _state, BreastAbsorbPOVState::AbsorbSlowMo);
        }
    }

    void UpdateAbsorbSlowMo(float dt) {
        float t = AdvanceStageTimer(_cam, dt, _settings.ImpactSlowMoInTime + _settings.ImpactSlowMoTime);
        float decay = 1.0f - Ease(t);
        float scale = VictimScale(_victim);

        RE::NiPoint3 shakeOffset = GTS::ComputeImpactShakeOffset(_cam, _settings.ImpactShakeMagnitude, _settings.ImpactShakeFrequency, scale, decay);
        _cam.cameraPos = _cam.stageFromPos + shakeOffset;
        _cam.cameraRot = _cam.stageFromRot;

        if (t >= 1.0f) {
            EnterStage(_cam, _state, BreastAbsorbPOVState::ReturnCamera);
        }
    }


    void UpdatePostDeathRecovery(float dt) {
        float t = GTS::AdvanceStageTimerSafe(_cam, dt, _settings.postDeathRecoveryTime);

        float pullT = Clamp01(_cam.timer / std::max(_settings.postDeathTransitionTime, 1e-4f));
        RE::NiPoint3 blendedAnchor = Lerp(_cam.stageFromPos, EnemyAnchorPos(), Ease(pullT));
        _cam.cameraPos = GTS::SmoothTowards(_cam.cameraPos, blendedAnchor, dt, _settings.PositionSmoothHalflife);
        RE::NiPoint3 node = IsAlternativeKillMove() ? AltLookTarget() : GiantNodeOrHeadPos();

        float bt = AdvanceBlend(_cam, dt);
        RE::NiMatrix3 liveTarget = BuildLookAt(_cam.cameraPos, node);
        _cam.cameraRot = SlerpMatrix(_cam.rotBlendFrom, liveTarget, bt);

        if (t >= 1.0f) {
            EnterStage(_cam, _state, BreastAbsorbPOVState::ReturnCamera);
        }
    }

    void UpdateReturnCamera(float dt) {
        float t = GTS::AdvanceStageTimerSafe(_cam, dt, _settings.ReturnTime);
        float eased = Ease(t);
        GTS::EaseCameraToStart(_cam, eased);

        Time::SGTM(isInConfiguringMode() ? configureModeTimeSlowdown : _returnFromSGTM + (1.0f - _returnFromSGTM) * eased);

        if (t >= 1.0f) {
            _cam.cameraPos = _cam.startPos;
            _cam.cameraRot = _cam.startRot;

            Time::SGTM(1.0f);
            _victim = nullptr;
            _giant = nullptr;
            _giantNode = nullptr;
            _cam.active = false;
            _TimePassed = 0.0f;
            _state = BreastAbsorbPOVState::None;
        }
    }
}
namespace GTS {
    void StartBreastAbsorbKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode, bool TinyCalamity) {
        if (!giant) {
            return;
        }
        if (!giant->IsPlayerRef()) {
            return;
        }
        if (!victim) {
            return;
        }
        if (!CanTriggerKillMove(PlayerCharacter::GetSingleton(), victim, Cause, base_damage, crush_mult)) {
            logger::info("Can't start Breast killmove");
            return;
        }
        
        auto camera = RE::PlayerCamera::GetSingleton();
        auto root = camera->cameraRoot.get();

        _cam.startPos = root->world.translate;
        _cam.startRot = root->world.rotate;
        _cam.cameraPos = _cam.startPos;
        _cam.cameraRot = _cam.startRot;
        _cam.active = true;
        _cam.blendTimer = 0.0f;
        _cam.blendDuration = 0.0f;
        _cam.impactActive = false;
        _cam.impactTimer = 0.0f;
        _settings.AltOrbitAngle = _OrbitAngleTarget * (RandomBool() ? -1.0f : 1.0f);
        

        _giant = giant;
        _victim = victim;
        _giantNode = giantLookNode;
        _returnFromSGTM = 1.0f;
        _TimePassed = 0.0f;

        _cam.stageFromPos = _cam.startPos; 
        EnterStage(_cam, _state, BreastAbsorbPOVState::EnterEnemyPOV);
    }

    void UpdateBreastAbsorbState() {
        if (!_cam.active) {
            return;
        }

        if (_state != BreastAbsorbPOVState::None && _state != BreastAbsorbPOVState::ReturnCamera && !IsPlayerAnimBusy()) {
            EnterStage(_cam, _state, BreastAbsorbPOVState::ReturnCamera);
        }

        float dt = Time::WorldTimeDelta() * GetAnimationSlowdown(PlayerCharacter::GetSingleton());

        switch (_state) {
            case BreastAbsorbPOVState::EnterEnemyPOV:     UpdateEnemyPOV(dt);           break;
            case BreastAbsorbPOVState::LookAtGiantNode:   UpdateLookAtGiantNode(dt);    break;
            case BreastAbsorbPOVState::AbsorbSlowMo:      UpdateAbsorbSlowMo(dt);       break;
            case BreastAbsorbPOVState::PostDeathRecovery: UpdatePostDeathRecovery(dt);  break;
            case BreastAbsorbPOVState::ReturnCamera:      UpdateReturnCamera(dt);       break;
            default: break;
        }
        ApplyImpactSlowMo(_cam, dt, _settings.SlowMoMin, _settings.ImpactSlowMoExtra, _settings.ImpactSlowMoInTime, _settings.ImpactSlowMoTime);
        AdvanceCustomTimer(dt, _settings.AltDuration, _TimePassed);
    }

    bool UpdateBreastAbsorbKillMove() {
        if (_state == BreastAbsorbPOVState::None) {
            return false;
        }

        UpdateBreastAbsorbState();
        RE::NiPoint3 rayStart = GiantNodeOrHeadPos(_giantNode, _giant, _cam.cameraPos);
        if (!DriveCameraWithCollision(_cam, rayStart)) {
            return false;
        }
        
        return true;
    }

    void RecordBreastAbsorbStartingPosition() {
        RecordStartingPosition(_cam, _state, BreastAbsorbPOVState::None);
    }

    bool OverrideHeadtracking_BreastAbsorb(NiPoint3 &target) {
        if (!_cam.active) {
            return false;
        }
        
        target = IsAlternativeKillMove() ? _victim->GetPosition() : _cam.cameraPos;
        return true;
    }
}