/**
 * @file sha256_compress_arm64.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ARM64 SHA-256 block compression using ARMv8.2-a SHA-2 hardware extensions.
 */
#pragma once

#ifdef __aarch64__

#include <cstdint>

extern "C" {
    /**
     * @name sha256_compress_arm64
     * @brief Compress one 64-byte SHA-256 block using ARM SHA-2 extension instructions.
     * @param state Pointer to 8-word (32-byte) hash state array, updated in place.
     * @param block Pointer to 64-byte input block.
     */
    void sha256_compress_arm64(std::uint32_t state[8], const std::uint8_t block[64]);
}

#endif // __aarch64__
