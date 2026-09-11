#pragma once

namespace GTS::AutoAim {

	// While one of these is alive, that actor wins any aim search whose zone it is already inside.
	// It cannot pull in an actor the search would have rejected, so a preference only decides
	// between targets that were all valid anyway.
	//
	// Scoped rather than a setter so an early return in a Resolve cannot leave it stuck on.
	class PreferTarget {

		public:
		explicit PreferTarget(RE::Actor* a_Actor);
		~PreferTarget();

		PreferTarget(const PreferTarget&) = delete;
		PreferTarget& operator=(const PreferTarget&) = delete;

		[[nodiscard]] static RE::FormID Current();

		private:
		RE::FormID m_Previous = 0;
	};
}

namespace GTS {
    //-----------------------------------------------------
	// FOOT OR HAND AUTO AIM
	//-----------------------------------------------------
	NiPoint3 GetPresetAimPosition(Actor* giant, bool left_foot, float side_offset, float forward_offset);
	Actor* FindClosestTargetInRectangle(Actor* giant, const NiPoint3 origin, float width, float length);
	Actor* FindClosestTargetBetweenTwoPoints_Rhomb(Actor* giant, const NiPoint3 pointL, const NiPoint3 pointR, float maxSearchDistance, bool& leftFoot);
	Actor* FindClosestTargetBetweenTwoPoints(Actor* giant, const NiPoint3 pointL, const NiPoint3 pointR, float maxSearchDistance, bool& leftFoot);

	void CalculateForwardBlend(Actor* giant, const NiPoint3& footPos, const NiPoint3& targetPos, AnimationBlendInfo& info);
	void CalculateDirectionalBlend2D(Actor* giant, const NiPoint3& footPos,const NiPoint3& targetPos, AnimationBlendInfo& info);
	void CalculateAngleBasedSideBlend(Actor* giant, const NiPoint3& footPos, const NiPoint3& targetPos, AnimationBlendInfo& info);

	void CalculateRectangleBlend(Actor* giant, const NiPoint3& origin, const NiPoint3& target, AnimationBlendInfo& info);
	void GetRectangleCoordinates(Actor* giant,const NiPoint3& origin, const NiPoint3& target, float length, float& forwardDist, float& rightDist);
}