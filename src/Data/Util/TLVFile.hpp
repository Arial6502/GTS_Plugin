#pragma once

#include "TLVSerializer.hpp"
#include "VectorSerializer.hpp"

// Standalone-file counterpart to the co-save record wrappers (BasicRecord/VectorRecord/MapRecord).
// Reuses TLVSerializer/VectorSerializer unchanged (both already just move plain std::vector<uint8_t>

namespace Serialization {

    enum class FileResult : uint8_t {
        Ok = 0,
        OpenFailed,
        WriteFailed,
        ReadFailed,
        TooSmall,
        BadMagic,
        UnsupportedFormatVersion,
        PayloadTypeMismatch,
        SizeMismatch,
        ChecksumMismatch,
        DeserializeFailed,
    };

    inline const char* ToString(FileResult a_result) {
        switch (a_result) {
            case FileResult::Ok:                        return "OK";
            case FileResult::OpenFailed:                return "Could not open the file";
            case FileResult::WriteFailed:               return "Could not write the file";
            case FileResult::ReadFailed:                return "Could not read the file";
            case FileResult::TooSmall:                  return "File is too small to be valid";
            case FileResult::BadMagic:                  return "Not a BingusUtils data file";
            case FileResult::UnsupportedFormatVersion:  return "File was made by an incompatible version";
            case FileResult::PayloadTypeMismatch:       return "File does not contain the expected kind of data";
            case FileResult::SizeMismatch:              return "File size does not match its header";
            case FileResult::ChecksumMismatch:          return "File is corrupted (checksum mismatch)";
            case FileResult::DeserializeFailed:         return "File contents could not be parsed";
            default:                                    return "Unknown error";
        }
    }

    namespace detail_file {

        // -------------------- CRC-32 (IEEE 802.3), table generated at compile time --------------------
        consteval std::array<std::uint32_t, 256> make_crc32_table() {
            std::array<std::uint32_t, 256> table{};
            for (std::uint32_t i = 0; i < 256; ++i) {
                std::uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                table[i] = c;
            }
            return table;
        }

        inline constexpr std::array<std::uint32_t, 256> CRC32_TABLE = make_crc32_table();

        inline std::uint32_t Crc32(absl::Span<const std::uint8_t> a_data) {
            std::uint32_t crc = 0xFFFFFFFFu;
            for (std::uint8_t b : a_data) {
                crc = CRC32_TABLE[(crc ^ b) & 0xFFu] ^ (crc >> 8);
            }
            return crc ^ 0xFFFFFFFFu;
        }

        template <typename U>
        void append_le(std::vector<std::uint8_t>& out, U value) {
            static_assert(std::is_integral_v<U>, "append_le requires integral");
            for (std::size_t b = 0; b < sizeof(U); ++b)
                out.push_back(static_cast<std::uint8_t>((value >> (8 * b)) & 0xFF));
        }

        template <typename U>
        U read_le(const std::uint8_t* p) {
            static_assert(std::is_integral_v<U>, "read_le requires integral");
            U v = 0;
            for (std::size_t b = 0; b < sizeof(U); ++b)
                v |= (U(p[b]) << (8 * b));
            return v;
        }

        inline constexpr std::uint32_t FILE_MAGIC = 0x50545347u;       // "GTSP" in little-endian
        inline constexpr std::uint16_t FILE_FORMAT_VERSION = 1;
        inline constexpr std::size_t HEADER_SIZE = 4 + 2 + 4 + 4 + 4;  // magic+formatVer+payloadType+payloadSize+crc32

        inline void WriteHeader(std::vector<std::uint8_t>& out, std::uint32_t a_payloadType, std::uint32_t a_payloadSize, std::uint32_t a_crc32) {
            append_le(out, FILE_MAGIC);
            append_le(out, FILE_FORMAT_VERSION);
            append_le(out, a_payloadType);
            append_le(out, a_payloadSize);
            append_le(out, a_crc32);
        }

        // Writes a_bytes to a_path via a temp-file-then-rename so a failed/interrupted write can
        // never leave a half-written file at a_path (the previous good file, if any, is untouched
        // until the rename - which itself replaces the destination atomically on Windows).
        inline FileResult WriteFileAtomic(const std::filesystem::path& a_path, absl::Span<const std::uint8_t> a_bytes) {
            std::error_code ec;
            if (a_path.has_parent_path()) {
                std::filesystem::create_directories(a_path.parent_path(), ec);
            }

            const auto tmpPath = a_path.string() + ".tmp";

            {
                std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
                if (!out.is_open()) {
                    return FileResult::OpenFailed;
                }

                if (!a_bytes.empty()) {
                    out.write(reinterpret_cast<const char*>(a_bytes.data()), static_cast<std::streamsize>(a_bytes.size()));
                }

                if (!out.good()) {
                    out.close();
                    std::filesystem::remove(tmpPath, ec);
                    return FileResult::WriteFailed;
                }
            }

            std::filesystem::rename(tmpPath, a_path, ec);
            if (ec) {
                std::filesystem::remove(tmpPath, ec);
                return FileResult::WriteFailed;
            }

            return FileResult::Ok;
        }

        inline FileResult ReadFileRaw(const std::filesystem::path& a_path, std::vector<std::uint8_t>& out) {
            std::ifstream in(a_path, std::ios::binary | std::ios::ate);
            if (!in.is_open()) {
                return FileResult::OpenFailed;
            }

            const std::streamsize size = in.tellg();
            if (size < 0) {
                return FileResult::ReadFailed;
            }

            in.seekg(0, std::ios::beg);
            out.resize(static_cast<std::size_t>(size));
            if (size > 0 && !in.read(reinterpret_cast<char*>(out.data()), size)) {
                return FileResult::ReadFailed;
            }

            return FileResult::Ok;
        }

