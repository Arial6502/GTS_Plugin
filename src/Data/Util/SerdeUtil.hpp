#pragma once

namespace Serialization::detail {

	template <class T>
	bool CheckFloat(T& value) {
		if constexpr (std::is_floating_point_v<T>) {
			if (std::isnan(value)) {
				value = 0;
				return true;
			}
		}
		return false;
	}

	static std::string Uint32ToStr(std::uint32_t value) {
		char bytes[4];
		bytes[3] = static_cast<char>((value >> 24) & 0xFF);
		bytes[2] = static_cast<char>((value >> 16) & 0xFF);
		bytes[1] = static_cast<char>((value >> 8) & 0xFF);
		bytes[0] = static_cast<char>(value & 0xFF);
		return std::string(bytes, 4);
	}

    inline std::vector<std::pair<std::uint32_t, const char*>>& IDRegistry() {
        static std::vector<std::pair<std::uint32_t, const char*>> registry;
        return registry;
    }

    struct IDRegistrar {
        IDRegistrar(std::uint32_t id, const char* name) {
            for (auto& [existingID, existingName] : IDRegistry()) {
                if (existingID == id) {
					GTS::ReportAndExit(fmt::format("Cosave record ID collision: '{}' and '{}' both use ID {}", existingName, name, Uint32ToStr(id)));
                    std::abort();
                }
            }
            IDRegistry().emplace_back(id, name);
        }
    };


}