#include "Actions/Nodes/Calamity/CalamityCommon.hpp"

#include "Actions/Core/ActionRegistry.hpp"

namespace {

	absl::flat_hash_map<RE::FormID, std::vector<RE::ActorHandle>> g_Targets = {};
}

namespace GTS::Actions::Calamity {

	void AddTarget(RE::FormID a_Giant, RE::Actor* a_Tiny, float a_Until) {

		if (!a_Giant || !a_Tiny) {
			return;
		}

		g_Targets[a_Giant].push_back(a_Tiny->GetHandle());

		if (auto* data = Transient::GetActorData(a_Tiny)) {
			data->ShrinkUntil = a_Until;
		}
	}

	void ClearTargets(RE::FormID a_Giant) {
		g_Targets.erase(a_Giant);
	}

	std::vector<RE::Actor*> Targets(RE::FormID a_Giant) {

		auto it = g_Targets.find(a_Giant);

		if (it == g_Targets.end()) {
			return {};
		}

		std::vector<RE::Actor*> out;
		out.reserve(it->second.size());

		for (const auto& handle : it->second) {
			if (auto ptr = handle.get()) {
				out.push_back(ptr.get());
			}
		}

		return out;
	}

	bool IsTarget(RE::FormID a_Tiny) {

		for (const auto& list : g_Targets | std::views::values) {
			for (const auto& handle : list) {
				if (auto ptr = handle.get(); ptr && ptr->formID == a_Tiny) {
					return true;
				}
			}
		}

		return false;
	}

	float ShrinkUntil(RE::Actor* a_Tiny) {
		auto* data = Transient::GetActorData(a_Tiny);
		return data ? data->ShrinkUntil : 1.0f;
	}

	bool Shrink(RE::Actor* a_Giant, RE::Actor* a_Tiny, float a_Until) {

		if (!a_Giant || !a_Tiny) {
			return false;
		}

		AddTarget(a_Giant->formID, a_Tiny, a_Until);

		if (ActionRegistry::Perform(a_Giant, "Calamity.Shrink") != RequestResult::kAccepted) {
			ClearTargets(a_Giant->formID);
			return false;
		}

		return true;
	}

	bool Erase(RE::Actor* a_Giant, RE::Actor* a_Tiny) {

		if (!a_Giant || !a_Tiny) {
			return false;
		}

		AddTarget(a_Giant->formID, a_Tiny, 1.0f);

		if (ActionRegistry::Perform(a_Giant, "Calamity.Erase") != RequestResult::kAccepted) {
			ClearTargets(a_Giant->formID);
			return false;
		}

		return true;
	}
}
