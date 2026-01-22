/**
 * @file DeterministicElimination.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Csm/Csm.h"

#include <cstddef>
#include <stdexcept>

namespace crsce::decompress {
    /**
     * @brief Implementation detail.
     */
    DeterministicElimination::DeterministicElimination(Csm &csm, ConstraintState &state)
        : csm_(csm), st_(state) {
        validate_bounds(st_);
    }

    void DeterministicElimination::validate_bounds(const ConstraintState &st) {
        for (std::size_t i = 0; i < S; ++i) {
            if (st.U_row.at(i) > S || st.U_col.at(i) > S || st.U_diag.at(i) > S || st.U_xdiag.at(i) > S) {
                throw std::invalid_argument("U out of range");
            }
            if (st.R_row.at(i) > st.U_row.at(i) || st.R_col.at(i) > st.U_col.at(i) ||
                st.R_diag.at(i) > st.U_diag.at(i) || st.R_xdiag.at(i) > st.U_xdiag.at(i)) {
                throw std::invalid_argument("R > U invariant violated");
            }
        }
    }

    void DeterministicElimination::apply_cell(const std::size_t r, const std::size_t c, const bool value) {
        const auto d = diag_index(r, c);
        const auto x = xdiag_index(r, c);

        if (csm_.is_locked(r, c)) {
            // Already assigned; forced moves apply only to remaining unassigned vars on the line.
            return;
        }
        // Update residuals first to ensure invariants, then write/lock
        if (st_.U_row.at(r) == 0 || st_.U_col.at(c) == 0 || st_.U_diag.at(d) == 0 || st_.U_xdiag.at(x) == 0) {
            throw std::runtime_error("U already zero on a line while assigning");
        }
        --st_.U_row.at(r);
        --st_.U_col.at(c);
        --st_.U_diag.at(d);
        --st_.U_xdiag.at(x);
        if (value) {
            if (st_.R_row.at(r) == 0) {
                throw std::runtime_error("R_row underflow");
            }
            if (st_.R_col.at(c) == 0) {
                throw std::runtime_error("R_col underflow");
            }
            if (st_.R_diag.at(d) == 0) {
                throw std::runtime_error("R_diag underflow");
            }
            if (st_.R_xdiag.at(x) == 0) {
                throw std::runtime_error("R_xdiag underflow");
            }
            --st_.R_row.at(r);
            --st_.R_col.at(c);
            --st_.R_diag.at(d);
            --st_.R_xdiag.at(x);
        }
        csm_.put(r, c, value);
        csm_.lock(r, c);
    }

    void DeterministicElimination::force_row(const std::size_t r, const bool value, std::size_t &progress) {
        for (std::size_t c = 0; c < S; ++c) {
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }

    void DeterministicElimination::force_col(const std::size_t c, const bool value, std::size_t &progress) {
        for (std::size_t r = 0; r < S; ++r) {
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }

    void DeterministicElimination::force_diag(const std::size_t d, const bool value, std::size_t &progress) {
        for (std::size_t r = 0; r < S; ++r) {
            const std::size_t c = (d + S - (r % S)) % S;
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }

    void DeterministicElimination::force_xdiag(const std::size_t x, const bool value, std::size_t &progress) {
        for (std::size_t r = 0; r < S; ++r) {
            const std::size_t c = (r + S - (x % S)) % S;
            const auto before_locked = csm_.is_locked(r, c);
            apply_cell(r, c, value);
            if (!before_locked) {
                ++progress;
            }
        }
    }

    std::size_t DeterministicElimination::solve_step() {
        validate_bounds(st_);
        std::size_t progress = 0;
        // Rows
        for (std::size_t r = 0; r < S; ++r) {
            const auto U = st_.U_row.at(r);
            const auto R = st_.R_row.at(r);
            if (U == 0) {
                if (R != 0) {
                    throw std::runtime_error("row contradiction: U=0 but R>0");
                }
                continue;
            }
            if (R == 0) {
                force_row(r, false, progress);
            } else if (R == U) {
                force_row(r, true, progress);
            }
        }
        // Column/diag/xdiag passes are deferred to subsequent iterations to avoid double-counting within one step.
        return progress;
    }

    bool DeterministicElimination::solved() const {
        for (std::size_t i = 0; i < S; ++i) {
            if (st_.U_row.at(i) != 0 || st_.U_col.at(i) != 0 || st_.U_diag.at(i) != 0 || st_.U_xdiag.at(i) != 0) {
                return false;
            }
        }
        return true;
    }

    void DeterministicElimination::reset() {
        // No internal state to reset currently
    }

    std::size_t DeterministicElimination::hash_step() {
        // Placeholder for LH/known-rows elimination integration; currently a no-op
        return 0U;
    }
} // namespace crsce::decompress
