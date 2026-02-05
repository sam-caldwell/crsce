/**
 * @file pre_polish_boundary_commit.h
 * @brief Helper to attempt committing the boundary row before polish stage.
 */
#pragma once

#include <span>
#include <string>

#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    // Lock in any contiguous valid prefix (rows 0..k-1) verified by LH; updates baseline and snapshot.
    // Returns true if any rows were newly committed.
    bool commit_valid_prefix(Csm &csm_out,
                             ConstraintState &st,
                             std::span<const std::uint8_t> lh,
                             Csm &baseline_csm,
                             ConstraintState &baseline_st,
                             BlockSolveSnapshot &snap,
                             int rs);

    bool pre_polish_boundary_commit(Csm &csm_out,
                                    ConstraintState &st,
                                    std::span<const std::uint8_t> lh,
                                    const std::string &seed,
                                    Csm &baseline_csm,
                                    ConstraintState &baseline_st,
                                    BlockSolveSnapshot &snap,
                                    int rs);

    // Adaptive boundary finisher: focus on the current prefix boundary row with budgets that scale with stall_ticks.
    bool finish_boundary_row_adaptive(Csm &csm_out,
                                      ConstraintState &st,
                                      std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs,
                                      int stall_ticks);

    // Scan all rows; for any row whose digest matches, lock the row and update baseline/snapshot.
    // Returns true if any row was committed.
    bool commit_any_verified_rows(Csm &csm_out,
                                  ConstraintState &st,
                                  std::span<const std::uint8_t> lh,
                                  Csm &baseline_csm,
                                  ConstraintState &baseline_st,
                                  BlockSolveSnapshot &snap,
                                  int rs);

    // Pick a near-complete row (fewest unknowns > 0) and try local flips + DE bursts to make it match digest.
    // On success, lock the row and update baseline/snapshot. Returns true if any row was committed.
    bool finish_near_complete_any_row(Csm &csm_out,
                                      ConstraintState &st,
                                      std::span<const std::uint8_t> lh,
                                      Csm &baseline_csm,
                                      ConstraintState &baseline_st,
                                      BlockSolveSnapshot &snap,
                                      int rs);

    // Process top-N near-complete rows (fewest unknowns) and for each, sample up to K most ambiguous cells.
    // Commits first row that matches digest after local flips; returns true if any row committed.
    bool finish_near_complete_top_rows(Csm &csm_out,
                                       ConstraintState &st,
                                       std::span<const std::uint8_t> lh,
                                       Csm &baseline_csm,
                                       ConstraintState &baseline_st,
                                       BlockSolveSnapshot &snap,
                                       int rs,
                                       std::size_t top_n,
                                       std::size_t top_k_cells);

    // Scored-row finisher: try a provided set of rows (ordered) with near-complete U; returns true on any verify/commit.
    bool finish_near_complete_rows_scored(Csm &csm_out,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs,
                                          const std::vector<std::size_t> &rows,
                                          std::size_t top_k_cells);

    // Scored-column finisher: try a provided set of columns (ordered) with near-complete U.
    bool finish_near_complete_columns_scored(Csm &csm_out,
                                             ConstraintState &st,
                                             std::span<const std::uint8_t> lh,
                                             Csm &baseline_csm,
                                             ConstraintState &baseline_st,
                                             BlockSolveSnapshot &snap,
                                             int rs,
                                             const std::vector<std::size_t> &cols,
                                             std::size_t top_k_cells);

    // Column-first finisher: process top-N near-complete columns (fewest unknowns) and for each column,
    // sample up to K most ambiguous cells (by |p-0.5|) across rows. For any improvement that yields a
    // row whose digest matches, commit that row (lock all cells) and update baseline/snapshot.
    // Returns true if any row was committed.
    bool finish_near_complete_top_columns(Csm &csm_out,
                                          ConstraintState &st,
                                          std::span<const std::uint8_t> lh,
                                          Csm &baseline_csm,
                                          ConstraintState &baseline_st,
                                          BlockSolveSnapshot &snap,
                                          int rs,
                                          std::size_t top_n_cols,
                                          std::size_t top_k_cells);

    // Scan for rows fully locked (U_row[r]==0) whose digest does not match; revert to baseline and record restart.
    // Returns true if a contradiction was found and a restart performed.
    bool audit_and_restart_on_contradiction(Csm &csm_out,
                                            ConstraintState &st,
                                            std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs,
                                            std::uint64_t valid_bits,
                                            int cooldown_ticks,
                                            int &since_last_restart);
}
