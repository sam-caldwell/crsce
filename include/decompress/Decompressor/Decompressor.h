/**
 * @file Decompressor.h
 * @brief CRSCE v1 decompressor scaffolding: header parsing and payload split.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace crsce::decompress {
    struct HeaderV1Fields {
        std::uint64_t original_size_bytes{0};
        std::uint64_t block_count{0};
    };

    class Decompressor {
    public:
        static constexpr std::size_t kHeaderSize = 28;
        static constexpr std::size_t kLhBytes = 511 * 32; // 16,352
        static constexpr std::size_t kSumsBytes = 4 * 575; // 2,300
        static constexpr std::size_t kBlockBytes = kLhBytes + kSumsBytes; // 18,652

        /** Parse header v1 (validates magic, version, size, CRC32). */
        static bool parse_header(const std::array<std::uint8_t, kHeaderSize> &hdr,
                                 HeaderV1Fields &out);

        /** Split payload buffer into LH and sums spans if size is exact. */
        static bool split_payload(const std::vector<std::uint8_t> &block,
                                  std::span<const std::uint8_t> &out_lh,
                                  std::span<const std::uint8_t> &out_sums);

        /**
         * Construct a decompressor bound to an input file path.
         * Stream opens lazily; call read_header() first to initialize sizes.
         */
        explicit Decompressor(const std::string &input_path);

        /** True if the underlying stream is good. */
        [[nodiscard]] bool good() const { return in_.good(); }

        /** Read and parse the header from the stream. */
        bool read_header(HeaderV1Fields &out);

        /** Read the next full block payload; returns nullopt on EOF/short-read. */
        std::optional<std::vector<std::uint8_t> > read_block();

        /** Blocks remaining to read after header is parsed. */
        [[nodiscard]] std::uint64_t blocks_remaining() const noexcept { return blocks_remaining_; }

    private:
        std::ifstream in_;
        std::uint64_t blocks_remaining_{0};
    };
} // namespace crsce::decompress
