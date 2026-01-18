/**
 * @file Sha256.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Internal SHA-256 declarations used by BitHashBuffer.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::common::detail::sha256 {

// Type aliases for clarity in SHA-256 routines
/**
 * @name u8
 * @brief Unsigned 8-bit integer alias used in SHA-256 routines.
 */
using u8 = std::uint8_t;
/**
 * @name u32
 * @brief Unsigned 32-bit integer alias used in SHA-256 routines.
 */
using u32 = std::uint32_t;
/**
 * @name u64
 * @brief Unsigned 64-bit integer alias used in SHA-256 routines (length encoding).
 */
using u64 = std::uint64_t;

/**
 * @name rotr
 * @brief Rotate right utility for 32-bit values.
 * @param x Input word.
 * @param n Rotation amount [0..31].
 * @return x rotated right by n bits.
 */
u32 rotr(u32 x, u32 n);

/**
 * @name ch
 * @brief SHA-256 choose function.
 * @param x Word x.
 * @param y Word y.
 * @param z Word z.
 * @return (x & y) ^ (~x & z)
 */
u32 ch(u32 x, u32 y, u32 z);

/**
 * @name maj
 * @brief SHA-256 majority function.
 * @param x Word x.
 * @param y Word y.
 * @param z Word z.
 * @return Majority bit per position among x, y, z.
 */
u32 maj(u32 x, u32 y, u32 z);

/**
 * @name big_sigma0
 * @brief SHA-256 Σ0 function.
 * @param x Input word.
 * @return Σ0(x) = ROTR2(x) ^ ROTR13(x) ^ ROTR22(x).
 */
u32 big_sigma0(u32 x);

/**
 * @name big_sigma1
 * @brief SHA-256 Σ1 function.
 * @param x Input word.
 * @return Σ1(x) = ROTR6(x) ^ ROTR11(x) ^ ROTR25(x).
 */
u32 big_sigma1(u32 x);

/**
 * @name small_sigma0
 * @brief SHA-256 σ0 function.
 * @param x Input word.
 * @return σ0(x) = ROTR7(x) ^ ROTR18(x) ^ (x >> 3).
 */
u32 small_sigma0(u32 x);

/**
 * @name small_sigma1
 * @brief SHA-256 σ1 function.
 * @param x Input word.
 * @return σ1(x) = ROTR17(x) ^ ROTR19(x) ^ (x >> 10).
 */
u32 small_sigma1(u32 x);

/**
 * @name store_be32
 * @brief Store a 32-bit word in big-endian order.
 * @param dst Destination pointer (>= 4 bytes).
 * @param x Value to store.
 */
void store_be32(u8* dst, u32 x);

/**
 * @name load_be32
 * @brief Load a 32-bit word from big-endian order.
 * @param src Source pointer (>= 4 bytes).
 * @return Parsed 32-bit value.
 */
u32 load_be32(const u8* src);

/**
 * @name K
 * @brief SHA-256 round constants table (64 entries).
 */
extern const std::array<u32, 64> K;

/**
 * @name sha256_digest
 * @brief Compute SHA-256 over a byte span.
 * @param data Pointer to data buffer.
 * @param len Number of bytes.
 * @return 32-byte SHA-256 digest.
 */
std::array<u8, 32> sha256_digest(const u8* data, std::size_t len);

} // namespace crsce::common::detail::sha256
