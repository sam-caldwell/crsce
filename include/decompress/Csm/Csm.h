/**
 * @file Csm.h
 * @brief CRSCE v1 Cross-Sum Matrix (CSM) container for decompression.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @class Csm
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

        /** Construct a zero-initialized matrix with all locks cleared. */
        Csm();

        /** Reset all layers to defaults: bits=0, locks=false, data=0.0. */
        void reset() noexcept;

        /** Store bit value v at position (r,c). Bounds: 0..510. */
        void put(std::size_t r, std::size_t c, bool v);

        /** Retrieve bit value at (r,c). */
        [[nodiscard]] bool get(std::size_t r, std::size_t c) const;

        /** Lock bit at (r,c) to mark as solved. */
        void lock(std::size_t r, std::size_t c);

        /** Query whether (r,c) is locked. */
        [[nodiscard]] bool is_locked(std::size_t r, std::size_t c) const;

        /** Set auxiliary data value for (r,c). */
        void set_data(std::size_t r, std::size_t c, double value);

        /** Get auxiliary data value for (r,c). */
        [[nodiscard]] double get_data(std::size_t r, std::size_t c) const;

        /** Access raw backing sizes for test/inspection. */
        [[nodiscard]] static constexpr std::size_t bytes_capacity() noexcept { return kBytes; }

    private:
        [[nodiscard]] static constexpr bool in_bounds(const std::size_t r, const std::size_t c) noexcept {
            return r < kS && c < kS;
        }

        [[nodiscard]] static constexpr std::size_t index_of(const std::size_t r, const std::size_t c) noexcept {
            return (r * kS) + c; // row-major
        }

        [[nodiscard]] static constexpr std::size_t byte_index(std::size_t const bit_index) noexcept {
            return bit_index >> 3; // divide by 8
        }

        [[nodiscard]] static constexpr std::uint8_t bit_mask(std::size_t bit_index) noexcept {
            // LSB-first within a byte to keep operations simple
            const auto off = static_cast<std::uint8_t>(bit_index & 0x7U);
            return static_cast<std::uint8_t>(1U << off);
        }

        // Storage
        std::vector<std::uint8_t> bits_; // kBytes bytes; LSB-first per byte
        std::vector<std::uint8_t> locks_; // kTotalBits entries: 0 (unlocked) or 1 (locked)
        std::vector<double> data_; // kTotalBits entries
    };
} // namespace crsce::decompress
