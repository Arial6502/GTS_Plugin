#pragma once
#include "Data/Util/BasicRecord.hpp"
#include "Data/Util/MapSerializer.hpp"

namespace Serialization {

    template <typename Value, const uint32_t uid, const uint32_t ver = 1>
    struct MapRecord {
        absl::flat_hash_map<RE::FormID, Value> value;
        static constexpr auto ID = std::byteswap(uid);
        static inline detail::IDRegistrar _idRegistrar{ ID, "MapRecord" };

        MapRecord() = default;
        MapRecord(const absl::flat_hash_map<RE::FormID, Value>& val) : value(val) {}

        void Load(SKSE::SerializationInterface* serializationInterface, std::uint32_t type, std::uint32_t version, uint32_t size) {
            if (type == ID) {
                logger::debug("{}: Map is being Read", detail::Uint32ToStr(ID));
                if (version == ver) {
                    // Read the serialized map data
                    std::vector<uint8_t> buffer(size);
                    if (serializationInterface->ReadRecordData(buffer.data(), size)) {
                        try {
                            uint32_t dataVersion = 0;
                            MapSerializer<RE::FormID, Value>::Deserialize(
                                value,
                                absl::Span(buffer.data(), size),
                                dataVersion
                            );

                            logger::debug("{}: Map Read OK! Entry count: {}", detail::Uint32ToStr(ID), value.size());

                            // Resolve FormIDs after loading
                            absl::flat_hash_map<RE::FormID, Value> resolvedMap;

                            auto nam = detail::Uint32ToStr(ID);

                            for (auto& [oldFormID, data] : value) {
                                RE::FormID newFormID;
                                if (serializationInterface->ResolveFormID(oldFormID, newFormID)) {
                                    logger::trace("{} data loaded for FormID {:08X}", nam, oldFormID);
                                    if (RE::TESForm::LookupByID<RE::Actor>(newFormID)) {
                                    	resolvedMap.insert_or_assign(newFormID, std::move(data));
                                    }
                                    else {
                                        logger::warn("{} FormID {:08X} could not be found after loading the save.", nam, newFormID);
                                    }
                                }
                                else {
                                    logger::warn("{} FormID {:08X} could not be resolved. Not adding to map.", nam, oldFormID);
                                }
                            }
                            value = std::move(resolvedMap);
                            return;
                        }
                        catch (const std::exception& e) {
                            logger::error("{}: Map deserialization failed: {}", detail::Uint32ToStr(ID), e.what());
                        }
                    }
                }
                logger::error("{}: Map could not be loaded!", detail::Uint32ToStr(ID));
            }

        }

        void Save(SKSE::SerializationInterface* serializationInterface) const {
            if (value.empty()) {
                logger::debug("{}: Nothing To Save, map is empty", detail::Uint32ToStr(ID));
                return;
            }

            logger::debug("{}: Map is being saved! Entry count: {}", detail::Uint32ToStr(ID), value.size());
            if (serializationInterface->OpenRecord(ID, ver)) {
                try {
                    auto nam = detail::Uint32ToStr(ID);
                    auto buffer = MapSerializer<RE::FormID, Value>::Serialize(value, ver);
                    if (serializationInterface->WriteRecordData(buffer.data(), static_cast<uint32_t>(buffer.size()))) {
                        logger::debug("{}: Map Save OK!", detail::Uint32ToStr(ID));

                        for (auto const& [ActorFormID, Data] : value) {
                            logger::trace("{} data serialized for Actor FormID {:08X}", nam, ActorFormID);
                        }

                        return;
                    }
                }
                catch (const std::exception& e) {
                    logger::error("{}: Map serialization failed: {}", detail::Uint32ToStr(ID), e.what());
                }
            }
            logger::error("{}: Map could not be saved", detail::Uint32ToStr(ID));
        }
    };
}