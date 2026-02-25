//
// RectScorerObjC_test.cpp
// Minimal unit harness for Metal GPU scorer vs CPU reference.
// Gated behind CRSCE_BUILD_TESTS via CMake. Skips if GPU unavailable.
//

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

#include "decompress/Gpu/Metal/RectScorer.h"

using crsce::decompress::gpu::metal::RectCandidate;

static int32_t cpu_score_one(const std::vector<uint16_t> &dcnt,
                             const std::vector<uint16_t> &xcnt,
                             const std::vector<uint16_t> &dsm,
                             const std::vector<uint16_t> &xsm,
                             const RectCandidate &c) {
    auto score_family = [](const std::vector<uint16_t> &cnt,
                           const std::vector<uint16_t> &tgt,
                           const uint16_t idxs[4], const int8_t deltas[4]) -> int32_t {
        uint16_t set[4]{};
        int16_t acc[4]{};
        uint16_t n = 0;
        for (int i = 0; i < 4; ++i) {
            const uint16_t idx = idxs[i];
            bool found = false;
            for (uint16_t j = 0; j < n; ++j) {
                if (set[j] == idx) { acc[j] += deltas[i]; found = true; break; }
            }
            if (!found) { set[n] = idx; acc[n] = deltas[i]; ++n; }
        }
        int32_t before = 0, after = 0;
        for (uint16_t j = 0; j < n; ++j) {
            const int32_t cb = static_cast<int32_t>(cnt[set[j]]);
            const int32_t tb = static_cast<int32_t>(tgt[set[j]]);
            const int32_t ca = cb + static_cast<int32_t>(acc[j]);
            before += std::abs(cb - tb);
            after  += std::abs(ca - tb);
        }
        return after - before;
    };

    const uint16_t di[4] = { c.d[0], c.d[1], c.d[2], c.d[3] };
    const uint16_t xi[4] = { c.x[0], c.x[1], c.x[2], c.x[3] };
    const int8_t   dt[4] = { c.delta[0], c.delta[1], c.delta[2], c.delta[3] };

    return score_family(dcnt, dsm, di, dt) + score_family(xcnt, xsm, xi, dt);
}

TEST(GpuMetal, RectScorer_AvailabilityAndCorrectness) {
    // Small synthetic case
    const size_t S = 8;
    std::vector<uint16_t> dcnt{1,2,3,4,5,6,7,8};
    std::vector<uint16_t> xcnt{2,3,4,5,6,7,8,9};
    std::vector<uint16_t> dsm {1,2,2,4,6,5,7,8}; // slight differences
    std::vector<uint16_t> xsm {2,4,4,6,6,8,8,9};

    std::vector<RectCandidate> cands;
    cands.resize(3);
    // Candidate 0
    cands[0].d = {0,1,2,3};
    cands[0].x = {1,2,3,4};
    cands[0].delta = {+1,-1,+1,-1};
    // Candidate 1 (with duplicate indices in a family)
    cands[1].d = {2,2,5,5};
    cands[1].x = {3,3,6,6};
    cands[1].delta = {+1,+1,-1,-1};
    // Candidate 2
    cands[2].d = {7,6,5,4};
    cands[2].x = {4,5,6,7};
    cands[2].delta = {-1,-1,+1,+1};

    std::vector<int32_t> out;
    const bool has_gpu = crsce::decompress::gpu::metal::score_rectangles(dcnt, xcnt, dsm, xsm, cands, out);
    if (!has_gpu) {
        GTEST_SKIP() << "Metal GPU not available on this host/preset";
    }

    ASSERT_EQ(out.size(), cands.size());

    // Compare vs CPU reference
    for (size_t i = 0; i < cands.size(); ++i) {
        const int32_t ref = cpu_score_one(dcnt, xcnt, dsm, xsm, cands[i]);
        EXPECT_EQ(out[i], ref) << "Mismatch at i=" << i;
    }
}

