#include "Debug/DebugProjector.hpp"

#include "Managers/Cameras/CamUtil.hpp"

namespace GTS {

	bool DebugProjector::Begin(const ImVec2& a_viewportPos, const ImVec2& a_viewportSize) {

		m_origin = a_viewportPos;
		m_size = a_viewportSize;
		m_projections = 0;

		m_camera = RE::Main::WorldRootCamera();

		if (!m_camera || m_size.x <= 0.0f || m_size.y <= 0.0f) {
			m_camera = nullptr;
			return false;
		}

		m_cameraPos = m_camera->world.translate;
		m_forward = GetCameraRotation() * RE::NiPoint3(0.0f, 1.0f, 0.0f);

		const float frustumNear = m_camera->GetRuntimeData2().viewFrustum.fNear;
		m_nearDist = std::isfinite(frustumNear) && frustumNear > 0.0f ? frustumNear : kDefaultNear;

		return true;
	}

	bool DebugProjector::Project(const RE::NiPoint3& a_world, ImVec2& a_out) const {

		if (!m_camera) {
			return false;
		}

		//A point behind the camera still projects "successfully", to a mirrored position, so this is the only thing keeping it off screen.
		if ((a_world - m_cameraPos).Dot(m_forward) <= m_nearDist) {
			return false;
		}

		++m_projections;

		float x = 0.0f;
		float y = 0.0f;
		float depth = 0.0f;

		const bool ok = RE::NiCamera::WorldPtToScreenPt3(
			m_camera->GetRuntimeData().worldToCam,
			m_camera->GetRuntimeData2().port,
			a_world, x, y, depth, kZeroTolerance);

		if (!ok) {
			return false;
		}

		//The game hands back normalised coordinates with y running up the screen. ImGui wants
		//pixels with y running down.
		const float px = m_origin.x + x * m_size.x;
		const float py = m_origin.y + (1.0f - y) * m_size.y;

		if (!std::isfinite(px) || !std::isfinite(py)) {
			return false;
		}

		a_out = ImVec2(px, py);
		return true;
	}

	RE::NiPoint3 DebugProjector::ClipToNearPlane(const RE::NiPoint3& a_visible, const RE::NiPoint3& a_hidden) const {

		RE::NiPoint3 inside = a_visible;
		RE::NiPoint3 outside = a_hidden;

		ImVec2 unused{};

		for (int i = 0; i < kClipIterations; ++i) {

			const RE::NiPoint3 mid = (inside + outside) * 0.5f;

			if (Project(mid, unused)) {
				inside = mid;
			}
			else {
				outside = mid;
			}
		}

		return inside + (a_visible - inside) * kClipBackoff;
	}
}
