/**
 * @file sha1_compress_arm64.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ARM64 SHA-1 block compression using ARMv8.2-a SHA-2 hardware extensions.
 */
#pragma once

#ifdef __aarch64__

#include <cstdint>

extern "C" {
    /**
     * @name sha1_compress_arm64
     * @brief Compress one 64-byte SHA-1 block using ARM SHA-1 extension instructions.
     * @param state Pointer to 5-word (20-byte) hash state array, updated in place.
     * @param block Pointer to 64-byte input block.
     */
    void sha1_compress_arm64(std::uint32_t state[5], const std::uint8_t block[64]);
}

#endif // __aarch64__
