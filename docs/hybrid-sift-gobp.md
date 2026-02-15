# Hybrid Radditz‑Sift with GOBP Guidance

## Overview

This proposal unifies `radditz-sift-{vsm,dsm,xsm}` into a single hybrid pass that enforces `VSM`, `DSM`, and `XSM`
simultaneously while using GOBP beliefs to guide search. `Bitsplash` continues to bound each row to a fixed-width
candidate set; the hybrid sift performs one streaming traversal, forward-checks all three cross-sum residuals at every
step, and orders variables/values using calibrated belief scores. The goals are to reduce wrong-path exploration induced
by `VSM`-only branching, lower I/O and handoff overhead, and cut end-to-end latency without sacrificing completeness.
Beliefs are used for ordering and bounding, not for unsound pruning.

## Goals (What I need to accomplish here)

- Refine `BitSplash` to distribute `LSM[r]` bits according to GOBP beliefs.
- Ensure `BitSplash` results in LSM satisfaction before `Radditz Sift`
- Create `Cell` class to represent a bit-cell position `(r,c)` (used in swaps) from LSM/VSM view
- Create `xCell` class to represent a bit-cell position `(d,x)` (used in swaps) from DSM/XSM view.
- Extend `Csm` class to have `swap(Cell A, Cell B)` to facilitate 2x2 swaps in `(r,c)` coordinate system.
- Extend `Csm` class to have `swap_x(xCell A, xCellB)` to facilitate 2x2 swaps in `(d,x)` coordinate system.
- Single-pass satisfaction:
    - Enforce `VSM`, `DSM`, and `XSM` together to avoid early `VSM`-only mistakes.
    - Use GOBP-influence to help guide `VSM`, `DSM`, `XSM` satisfaction.
- Performance:
    - Reduce explored states via stronger forward-checking and belief-guided ordering.
    - Ensure decision history is tracked to avoid making the same mistakes.
        - Apply Depth-first default with optional limited discrepancy or small-width beam at top `N` levels, where `N`
          is
          adaptive.
    - When backtracking eliminates a given decision, it should prune any records of its child decisions from history to
      free memory.
- Modularity: Keep per-model outputs as isolated sinks for diagnostics and targeted runs.
- Safety: Preserve soundness/completeness under default settings; support time-bounded variants.
- Observability: Add clear metrics, traces, and toggles for gradual rollout and easy rollback.

## Out of scope

- Leave of `Bitsplash` unchanged.
- Algorithmic changes to `VSM`/`DSM`/`XSM` target definitions.
- Hard dependency on beliefs for correctness (they must remain advisory).

## Background

Today, `Radditz Sift` runs in three phases—`VSM`, `DSM`, `XSM`—creating orchestration and I/O overhead and, more
importantly, committing to `VSM`-consistent branches that can unintentionally eliminate the correct solution.
`Bitsplash` already trims the per-row search space by constraining rows to a fixed number of bits. But `BitSplash` only
eliminates incorrect decisions. By contrast `Radditz Sift` (like its namesake) makes almost certainly bad decision and
stands its ground that leads us in the wrong direction. This is not detectable until considerably later after `DSM` and
`XSM` have also allowed `Radditz Sift` to make additional decisions. We need to mitigate this risk by making the
`Radditz Sift` decisions more informed. The current `Radditz Sift` makes each decision based on a single dimension
(`VSM`, `DSM`, `XSM`) in separate phases. The hybrid approach makes the decisions across all three cross-sum dimensions
at the same time with advisory input from GOBP beliefs. This approach will enumerate each row’s candidates once,
maintain residual sums across vertical and both diagonal families, and prune as soon as any residual becomes infeasible.
GOBP provides per-cell (or aggregate) beliefs that can rank both which row to place next and which candidate to try
first, turning aimless backtracking into a guided search while remaining exhaustive.

## Proposed Design

At a high level, Hybrid Sift performs a single depth-first (or iterative-deepening/beam-augmented) traversal across row
positions. For each step:

1. Candidate enumeration: `Bitsplash` yields a deduplicated, fixed-size set of row candidates for the current row index
   k.
2. Residual bookkeeping: Maintain `VSM` residuals per column and `DSM`/`XSM` residuals per diagonal. After tentatively
   placing a candidate, update residuals incrementally.
