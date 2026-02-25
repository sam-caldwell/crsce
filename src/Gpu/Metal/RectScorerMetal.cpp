/**
 * @file RectScorerMetal.cpp
 * @brief Apple Metal interop to score 2x2 rectangle candidates in parallel.
 *        Compiled and enabled only when CRSCE_HAS_METAL is defined and metal-cpp is available.
 */

#include <cstddef>
#include <cstdint>
#include <vector>
// POD types used by the GPU path
#include "decompress/Gpu/Metal/Types.h"
#if defined(__APPLE__) && defined(CRSCE_HAS_METAL)
#  include <mutex>
#  include "common/O11y/O11y.h"
#  include "decompress/Gpu/Metal/RectScorerObjC.h"
#endif

// NOLINTBEGIN(misc-include-cleaner,readability-braces-around-statements,misc-const-correctness,modernize-use-designated-initializers,misc-use-internal-linkage)
namespace crsce::decompress::gpu::metal {

    namespace {
    bool score_rectangles_impl(const std::vector<std::uint16_t> &dcnt,
                               const std::vector<std::uint16_t> &xcnt,
                               const std::vector<std::uint16_t> &vcnt,
                               const std::vector<std::uint16_t> &dsm,
                               const std::vector<std::uint16_t> &xsm,
                               const std::vector<std::uint16_t> &vsm,
                               const std::vector<RectCandidate> &cands,
                               std::vector<std::int32_t> &out_dphi) {
#if defined(__APPLE__) && defined(CRSCE_HAS_METAL)
        if (cands.empty()) return false;
        const std::size_t S = dcnt.size();
        if (S == 0 || xcnt.size() != S || vcnt.size() != S || dsm.size() != S || xsm.size() != S || vsm.size() != S) return false;
        const std::size_t N = cands.size();
        out_dphi.resize(N);

        // Bridge to C ABI expected by the ObjC++ shim
        std::vector<RectCandidateC> packed;
        packed.resize(N);
        for (std::size_t i = 0; i < N; ++i) {
            packed[i].d[0] = cands[i].d[0];
            packed[i].d[1] = cands[i].d[1];
            packed[i].d[2] = cands[i].d[2];
            packed[i].d[3] = cands[i].d[3];
            packed[i].x[0] = cands[i].x[0];
            packed[i].x[1] = cands[i].x[1];
            packed[i].x[2] = cands[i].x[2];
            packed[i].x[3] = cands[i].x[3];
            packed[i].t[0] = cands[i].delta[0];
            packed[i].t[1] = cands[i].delta[1];
            packed[i].t[2] = cands[i].delta[2];
            packed[i].t[3] = cands[i].delta[3];
            packed[i].pad = 0;
        }

        const bool ok = crsce_metal_score_rects(dcnt.data(), xcnt.data(), vcnt.data(), dsm.data(), xsm.data(), vsm.data(), packed.data(), S, N, out_dphi.data());
        static std::once_flag once;
        if (ok) {
            std::call_once(once, [](){
                ::crsce::o11y::O11y::instance().metric("gpu_metal_available", 1);
            });
        }
        return ok;
#else
        (void)dcnt; (void)xcnt; (void)vcnt; (void)dsm; (void)xsm; (void)vsm; (void)cands; (void)out_dphi;
        return false;
#endif
    }
    } // anonymous namespace

    bool score_rectangles(const std::vector<std::uint16_t> &dcnt, // NOLINT(misc-use-internal-linkage)
                          const std::vector<std::uint16_t> &xcnt,
                          const std::vector<std::uint16_t> &vcnt,
                          const std::vector<std::uint16_t> &dsm,
                          const std::vector<std::uint16_t> &xsm,
                          const std::vector<std::uint16_t> &vsm,
                          const std::vector<RectCandidate> &cands,
                          std::vector<std::int32_t> &out_dphi) {
        return score_rectangles_impl(dcnt, xcnt, vcnt, dsm, xsm, vsm, cands, out_dphi);
    }
}
// NOLINTEND(misc-include-cleaner,readability-braces-around-statements,misc-const-correctness,modernize-use-designated-initializers,misc-use-internal-linkage)
