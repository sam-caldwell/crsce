/**
 * @file gobp_simulate_lh_after.h
 * @brief Helper to simulate a 2x2 swap and count verified LH rows, then revert.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <cstddef>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"

namespace crsce::decompress::detail {
    /**
     * @name gobp_simulate_lh_after
     * @brief Temporarily apply the swap, count LH-verified rows, then restore original bits.
     * @param csm Cross-Sum Matrix (modified then restored).
     * @param lh Row hash payload.
     * @param r0 First row index.
     * @param r1 Second row index.
     * @param c0 First column index.
     * @param c1 Second column index.
     * @param b00 Original bit at (r0,c0).
     * @param b11 Original bit at (r1,c1).
     * @param b01 Original bit at (r0,c1).
     * @param b10 Original bit at (r1,c0).
     * @param main True if 1s are on main diagonal of the 2x2; false if on off-diagonal.
     * @param ver RowHashVerifier instance to use.
     * @return int Number of rows (among r0,r1) that verify after the simulated swap.
     */
    inline int gobp_simulate_lh_after(Csm &csm,
                                      std::span<const std::uint8_t> lh,
                                      std::size_t r0, std::size_t r1,
                                      std::size_t c0, std::size_t c1,
                                      bool b00, bool b11, bool b01, bool b10,
                                      bool main,
                                      const RowHashVerifier &ver) {
        if (main) {
            csm.clear(r0, c0); csm.clear(r1, c1);
            csm.set(r0, c1);   csm.set(r1, c0);
        } else {
            csm.clear(r0, c1); csm.clear(r1, c0);
            csm.set(r0, c0);   csm.set(r1, c1);
        }
        int lh_after = 0;
        if (ver.verify_row(csm, lh, r0)) { ++lh_after; }
        if (ver.verify_row(csm, lh, r1)) { ++lh_after; }
        // revert to original
        if (b00) { csm.set(r0, c0); } else { csm.clear(r0, c0); }
        if (b11) { csm.set(r1, c1); } else { csm.clear(r1, c1); }
        if (b01) { csm.set(r0, c1); } else { csm.clear(r0, c1); }
        if (b10) { csm.set(r1, c0); } else { csm.clear(r1, c0); }
        return lh_after;
    }
}

