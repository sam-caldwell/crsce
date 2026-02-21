/**
 * @file Decompressor.h
 * @brief CRSCE v1 decompressor scaffolding: header parsing and payload split.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint> // NOLINT
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "decompress/Decompressor/HeaderV1Fields.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Solvers/SelectedSolver.h"

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
        [[nodiscard]] bool good() const { return input_path_.good(); }

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

        // for_each_block removed (tests-only helper); use read_header + read_block + split_payload instead.

        /**
         * @name Decompressor::decompress_file
         * @brief Run full-file decompression and write reconstructed bytes to output_path_.
         * @return bool True on success; false otherwise.
         */
        bool decompress_file();

        /**
         * @name Decompressor::solve_block
         * @brief Construct a per-block solver and delegate the full pipeline.
         *        Orchestrates snapshot setup and final verification only.
         * @param lh Span over LH payload bytes.
         * @param sums Strongly-typed cross-sum targets.
         * @param csm_out Output CSM.
         * @param valid_bits Valid bits in the block (padding beyond treated as zero-locked).
         * @return true on success; false on failure.
         */
        bool solve_block(std::span<const std::uint8_t> lh,
                         const CrossSums &sums,
                         Csm &csm_out,
                         std::uint64_t valid_bits) const;

    private:
        /**
         * @name input_path_
         * @brief Input file stream opened in binary mode.
         */
        std::ifstream input_path_;

        /**
         * @name blocks_remaining_
         * @brief Number of blocks remaining to read after header parsing.
         */
        std::uint64_t blocks_remaining_{0};

        /**
         * @name output_path_
         * @brief Destination path for decompressed output bytes.
         */
        std::string output_path_;

        // No solver state is stored between blocks; solver constructed per block.
    };
} // namespace crsce::decompress
