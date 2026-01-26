/**
 * @file Sha256_digest.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief SHA-256 digest function.
 */
#include "common/BitHashBuffer/detail/Sha256Types.h"      // u8
#include "common/BitHashBuffer/detail/Sha256Types32.h"    // u32
#include "common/BitHashBuffer/detail/Sha256Types64.h"    // u64
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "common/BitHashBuffer/detail/sha256/K.h"
#include "common/BitHashBuffer/detail/sha256/ch.h"
#include "common/BitHashBuffer/detail/sha256/maj.h"
#include "common/BitHashBuffer/detail/sha256/big_sigma0.h"
#include "common/BitHashBuffer/detail/sha256/big_sigma1.h"
#include "common/BitHashBuffer/detail/sha256/small_sigma0.h"
#include "common/BitHashBuffer/detail/sha256/small_sigma1.h"
#include "common/BitHashBuffer/detail/sha256/load_be32.h"
#include "common/BitHashBuffer/detail/sha256/store_be32.h"

#include <array>
#include <cstring>
#include <iterator>
#include <memory>
#include <span>

namespace crsce::common::detail::sha256 {
    /**
     * @name sha256_digest
     * @brief Compute SHA-256 over a byte span.
     * @param data Pointer to data buffer.
     * @param len Number of bytes.
     * @return 32-byte SHA-256 digest.
     */
    std::array<u8, 32> sha256_digest(const u8 *data, std::size_t len) {
        u32 h0 = 0x6a09e667U;
        u32 h1 = 0xbb67ae85U;
        u32 h2 = 0x3c6ef372U;
        u32 h3 = 0xa54ff53aU;
        u32 h4 = 0x510e527fU;
        u32 h5 = 0x9b05688cU;
        u32 h6 = 0x1f83d9abU;
        u32 h7 = 0x5be0cd19U;

        const std::size_t full_chunks = len / 64U;
        const std::size_t rem = len % 64U;

        std::array<u32, 64> w{};

        for (std::size_t chunk = 0; chunk < full_chunks; ++chunk) {
            const std::span<const u8> data_span{data, len};
            auto block = data_span.subspan(chunk * 64U, 64U);
            for (int i = 0; i < 16; ++i) {
                const std::size_t off = static_cast<std::size_t>(i) * 4U;
                w.at(static_cast<std::size_t>(i)) = load_be32(block.subspan(off).data());
            }
            for (int i = 16; i < 64; ++i) {
                const auto i_sz = static_cast<std::size_t>(i);
                w.at(i_sz) = small_sigma1(w.at(i_sz - 2)) + w.at(i_sz - 7)
                             + small_sigma0(w.at(i_sz - 15)) + w.at(i_sz - 16);
            }

            u32 a = h0;
            u32 b = h1;
            u32 c = h2;
            u32 d = h3;
            u32 e = h4;
            u32 f = h5;
            u32 g = h6;
            u32 h = h7;
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
            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
            h5 += f;
            h6 += g;
            h7 += h;
        }

        // Prepare final padding blocks
        std::array<u8, 128> tail{};
        /**
         * @brief Implementation detail.
         */
        if (rem > 0) {
            const std::span<const u8> data_span{data, len};
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

            const std::span<const u8> tail_span{tail.data(), tail.size()};
            for (int i = 0; i < 16; ++i) {
                w.at(static_cast<std::size_t>(i)) =
                        load_be32(tail_span.subspan(static_cast<std::size_t>(i) * 4U).data());
            }
            for (int i = 16; i < 64; ++i) {
                const auto i_sz = static_cast<std::size_t>(i);
                w.at(i_sz) = small_sigma1(w.at(i_sz - 2)) + w.at(i_sz - 7)
                             + small_sigma0(w.at(i_sz - 15)) + w.at(i_sz - 16);
            }
            u32 a = h0;
            u32 b = h1;
            u32 c = h2;
            u32 d = h3;
            u32 e = h4;
            u32 f = h5;
            u32 g = h6;
            u32 h = h7;
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
            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
            h5 += f;
            h6 += g;
            h7 += h;
        } else {
            const std::span<const u8> tail_span2{tail.data(), tail.size()};
            for (int i = 0; i < 16; ++i) {
                w.at(static_cast<std::size_t>(i)) = load_be32(
                    tail_span2.subspan(static_cast<std::size_t>(i) * 4U).data());
            }
            for (int i = 16; i < 64; ++i) {
                const auto i_sz = static_cast<std::size_t>(i);
                w.at(i_sz) = small_sigma1(w.at(i_sz - 2)) + w.at(i_sz - 7)
                             + small_sigma0(w.at(i_sz - 15)) + w.at(i_sz - 16);
            }
            u32 a = h0;
            u32 b = h1;
            u32 c = h2;
            u32 d = h3;
            u32 e = h4;
            u32 f = h5;
            u32 g = h6;
            u32 h = h7;
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
            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
            h5 += f;
            h6 += g;
            h7 += h;

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
            for (int i = 0; i < 16; ++i) {
                w.at(static_cast<std::size_t>(i)) = load_be32(
                    tail2_span.subspan(static_cast<std::size_t>(i) * 4U).data());
            }
            for (int i = 16; i < 64; ++i) {
                const auto i_sz = static_cast<std::size_t>(i);
                w.at(i_sz) = small_sigma1(w.at(i_sz - 2)) + w.at(i_sz - 7)
                             + small_sigma0(w.at(i_sz - 15)) + w.at(i_sz - 16);
            }
            a = h0;
            b = h1;
            c = h2;
            d = h3;
            e = h4;
            f = h5;
            g = h6;
            h = h7;
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
            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
            h5 += f;
            h6 += g;
            h7 += h;
        }

        std::array<u8, 32> out{};
        auto *p0 = std::to_address(std::next(out.begin(), 0));
        auto *p1 = std::to_address(std::next(out.begin(), 4));
        auto *p2 = std::to_address(std::next(out.begin(), 8));
        auto *p3 = std::to_address(std::next(out.begin(), 12));
        auto *p4 = std::to_address(std::next(out.begin(), 16));
        auto *p5 = std::to_address(std::next(out.begin(), 20));
        auto *p6 = std::to_address(std::next(out.begin(), 24));
        auto *p7 = std::to_address(std::next(out.begin(), 28));
        store_be32(p0, h0);
        store_be32(p1, h1);
        store_be32(p2, h2);
        store_be32(p3, h3);
        store_be32(p4, h4);
        store_be32(p5, h5);
        store_be32(p6, h6);
        store_be32(p7, h7);
        return out;
    }
} // namespace crsce::common::detail::sha256
