/**
 * @file Csm_get.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm::get() implementation.
 */
#include "common/Csm/Csm.h"

#include <cstdint>
#include <stdexcept>

namespace crsce::common {
    /**
     * @name get
     * @brief Read bit (r, c) from the matrix using MSB-first indexing.
     * @param r Row index in [0, kS).
     * @param c Column index in [0, kS).
     * @return 1 if the bit is set, 0 otherwise.
     * @throws std::out_of_range if r or c >= kS.
     */
    auto Csm::get(const std::uint16_t r, const std::uint16_t c) const -> std::uint8_t {
        if (r >= kS || c >= kS) {
            throw std::out_of_range("Csm::get: index out of range");
        }
        const auto word = static_cast<std::uint16_t>(c / 64);
        const auto bit = static_cast<std::uint16_t>(63 - (c % 64));
        return static_cast<std::uint8_t>((rows_[r][word] >> bit) & 1ULL); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::common
