/**
 * @file prelock_padded_tail.cpp
 * @brief Pre-lock the padded tail cells of the final block to zeros and update unknown counts.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Block/detail/prelock_padded_tail.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Utils/detail/calc_d.h"
#include "decompress/Utils/detail/calc_x.h"
#include "common/O11y/metric_i64.h"

namespace crsce::decompress::detail {
    /**
     * @name prelock_padded_tail
     * @brief For blocks whose bit budget is not a full S×S, lock the trailing cells to 0.
     * @param csm Cross‑Sum Matrix to modify (bits/locks).
     * @param st Constraint state (R/U) to decrement for each locked cell.
     * @param valid_bits Number of meaningful bits in this block; [valid_bits, S×S) are padded.
     * @return void
     */
    void prelock_padded_tail(Csm &csm, ConstraintState &st, const std::uint64_t valid_bits) {
        constexpr std::size_t S = Csm::kS;
        const std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);
        if (valid_bits >= total_bits) { return; }

        static int prelock_dbg = -1; // -1 unset, 0 off, 1 on
        if (prelock_dbg < 0) {
            const char *e = std::getenv("CRSCE_PRELOCK_DEBUG"); // NOLINT(concurrency-mt-unsafe)
            prelock_dbg = (e && *e) ? 1 : 0;
        }
        if (prelock_dbg == 1) {
            const auto start_r = static_cast<std::size_t>(valid_bits / S);
            const auto start_c = static_cast<std::size_t>(valid_bits % S);
            ::crsce::o11y::metric(
                "prelock_padded_tail",
                static_cast<std::int64_t>(valid_bits),
                {
                    {"r", std::to_string(start_r)},
                    {"c", std::to_string(start_c)},
                    {"count", std::to_string(total_bits - valid_bits)}
                }
            );
        }

        for (std::uint64_t idx = valid_bits; idx < total_bits; ++idx) {
            const auto r = static_cast<std::size_t>(idx / S);
            const auto c = static_cast<std::size_t>(idx % S);
            csm.clear(r, c);
            csm.lock(r, c);
            if (st.U_row.at(r) > 0) { --st.U_row.at(r); }
            if (st.U_col.at(c) > 0) { --st.U_col.at(c); }
            const std::size_t d = ::crsce::decompress::detail::calc_d(r, c);
            const std::size_t x = ::crsce::decompress::detail::calc_x(r, c);
            if (st.U_diag.at(d) > 0) { --st.U_diag.at(d); }
            if (st.U_xdiag.at(x) > 0) { --st.U_xdiag.at(x); }
        }
    }
}
