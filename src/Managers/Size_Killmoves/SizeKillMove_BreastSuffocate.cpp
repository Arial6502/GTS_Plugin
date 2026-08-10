#include "Managers/Size_Killmoves/SizeKillMove_BreastSuffocate.hpp"
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
using namespace BreastSuffocateKillMove;
namespace {
    constexpr float kDegToRad = 3.14159265f / 180.0f;
    bool IsAlternativeKillMove() {
        return Config::KillMove.bThirdPersonBreastKillMove;
    }
}
namespace BreastSuffocateKillMove {
    RE::NiPoint3 AltLookTarget() {
        return GTS::VictimHeadPos(_victim, _cam.cameraPos, Config::KillMove.fBreastSuffocate_FocusHeightOffset_NoBone * GiantScale(_giant));
    }

    RE::NiPoint3 EnemyAnchorPos() {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return GTS::VictimHeadPos(_victim, _cam.cameraPos, Config::KillMove.fBreastSuffocate_FocusHeightOffset_NoBone);
        }
        const auto S = _settings;
        const auto &SC = Config::KillMove;
        const float angleZ = player->GetAngleZ();
        const float pScale = GiantScale(_giant);
        const float eScale = std::max(VictimScale(_victim), S.MinAnchorScale);
        const RE::NiPoint3 headPos = AltLookTarget();

        if (IsAlternativeKillMove()) {
            const float pulledOutZoom = std::max(0.325f, 1.0f - _PulledOutTimer);
            const float radius = S.AltOrbitDistance * pScale * pulledOutZoom;
            const float elevation = S.AltElevationAngle * kDegToRad;

            const float orbitT = Clamp01(_TimePassed / std::max(S.AltOrbitTime, 0.01f));
            const float orbitAngle = S.AltOrbitAngle * kDegToRad * Ease(orbitT);

            RE::NiPoint3 horizontal(std::sin(angleZ), std::cos(angleZ), 0.0f);
            RE::NiPoint3 baseOffset = horizontal * (radius * std::cos(elevation));
            baseOffset.z = radius * std::sin(elevation);

            return headPos + RotateAroundAxis(baseOffset, RE::NiPoint3(0.f, 0.f, 1.f), orbitAngle);
        }
        

        // Player's facing direction in world space.
        RE::NiPoint3 forward(std::sin(angleZ),std::cos(angleZ),0.0f);

        const RE::NiPoint3 up(0.0f, 0.0f, 1.0f);
        
        //-----------------Blend between Small and Big offsets. Issue: Camera clips at small sizes.
        const float focusForward_big = SC.fBreastSuffocate_ForwardFromGTS_AtLarge * eScale * pScale;
        const float focusUp_big = SC.fBreastSuffocate_FocusHeightOffset_AtLarge * eScale * pScale;

        const float focusForward_small = SC.fBreastSuffocate_ForwardFromGTS_AtSmall * eScale * pScale;
        const float focusUp_small   = SC.fBreastSuffocate_FocusHeightOffset_AtSmall * eScale * pScale;
        //-----------------
        const float scaleCompensation = std::min(pScale / 8.0f, 1.0f);
        //-----------------Blend between them
        const float focusForward = std::lerp(focusForward_small, focusForward_big, scaleCompensation);
        const float focusUp = std::lerp(focusUp_small, focusUp_big, scaleCompensation);
        //-----------------

        //-----------------Pulled Out clips into face, need to reduce it at small sizes too
        const float focusForward_Pulled_big = SC.fBreastSuffocate_PulledOutForwardOffset_AtLarge * eScale * pScale;
        const float focusForward_Pulled_small = SC.fBreastSuffocate_PulledOutForwardOffset_AtSmall * eScale * pScale;
        const float focusForward_Pulled = std::lerp(focusForward_Pulled_small, focusForward_Pulled_big, scaleCompensation);
        //-----------------
        const float focusUp_Pulled = SC.fBreastSuffocate_PulledOutUpOffset * eScale * pScale;

        const RE::NiPoint3 forwardOffset = headPos +forward * (focusForward) + up * (focusUp); // When still betweeen breasts
        const RE::NiPoint3 pulledOutOffset = headPos + forward * (focusForward_Pulled) + up * (focusUp_Pulled); // When hand pulls out

