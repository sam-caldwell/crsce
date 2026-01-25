/**
 * @file Decompressor.h
 * @brief CRSCE v1 decompressor scaffolding: header parsing and payload split.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "decompress/Decompressor/HeaderV1Fields.h"

namespace crsce::decompress {

    /**
     * @name Decompressor
     * @brief CRSCE v1 decompressor: header parsing and block iteration helpers.
     */
    class Decompressor {
    public:
        static constexpr std::size_t kHeaderSize = 28;
        static constexpr std::size_t kLhBytes = 511 * 32; // 16,352
        static constexpr std::size_t kSumsBytes = 4 * 575; // 2,300
        static constexpr std::size_t kBlockBytes = kLhBytes + kSumsBytes; // 18,652

        /** Parse header v1 (validates magic, version, size, CRC32). */
        static bool parse_header(const std::array<std::uint8_t, kHeaderSize> &hdr,
                                 crsce::decompress::HeaderV1Fields &out);

        /** Split payload buffer into LH and sums spans if size is exact. */
        static bool split_payload(const std::vector<std::uint8_t> &block,
                                  std::span<const std::uint8_t> &out_lh,
                                  std::span<const std::uint8_t> &out_sums);

        /**
         * Construct a decompressor bound to an input file path.
         * Stream opens lazily; call read_header() first to initialize sizes.
         */
        explicit Decompressor(const std::string &input_path);

        /**
         * Construct a decompressor with input and output paths. The output path is stored for
         * use by decompress_file().
         */
        explicit Decompressor(const std::string &input_path, const std::string &output_path);

        /**
         * @name good
         * @brief True if the underlying stream opened successfully.
         * @return true if the stream is in a good state; false otherwise.
         */
        [[nodiscard]] bool good() const { return in_.good(); }

        /** Read and parse the header from the stream. */
        bool read_header(crsce::decompress::HeaderV1Fields &out);

        /** Read the next full block payload; returns nullopt on EOF/short-read. */
        std::optional<std::vector<std::uint8_t> > read_block();

        /**
         * @name blocks_remaining
         * @brief Blocks remaining to read after header is parsed.
         * @return Remaining block count.
         */
        [[nodiscard]] std::uint64_t blocks_remaining() const noexcept { return blocks_remaining_; }

        /**
         * Iterate all blocks, invoking a callback with LH and sums spans for each.
         * Returns false if header is invalid or any block cannot be read fully.
         */
        bool for_each_block(crsce::decompress::HeaderV1Fields &hdr,
                            const std::function<void(std::span<const std::uint8_t> lh,
                                                     std::span<const std::uint8_t> sums)> &fn);

        /**
         * Run full-file decompression and write the reconstructed bytes to output_path_.
         * Returns true on success.
         */
        bool decompress_file();

    private:
        /**
         * @name in_
         * @brief Input file stream opened in binary mode.
         */
        std::ifstream in_;

        /**
         * @name blocks_remaining_
         * @brief Number of blocks remaining to read after header parsing.
         */
        std::uint64_t blocks_remaining_{0};

        /**
         * @name output_path_
         * @brief Destination path for decompressed output bytes.
         */
        std::string output_path_{};
    };
} // namespace crsce::decompress
