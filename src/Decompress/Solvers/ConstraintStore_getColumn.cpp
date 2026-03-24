/**
 * @file ConstraintStore_getColumn.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getColumn implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name getColumn
     * @brief Assemble column c from rowBits_ as 8 uint64 words (MSB-first).
     *
     * Iterates all 511 rows, extracting bit c from each row's rowBits_ storage,
     * and packs the results into a new 8-word array using the same MSB-first layout
     * as getRow(). The output can be passed directly to SHA-1 for column hashing.
     *
     * @param c Column index in [0, 510].
     * @return 8 uint64 words containing the 511 bits of column c packed MSB-first.
     * @throws None
     */
    auto ConstraintStore::getColumn(const std::uint16_t c) const -> std::array<std::uint64_t, 2> {
        std::array<std::uint64_t, 2> result{};

        // Locate column c within the row's uint64 words (MSB-first layout)
        const auto srcWord = c / 64U;
        const auto srcBit  = 63U - (c % 64U);  // MSB-first: bit 0 is at position 63
        const auto srcMask = static_cast<std::uint64_t>(1ULL << srcBit);

        // For each row r, extract bit c and pack into result at position r (MSB-first)
        for (std::uint16_t r = 0; r < kS; ++r) {
            const auto bit = (rowBits_[r][srcWord] & srcMask) != 0 ? 1U : 0U; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            if (bit != 0) {
                const auto dstWord = r / 64U;
                const auto dstBit  = 63U - (r % 64U);
                result[dstWord] |= (1ULL << dstBit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }

        return result;
    }
} // namespace crsce::decompress::solvers
