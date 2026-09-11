#include "Actions/Core/GraphSnapshot.hpp"

namespace {

	using namespace GTS::Actions;

	struct VarSlot {
		std::string Name;
		RE::BSFixedString Interned;
	};

	std::vector<VarSlot> g_Slots = {};
	bool g_Frozen = false;
}

namespace GTS::Actions {

	std::uint32_t GraphVars::Declare(std::string_view a_Name) {

		if (a_Name.empty()) {
			return kNoBit;
		}

		for (std::uint32_t i = 0; i < g_Slots.size(); ++i) {
			if (g_Slots[i].Name == a_Name) {
				return i;
			}
		}

		if (g_Frozen) {
			logger::error("GraphVars: '{}' declared after freeze, ignored", a_Name);
			return kNoBit;
		}

		if (g_Slots.size() >= kMaxVars) {
			GTS::ReportAndExit(std::format("Action nodes declare more than {} distinct graph variables. The snapshot is a 64 bit mask.", kMaxVars));
			return kNoBit;
		}

		g_Slots.emplace_back(std::string(a_Name), RE::BSFixedString(std::string(a_Name).c_str()));
		return static_cast<std::uint32_t>(g_Slots.size() - 1);
	}

	GraphSignature GraphVars::Compile(std::span<const GraphExpect> a_Signature) {

		GraphSignature out{};

		for (const auto& expect : a_Signature) {

			const std::uint32_t bit = Declare(expect.Name);
			if (bit == kNoBit) {
				continue;
			}

			const GraphMask flag = GraphMask{1} << bit;
			out.Mask |= flag;

			if (expect.Value) {
				out.Want |= flag;
			}
		}

		return out;
	}

	void GraphVars::Freeze() {
		g_Frozen = true;
	}

	std::string_view GraphVars::Name(std::uint32_t a_Bit) {
		return a_Bit < g_Slots.size() ? std::string_view(g_Slots[a_Bit].Name) : "?";
	}

	std::uint32_t GraphVars::Count() {
		return static_cast<std::uint32_t>(g_Slots.size());
	}

	GraphSnapshot GraphVars::Read(RE::Actor* a_Actor) {

		GraphSnapshot snapshot{};

		if (!a_Actor || !a_Actor->Is3DLoaded()) {
			return snapshot;
		}

		for (std::uint32_t i = 0; i < g_Slots.size(); ++i) {

			bool value = false;
			if (!a_Actor->GetGraphVariableBool(g_Slots[i].Interned, value)) {
				continue;
			}

			const GraphMask flag = GraphMask{1} << i;
			snapshot.Known |= flag;

			if (value) {
				snapshot.Value |= flag;
			}
		}

		return snapshot;
	}

	std::string GraphVars::Describe(const GraphSnapshot& a_Snapshot) {

		std::string out;

		for (std::uint32_t i = 0; i < g_Slots.size(); ++i) {

			if (!a_Snapshot.Has(i)) {
				continue;
			}

			if (!out.empty()) {
				out += ",";
			}

			out += std::format("{}={}", g_Slots[i].Name, a_Snapshot.Get(i) ? 1 : 0);
		}

		return out.empty() ? "none" : out;
	}

	std::uint32_t GraphSnapshot::FirstMismatch(const GraphSignature& a_Signature) const {

		for (std::uint32_t i = 0; i < GraphVars::Count(); ++i) {

			const GraphMask flag = GraphMask{1} << i;
			if ((a_Signature.Mask & flag) == 0) {
				continue;
			}

			const bool want = (a_Signature.Want & flag) != 0;
			if (!Has(i) || Get(i) != want) {
				return i;
			}
		}

		return GraphVars::kNoBit;
	}
}
