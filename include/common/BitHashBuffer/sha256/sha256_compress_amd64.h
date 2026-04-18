/**
 * @file sha256_compress_amd64.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief AMD64 SHA-256 block compression using x86 SHA-NI hardware extensions.
 */
#pragma once

#if defined(__x86_64__) && defined(__SHA__)

#include <cstdint>

extern "C" {
    /**
     * @name sha256_compress_amd64
     * @brief Compress one 64-byte SHA-256 block using x86 SHA-NI extension instructions.
     * @param state Pointer to 8-word (32-byte) hash state array, updated in place.
     * @param block Pointer to 64-byte input block.
     */
    void sha256_compress_amd64(std::uint32_t state[8], const std::uint8_t block[64]);
}

#endif // defined(__x86_64__) && defined(__SHA__)
