/**
 * @file blocks_for_input_bytes.cpp
 * @brief Compute number of 511x511-bit blocks required for given byte count.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/blocks_for_input_bytes.h"
#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name blocks_for_input_bytes
     * @brief Compute number of 511x511-bit blocks required for given byte count.
     * @param input_bytes Input byte length.
     * @return Required block count (ceil).
     */
    std::uint64_t blocks_for_input_bytes(const std::uint64_t input_bytes) {
        constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;
        const std::uint64_t bits = input_bytes * 8ULL;
        return (bits + kBitsPerBlock - 1ULL) / kBitsPerBlock;
    }
}
