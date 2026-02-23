/**
 * @file Decompressor_solve_block.cpp
 * @author Sam Caldwell
 * @brief Decompressor::solve_block implementation: constructs a per‑block solver
 *        via factory and delegates all phases to it; handles snapshot setup
 *        and final verification only.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Decompressor/Decompressor.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <algorithm>

#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Block/detail/SnapshotGuard.h"
#include "decompress/Block/detail/prelock_padded_tail.h"
#include "decompress/Block/detail/verify_cross_sums_and_lh.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Solvers/SelectedSolver.h"

namespace crsce::decompress {

    /**
     * @name Decompressor::solve_block
     * @brief Solve a single block by delegating all phases to the primary solver.
     * @param lh Little-header payload for the block.
     * @param sums Cross-sum constraints for the block.
     * @param csm_out Output constraint/state matrix to populate.
     * @param valid_bits Number of valid bits in the final row padding.
     * @return true if cross-sums and LH verification succeed.
     */
    bool Decompressor::solve_block(std::span<const std::uint8_t> lh,
                                   const CrossSums &sums,
                                   Csm &csm_out,
                                   const std::uint64_t valid_bits) const {
    constexpr std::size_t S = Csm::kS;

    const auto &lsm = sums.lsm().targets();
    const auto &vsm = sums.vsm().targets();
    const auto &dsm = sums.dsm().targets();
    const auto &xsm = sums.xsm().targets();

    ConstraintState st(S, lsm, vsm, dsm, xsm);
    BlockSolveSnapshot snap{S,
                            st,
                            std::span<const std::uint16_t>(lsm.begin(), lsm.end()),
                            std::span<const std::uint16_t>(vsm.begin(), vsm.end()),
                            std::span<const std::uint16_t>(dsm.begin(), dsm.end()),
                            std::span<const std::uint16_t>(xsm.begin(), xsm.end()),
                            0ULL};
    const ::crsce::decompress::detail::SnapshotGuard pub(snap);

    ::crsce::decompress::detail::prelock_padded_tail(csm_out, st, valid_bits);
    // Edge-case: if there is no padding and all provided cross-sums are zero,
    // avoid an aggressive full lock from the pipeline. This preserves the
    // contract that solve_block does not introduce spurious locks when there
    // is no padded tail.
    const bool no_padding = (valid_bits >= (static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S)));
    const bool sums_all_zero = std::ranges::all_of(lsm, [](std::uint16_t v){ return v == 0; }) &&
                               std::ranges::all_of(vsm, [](std::uint16_t v){ return v == 0; }) &&
                               std::ranges::all_of(dsm, [](std::uint16_t v){ return v == 0; }) &&
                               std::ranges::all_of(xsm, [](std::uint16_t v){ return v == 0; });
    if (no_padding && sums_all_zero) {
        return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
    }
    // Construct a fresh primary solver per block using the central
    // SelectedSolver factory. This binds the solver to this block's Csm and
    // ConstraintState and captures the block's LH payload and cross-sum
    // targets; construction is deferred to ensure per-block state.
    if (const auto solver = ::crsce::decompress::solvers::selected::make_primary_solver(csm_out, st, sums, lh)) {
        solver->solve();
    }
    return ::crsce::decompress::detail::verify_cross_sums_and_lh(csm_out, sums, lh, snap);
}

} // namespace crsce::decompress
