#pragma once

namespace GTS::Actions {

	struct GraphExpect {
		std::string_view Name;
		bool Value;
	};

	using GraphMask = std::bitset<256>;

	struct GraphSignature {
		GraphMask Mask;
		GraphMask Want;

		[[nodiscard]] bool Empty() const {
			return Mask.none();
		}
	};

	struct GraphSnapshot {
		GraphMask Known;
		GraphMask Value;

		[[nodiscard]] bool Has(std::uint32_t a_Bit) const {
			return a_Bit < 256 && Known.test(a_Bit);
		}

		[[nodiscard]] bool Get(std::uint32_t a_Bit) const {
			return a_Bit < 256 && Value.test(a_Bit);
		}

		[[nodiscard]] bool Matches(const GraphSignature& a_Signature) const {
			return (Known & a_Signature.Mask) == a_Signature.Mask && (Value & a_Signature.Mask) == a_Signature.Want;
		}

		[[nodiscard]] std::uint32_t FirstMismatch(const GraphSignature& a_Signature) const;
	};

	class GraphVars {

		public:
		static constexpr std::uint32_t kMaxVars = 256;
		static constexpr std::uint32_t kNoBit = 0xFFFFFFFFu;
		static std::uint32_t Declare(std::string_view a_Name);
		static GraphSignature Compile(std::span<const GraphExpect> a_Signature);
		static void Freeze();

		[[nodiscard]] static std::string_view Name(std::uint32_t a_Bit);
		[[nodiscard]] static std::uint32_t Count();
		[[nodiscard]] static std::string Describe(const GraphSnapshot& a_Snapshot);
		[[nodiscard]] static GraphSnapshot Read(RE::Actor* a_Actor);
	};
}
