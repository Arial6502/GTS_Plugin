#include "Managers/Size_Killmoves/SizeKillMove_WrathfulCalamity.hpp"
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
using namespace WrathfulCalamity;
namespace WrathfulCalamity {

    RE::NiPoint3 GiantNodeOrHeadPos() {
        return GTS::GiantNodeOrHeadPos(_giantNode, _giant, _cam.cameraPos);
    }

    RE::NiPoint3 EnemyAnchorPos() {
        RE::NiPoint3 headPos = GTS::VictimHeadPos(_victim, _cam.cameraPos);
        RE::NiMatrix3 headRot = GTS::VictimHeadRot(_victim);
        RE::NiPoint3 forward = headRot * RE::NiPoint3(0.f, 1.f, 0.f);
        RE::NiPoint3 up(0.f, 0.f, 1.f);

        float scale = std::max(VictimScale(_victim), _settings.MinAnchorScale);
        return headPos
            + forward * (_settings.FocusForwardOffset * scale)
            + up * (_settings.FocusHeightOffset * scale);
    }

    void UpdateFocusEnemy(float dt) {
        float t = AdvanceStageTimer(_cam, dt, _settings.FocusEnemyTime);
        float eased = Ease(t);

        RE::NiPoint3 anchorTarget = EnemyAnchorPos();
        _cam.cameraPos = Lerp(_cam.stageFromPos, anchorTarget, eased);
        _cam.cameraRot = BuildLookAt(_cam.cameraPos, GTS::VictimHeadPos(_victim, _cam.cameraPos));

        Time::SGTM(1.0f);

        if (t >= 1.0f) {
            EnterStage(_cam, _state, WrathfulPOVState::LookAtGiantNode);
            _cam.rotBlendFrom = _cam.cameraRot;
            BeginBlend(_cam, _settings.ToGiantNodeBlendTime);
        }
    }

    void UpdateLookAtGiantNode(float dt) {
        float rampT = AdvanceStageTimer(_cam, dt, _settings.SlowMoRampTime);

        _cam.cameraPos = EnemyAnchorPos(); 
        RE::NiPoint3 node = GiantNodeOrHeadPos();
        if (_cam.timer >= 1.5f) {
            if (_victim && _victim->GetAlpha() > 0.0f) {
                _victim->SetAlpha(0.0f); // Hide vicitm
            }
        }
        float bt = AdvanceBlend(_cam, dt);
        RE::NiMatrix3 liveTarget = BuildLookAt(_cam.cameraPos, node);
        _cam.cameraRot = SlerpMatrix(_cam.rotBlendFrom, liveTarget, bt);

        float slowMo = 1.0f - (1.0f - _settings.SlowMoMin) * Ease(rampT);
        Time::SGTM(slowMo);

        bool dead = _victim && _victim->IsDead();

        if (dead) {
            _cam.stageFromPos = _cam.cameraPos;
            _cam.stageFromRot = _cam.cameraRot;
            _returnFromSGTM = slowMo;
            EnterStage(_cam, _state, WrathfulPOVState::ImpactHold);
        }
    }

    void UpdateImpactHold(float dt) {
        float t = GTS::AdvanceStageTimerSafe(_cam, dt, _settings.ImpactHoldTime);

        _cam.cameraPos = _cam.stageFromPos;
        _cam.cameraRot = _cam.stageFromRot;

        float dip = _returnFromSGTM * _settings.ImpactSlowMoCut;
        Time::SGTM(dip);

        if (t >= 1.0f) {
            _returnFromSGTM = dip;
            EnterStage(_cam, _state, WrathfulPOVState::ReturnCamera);
        }
    }

    void UpdateReturnCamera(float dt) {
        float t = GTS::AdvanceStageTimerSafe(_cam, dt, _settings.ReturnTime);
        float eased = Ease(t);
        GTS::EaseCameraToStart(_cam, eased);

        Time::SGTM(_returnFromSGTM + (1.0f - _returnFromSGTM) * eased);

        if (t >= 1.0f) {
            _cam.cameraPos = _cam.startPos;
            _cam.cameraRot = _cam.startRot;

            Time::SGTM(1.0f);

            _victim = nullptr;
            _giant = nullptr;
            _giantNode = nullptr;
            _cam.active = false;
            _state = WrathfulPOVState::None;
        }
    }
}
namespace GTS {
    void StartWrathfulCalamityKillmove(RE::Actor* giant, RE::Actor* victim, RE::NiAVObject* giantLookNode, DamageSource Cause, float base_damage, float crush_mult, bool isFootNode, bool TinyCalamity) {
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
            logger::info("Can't start Wrathful Calamity killmove");
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

        _giant = giant;
        _victim = victim;
        _giantNode = giantLookNode;
        _returnFromSGTM = 1.0f;

        _cam.stageFromPos = _cam.startPos; 
        EnterStage(_cam, _state, WrathfulPOVState::FocusEnemy);
    }

    void UpdateWrathfulKillmove() {
        if (!_cam.active) {
            return;
        }

        if (_state != WrathfulPOVState::None && _state != WrathfulPOVState::ReturnCamera && !IsPlayerAnimBusy()) {
            EnterStage(_cam, _state, WrathfulPOVState::ReturnCamera);
        }

        float dt = Time::WorldTimeDelta() * GetAnimationSlowdown(PlayerCharacter::GetSingleton());

        switch (_state) {
            case WrathfulPOVState::FocusEnemy:      UpdateFocusEnemy(dt);      break;
            case WrathfulPOVState::LookAtGiantNode: UpdateLookAtGiantNode(dt); break;
            case WrathfulPOVState::ImpactHold:      UpdateImpactHold(dt);      break;
            case WrathfulPOVState::ReturnCamera:    UpdateReturnCamera(dt);    break;
            default: break;
        }
    }

    bool UpdateWrathfulCalamityKillMove() {
        if (_state == WrathfulPOVState::None) {
            return false;
        }

        UpdateWrathfulKillmove();
        RE::NiPoint3 rayStart = GiantNodeOrHeadPos(_giantNode, _giant, _cam.cameraPos);
        if (!DriveCameraWithCollision(_cam, rayStart)) {
            return false;
        }
        
        return true;
    }

    void RecordWrathfulCalamityStartingPosition() {
        RecordStartingPosition(_cam, _state, WrathfulPOVState::None);
    }

    bool OverrideHeadtracking_WrathfulCalamity(NiPoint3 &target) {
        if (!_cam.active) {
            return false;
        }
        target = _cam.cameraPos;
        return true;
    }
}