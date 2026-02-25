/**
 * @file Types.h
 * @brief POD types shared by Metal rectangle scorer interface.
 *        Keep this header C++-only; Metal kernels duplicate compatible structs.
 */
#pragma once

#include <cstdint>
#include <array>

namespace crsce::decompress::gpu::metal {
    /**
     * @brief Packed rectangle candidate describing a 2x2 swap impact.
     * - d[4]: diagonal indices for the four cells (r0,c0),(r0,c1),(r1,c0),(r1,c1)
     * - x[4]: anti-diagonal indices for the four cells, same order as above
     * - delta[4]: per-cell delta (+1 for 0→1, -1 for 1→0) for proposed swap
     */
    struct RectCandidate {
        std::array<std::uint16_t, 4> d{};
        std::array<std::uint16_t, 4> x{};
        std::array<std::int8_t,   4> delta{};
        std::int8_t   _pad{0}; // explicit padding to 16-byte alignment (best-effort)
    };
}
