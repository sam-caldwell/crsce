/**
 * @file compute_prefix.cpp
 * @brief Incrementally compute verified LH prefix for current CSM/state.
 */
#include "decompress/Block/detail/compute_prefix.h"

#include <chrono>

#include "decompress/RowHashVerifier/RowHashVerifier.h"

namespace crsce::decompress::detail {

std::size_t compute_prefix(std::vector<std::uint64_t> &pc_ver_seen,
                           std::vector<char> &pc_ok,
                           std::size_t &pc_prefix_len,
                           const Csm &csm,
                           const ConstraintState &st,
                           const std::span<const std::uint8_t> lh,
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

