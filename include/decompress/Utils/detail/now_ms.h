/**
 * @file now_ms.h
 * @brief Milliseconds since epoch utility for decompressor.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::detail {
    /**
     * @name now_ms
     * @brief Return current time in milliseconds since UNIX epoch (UTC).
     * @return Milliseconds since epoch as unsigned 64-bit.
     */
    std::uint64_t now_ms();
}

