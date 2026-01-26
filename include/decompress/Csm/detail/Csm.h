/**
 * @file Csm.h
 * @brief CRSCE v1 Cross-Sum Matrix (CSM) container for decompression.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::decompress {
    /**
     * @class Csm
     * @name Csm
     * @brief Represents a 511x511 bit matrix with parallel lock and data layers.
     *
     * - Bit layer: stores the reconstructed CSM bits.
     * - Lock layer: marks solved bits that must not be changed by heuristic passes.
     * - Data layer: auxiliary scores/stats used by probabilistic guidance (LBP, etc.).
     */
    class Csm {
    public:
        static constexpr std::size_t kS = 511;
        static constexpr std::size_t kTotalBits = kS * kS;
        static constexpr std::size_t kBytes = (kTotalBits + 7) / 8;

        /**
         * @name Csm::Csm
         * @brief Construct a zero-initialized matrix with all locks cleared.
         */
        Csm();

        /**
         * @name Csm::reset
         * @brief Reset all layers to defaults: bits=0, locks=false, data=0.0.
         * @return void
         */
        void reset() noexcept;

        /**
         * @name Csm::put
         * @brief Store bit value at position (r,c). Bounds: 0..510.
         * @param r Row index.
         * @param c Column index.
         * @param v Bit value to store.
         * @return void
         */
        void put(std::size_t r, std::size_t c, bool v);

        /**
         * @name Csm::get
         * @brief Retrieve bit value at position (r,c).
         * @param r Row index.
         * @param c Column index.
         * @return bool Stored bit value.
         */
        [[nodiscard]] bool get(std::size_t r, std::size_t c) const;

        /**
         * @name Csm::lock
         * @brief Lock bit at (r,c) to mark as solved.
         * @param r Row index.
         * @param c Column index.
         * @return void
         */
        void lock(std::size_t r, std::size_t c);

        /**
         * @name Csm::is_locked
         * @brief Query whether position (r,c) is locked.
         * @param r Row index.
         * @param c Column index.
         * @return bool True if locked; false otherwise.
         */
        [[nodiscard]] bool is_locked(std::size_t r, std::size_t c) const;

        /**
         * @name Csm::set_data
         * @brief Set auxiliary data value for (r,c).
         * @param r Row index.
         * @param c Column index.
         * @param value Data value to set.
         * @return void
         */
        void set_data(std::size_t r, std::size_t c, double value);

        /**
         * @name Csm::get_data
         * @brief Get auxiliary data value for (r,c).
         * @param r Row index.
         * @param c Column index.
         * @return double Data value stored for the cell.
         */
        [[nodiscard]] double get_data(std::size_t r, std::size_t c) const;

        /**
         * @name bytes_capacity
         * @brief Access raw backing size in bytes for the bit layer.
         * @return Number of bytes in bits_ buffer (kBytes).
         */
        [[nodiscard]] static constexpr std::size_t bytes_capacity() noexcept { return kBytes; }

    private:
        /**
         * @name in_bounds
         * @brief Check matrix bounds for (r,c).
         * @param r Row index.
         * @param c Column index.
         * @return true if 0 ≤ r,c < kS; false otherwise.
         */
        [[nodiscard]] static constexpr bool in_bounds(const std::size_t r, const std::size_t c) noexcept {
            return r < kS && c < kS;
        }

        /**
         * @name index_of
         * @brief Compute linear index for (r,c) in row-major order.
         * @param r Row index.
         * @param c Column index.
         * @return Linear index into data_ for (r,c).
         */
        [[nodiscard]] static constexpr std::size_t index_of(const std::size_t r, const std::size_t c) noexcept {
            return (r * kS) + c; // row-major
        }

        /**
         * @name byte_index
         * @brief Compute byte index containing a given bit position.
         * @param bit_index Bit index into the bit layer.
         * @return Index into bits_ (byte address) containing the bit.
         */
        [[nodiscard]] static constexpr std::size_t byte_index(std::size_t const bit_index) noexcept {
            return bit_index >> 3; // divide by 8
        }

        /**
         * @name bit_mask
         * @brief Compute bit mask within a byte for a given bit index.
         * @param bit_index Bit index into the bit layer.
         * @return Mask with a single 1 at the position for bit_index within its byte.
         */
        [[nodiscard]] static constexpr std::uint8_t bit_mask(std::size_t bit_index) noexcept {
            // LSB-first within a byte to keep operations simple
            const auto off = static_cast<std::uint8_t>(bit_index & 0x7U);
            return static_cast<std::uint8_t>(1U << off);
        }

        /**
         * @name bits_
         * @brief Packed bit layer (LSB-first per byte), size kBytes.
         */
        std::vector<std::uint8_t> bits_;

        /**
         * @name locks_
         * @brief Lock layer: 0=unlocked, 1=locked for each of kTotalBits positions.
         */
        std::vector<std::uint8_t> locks_;

        /**
         * @name data_
         * @brief Auxiliary floating-point data per cell (beliefs or scores), size kTotalBits.
         */
        std::vector<double> data_;
    };
} // namespace crsce::decompress
