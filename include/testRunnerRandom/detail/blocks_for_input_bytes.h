/**
 * @file blocks_for_input_bytes.h
 * @brief Compute timeout blocks for a given input size.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name blocks_for_input_bytes
     * @brief Compute number of 511x511-bit blocks required for given input size.
     */
    std::uint64_t blocks_for_input_bytes(std::uint64_t input_bytes);
}
