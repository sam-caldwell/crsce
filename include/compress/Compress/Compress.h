/**
 * @file Compress.h
 * @brief CRSCE v1 compressor (block-level accumulator; no container I/O yet).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/BitHashBuffer/BitHashBuffer.h"
#include "common/CrossSum/CrossSum.h"

namespace crsce::compress {
    /**
     * @class Compress
     * @name Compress
     * @brief Streams input bits row-major into a 511x511 block accumulator.
     *
     * - Updates LSM/VSM/DSM/XSM on bit=1.
     * - Feeds LH by hashing each row (511 bits) plus a 1-bit zero pad to reach 512 bits.
     * - Exposes helpers to serialize cross-sums and retrieve queued LH digests as bytes.
     *
     * Note: This class handles a single block accumulator at a time and does not
     * yet write the CRSCE header/container. That wiring is deferred.
     */
    class Compress {
    public:
        static constexpr std::size_t kS = 511; ///< Matrix dimension
        static constexpr std::size_t kBitsPerRow = kS; ///< 511 data bits per row
        static constexpr std::size_t kRowPadBits = 1; ///< 1 zero pad bit per row
        static constexpr std::size_t kBitsPerBlock = kS * kS; ///< 511*511 data bits per block

        /**
         * @name Compress::Compress
         * @brief Construct with source/destination paths and optional LH seed.
         * @param input_path Source file path to read.
         * @param output_path Destination CRSCE container path.
         * @param lh_seed Seed string for LH initialization.
         */
        explicit Compress(std::string input_path,
                          std::string output_path,
                          const std::string &lh_seed = "CRSCE_v1_seed");

        /**
         * @name Compress::push_bit
         * @brief Push a single data bit (row-major: columns advance fastest).
         * @param bit Data bit to push.
         * @return void
         */
        void push_bit(bool bit);

        /**
         * @name Compress::finalize_block
         * @brief Finalize the current row if partial (pads zeros + 1 LH pad bit).
         * @return void
         */
        void finalize_block();

        /**
         * @name lh_count
         * @brief Number of LH hashes currently queued (rows completed).
         * @return Current LH digest count.
         */
        [[nodiscard]] std::size_t lh_count() const noexcept { return lh_.count(); }

        /**
         * @name lsm
         * @brief Access lowest-sum (row) cross-sums for testing.
         * @return Const reference to LSM vector.
         */
        [[nodiscard]] const crsce::common::CrossSum &lsm() const noexcept { return lsm_; }
        /**
         * @name vsm
         * @brief Access vertical-sum (column) cross-sums for testing.
         * @return Const reference to VSM vector.
         */
        [[nodiscard]] const crsce::common::CrossSum &vsm() const noexcept { return vsm_; }
        /**
         * @name dsm
         * @brief Access diagonal-sum cross-sums for testing.
         * @return Const reference to DSM vector.
         */
        [[nodiscard]] const crsce::common::CrossSum &dsm() const noexcept { return dsm_; }
        /**
         * @name xsm
         * @brief Access anti-diagonal-sum cross-sums for testing.
         * @return Const reference to XSM vector.
         */
        [[nodiscard]] const crsce::common::CrossSum &xsm() const noexcept { return xsm_; }

        /**
         * @name Compress::serialize_cross_sums
         * @brief Serialize cross-sums in order LSM,VSM,DSM,XSM (contiguous bytes).
         * @return std::vector<std::uint8_t> Concatenated serialization of all cross-sums.
         */
        [[nodiscard]] std::vector<std::uint8_t> serialize_cross_sums() const;

        /**
         * @name Compress::pop_all_lh_bytes
         * @brief Pop all available LH digests as a contiguous byte vector (32*N bytes).
         * @return std::vector<std::uint8_t> All pending LH bytes; clears LH queue.
         */
        [[nodiscard]] std::vector<std::uint8_t> pop_all_lh_bytes();

        /**
         * @name Compress::compress_file
         * @brief Compress the input file to the CRSCE v1 container at output_path_.
         * @return bool True on success; false on error.
         */
        bool compress_file();

    private:
        /**
         * @name Compress::pad_and_finalize_row_if_needed
         * @brief If the current row is partial, pad zeros and finalize for LH.
         * @return void
         */
        void pad_and_finalize_row_if_needed();

        /**
         * @name Compress::advance_coords_after_bit
         * @brief Advance row/column coordinates after pushing a bit.
         * @return void
         */
        void advance_coords_after_bit();

        /**
         * @name Compress::reset_block
         * @brief Reset internal state for a new block.
         * @return void
         */
        void reset_block();

        /**
         * @name Compress::push_zero_row
         * @brief Push a full row of zeros and finalize its LH.
         * @return void
         */
        void push_zero_row();

        /**
         * @name input_path_
         * @brief Source file path to compress.
         */
        std::string input_path_;

        /**
         * @name output_path_
         * @brief Destination path for CRSCE v1 container.
         */
        std::string output_path_;

        /**
         * @name seed_
         * @brief Seed string used to derive initial LH seed hash.
         */
        std::string seed_;

        /**
         * @name lsm_
         * @brief Row cross-sum accumulator.
         */
        crsce::common::CrossSum lsm_;
        /**
         * @name vsm_
         * @brief Column cross-sum accumulator.
         */
        crsce::common::CrossSum vsm_;
        /**
         * @name dsm_
         * @brief Diagonal cross-sum accumulator.
         */
        crsce::common::CrossSum dsm_;
        /**
         * @name xsm_
         * @brief Anti-diagonal cross-sum accumulator.
         */
        crsce::common::CrossSum xsm_;

        /**
         * @name lh_
         * @brief Row-level chained hasher producing 32-byte LH digests per row.
         */
        crsce::common::BitHashBuffer lh_;

        /**
         * @name r_
         * @brief Current row index [0..510].
         */
        std::size_t r_{0};
        /**
         * @name c_
         * @brief Current column index [0..510].
         */
        std::size_t c_{0};
        /**
         * @name row_bit_count_
         * @brief Number of bits pushed in current row [0..511).
         */
        std::size_t row_bit_count_{0};
        /**
         * @name total_bits_
         * @brief Total number of data bits pushed in this block.
         */
        std::size_t total_bits_{0};
    };
} // namespace crsce::compress
