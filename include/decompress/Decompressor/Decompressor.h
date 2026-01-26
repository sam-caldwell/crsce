/**
 * @file Decompressor.h
 * @brief CRSCE v1 decompressor scaffolding: header parsing and payload split.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
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

        /**
         * @name Decompressor::parse_header
         * @brief Parse header v1 (validates magic, version, sizes, CRC32).
         * @param hdr 28-byte header buffer.
         * @param out Parsed header fields on success.
         * @return bool True on success; false if invalid.
         */
        static bool parse_header(const std::array<std::uint8_t, kHeaderSize> &hdr,
                                 crsce::decompress::HeaderV1Fields &out);

        /**
         * @name Decompressor::split_payload
         * @brief Split a block payload into LH and cross-sum spans if size matches kBlockBytes.
         * @param block Full block payload buffer.
         * @param out_lh Output span over LH bytes.
         * @param out_sums Output span over cross-sum bytes.
         * @return bool True if block size is valid; false otherwise.
         */
        static bool split_payload(const std::vector<std::uint8_t> &block,
                                  std::span<const std::uint8_t> &out_lh,
                                  std::span<const std::uint8_t> &out_sums);

        /**
         * @name Decompressor::Decompressor
         * @brief Construct a decompressor bound to an input file path.
         * @param input_path Path to a CRSCE container file.
         */
        explicit Decompressor(const std::string &input_path);

        /**
         * @name Decompressor::Decompressor
         * @brief Construct a decompressor with input and output paths.
         * @param input_path Path to a CRSCE container file.
         * @param output_path Destination for decompressed output.
         */
        explicit Decompressor(const std::string &input_path, const std::string &output_path);

        /**
         * @name good
         * @brief True if the underlying stream opened successfully.
         * @return true if the stream is in a good state; false otherwise.
         */
        [[nodiscard]] bool good() const { return in_.good(); }

        /**
         * @name Decompressor::read_header
         * @brief Read and parse the header from the input stream.
         * @param out Parsed header fields on success.
         * @return bool True on success; false on I/O or parse error.
         */
        bool read_header(crsce::decompress::HeaderV1Fields &out);

        /**
         * @name Decompressor::read_block
         * @brief Read the next full block payload.
         * @return std::optional<std::vector<std::uint8_t>> Block bytes or nullopt on EOF/short-read.
         */
        std::optional<std::vector<std::uint8_t> > read_block();

        /**
         * @name blocks_remaining
         * @brief Blocks remaining to read after header is parsed.
         * @return Remaining block count.
         */
        [[nodiscard]] std::uint64_t blocks_remaining() const noexcept { return blocks_remaining_; }

        /**
         * @name Decompressor::for_each_block
         * @brief Iterate all blocks, invoking a callback with LH and sums spans for each.
         * @param hdr Parsed header fields (input/output for state).
         * @param fn Callback invoked per block with spans to LH and sums bytes.
         * @return bool False if header invalid or any block cannot be read fully; true otherwise.
         */
        bool for_each_block(crsce::decompress::HeaderV1Fields &hdr,
                            const std::function<void(std::span<const std::uint8_t> lh,
                                                     std::span<const std::uint8_t> sums)> &fn);

        /**
         * @name Decompressor::decompress_file
         * @brief Run full-file decompression and write reconstructed bytes to output_path_.
         * @return bool True on success; false otherwise.
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