        // Validates the header and hands back a span over the payload bytes (still owned by a_raw).
        inline FileResult ValidateHeader(const std::vector<std::uint8_t>& a_raw, std::uint32_t a_expectedPayloadType, absl::Span<const std::uint8_t>& out_payload) {
            if (a_raw.size() < HEADER_SIZE) {
                return FileResult::TooSmall;
            }

            std::size_t i = 0;
            const std::uint32_t magic = read_le<std::uint32_t>(a_raw.data() + i); i += 4;
            if (magic != FILE_MAGIC) {
                return FileResult::BadMagic;
            }

            const std::uint16_t formatVersion = read_le<std::uint16_t>(a_raw.data() + i); i += 2;
            if (formatVersion != FILE_FORMAT_VERSION) {
                return FileResult::UnsupportedFormatVersion;
            }

            const std::uint32_t payloadType = read_le<std::uint32_t>(a_raw.data() + i); i += 4;
            if (payloadType != a_expectedPayloadType) {
                return FileResult::PayloadTypeMismatch;
            }

            const std::uint32_t payloadSize = read_le<std::uint32_t>(a_raw.data() + i); i += 4;
            const std::uint32_t expectedCrc = read_le<std::uint32_t>(a_raw.data() + i); i += 4;

            if (a_raw.size() - i != payloadSize) {
                return FileResult::SizeMismatch;
            }

            const absl::Span<const std::uint8_t> payload(a_raw.data() + i, payloadSize);
            if (Crc32(payload) != expectedCrc) {
                return FileResult::ChecksumMismatch;
            }

            out_payload = payload;
            return FileResult::Ok;
        }
    }

    // Reads/writes reflectable structs and vectors thereof to standalone files, wrapping the payload
    // in a small versioned+checksummed container so foreign/corrupt
    // files are rejected with a specific reason instead of crashing or silently loading garbage.
    class TLVFile {
		public:
        template <typename Entry>
        static FileResult SaveVector(const std::filesystem::path& a_path, std::uint32_t a_payloadType, const std::vector<Entry>& a_data, std::uint32_t a_schemaVersion = 1) {
            try {
                const auto payload = VectorSerializer<Entry>::Serialize(absl::Span(a_data.data(), a_data.size()), a_schemaVersion);
                return WritePayload(a_path, a_payloadType, payload);
            }
            catch (const std::exception&) {
                return FileResult::WriteFailed;
            }
        }

        template <typename Entry>
        static FileResult LoadVector(const std::filesystem::path& a_path, std::uint32_t a_payloadType, std::vector<Entry>& out) {
            std::vector<std::uint8_t> raw;
            if (const FileResult readRes = detail_file::ReadFileRaw(a_path, raw); readRes != FileResult::Ok) {
                return readRes;
            }

            absl::Span<const std::uint8_t> payload;
            if (const FileResult headerRes = detail_file::ValidateHeader(raw, a_payloadType, payload); headerRes != FileResult::Ok) {
                return headerRes;
            }

            try {
                std::uint32_t dataVersion = 0;
                std::vector<Entry> loaded;
                VectorSerializer<Entry>::Deserialize(loaded, payload, dataVersion);
                out = std::move(loaded);
                return FileResult::Ok;
            }
            catch (const std::exception&) {
                return FileResult::DeserializeFailed;
            }
        }

        template <typename T>
        static FileResult SaveObject(const std::filesystem::path& a_path, std::uint32_t a_payloadType, const T& a_obj) {
            try {
                const auto payload = TLVSerializer::Serialize(a_obj);
                return WritePayload(a_path, a_payloadType, payload);
            }
            catch (const std::exception&) {
                return FileResult::WriteFailed;
            }
        }

        template <typename T>
        static FileResult LoadObject(const std::filesystem::path& a_path, std::uint32_t a_payloadType, T& out) {
            std::vector<std::uint8_t> raw;
            if (const FileResult readRes = detail_file::ReadFileRaw(a_path, raw); readRes != FileResult::Ok) {
                return readRes;
            }

            absl::Span<const std::uint8_t> payload;
            if (const FileResult headerRes = detail_file::ValidateHeader(raw, a_payloadType, payload); headerRes != FileResult::Ok) {
                return headerRes;
            }

            try {
                T loaded{};
                TLVSerializer::Deserialize(loaded, payload);
                out = std::move(loaded);
                return FileResult::Ok;
            }
            catch (const std::exception&) {
                return FileResult::DeserializeFailed;
            }
        }

		private:
        static FileResult WritePayload(const std::filesystem::path& a_path, std::uint32_t a_payloadType, const std::vector<std::uint8_t>& a_payload) {
            const std::uint32_t crc = detail_file::Crc32(absl::Span(a_payload.data(), a_payload.size()));

            std::vector<std::uint8_t> out;
            out.reserve(detail_file::HEADER_SIZE + a_payload.size());
            detail_file::WriteHeader(out, a_payloadType, static_cast<std::uint32_t>(a_payload.size()), crc);
            out.insert(out.end(), a_payload.begin(), a_payload.end());

            return detail_file::WriteFileAtomic(a_path, absl::Span(out.data(), out.size()));
        }
    };
}
