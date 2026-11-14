#pragma once

namespace GTS {

	struct ControllerShapeData {
		hkpShape* shape;
		hkTransform transform; // translation is the controller position in Havok units, rotation is identity
	};

	// The shape's world AABB, grown about its own centre by a_Scale. Every hkpShape implements
	// GetAabbImpl, so list, MOPP, capsule and convex all resolve through one call and a shape type
	// nothing here knows about still produces bounds instead of a miss.
	bool GetShapeBounds(const hkpShape* a_Shape, const hkTransform& a_Transform, float a_Scale, hkAabb& a_Out);

	bool SphereOverlapsShape(const hkpShape* a_Shape, const hkVector4& a_Center, float a_RadiusSq, const hkTransform& a_Transform, float a_Scale);

	hkpShape* GetControllerShape(Actor* a_Actor);

	void DebugDrawShape(const hkpShape* a_Shape, const hkTransform& a_Transform, float a_Scale, float a_ToSkyrim, int a_Duration);

	std::optional<ControllerShapeData> GetControllerShapeData(Actor* a_Actor);
}
