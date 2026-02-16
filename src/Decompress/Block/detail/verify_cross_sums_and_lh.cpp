/**
 * @file verify_cross_sums_and_lh.cpp
 * @brief Verify cross-sum families and then all row LH digests; update snapshot timings/phase.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/verify_cross_sums_and_lh.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include <chrono>
#include "decompress/Block/detail/set_block_solve_snapshot.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "common/O11y/event.h"
#include "common/O11y/metric_i64.h"

namespace crsce::decompress::detail {
    /**
     * @name verify_cross_sums_and_lh
     * @brief Run cross-sum checks and row-hash verification; emit timing to `snap`.
     * @param csm Cross‑Sum Matrix to verify.
     * @param lsm Row targets.
     * @param vsm Column targets.
     * @param dsm Diagonal targets.
     * @param xsm Anti‑diagonal targets.
     * @param lh LH bytes for all rows.
     * @param snap Snapshot to record timings/status.
     * @return bool True if both cross-sums and LH verify; false otherwise.
     */
    bool verify_cross_sums_and_lh(Csm &csm,
                                  const CrossSums &sums,
                                  const std::span<const std::uint8_t> lh,
                                  BlockSolveSnapshot &snap) {
        const auto t0cs = std::chrono::steady_clock::now();
        const bool okcs = verify_cross_sums(csm, sums);
        const auto t1cs = std::chrono::steady_clock::now();
        snap.time_cross_verify_ms += static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(t1cs - t0cs).count());
        if (!okcs) {
            ::crsce::o11y::event("block_end_verify_fail");
            ::crsce::o11y::metric("block_end_verify_fail", 1LL);
            snap.phase = BlockSolveSnapshot::Phase::verify;
            snap.message = "cross-sum verification failed";
            // Emit aggregate cross-sum metrics for failure diagnostics
            sums.emit_metrics("crosssum", csm, nullptr);
            set_block_solve_snapshot(snap);
            return false;
        }
        const RowHashVerifier verifier;
        const auto t0va = std::chrono::steady_clock::now();
        const bool result = verifier.verify_all(csm, lh);
        const auto t1va = std::chrono::steady_clock::now();
        const auto ms = static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(t1va - t0va).count());
        snap.time_verify_all_ms += ms;
        snap.time_lh_ms += ms;
        // Emit aggregate cross-sum metrics on success as well for observability
        sums.emit_metrics("crosssum", csm, nullptr);
        ::crsce::o11y::event(result ? "block_end_ok" : "block_end_verify_fail");
        return result;
    }
}
