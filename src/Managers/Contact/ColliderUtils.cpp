#include "Managers/Contact/ColliderUtils.hpp"

namespace GTS {

	bool GetShapeBounds(const hkpShape* a_Shape, const hkTransform& a_Transform, float a_Scale, hkAabb& a_Out) {

		if (!a_Shape) {
			return false;
		}

		a_Shape->GetAabbImpl(a_Transform, 0.0f, a_Out);

		const __m128 mn = a_Out.min.quad;
		const __m128 mx = a_Out.max.quad;
		const __m128 half = _mm_set1_ps(0.5f);
		const __m128 mid = _mm_mul_ps(_mm_add_ps(mn, mx), half);
		const __m128 ext = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(mx, mn), half), _mm_set1_ps(std::max(a_Scale, 0.01f)));

		a_Out.min.quad = _mm_sub_ps(mid, ext);
		a_Out.max.quad = _mm_add_ps(mid, ext);
		return true;
	}

	bool SphereOverlapsShape(const hkpShape* a_Shape, const hkVector4& a_Center, float a_RadiusSq, const hkTransform& a_Transform, float a_Scale) {

		hkAabb box{};

		if (!GetShapeBounds(a_Shape, a_Transform, a_Scale, box)) {
			return false;
		}

		const __m128 c = a_Center.quad;
		const __m128 closest = _mm_min_ps(_mm_max_ps(c, box.min.quad), box.max.quad);
		const __m128 delta = _mm_sub_ps(closest, c);

		return _mm_cvtss_f32(_mm_dp_ps(delta, delta, 0x71)) <= a_RadiusSq;
	}

	hkpShape* GetControllerShape(Actor* a_Actor) {

		bhkCharacterController* controller = a_Actor->GetCharController();

		if (!controller) {
			return nullptr;
		}

		if (auto* rigid = skyrim_cast<bhkCharRigidBodyController*>(controller)) {
			hkRefPtr rigidBody{ static_cast<hkpCharacterRigidBody*>(rigid->characterRigidBody.referencedObject.get()) };
			if (rigidBody && rigidBody->m_character) {
				return const_cast<hkpShape*>(rigidBody->m_character->collidable.shape);
			}
		}
		else if (auto* proxy = skyrim_cast<bhkCharProxyController*>(controller)) {
			hkRefPtr charProxy{ static_cast<hkpCharacterProxy*>(proxy->proxy.referencedObject.get()) };
			if (charProxy && charProxy->shapePhantom) {
				return const_cast<hkpShape*>(charProxy->shapePhantom->collidable.shape);
			}
		}

		return nullptr;
	}

	void DebugDrawShape(const hkpShape* a_Shape, const hkTransform& a_Transform, float a_Scale, float a_ToSkyrim, int a_Duration) {

		hkAabb box{};

		if (!GetShapeBounds(a_Shape, a_Transform, a_Scale, box)) {
			return;
		}

		auto ToSkyrim = [&](const hkVector4& a_Vec) {
			return NiPoint3(a_Vec.quad.m128_f32[0] * a_ToSkyrim, a_Vec.quad.m128_f32[1] * a_ToSkyrim, a_Vec.quad.m128_f32[2] * a_ToSkyrim);
		};

		DebugDraw::AABB(ToSkyrim(box.min), ToSkyrim(box.max), {
			.Color = DebugCol::Red,
			.Thickness = 1.0f,
			.LifetimeMs = static_cast<std::uint32_t>(std::max(a_Duration, 0))
		});
	}

	std::optional<ControllerShapeData> GetControllerShapeData(Actor* a_Actor) {

		hkpShape* shape = GetControllerShape(a_Actor);

		if (!shape) {
			return std::nullopt;
		}

		bhkCharacterController* controller = a_Actor->GetCharController();

		// false leaves the position at the collidable's own origin, which is the space the shape
		// vertices are already in. Passing true would shift it down to the feet and misplace the box.
		hkVector4 controllerPos{};
		controller->GetPosition(controllerPos, false);

		hkTransform transform{};
		transform.rotation.col0.quad = _mm_set_ps(0.0f, 0.0f, 0.0f, 1.0f);
		transform.rotation.col1.quad = _mm_set_ps(0.0f, 0.0f, 1.0f, 0.0f);
		transform.rotation.col2.quad = _mm_set_ps(0.0f, 1.0f, 0.0f, 0.0f);
		transform.translation.quad = controllerPos.quad;

		return ControllerShapeData{ shape, transform };
	}
}
