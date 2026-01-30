/**
 * @file BlockSolver.cpp
 * @brief High-level block solver implementation: reconstruct CSM from LH and cross-sum payloads.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Block/detail/solve_block.h"
#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Utils/detail/verify_cross_sums.h"
#include "decompress/Utils/detail/decode9.tcc"

#include <cstddef>
#include <cstdint> //NOLINT
#include <iostream>
#include <span>
#include <string>

namespace crsce::decompress {
    /**
     * @name solve_block
     * @brief Reconstruct a block CSM from LH and cross-sum payloads.
     * @param lh Span of 511 chained LH digests (32 bytes each).
     * @param sums Span of 4 serialized cross-sum vectors (2300 bytes total).
     * @param csm_out Output CSM to populate.
     * @param seed Seed string used by LH chain.
     * @return bool True on success; false if solving or verification fails.
     */
    bool solve_block(const std::span<const std::uint8_t> lh,
                     const std::span<const std::uint8_t> sums,
                     Csm &csm_out,
                     const std::string &seed) {
        constexpr std::size_t S = Csm::kS;
        constexpr std::size_t vec_bytes = 575U;

        const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
        const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
        const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
        const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

        ConstraintState st{};
        st.R_row = lsm;
        st.R_col = vsm;
        st.R_diag = dsm;
        st.R_xdiag = xsm;
        st.U_row.fill(S);
        st.U_col.fill(S);
        st.U_diag.fill(S);
        st.U_xdiag.fill(S);

        csm_out.reset();
        DeterministicElimination det{csm_out, st};
        GobpSolver gobp{csm_out, st, /*damping*/ 0.25, /*assign_confidence*/ 0.995};

        constexpr int kMaxIters = 200;
        for (int iter = 0; iter < kMaxIters; ++iter) {
            std::size_t progress = 0;
            progress += det.solve_step();
            progress += gobp.solve_step();
            if (det.solved() || gobp.solved()) {
                std::cerr << "Block Solver terminating with possible solution\n";
                break;
            }
            if (progress == 0) {
                std::cerr << "Block Solver failed with no progress\n";
                (void) gobp.solve_step();
                break;
            }
            std::cerr << "Block Solver iteration ended at " << iter << " of " << kMaxIters << " iterations"
                    << " (with " << progress << " progress)\n";
        }
        std::cerr << "Block Solver terminating...\n";
        if (!(det.solved() && gobp.solved())) {
            std::cerr << "Block Solver failed\n";
            return false;
        }
        if (!verify_cross_sums(csm_out, lsm, vsm, dsm, xsm)) {
            std::cerr << "Block Solver failed to verify cross sums\n";
            return false;
        }
        std::cerr << "Block Solver finished successfully (LH verification pending)\n";
        const LHChainVerifier verifier(seed);
        const bool result = verifier.verify_all(csm_out, lh);
        if (result) {
            std::cerr << "Block Solver finished successfully\n";
        } else {
            std::cerr << "Block Solver failed to verify cross sums\n";
        }
        return result;
    }
}
