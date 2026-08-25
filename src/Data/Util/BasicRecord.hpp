#pragma once

#include "SerdeUtil.hpp"

namespace Serialization {

	// Format: [uint32 byteCount][byteCount bytes (no null terminator)]
	// Supports std::string and std::string_view on save; std::string on load.

	static bool ReadU32(SKSE::SerializationInterface* s, std::uint32_t& out){
		return s->ReadRecordData(&out, sizeof(out));
	}

	static bool WriteU32(SKSE::SerializationInterface* s, std::uint32_t v){
		return s->WriteRecordData(&v, sizeof(v));
	}

	// Current layout is [uint32 len][len bytes]. Records written before the header shrank use
	// [uint64 len][len bytes], so both are accepted - but only when the header actually agrees
	// with the record size. Anything else is refused rather than returned as a partial string.
	static bool ReadStringPayload(SKSE::SerializationInterface* s, std::uint32_t payloadSize, std::string& out){

		out.clear();

		if (payloadSize == 0) {
			return true;
		}

		if (payloadSize < sizeof(std::uint32_t)) {
			return false;   //Too small to contain even a length header.
		}

		std::uint32_t len = 0;
		if (!s->ReadRecordData(&len, sizeof(len))) {
			return false;
		}

		const auto readBody = [&]() {
			out.resize(len);
			return len == 0 || s->ReadRecordData(out.data(), len);
		};

		if (payloadSize - sizeof(std::uint32_t) == len) {
			return readBody();
		}

		// Legacy uint64 header: the low half has just been consumed, so the next four bytes are
		// the high half and have to be zero for the length to be believable.
		if (payloadSize >= 2 * sizeof(std::uint32_t)) {

			std::uint32_t high = 0;
			if (!s->ReadRecordData(&high, sizeof(high))) {
				return false;
			}

			if (high == 0 && payloadSize - 2 * sizeof(std::uint32_t) == len) {
				return readBody();
			}
		}

		return false;
	}

	static bool WriteStringPayload(SKSE::SerializationInterface* s, std::string_view sv){
		const std::uint32_t len = static_cast<std::uint32_t>(sv.size());
		if (!WriteU32(s, len)) {
			return false;
		}
		if (len == 0) {
			return true;
		}
		return s->WriteRecordData(sv.data(), len);
	}

	// -------------------- record --------------------
	template <class T, std::uint32_t uid, std::uint32_t ver = 1>
	struct BasicRecord{
		using value_type = T;

		T value{};
		static constexpr auto ID = std::byteswap(uid);
		static inline detail::IDRegistrar _idRegistrar{ ID, "BasicRecord" };

		BasicRecord() = default;
		explicit BasicRecord(const T& v) : value(v) {}

		void Load(SKSE::SerializationInterface* s, std::uint32_t type, std::uint32_t version, std::uint32_t size){
			if (type != ID) {
				return;
			}

			logger::trace("{}: Is being Read", detail::Uint32ToStr(ID));

			if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
				// Accept variable size (string payload), not sizeof(T).
				if (version == ver) {
					std::string tmp;
					if (ReadStringPayload(s, size, tmp)) {
						value = std::move(tmp);
						logger::trace("{}: Read OK! (string, bytes={})", detail::Uint32ToStr(ID), value.size());
						return;
					}
				}
				logger::error("{}: Could not be loaded! (string)", detail::Uint32ToStr(ID));
			}
			else {
				if (version == ver && size == sizeof(T)) {
					if (s->ReadRecordData(&value, sizeof(T))) {
						logger::trace("{}: Read OK!", detail::Uint32ToStr(ID));
						if (detail::CheckFloat(value)) {
							logger::warn("{}: Was NaN!", detail::Uint32ToStr(ID));
						}
						return;
					}
				}
				logger::error("{}: Could not be loaded!", detail::Uint32ToStr(ID));
			}
		}

		void Save(SKSE::SerializationInterface* s) const {
			logger::trace("{}: Is being saved!", detail::Uint32ToStr(ID));

			if (!s->OpenRecord(ID, ver)) {
				logger::error("{}: Could not be saved", detail::Uint32ToStr(ID));
				return;
			}

			if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
				if (WriteStringPayload(s, std::string_view{ value })) {
					logger::trace("{}: Save OK! (string, bytes={})", detail::Uint32ToStr(ID), value.size());
					return;
				}
				logger::error("{}: Could not be saved (string)", detail::Uint32ToStr(ID));
			}
			else {
				if (s->WriteRecordData(&value, sizeof(T))) {
					logger::trace("{}: Save OK!", detail::Uint32ToStr(ID));
					return;
				}
				logger::error("{}: Could not be saved", detail::Uint32ToStr(ID));
			}
		}
	};

	template <std::uint32_t uid, std::uint32_t ver = 1>
	using StringRecord = BasicRecord<std::string, uid, ver>;

	template <std::uint32_t uid, std::uint32_t ver = 1>
	struct StringViewRecord {
		std::string_view value{};
		static constexpr auto ID = std::byteswap(uid);
		static inline detail::IDRegistrar _idRegistrar{ ID, "StringRecord" };

		void Load(SKSE::SerializationInterface*, std::uint32_t, std::uint32_t, std::uint32_t) = delete;

		void Save(SKSE::SerializationInterface* s) const {
			logger::trace("{}: String is being saved! Length: {}", detail::Uint32ToStr(ID), value.size());
			if (s->OpenRecord(ID, ver) && WriteStringPayload(s, value)) {
				logger::trace("{}: String Save OK!", detail::Uint32ToStr(ID));
				return;
			}
			logger::error("{}: String could not be saved", detail::Uint32ToStr(ID));
		}
	};
}