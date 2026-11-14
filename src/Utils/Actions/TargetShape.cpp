#include "Utils/Actions/TargetShape.hpp"

namespace {

	RE::NiPoint3 Forward(RE::Actor* a_Actor) {
		return GTS::RotateAngleAxis({ 0.0f, 1.0f, 0.0f }, -a_Actor->data.angle.z, { 0.0f, 0.0f, 1.0f });
	}
}

namespace GTS::TargetShape {

	bool InReach(RE::Actor* a_Pred, RE::Actor* a_Prey, float a_Reach) {
		return (a_Pred->GetPosition() - a_Prey->GetPosition()).Length() <= a_Reach;
	}

	bool InCone(RE::Actor* a_Pred, RE::Actor* a_Prey, const Cone& a_Cone) {

		const RE::NiPoint3 predPos = a_Pred->GetPosition();
		const RE::NiPoint3 forward = Forward(a_Pred);

		RE::NiPoint3 toPrey = a_Prey->GetPosition() - predPos;

		if (toPrey.Length() > 1e-4f) {
			toPrey = toPrey / toPrey.Length();
			if (forward.Dot(toPrey) <= 0.0f) {
				return false;
			}
		}

		const float width = a_Cone.Width * get_visual_scale(a_Pred);

		// The angle goes into tan() as radians without converting, as every controller's copy did. Reach
		// was tuned against the result, so it stays.
		const float shift = std::fabs((width / 2.0f) / std::tan(a_Cone.AngleDeg / 2.0f));
		const RE::NiPoint3 apex = predPos - forward * shift;

		RE::NiPoint3 fromApex = a_Prey->GetPosition() - apex;

		if (fromApex.Length() <= width * 0.4f) {
			return true;
		}

		fromApex = fromApex / fromApex.Length();
		return forward.Dot(fromApex) > std::cos(a_Cone.AngleDeg * std::numbers::pi_v<float> / 180.0f);
	}

	std::vector<RE::Actor*> InFront(RE::Actor* a_Pred, std::vector<RE::Actor*> a_Candidates, const Cone& a_Cone) {

		const RE::NiPoint3 predPos = a_Pred->GetPosition();

		std::erase_if(a_Candidates, [&](RE::Actor* a_Prey) {
			return !a_Prey || !InCone(a_Pred, a_Prey, a_Cone);
		});

		std::ranges::sort(a_Candidates, [&](const RE::Actor* a_A, const RE::Actor* a_B) {
			return (a_A->GetPosition() - predPos).Length() < (a_B->GetPosition() - predPos).Length();
		});

		return a_Candidates;
	}
}
