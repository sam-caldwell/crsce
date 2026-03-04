/**
 * @file Sha1_digest.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief SHA-1 digest function.
 */
#include "common/BitHashBuffer/sha1/sha1_digest.h"
#include "common/BitHashBuffer/sha256/store_be32.h"

#ifdef __aarch64__
#include "common/BitHashBuffer/sha1/sha1_compress_arm64.h"
#endif

#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <span>

namespace crsce::common::detail::sha1 {

#ifndef __aarch64__
    /**
     * @name rotl32
     * @brief Rotate a 32-bit word left by n bits.
     * @param x Value to rotate.
     * @param n Number of bits to rotate (0 < n < 32).
     * @return Rotated value.
     */
    static constexpr std::uint32_t rotl32(std::uint32_t x, unsigned n) {
        return (x << n) | (x >> (32U - n));
    }

    /**
     * @name sha1_ch
     * @brief SHA-1 Choose function: Ch(x,y,z) = (x & y) ^ (~x & z).
     * @param x First word.
     * @param y Second word.
     * @param z Third word.
     * @return Result of Choose function.
     */
    static constexpr std::uint32_t sha1_ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        return (x & y) ^ (~x & z);
    }

    /**
     * @name sha1_parity
     * @brief SHA-1 Parity function: Parity(x,y,z) = x ^ y ^ z.
     * @param x First word.
     * @param y Second word.
     * @param z Third word.
     * @return Result of Parity function.
     */
    static constexpr std::uint32_t sha1_parity(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        return x ^ y ^ z;
    }

    /**
     * @name sha1_maj
     * @brief SHA-1 Majority function: Maj(x,y,z) = (x & y) ^ (x & z) ^ (y & z).
     * @param x First word.
     * @param y Second word.
     * @param z Third word.
     * @return Result of Majority function.
     */
    static constexpr std::uint32_t sha1_maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    /**
     * @name load_be32
     * @brief Load a 32-bit big-endian value from a byte pointer.
     * @param p Pointer to 4 bytes in big-endian order.
     * @return Host-order 32-bit value.
     */
    static std::uint32_t load_be32(const std::uint8_t *p) {
        return (static_cast<std::uint32_t>(p[0]) << 24U)
             | (static_cast<std::uint32_t>(p[1]) << 16U)
             | (static_cast<std::uint32_t>(p[2]) << 8U)
             | (static_cast<std::uint32_t>(p[3]));
    }

    /// @name K_SHA1
    /// @brief SHA-1 round constants.
    static constexpr std::array<std::uint32_t, 4> K_SHA1 = {
        0x5A827999U, 0x6ED9EBA1U, 0x8F1BBCDCU, 0xCA62C1D6U
    };

    /**
     * @name compress_block_sw
     * @brief Software SHA-1 block compression (C++ fallback).
     * @param state 5-word hash state array, updated in place.
     * @param block Pointer to 64-byte input block.
     */
    static void compress_block_sw(std::array<std::uint32_t, 5> &state, const std::uint8_t *block) {
        std::array<std::uint32_t, 80> w{};

        // Load and expand message schedule
        for (int i = 0; i < 16; ++i) {
            w.at(static_cast<std::size_t>(i)) = load_be32(block + static_cast<std::size_t>(i) * 4U);
        }
        for (int i = 16; i < 80; ++i) {
            const auto idx = static_cast<std::size_t>(i);
            w.at(idx) = rotl32(w.at(idx - 3) ^ w.at(idx - 8) ^ w.at(idx - 14) ^ w.at(idx - 16), 1);
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];

        for (int i = 0; i < 80; ++i) {
            const auto idx = static_cast<std::size_t>(i);
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = sha1_ch(b, c, d);
                k = K_SHA1[0];
            } else if (i < 40) {
                f = sha1_parity(b, c, d);
                k = K_SHA1[1];
            } else if (i < 60) {
                f = sha1_maj(b, c, d);
                k = K_SHA1[2];
            } else {
                f = sha1_parity(b, c, d);
                k = K_SHA1[3];
            }
            const std::uint32_t temp = rotl32(a, 5) + f + e + k + w.at(idx);
            e = d;
            d = c;
            c = rotl32(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }
#endif // !__aarch64__

    /**
     * @name sha1_digest
     * @brief Compute SHA-1 over a byte span.
     * @param data Pointer to data buffer.
     * @param len Number of bytes.
     * @return 20-byte SHA-1 digest.
     */
    std::array<std::uint8_t, 20> sha1_digest(const std::uint8_t *data, std::size_t len) {
        std::array<std::uint32_t, 5> state = {
            0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U
        };

        const std::span<const std::uint8_t> data_span{data, len};
        const std::size_t full_chunks = len / 64U;
        const std::size_t rem = len % 64U;

        for (std::size_t chunk = 0; chunk < full_chunks; ++chunk) {
            auto block = data_span.subspan(chunk * 64U, 64U);
#ifdef __aarch64__
            sha1_compress_arm64(state.data(), block.data());
#else
            compress_block_sw(state, block.data());
#endif
        }

        // Prepare final padding blocks
        std::array<std::uint8_t, 128> tail{};
        if (rem > 0) {
            auto src = data_span.subspan(full_chunks * 64U, rem);
            std::memcpy(tail.data(), src.data(), rem);
        }
        tail.at(rem) = 0x80U;

        const auto total_bits = static_cast<std::uint64_t>(len) * 8ULL;

        if (rem + 1 + 8 <= 64) {
            constexpr std::size_t off = 64U - 8U;
            tail[off + 0] = static_cast<std::uint8_t>(total_bits >> 56 & 0xffU);
            tail[off + 1] = static_cast<std::uint8_t>(total_bits >> 48 & 0xffU);
            tail[off + 2] = static_cast<std::uint8_t>(total_bits >> 40 & 0xffU);
            tail[off + 3] = static_cast<std::uint8_t>(total_bits >> 32 & 0xffU);
            tail[off + 4] = static_cast<std::uint8_t>(total_bits >> 24 & 0xffU);
            tail[off + 5] = static_cast<std::uint8_t>(total_bits >> 16 & 0xffU);
            tail[off + 6] = static_cast<std::uint8_t>(total_bits >> 8 & 0xffU);
            tail[off + 7] = static_cast<std::uint8_t>(total_bits & 0xffU);

            const std::span<const std::uint8_t> tail_block{tail.data(), 64U};
#ifdef __aarch64__
            sha1_compress_arm64(state.data(), tail_block.data());
#else
            compress_block_sw(state, tail_block.data());
#endif
        } else {
            const std::span<const std::uint8_t> tail_block1{tail.data(), 64U};
#ifdef __aarch64__
            sha1_compress_arm64(state.data(), tail_block1.data());
#else
            compress_block_sw(state, tail_block1.data());
#endif

            std::array<std::uint8_t, 64> tail2{};
            tail2[56] = static_cast<std::uint8_t>((total_bits >> 56) & 0xffU);
            tail2[57] = static_cast<std::uint8_t>((total_bits >> 48) & 0xffU);
            tail2[58] = static_cast<std::uint8_t>((total_bits >> 40) & 0xffU);
            tail2[59] = static_cast<std::uint8_t>((total_bits >> 32) & 0xffU);
            tail2[60] = static_cast<std::uint8_t>((total_bits >> 24) & 0xffU);
            tail2[61] = static_cast<std::uint8_t>((total_bits >> 16) & 0xffU);
            tail2[62] = static_cast<std::uint8_t>((total_bits >> 8) & 0xffU);
            tail2[63] = static_cast<std::uint8_t>(total_bits & 0xffU);

            const std::span<const std::uint8_t> tail2_span{tail2.data(), tail2.size()};
#ifdef __aarch64__
            sha1_compress_arm64(state.data(), tail2_span.data());
#else
            compress_block_sw(state, tail2_span.data());
#endif
        }

        std::array<std::uint8_t, 20> out{};
        for (int i = 0; i < 5; ++i) {
            crsce::common::detail::sha256::store_be32(
                std::to_address(std::next(out.begin(), i * 4)),
                state.at(static_cast<std::size_t>(i)));
        }
        return out;
    }
} // namespace crsce::common::detail::sha1