        return _settings.PulledOut ? pulledOutOffset : forwardOffset;
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
            EnterStage(_cam, _state, BreastSuffocatePOVState::LookAtGiantNode);
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
                _victim->SetAlpha(0.0f); // Hide victim
            }
        }
        float bt = AdvanceBlend(_cam, dt);
        RE::NiMatrix3 liveTarget = BuildLookAt(_cam.cameraPos, node);
        _cam.cameraRot = SlerpMatrix(_cam.rotBlendFrom, liveTarget, bt);

        float slowMo = 1.0f - (1.0f - _settings.SlowMoMin) * Ease(rampT);
        Time::SGTM(isInConfiguringMode() ? configureModeTimeSlowdown : slowMo);

        if (_settings.PulledOut || (_victim && _victim->IsDead() && AnimationVars::Cleavage::isBreastAbsorbing(_giant))) {
            _cam.stageFromPos = _cam.cameraPos;
            _cam.stageFromRot = _cam.cameraRot;
            _returnFromSGTM = slowMo;
            EnterStage(_cam, _state, BreastSuffocatePOVState::PostDeathRecovery);
        }
    }

    void UpdatePostDeathRecovery(float dt) {
        float t = GTS::AdvanceStageTimerSafe(_cam, dt, _settings.postDeathRecoveryTime);
        _settings.postDeathTimePassed += dt;

        float pullT = Clamp01(_cam.timer / std::max(_settings.PullOutTransitionTime, 1e-4f));
        RE::NiPoint3 blendedAnchor = Lerp(_cam.stageFromPos, EnemyAnchorPos(), Ease(pullT));
        _cam.cameraPos = GTS::SmoothTowards(_cam.cameraPos, blendedAnchor, dt, _settings.PositionSmoothHalflife); 

        RE::NiPoint3 node = IsAlternativeKillMove() ? AltLookTarget() : GiantNodeOrHeadPos();

        float bt = AdvanceBlend(_cam, dt);
        RE::NiMatrix3 liveTarget = BuildLookAt(_cam.cameraPos, node);
        _cam.cameraRot = SlerpMatrix(_cam.rotBlendFrom, liveTarget, bt);

        if (t >= 1.0f) {
            _victim->SetAlpha(1.0f);
            EnterStage(_cam, _state, BreastSuffocatePOVState::ReturnCamera);
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
            _settings.PulledOut = false;
            _settings.postDeathTimePassed = 0.0f;
            _victim = nullptr;
            _giant = nullptr;
            _giantNode = nullptr;
            _cam.active = false;
            _TimePassed = 0.0f;
            _state = BreastSuffocatePOVState::None;
        }
    }
}
namespace GTS {
    void StartBreastSuffocateKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode, bool TinyCalamity) {
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

        _settings.PulledOut = false;
        _settings.postDeathTimePassed = 0.0f;
        _settings.AltOrbitAngle = _OrbitAngleTarget * (RandomBool() ? -1.0f : 1.0f);

        _giant = giant;
        _victim = victim;
        _giantNode = giantLookNode;
        _returnFromSGTM = 1.0f;
        _TimePassed = 0.0f;

        _cam.stageFromPos = _cam.startPos; 
        EnterStage(_cam, _state, BreastSuffocatePOVState::EnterEnemyPOV);
    }

    void UpdateBreastSuffocateState() {
        if (!_cam.active) {
            return;
        }

        const bool CanForceEndSequence = _settings.postDeathTimePassed >= _settings.postDeathRecoveryTime;
        if (CanForceEndSequence && _state != BreastSuffocatePOVState::None && _state != BreastSuffocatePOVState::ReturnCamera && !IsPlayerAnimBusy()) {
            EnterStage(_cam, _state, BreastSuffocatePOVState::ReturnCamera);
        }

        float dt = Time::WorldTimeDelta() * GetAnimationSlowdown(PlayerCharacter::GetSingleton());

        switch (_state) {
            case BreastSuffocatePOVState::EnterEnemyPOV:     UpdateEnemyPOV(dt);            break;
            case BreastSuffocatePOVState::LookAtGiantNode:   UpdateLookAtGiantNode(dt);     break;
            case BreastSuffocatePOVState::PostDeathRecovery: UpdatePostDeathRecovery(dt);   break;
            case BreastSuffocatePOVState::ReturnCamera:      UpdateReturnCamera(dt);        break;
            default: break;
        }

        AdvanceCustomTimer(dt, _settings.AltDuration, _TimePassed);

        if (_settings.PulledOut) {
            AdvanceCustomTimer(dt, 1.5f, _PulledOutTimer);
        } else if (_PulledOutTimer != 0.0f) {
            _PulledOutTimer = 0.0f;
        }
    }

    bool UpdateBreastSuffocateKillMove() {
        if (_state == BreastSuffocatePOVState::None) {
            return false;
        }

        UpdateBreastSuffocateState();
        RE::NiPoint3 rayStart = GiantNodeOrHeadPos(_giantNode, _giant, _cam.cameraPos);
        if (!DriveCameraWithCollision(_cam, rayStart)) {
            return false;
        }
        
        return true;
    }

    void RecordBreastSuffocateStartingPosition() {
        RecordStartingPosition(_cam, _state, BreastSuffocatePOVState::None);
    }

    bool OverrideHeadtracking_BreastSuffocate(NiPoint3 &target) {
        if (!_cam.active) {
            return false;
        }

        target = IsAlternativeKillMove() ? _victim->GetPosition() : _cam.cameraPos;
        return true;
    }
}