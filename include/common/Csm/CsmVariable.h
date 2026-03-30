/**
 * @file CsmVariable.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Variable-dimension binary matrix for experimental solvers.
 *
 * Unlike Csm (fixed at kS=127), CsmVariable accepts any dimension s
 * at construction time. Rows are stored as vectors of uint64 words.
 * Used for B.62+ experiments at s=191 and beyond.
 */
#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace crsce::common {

    /**
     * @class CsmVariable
     * @name CsmVariable
     * @brief Variable-dimension s x s binary matrix.
     */
    class CsmVariable {
    public:
        /**
         * @name CsmVariable
         * @brief Construct a zero-initialized s x s binary matrix.
         * @param s Matrix dimension.
         * @throws None
         */
        explicit CsmVariable(const std::uint16_t s)
            : s_(s),
              wordsPerRow_(static_cast<std::uint16_t>((s + 63) / 64)),
              rows_(s, std::vector<std::uint64_t>(static_cast<std::size_t>((s + 63) / 64), 0)) {
        }

        /**
         * @name getS
         * @brief Return the matrix dimension.
         * @return s
         */
        [[nodiscard]] std::uint16_t getS() const { return s_; }

        /**
         * @name get
         * @brief Read cell (r, c).
         * @param r Row index in [0, s).
         * @param c Column index in [0, s).
         * @return 0 or 1.
         * @throws std::out_of_range if r or c >= s.
         */
        [[nodiscard]] std::uint8_t get(const std::uint16_t r, const std::uint16_t c) const {
            if (r >= s_ || c >= s_) {
                throw std::out_of_range("CsmVariable::get: index out of range");
            }
            return static_cast<std::uint8_t>(
                (rows_[r][c / 64] >> (c % 64)) & 1U); // NOLINT
        }

        /**
         * @name set
         * @brief Write cell (r, c).
         * @param r Row index in [0, s).
         * @param c Column index in [0, s).
         * @param v Value (0 or 1).
         * @throws std::out_of_range if r or c >= s.
         */
        void set(const std::uint16_t r, const std::uint16_t c, const std::uint8_t v) {
            if (r >= s_ || c >= s_) {
                throw std::out_of_range("CsmVariable::set: index out of range");
            }
            if (v) {
                rows_[r][c / 64] |= (std::uint64_t{1} << (c % 64)); // NOLINT
            } else {
                rows_[r][c / 64] &= ~(std::uint64_t{1} << (c % 64)); // NOLINT
            }
        }

        /**
         * @name getRow
         * @brief Get the raw uint64 words for row r.
         * @param r Row index.
         * @return Const reference to the row's word vector.
         */
        [[nodiscard]] const std::vector<std::uint64_t> &getRow(const std::uint16_t r) const {
            return rows_.at(r);
        }

        /**
         * @name popcount
         * @brief Count set bits in row r.
         * @param r Row index.
         * @return Number of 1-bits in [0, s).
         */
        [[nodiscard]] std::uint16_t popcount(const std::uint16_t r) const {
            std::uint16_t count = 0;
            for (std::uint16_t c = 0; c < s_; ++c) {
                count += get(r, c);
            }
            return count;
        }

        /**
         * @name serialize
         * @brief Serialize the matrix row-major into a byte buffer for SHA-256.
         *
         * Each row is stored as wordsPerRow uint64 words in big-endian byte order.
         * Total size: s * wordsPerRow * 8 bytes.
         *
         * @return Byte vector suitable for SHA-256 hashing.
         */
        [[nodiscard]] std::vector<std::uint8_t> serialize() const {
            const auto rowBytes = static_cast<std::size_t>(wordsPerRow_) * 8;
            std::vector<std::uint8_t> buf(static_cast<std::size_t>(s_) * rowBytes, 0);
            for (std::uint16_t r = 0; r < s_; ++r) {
                const auto offset = static_cast<std::size_t>(r) * rowBytes;
                for (std::uint16_t w = 0; w < wordsPerRow_; ++w) {
                    const auto val = rows_[r][w]; // NOLINT
                    const auto wOff = offset + static_cast<std::size_t>(w) * 8;
                    for (int b = 7; b >= 0; --b) {
                        buf[wOff + static_cast<std::size_t>(7 - b)] =
                            static_cast<std::uint8_t>((val >> (b * 8)) & 0xFFU); // NOLINT
                    }
                }
            }
            return buf;
        }

    private:
        /**
         * @name s_
         * @brief Matrix dimension.
         */
        std::uint16_t s_;

        /**
         * @name wordsPerRow_
         * @brief Number of uint64 words per row.
         */
        std::uint16_t wordsPerRow_;

        /**
         * @name rows_
         * @brief Row-major storage: s rows of wordsPerRow uint64 words each.
         */
        std::vector<std::vector<std::uint64_t>> rows_;
    };

} // namespace crsce::common
