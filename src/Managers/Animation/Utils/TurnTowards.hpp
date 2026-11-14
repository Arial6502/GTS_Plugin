#pragma once

namespace GTS {

	template<typename U>
	bool FaceTowardsPoint(U& anyTiny, const NiPoint3& point) {
		Actor* tiny =  GetActorPtr(anyTiny);

		if (!tiny) {
			return false;
		}

		const NiPoint3 direction = point - tiny->GetPosition();

		if (direction.x == 0.0f && direction.y == 0.0f) {
			return false;
		}

		tiny->data.angle.z = std::atan2(direction.x, direction.y);

		return true;
	}

	template<typename T, typename U>
	bool FaceTowards(T& anyGiant, U& anyTiny, std::string_view bone_name) {
		Actor* giant = GetActorPtr(anyGiant);
		if (!giant) {
			return false;
		}
		auto bone = find_node(giant, bone_name);
		if (!bone) {
			return false;
		}
		return FaceTowardsPoint(anyTiny, bone->world.translate);
	}

	template<typename T, typename U>
	bool FaceTowards(T& anyGiant, U& anyTiny) {
		return FaceTowardsPoint(anyTiny, anyGiant->GetPosition());
	}

	// Specialise so we can just use FaceTowards instead of FaceTowardsPoint
	// for all
	template<typename T>
	bool FaceTowards(T& anyTiny, const NiPoint3& point) {
		return FaceTowardsPoint(anyTiny, point);
	}

	template<typename T, typename U>
	bool FaceSame(T& anyGiant, U& anyTiny) {
		Actor* giant = GetActorPtr(anyGiant);
		if (!giant) {
			return false;
		}
		Actor* tiny = GetActorPtr(anyTiny);
		if (!tiny) {
			return false;
		}
		auto giantAngle = giant->data.angle.z;
		tiny->data.angle.z = giantAngle;
		return true;
	}

	template<typename T, typename U>
	bool FaceOpposite(T& anyGiant, U& anyTiny) {
		Actor* giant = GetActorPtr(anyGiant);
		if (!giant) {
			return false;
		}
		Actor* tiny = GetActorPtr(anyTiny);
		if (!tiny) {
			return false;
		}
		auto giantAngle = giant->data.angle.z;
		float opposite = static_cast<float>(giantAngle - std::numbers::pi);
		if (opposite < 0.0f) {
			opposite += static_cast<float>(2.0 * std::numbers::pi);
		}
		tiny->data.angle.z = opposite;
		return true;
	}

}
