#pragma once

namespace GTS {

	//Turns world positions into viewport pixels for the lifetime of one frame.
	//Projection goes through NiCamera::WorldPtToScreenPt3.
	class DebugProjector {

		public:
		//Caches the camera and viewport rect. False when there is no world camera yet.
		bool Begin(const ImVec2& a_viewportPos, const ImVec2& a_viewportSize);

		[[nodiscard]] bool Valid() const noexcept {
			return m_camera != nullptr;
		}

		//False when a_world sits at or behind the near plane, in which case a_out is left untouched.
		bool Project(const RE::NiPoint3& a_world, ImVec2& a_out) const;

		//Bisects from a_visible toward a_hidden and returns the last sample still in front of
		//the near plane, pulled back a hair so the projected result stays numerically sane.
		[[nodiscard]] RE::NiPoint3 ClipToNearPlane(const RE::NiPoint3& a_visible, const RE::NiPoint3& a_hidden) const;

		[[nodiscard]] const RE::NiPoint3& CameraPos() const noexcept {
			return m_cameraPos;
		}

		[[nodiscard]] const ImVec2& ViewportPos() const noexcept {
			return m_origin;
		}

		[[nodiscard]] const ImVec2& ViewportSize() const noexcept {
			return m_size;
		}

		[[nodiscard]] std::uint32_t Projections() const noexcept {
			return m_projections;
		}

		private:
		static constexpr float kZeroTolerance = 1e-5f;
		static constexpr int kClipIterations = 10;

		//Without this the clipped vertex can land arbitrarily close to w == 0 and project to a coordinate large enough to upset ImGui's polyline maths.
		static constexpr float kClipBackoff = 0.01f;

		//Fallback near distance incase the frustrum data is ever bad.
		static constexpr float kDefaultNear = 1.0f;

		RE::NiCamera* m_camera = nullptr;
		RE::NiPoint3 m_cameraPos;

		//Column 1 of the camera rotation. Skyrim is Y forward, which GetCameraRotation's callers already rely on (see USBarWindow reading entry[2][1] as pitch).
		RE::NiPoint3 m_forward;
		float m_nearDist = kDefaultNear;
		ImVec2 m_origin{ 0.0f, 0.0f };
		ImVec2 m_size{ 0.0f, 0.0f };

		mutable std::uint32_t m_projections = 0;
	};
}
