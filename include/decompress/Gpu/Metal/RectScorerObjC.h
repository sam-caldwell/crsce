/**
 * @file RectScorerObjC.h
 * @brief C ABI for Objective-C++ Metal rectangle scorer shim.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RectCandidateC {
    std::uint16_t d[4];
    std::uint16_t x[4];
    std::int8_t   t[4];
    std::int8_t   pad; // explicit padding for alignment
} RectCandidateC;

/**
 * Score rectangle candidates on GPU using Apple Metal.
 * Returns true on success; false if unavailable or on failure.
 */
bool crsce_metal_score_rects(const std::uint16_t *dcnt,
                             const std::uint16_t *xcnt,
                             const std::uint16_t *vcnt,
                             const std::uint16_t *dsm,
                             const std::uint16_t *xsm,
                             const std::uint16_t *vsm,
                             const RectCandidateC *cands,
                             std::size_t S,
                             std::size_t N,
                             std::int32_t *out_dphi);

#ifdef __cplusplus
} // extern "C"
#endif
