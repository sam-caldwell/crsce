/**
 * @file compute_prefix.cpp
 * @brief Incrementally compute verified LH prefix for current CSM/state.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/compute_prefix.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

#include "decompress/RowHashVerifier/RowHashVerifier.h"

namespace crsce::decompress::detail {

/**
 * @name compute_prefix
 * @brief Extend the verified LH prefix if leading rows remain fully determined and valid.
 * @param pc_ver_seen Per-row version numbers cached when last verified.
 * @param pc_ok Per-row verification flags (nonzero if row verified).
 * @param pc_prefix_len In/out current prefix length [0..pc_prefix_len).
 * @param csm Current CSM.
 * @param st Current constraint state.
 * @param lh LH digest span.
 * @param snap In/out snapshot for accumulating LH verification time.
 * @return New prefix length (same as updated pc_prefix_len).
 */
std::size_t compute_prefix(std::vector<std::uint64_t> &pc_ver_seen, // NOLINT(misc-use-internal-linkage)
                           std::vector<char> &pc_ok,
                           std::size_t &pc_prefix_len,
                           Csm &csm,
                           ConstraintState &st,
                           std::span<const std::uint8_t> lh,
                           BlockSolveSnapshot &snap) {
    constexpr std::size_t S = Csm::kS;
    if (pc_ver_seen.size() != S) {
        pc_ver_seen.assign(S, 0ULL);
        pc_ok.assign(S, 0);
        pc_prefix_len = 0;
    }

    std::size_t k = 0;
    while (k < pc_prefix_len) {
        if (pc_ok[k] && pc_ver_seen[k] == csm.row_version(k)) {
            ++k;
        } else {
            pc_prefix_len = k;
            break;
        }
    }
    for (std::size_t r = pc_prefix_len; r < S; ++r) {
        if (st.U_row.at(r) != 0 || st.R_row.at(r) != 0) {
            break;
        }
        const RowHashVerifier verifier{};
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = verifier.verify_row(csm, lh, r);
        const auto t1 = std::chrono::steady_clock::now();
        snap.time_lh_ms += static_cast<std::size_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
        );
        if (!ok) { break; }
        pc_ver_seen[r] = csm.row_version(r);
        pc_ok[r] = 1;
        pc_prefix_len = r + 1;
    }
    return pc_prefix_len;
}

} // namespace crsce::decompress::detail
