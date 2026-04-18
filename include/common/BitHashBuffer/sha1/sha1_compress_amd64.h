/**
 * @file sha1_compress_amd64.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief AMD64 SHA-1 block compression using x86 SHA-NI hardware extensions.
 */
#pragma once

#if defined(__x86_64__) && defined(__SHA__)

#include <cstdint>

extern "C" {
    /**
     * @name sha1_compress_amd64
     * @brief Compress one 64-byte SHA-1 block using x86 SHA-NI extension instructions.
     * @param state Pointer to 5-word (20-byte) hash state array, updated in place.
     * @param block Pointer to 64-byte input block.
     */
    void sha1_compress_amd64(std::uint32_t state[5], const std::uint8_t block[64]);
}

#endif // defined(__x86_64__) && defined(__SHA__)