3. Forward-checking and feasibility: Compute tight lower/upper bounds for each residual using the remaining rows’
   min/max attainable contributions; if any target becomes unattainable, prune immediately.
4. Ordering via beliefs: Choose the next row by a combined measure of residual tightness and belief entropy; order that
   row’s candidates by feasibility slack and belief log-score.
5. Sinks and side-effects: When a full assignment is found, emit to `VSM`/`DSM`/`XSM` sinks. During search, keep sinks
   read-only to preserve isolation.

## CSP Formulation and Bounds

- Variables: Row assignments `r0..r(R-1)`, each a bit-vector from `Bitsplash`’s candidate set.
- Constraints: Linear cross-sum targets over three families:
    - VSM: sum over rows at fixed column `c` equals `targetC[c]`.
    - DSM/XSM: sum over rows at diagonals `d1`, `d2` equals `targetD1[d1]`, `targetD2[d2]`.
- Forward-checking: For each residual bucket `b` in {columns, diag1, diag2}, track remaining sum `Rem[b]`. For
  unassigned rows `U`, precompute per-row min/max contributions to `b`: `minU[b]`, `maxU[b]`. If `Rem[b] < sum(minU[b])`
  or `Rem[b] > sum(maxU[b])`, prune.
- Tightening: Maintain rolling counts of remaining ones per row and per family to tighten min/max. Optionally precompute
  row→bucket incidence to update bounds in `O(k)` instead of `O(R·C)`.

## Belief Integration (GOBP)

- Variable ordering: Choose the next row with (a) smallest feasible candidate count, (b) tightest combined residual
  slack, and (c) highest belief entropy (hardest/most informative first).
- Value ordering: Score each candidate as `S = α·(sum log p(cell=1) over its ones) − β·penalty(slack_risk)`, with
  temperature `τ` to control confidence. Sort descending by `S`.
- Sound bounds from beliefs: Where possible, derive an optimistic upper bound on attainable residuals using beliefs (
  e.g., top-k belief mass in remaining rows for each bucket). Only use such bounds to prune if they are true upper
  bounds; otherwise keep beliefs advisory.
- Robustness: Apply temperature scaling, damping, and a small diversity budget (e.g., limited discrepancy search) to
  avoid over-commitment when beliefs are loopy or miscalibrated. Beliefs inform order, not feasibility, unless the bound
  is provably safe.

## Data Structures

- ResidualState: Arrays for `col_rem[C]`, `d1_rem[D1]`, `d2_rem[D2]`; plus fast incidence maps `row → {columns, d1, d2}`
  touched.
- Candidate: Compact bitset or packed indices for ones; cached per-bucket contribution deltas.
- BeliefGrid: Per-cell probabilities and per-row aggregates; expose row entropy and candidate log-scores.
- Caches: Transposition table keyed by a Zobrist hash of (row_index frontier + residual arrays reduced/modded) to
  memoize dead subspaces; LRU-limited to cap memory.
- Ordering queues: A small, mutable frontier structure to pick the next row using the combined heuristic.

## Algorithmic Flow

- Initialize residuals from targets; precompute candidate sets, per-row min/max to each bucket, and belief summaries.
- While exploring:
    - Select next row with heuristic (MCV + entropy).
    - Generate its candidates (already cached), filter by quick feasibility (no immediate residual
      negatives/overshoots), and sort by belief-guided score.
    - For each candidate:
        - Apply deltas to `ResidualState`.
        - Run forward-checking using min/max bounds; if infeasible, revert and continue.
        - If assignment complete, emit to sinks; else recurse.
        - Revert deltas on backtrack.
- Optionally enable beam-`k` at the top L levels for parallelism; ensure a fallback exhaustive sweep for completeness
  when not time-bounded.

## Correctness and Completeness

- Soundness: All pruning comes from exact residual updates and provable min/max bounds; beliefs never force acceptance
  of a wrong candidate.
- Completeness: Depth-first with only sound pruning is exhaustive. If enabling beam or time-limits, gate under a feature
  flag and flag results as heuristic; add an automatic fallback to full search when within resource budgets.
- Determinism: Tie-breakers and seed control produce reproducible traversals.

## Performance Expectations

- Fewer dead branches: Simultaneous `VSM`/`DSM`/`XSM` checks cut `VSM`-only wrong paths early.
- Lower orchestration/I/O: One pass, shared preprocessing, fewer materialized handoffs.
- CPU tradeoff: Slightly higher per-node bookkeeping (three-family bounds), but net node count should drop
  substantially.
