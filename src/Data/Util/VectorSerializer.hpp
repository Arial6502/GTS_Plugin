#pragma once
#include "TLVSerializer.hpp"

namespace Serialization {

    template <typename T>
    class VectorSerializer {
		public:
        static_assert(std::is_standard_layout_v<T> || std::is_aggregate_v<T>, "T must be POD/reflectable");

        static std::vector<std::uint8_t> Serialize(absl::Span<const T> vec, std::uint32_t version) {
            std::vector<std::uint8_t> out;
            append_le(out, version);
            append_le(out, static_cast<std::uint32_t>(vec.size()));

            for (auto const& elem : vec) {
                auto bytes = TLVSerializer::Serialize(elem);
                append_le(out, static_cast<std::uint32_t>(bytes.size()));
                out.insert(out.end(), bytes.begin(), bytes.end());
            }
            return out;
        }

        static void Deserialize(std::vector<T>& vec, absl::Span<const std::uint8_t> data, std::uint32_t& out_version) {
            std::size_t i = 0;
            if (data.size() < 8) {
                throw std::runtime_error("corrupt header");
            }

            out_version = read_le<std::uint32_t>(data.data() + i); i += 4;
            std::uint32_t count = read_le<std::uint32_t>(data.data() + i); i += 4;

            vec.clear();
            vec.reserve(count);

            for (std::uint32_t n = 0; n < count; ++n) {
                if (i + 4 > data.size()) throw std::runtime_error("corrupt vector entry header");

                std::uint32_t len = read_le<std::uint32_t>(data.data() + i); i += 4;
                if (i + len > data.size()) throw std::runtime_error("corrupt vector entry payload");

                T elem{};
                TLVSerializer::Deserialize(elem, absl::Span(data.data() + i, len));
                i += len;

                vec.emplace_back(std::move(elem));
            }
        }

    private:
        template <typename U>
        static void append_le(std::vector<std::uint8_t>& out, U v) {
            static_assert(std::is_integral_v<U>, "append_le requires integral");
            for (std::size_t b = 0; b < sizeof(U); ++b)
                out.push_back(static_cast<std::uint8_t>((v >> (8 * b)) & 0xFF));
        }

        template <typename U>
        static U read_le(const std::uint8_t* p) {
            static_assert(std::is_integral_v<U>, "read_le requires integral");
            U v = 0;
            for (std::size_t b = 0; b < sizeof(U); ++b)
                v |= (U(p[b]) << (8 * b));
            return v;
        }
    };

}