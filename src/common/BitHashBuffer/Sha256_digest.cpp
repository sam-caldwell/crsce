/**
 * @file Sha256_digest.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 digest function.
 */
#include "common/BitHashBuffer/Sha256Types.h"      // u8
#include "common/BitHashBuffer/Sha256Types32.h"    // u32
#include "common/BitHashBuffer/Sha256Types64.h"    // u64
#include "common/BitHashBuffer/sha256/sha256_digest.h"
#include "common/BitHashBuffer/sha256/store_be32.h"

#ifdef __aarch64__
#include "common/BitHashBuffer/sha256/sha256_compress_arm64.h"
#else
#include "common/BitHashBuffer/sha256/K.h"
#include "common/BitHashBuffer/sha256/ch.h"
#include "common/BitHashBuffer/sha256/maj.h"
#include "common/BitHashBuffer/sha256/big_sigma0.h"
#include "common/BitHashBuffer/sha256/big_sigma1.h"
#include "common/BitHashBuffer/sha256/small_sigma0.h"
#include "common/BitHashBuffer/sha256/small_sigma1.h"
#include "common/BitHashBuffer/sha256/load_be32.h"
#endif

#include <array>
#include <cstring>
#include <iterator>
#include <memory>
#include <span>

namespace crsce::common::detail::sha256 {

#ifndef __aarch64__
    /**
     * @name compress_block_sw
     * @brief Software SHA-256 block compression (C++ fallback).
     * @param state 8-word hash state array, updated in place.
     * @param w Pre-expanded message schedule (64 words).
     */
    static void compress_block_sw(std::array<u32, 8> &state, const std::array<u32, 64> &w) {
        u32 a = state[0];
        u32 b = state[1];
        u32 c = state[2];
        u32 d = state[3];
        u32 e = state[4];
        u32 f = state[5];
        u32 g = state[6];
        u32 h = state[7];
        for (int i = 0; i < 64; ++i) {
            const auto idx = static_cast<std::size_t>(i);
            const u32 t1 = h + big_sigma1(e) + ch(e, f, g) + K.at(idx) + w.at(idx);
            const u32 t2 = big_sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    /**
     * @name expand_schedule
     * @brief Expand 16 message words into 64-word schedule.
     * @param w Output schedule array.
     * @param block_span Span over 64-byte block data.
     */
    static void expand_schedule(std::array<u32, 64> &w, std::span<const u8> block_span) {
        for (int i = 0; i < 16; ++i) {
            const std::size_t off = static_cast<std::size_t>(i) * 4U;
            w.at(static_cast<std::size_t>(i)) = load_be32(block_span.subspan(off).data());
        }
        for (int i = 16; i < 64; ++i) {
            const auto i_sz = static_cast<std::size_t>(i);
            w.at(i_sz) = small_sigma1(w.at(i_sz - 2)) + w.at(i_sz - 7)
                         + small_sigma0(w.at(i_sz - 15)) + w.at(i_sz - 16);
        }
    }
#endif // !__aarch64__

    /**
     * @name sha256_digest
     * @brief Compute SHA-256 over a byte span.
     * @param data Pointer to data buffer.
     * @param len Number of bytes.
     * @return 32-byte SHA-256 digest.
     */
    std::array<u8, 32> sha256_digest(const u8 *data, std::size_t len) {
        std::array<u32, 8> state = {
            0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
        };

        const std::span<const u8> data_span{data, len};
        const std::size_t full_chunks = len / 64U;
        const std::size_t rem = len % 64U;

#ifndef __aarch64__
        std::array<u32, 64> w{};
#endif

        for (std::size_t chunk = 0; chunk < full_chunks; ++chunk) {
            auto block = data_span.subspan(chunk * 64U, 64U);
#ifdef __aarch64__
            sha256_compress_arm64(state.data(), block.data());
#else
            expand_schedule(w, block);
            compress_block_sw(state, w);
#endif
        }

        // Prepare final padding blocks
        std::array<u8, 128> tail{};
        if (rem > 0) {
            auto src = data_span.subspan(full_chunks * 64U, rem);
            std::memcpy(tail.data(), src.data(), rem);
        }
        tail.at(rem) = 0x80U;

        const u64 total_bits = static_cast<u64>(len) * 8ULL;

        if (rem + 1 + 8 <= 64) {
            constexpr std::size_t off = 64U - 8U;
            tail[off + 0] = static_cast<u8>(total_bits >> 56 & 0xffU);
            tail[off + 1] = static_cast<u8>(total_bits >> 48 & 0xffU);
            tail[off + 2] = static_cast<u8>(total_bits >> 40 & 0xffU);
            tail[off + 3] = static_cast<u8>(total_bits >> 32 & 0xffU);
            tail[off + 4] = static_cast<u8>(total_bits >> 24 & 0xffU);
            tail[off + 5] = static_cast<u8>(total_bits >> 16 & 0xffU);
            tail[off + 6] = static_cast<u8>(total_bits >> 8 & 0xffU);
            tail[off + 7] = static_cast<u8>(total_bits & 0xffU);

            const std::span<const u8> tail_block{tail.data(), 64U};
#ifdef __aarch64__
            sha256_compress_arm64(state.data(), tail_block.data());
#else
            expand_schedule(w, tail_block);
            compress_block_sw(state, w);
#endif
        } else {
            const std::span<const u8> tail_block1{tail.data(), 64U};
#ifdef __aarch64__
            sha256_compress_arm64(state.data(), tail_block1.data());
#else
            expand_schedule(w, tail_block1);
            compress_block_sw(state, w);
#endif

            std::array<u8, 64> tail2{};
            tail2[56] = static_cast<u8>((total_bits >> 56) & 0xffU);
            tail2[57] = static_cast<u8>((total_bits >> 48) & 0xffU);
            tail2[58] = static_cast<u8>((total_bits >> 40) & 0xffU);
            tail2[59] = static_cast<u8>((total_bits >> 32) & 0xffU);
            tail2[60] = static_cast<u8>((total_bits >> 24) & 0xffU);
            tail2[61] = static_cast<u8>((total_bits >> 16) & 0xffU);
            tail2[62] = static_cast<u8>((total_bits >> 8) & 0xffU);
            tail2[63] = static_cast<u8>(total_bits & 0xffU);

            const std::span<const u8> tail2_span{tail2.data(), tail2.size()};
#ifdef __aarch64__
            sha256_compress_arm64(state.data(), tail2_span.data());
#else
            expand_schedule(w, tail2_span);
            compress_block_sw(state, w);
#endif
        }

        std::array<u8, 32> out{};
        for (int i = 0; i < 8; ++i) {
            store_be32(std::to_address(std::next(out.begin(), i * 4)),
                       state.at(static_cast<std::size_t>(i)));
        }
        return out;
    }
} // namespace crsce::common::detail::sha256