- Memory: Additional residual arrays and caches; cap transposition tables and avoid storing full states.
- Parallelism: Split at top levels with beam/diversity; shard by early row choices. Memoization can be read-mostly;
  prefer lock-free reads and coarse-grained writes.

## Interfaces and Integration

- Entry point: `radditz_sift_hybrid(cfg, targets, beliefs?, sinks, flags)`.
- Sinks: `vsm_sink`, `dsm_sink`, `xsm_sink` remain independent; each can be toggled on/off.
- Flags: `enable_hybrid`, `enable_beliefs`, `tau/alpha/beta`, `beam_k`, `max_mem`, `time_budget`,
  `fallback_full_search`.
- Telemetry: Counts of generated/pruned candidates by reason (vsm/dsm/xsm bound, memo), depth histograms, time/memory,
  belief usage stats.

## Diagnostics and Testing

- Unit tests: Residual updates, min/max bounds, incidence indexing, diagonal indexing correctness, belief scoring
  monotonicity, memoization correctness.
- Property tests: Random target grids with fixed seeds to ensure hybrid ≡ baseline outputs under exhaustive mode.
- Scenario tests: Adversarial cases with conflicting sums; cases where beliefs are wrong; large sparse vs dense regimes.
- Golden runs: Compare branch counts and runtimes against today’s multi-phase path; require equivalence in outputs.

## Rollout Plan

- Phase 0: Remove existing radditz sift functionality, leaving placeholders in the top-level decompress pipeline
- Phase 1: Implement the new features.
- Phase 2: Implement at least 95% test coverage (ideally 100%) coverage
- Phase 3: Ensure radditz-sift heartbeat messages include the program PID, phase=radditz-sift, rectangles_count,
  rectangles_per_second, swap_count, swaps_per_second, lsm_sat, vsm_sat, dsm_sat, xsm_sat.
- Phase 4: Run 20-minute smoke test and capture baseline performance.

## Risks and Mitigations

- Tighter coupling reduces failure isolation:
    - Keep sinks modular;
    - Preserve the ability to run single-target diagnostics; add
    - Feature flags for rapid rollback.
- Memory blow-up from memoization:
    - LRU/size limits;
    - Hash reductions;
    - Selective memo only at deeper depths.
- Misleading beliefs:
    - Temperature scaling,
    - Diversity budget, and
    - Strict separation between ordering and pruning; add
    - Calibration monitoring.
- Complexity in diagonal indexing:
    - Centralize incidence precompute;
        - test thoroughly with randomized grids.
- Parallelism regressions:
    - Reintroduce beam at top levels;
    - Exploit row independence in candidate scoring.

## Proposed Defaults and Closure Plan

Resolved choices and actionable defaults replacing prior open questions.

- Heuristic Weights (value ordering):
  - Defaults: `α=1.0`, `β=1.0`, `τ=1.0` (deterministic, conservative).
  - Tunable flags: `CRSCE_HYB_ALPHA`, `CRSCE_HYB_BETA`, `CRSCE_HYB_TEMP` for test‑runner sweeps.

- Belief Refresh Frequency:
  - Default: beliefs fixed for the duration of a Hybrid Sift run (deterministic, zero overhead).
  - Optional stall‑based refresh (disabled by default):
    - `CRSCE_HYB_REFRESH_ON_STALL_MS` (default `0` = disabled). When `>0`, if no bound hits and no acceptances are
      observed for the given milliseconds, refresh beliefs (or locally update) at the next safe boundary.
    - `CRSCE_HYB_REFRESH_DEPTH_INTERVAL` (default `64`): coarse depth interval to consider refresh when enabled.

- Transposition Key Granularity (if/when enabled):
  - Key = `(row_index, compact residual signature per family, coarse candidate footprint hash)`.
  - Residual signature coarsened into 8–16 bucket histograms per family (cols/diag/xdiag) to maximize reuse and control
    memory.

- DSM/XSM Specialized Bounds:
  - Start with min/max forward‑checking for all families; adopt DSM/XSM tightening incrementally.
  - Implementation will include code comments listing candidates for tightening (e.g., corridor/window bounds based on
    candidate incidence; simple Hall‑like checks on diagonal capacities when slack is near zero) for posterity.
