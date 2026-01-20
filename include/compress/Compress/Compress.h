/**
 * @file Compress.h
 * @brief CRSCE v1 compressor (block-level accumulator; no container I/O yet).
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

        /** Construct with source/destination paths and optional LH seed. */
        explicit Compress(std::string input_path,
                          std::string output_path,
                          const std::string &lh_seed = "CRSCE_v1_seed");

        /** Push a single data bit (row-major: columns advance fastest). */
        void push_bit(bool bit);

        /** Finalize the current row if partial (pads zeros + 1 pad bit for LH). */
        void finalize_block();

        /** Number of LH hashes currently queued (rows completed). */
        [[nodiscard]] std::size_t lh_count() const noexcept { return lh_.count(); }

        /** Accessors for testing of vector totals. */
        [[nodiscard]] const crsce::common::CrossSum &lsm() const noexcept { return lsm_; }
        [[nodiscard]] const crsce::common::CrossSum &vsm() const noexcept { return vsm_; }
        [[nodiscard]] const crsce::common::CrossSum &dsm() const noexcept { return dsm_; }
        [[nodiscard]] const crsce::common::CrossSum &xsm() const noexcept { return xsm_; }

        /** Serialize cross-sums in order LSM,VSM,DSM,XSM (contiguous bytes). */
        [[nodiscard]] std::vector<std::uint8_t> serialize_cross_sums() const;

        /** Pop all available LH digests as a contiguous byte vector (32*N bytes). */
        [[nodiscard]] std::vector<std::uint8_t> pop_all_lh_bytes();

        /** Compress the input file to the CRSCE v1 container at output_path_. */
        bool compress_file();

    private:
        void pad_and_finalize_row_if_needed();

        void advance_coords_after_bit();
        void reset_block();
        void push_zero_row();

        std::string input_path_;
        std::string output_path_;
        std::string seed_;

        // Cross-sum accumulators
        crsce::common::CrossSum lsm_;
        crsce::common::CrossSum vsm_;
        crsce::common::CrossSum dsm_;
        crsce::common::CrossSum xsm_;

        // LH row hasher (chained across rows)
        crsce::common::BitHashBuffer lh_;

        // Coordinates and counters for row-major streaming
        std::size_t r_{0}; ///< Current row [0..510]
        std::size_t c_{0}; ///< Current column [0..510]
        std::size_t row_bit_count_{0}; ///< Bits pushed in current row [0..511)
        std::size_t total_bits_{0}; ///< Total data bits pushed into this block
    };
} // namespace crsce::compress
