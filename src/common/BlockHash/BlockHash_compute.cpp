/**
 * @file BlockHash_compute.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief BlockHash::compute implementation.
 */
#include "common/BlockHash/BlockHash.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/BitHashBuffer/sha256/sha256_digest.h"
#include "common/Csm/Csm.h"

namespace crsce::common {
    /**
     * @name compute
     * @brief Compute SHA-256 of a full CSM serialized as row-major big-endian bytes.
     * @param csm The Cross-Sum Matrix.
     * @return 32-byte SHA-256 digest.
     */
    auto BlockHash::compute(const Csm &csm) -> std::array<std::uint8_t, 32> {
        static constexpr std::uint16_t kS = 511;
        static constexpr std::size_t kRowBytes = 64; // 8 x uint64 = 64 bytes
        static constexpr std::size_t kTotalBytes = kS * kRowBytes; // 32,704

        std::vector<std::uint8_t> buf(kTotalBytes);
        for (std::uint16_t r = 0; r < kS; ++r) {
            const auto row = csm.getRow(r);
            for (int w = 0; w < 8; ++w) {
                const std::size_t off = (static_cast<std::size_t>(r) * kRowBytes) + (static_cast<std::size_t>(w) * 8);
                for (int b = 7; b >= 0; --b) {
                    buf[off + static_cast<std::size_t>(7 - b)] = // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        static_cast<std::uint8_t>(row.at(static_cast<std::size_t>(w)) >> (b * 8));
                }
            }
        }
        return detail::sha256::sha256_digest(buf.data(), buf.size());
    }
} // namespace crsce::common
