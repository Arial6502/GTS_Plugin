#pragma once
#include "VectorSerializer.hpp"
#include "SerdeUtil.hpp"

namespace Serialization {

    // VectorRecord variant: stores std::vector<Entry> as-is (no FormID resolving / filtering).
    template <typename Entry, const std::uint32_t uid, const std::uint32_t ver = 1>
    struct VectorRecord {
        std::vector<Entry> value;
        static constexpr auto ID = std::byteswap(uid);
        static inline detail::IDRegistrar _idRegistrar{ ID, "VectorRecord" };

        VectorRecord() = default;
        VectorRecord(std::vector<Entry> val) : value(std::move(val)) {}

        void Load(SKSE::SerializationInterface* serializationInterface, std::uint32_t type, std::uint32_t version, std::uint32_t size) {
           
        	if (type != ID) {
                return;
            }

            logger::debug("{}: Vector is being read", detail::Uint32ToStr(ID));

            if (version != ver) {
                logger::error("{}: Vector version mismatch (got {}, expected {})",
					detail::Uint32ToStr(ID), version, ver
                );
                return;
            }

            std::vector<std::uint8_t> buffer(size);
            if (!serializationInterface->ReadRecordData(buffer.data(), size)) {
                logger::error("{}: Vector could not be read", detail::Uint32ToStr(ID));
                return;
            }

            try {
                std::uint32_t dataVersion = 0;
                std::vector<Entry> loaded;
                VectorSerializer<Entry>::Deserialize(loaded, absl::Span(buffer.data(), size), dataVersion);

                value = std::move(loaded);
                logger::debug("{}: Vector read OK! Entry count: {}", detail::Uint32ToStr(ID), value.size());
            }
            catch (const std::exception& e) {
                logger::error("{}: Vector deserialization failed: {}", detail::Uint32ToStr(ID), e.what());
            }
        }

        void Save(SKSE::SerializationInterface* serializationInterface) const {
            logger::debug("{}: Vector is being saved! Entry count: {}", detail::Uint32ToStr(ID), value.size());

            if (!serializationInterface->OpenRecord(ID, ver)) {
                logger::error("{}: Vector could not be saved (OpenRecord failed)", detail::Uint32ToStr(ID));
                return;
            }

            try {
                auto buffer = VectorSerializer<Entry>::Serialize(absl::Span(value.data(), value.size()), ver);

                if (!serializationInterface->WriteRecordData(buffer.data(),
                    static_cast<std::uint32_t>(buffer.size()))) {
                    logger::error("{}: Vector could not be saved (WriteRecordData failed)", detail::Uint32ToStr(ID));
                    return;
                }

                logger::debug("{}: Vector save OK!", detail::Uint32ToStr(ID));
            }
            catch (const std::exception& e) {
                logger::error("{}: Vector serialization failed: {}", detail::Uint32ToStr(ID), e.what());
            }
        }
    };

}