/**
 * @file RectScorer.h
 * @brief C++ interface to score rectangle candidates via Apple Metal (if available).
 *        Returns false if GPU path is unavailable or failed; caller may fall back to CPU.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "decompress/Gpu/Metal/Types.h"

namespace crsce::decompress::gpu::metal {

    /**
     * @brief Score rectangle candidates in parallel on GPU.
     * @param dcnt Current per-diagonal ones counts (size S).
     * @param xcnt Current per-anti-diagonal ones counts (size S).
     * @param dsm Target per-diagonal sums (size S).
     * @param xsm Target per-anti-diagonal sums (size S).
     * @param cands Rectangle candidates (size N), each with 4 indices and deltas.
     * @param out_dphi Output per-candidate ΔΦ (L1 error change over DSM/XSM); negative is improvement.
     * @return bool True on success; false if unavailable or failure.
     */
    bool score_rectangles(const std::vector<std::uint16_t> &dcnt,
                          const std::vector<std::uint16_t> &xcnt,
                          const std::vector<std::uint16_t> &vcnt,
                          const std::vector<std::uint16_t> &dsm,
                          const std::vector<std::uint16_t> &xsm,
                          const std::vector<std::uint16_t> &vsm,
                          const std::vector<RectCandidate> &cands,
                          std::vector<std::int32_t> &out_dphi);
}
