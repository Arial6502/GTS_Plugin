#pragma once

#include "SerdeUtil.hpp"
#include "TLVSerializer.hpp"

namespace Serialization {

    template <class T, std::uint32_t uid, std::uint32_t ver = 1>
    struct TLVRecord {
        T value{};
        static constexpr auto ID = std::byteswap(uid);
        static inline detail::IDRegistrar _idRegistrar{ ID, "TLVRecord" };

        TLVRecord() = default;
        explicit TLVRecord(const T& a_value) : value(a_value) {}

        void Load(SKSE::SerializationInterface* a_this, std::uint32_t a_type, std::uint32_t a_version, std::uint32_t a_size) {

            if (a_type != ID) return;

            //A mismatch here means the record predates this format entirely.
            if (a_version != ver) {
                logger::warn("{}: version {} is not {}, keeping defaults", detail::Uint32ToStr(ID), a_version, ver);
                return;
            }

            std::vector<std::uint8_t> buffer(a_size);
            if (a_size > 0 && !a_this->ReadRecordData(buffer.data(), a_size)) {
                logger::error("{}: could not be read", detail::Uint32ToStr(ID));
                return;
            }

            try {
                T loaded{};
                TLVSerializer::Deserialize(loaded, absl::Span<const std::uint8_t>(buffer.data(), a_size));
                value = std::move(loaded);
                logger::debug("{}: read OK", detail::Uint32ToStr(ID));
            }
            catch (const std::exception& e) {
                logger::error("{}: deserialization failed: {}", detail::Uint32ToStr(ID), e.what());
            }
        }

        void Save(SKSE::SerializationInterface* a_this) const {

            if (!a_this->OpenRecord(ID, ver)) {
                logger::error("{}: could not be saved", detail::Uint32ToStr(ID));
                return;
            }

            try {
                const auto buffer = TLVSerializer::Serialize(value);

                if (!a_this->WriteRecordData(buffer.data(), static_cast<std::uint32_t>(buffer.size()))) {
                    logger::error("{}: write failed", detail::Uint32ToStr(ID));
                    return;
                }

                logger::debug("{}: save OK", detail::Uint32ToStr(ID));
            }
            catch (const std::exception& e) {
                logger::error("{}: serialization failed: {}", detail::Uint32ToStr(ID), e.what());
            }
        }
    };
}
