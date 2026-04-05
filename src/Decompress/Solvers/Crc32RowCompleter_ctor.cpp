/**
 * @file Crc32RowCompleter_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Crc32RowCompleter constructor.
 *
 * The CRC-32 generator matrix and affine constant are now computed at compile time
 * (constexpr in Crc32RowCompleter.h). The constructor only stores the expected CRCs.
 */
#include "decompress/Solvers/Crc32RowCompleter.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {

    /**
     * @name Crc32RowCompleter
     * @brief Construct the completer with expected per-row CRC-32 values.
     * @param expectedCrcs Array of expected CRC-32 values per row.
     * @throws None
     */
    Crc32RowCompleter::Crc32RowCompleter(const std::array<std::uint32_t, kS> &expectedCrcs)
        : expectedCrcs_(expectedCrcs) {
    }

} // namespace crsce::decompress::solvers
