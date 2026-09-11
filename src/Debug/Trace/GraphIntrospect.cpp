#include "Debug/Trace/GraphIntrospect.hpp"

namespace GTS {

	bool GraphIntrospect::BuildSlots(RE::Actor* a_Actor, std::vector<GraphVarSlot>& a_Out) {

		a_Out.clear();

		absl::flat_hash_set<std::string> seen = {};

		ForEachGraph(a_Actor, [&](std::int32_t, const RE::BSFixedString&, const RE::hkbBehaviorGraphStringData* a_Strings) {

			for (const auto& entry : a_Strings->variableNames) {

				if (entry.empty() || !seen.emplace(entry.c_str()).second) {
					continue;
				}

				GraphVarSlot slot{};
				slot.Name = RE::BSFixedString(entry.c_str());
				slot.Display = entry.c_str();

				// The getters type-check against the graph's own variable info, so probing is how the
				// kind is discovered without hkbVariableInfo being mapped. Narrowest wins.
				{
					bool b = false;
					std::int32_t i = 0;
					float f = 0.0f;

					if (a_Actor->GetGraphVariableBool(slot.Name, b)) {
						slot.Kind = GraphVarKind::kBool;
						slot.Raw = b ? 1u : 0u;
					}
					else if (a_Actor->GetGraphVariableInt(slot.Name, i)) {
						slot.Kind = GraphVarKind::kInt;
						slot.Raw = std::bit_cast<std::uint32_t>(i);
					}
					else if (a_Actor->GetGraphVariableFloat(slot.Name, f)) {
						slot.Kind = GraphVarKind::kFloat;
						slot.Raw = std::bit_cast<std::uint32_t>(f);
					}
				}

				if (slot.Kind != GraphVarKind::kNone) {
					slot.Previous = slot.Raw;
					a_Out.push_back(std::move(slot));
				}
			}
		});

		return !a_Out.empty();
	}

	bool GraphIntrospect::Read(RE::Actor* a_Actor, const GraphVarSlot& a_Slot, std::uint32_t& a_Out) {

		switch (a_Slot.Kind) {
			case GraphVarKind::kBool:
			{
				bool value = false;
				if (!a_Actor->GetGraphVariableBool(a_Slot.Name, value)) {
					return false;
				}
				a_Out = value ? 1u : 0u;
				return true;
			}
			case GraphVarKind::kInt:
			{
				std::int32_t value = 0;
				if (!a_Actor->GetGraphVariableInt(a_Slot.Name, value)) {
					return false;
				}
				a_Out = std::bit_cast<std::uint32_t>(value);
				return true;
			}
			case GraphVarKind::kFloat:
			{
				float value = 0.0f;
				if (!a_Actor->GetGraphVariableFloat(a_Slot.Name, value)) {
					return false;
				}
				a_Out = std::bit_cast<std::uint32_t>(value);
				return true;
			}
			default:
			{
				return false;
			}
		}
	}

	std::string GraphIntrospect::Describe(const GraphVarSlot& a_Slot, std::uint32_t a_Raw) {

		switch (a_Slot.Kind) {
			case GraphVarKind::kBool:
			{
				return a_Raw ? "true" : "false";
			}
			case GraphVarKind::kInt:
			{
				return std::format("{}", std::bit_cast<std::int32_t>(a_Raw));
			}
			case GraphVarKind::kFloat:
			{
				return std::format("{:.4f}", std::bit_cast<float>(a_Raw));
			}
			default:
			{
				return "?";
			}
		}
	}

	std::string_view GraphIntrospect::KindName(GraphVarKind a_Kind) {

		switch (a_Kind) {
			case GraphVarKind::kBool:  { return "bool"; }
			case GraphVarKind::kInt:   { return "int"; }
			case GraphVarKind::kFloat: { return "float"; }
			default:                   { return "?"; }
		}
	}

	bool GraphIntrospect::IsGTSName(std::string_view a_Name) {

		if (a_Name.size() < 3) {
			return false;
		}

		return (a_Name[0] == 'G' || a_Name[0] == 'g')
			&& (a_Name[1] == 'T' || a_Name[1] == 't')
			&& (a_Name[2] == 'S' || a_Name[2] == 's');
	}
}
