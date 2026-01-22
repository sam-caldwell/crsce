# GobpSolver — Parameters and Tuning Guide

`GobpSolver` is a CPU‑based, single‑host Game Of Beliefs Protocol (GOBP) step used during CRSCE v1 decompression. It
computes local beliefs, based on Loopy Beliefs Propagation (LBP) from line constraints and deterministically assigns
cells when confidence crosses configured thresholds. This document describes its parameters and practical tuning
guidance.

## Overview

- Computes per‑cell belief from residual constraints (R/U) across four line families: row, column, diagonal, and
  anti‑diagonal.
- Combines line beliefs via a naive independence assumption using odds products, then converts back to probability.
- Applies exponential damping when writing beliefs into the CSM data layer (smooths updates across iterations).
- Makes deterministic assignments when blended belief crosses thresholds, updating U (and R for 1‑assignments) and
  locking the cell.

## Parameters

### `damping` (double, default: 0.5; range: [0.0, 1.0])

- Controls blending between the previous data value and the newly computed belief for a cell.
- Formula: `blended = damping * prev + (1 - damping) * belief`.
- Effects:
    - Lower values (e.g., 0.0–0.3): more responsive to current constraints; can oscillate on noisy constraints.
    - Mid values (e.g., 0.4–0.7): good general‑purpose smoothing.
    - High values (e.g., 0.8–1.0): very conservative; slows convergence but stabilizes against jitter.

### `assign_confidence` (double, default: 0.995; clamped to (0.5, 1.0])

- Threshold to convert a belief into a deterministic assignment.
- Rule: if `blended >= assign_confidence` then assign 1; if `blended <= 1 - assign_confidence` then assign 0.
- Effects:
    - Lower (e.g., 0.90–0.95): more aggressive assignment; faster but riskier if constraints are weak.
    - Higher (e.g., 0.98–0.999): more conservative; slower but safer under noisy or partial constraints.

## Recommended Defaults

- Start with: `damping = 0.5`, `assign_confidence = 0.995`.
- If DE (deterministic elimination) doesn’t make progress or constraints are sparse, temporarily lower
  `assign_confidence` to 0.98–0.99 for a few iterations, then restore to 0.995+.
- If beliefs fluctuate across iterations, raise `damping` to ~0.7–0.85.

## Practical Tuning

1. Begin with a DE pass (sound forced moves), then one GOBP step.
2. If GOBP made many assignments, run DE again (new forced moves may appear), then another GOBP step.
3. Repeat until no progress. If stuck:
    - Reduce `assign_confidence` slightly (e.g., 0.995 → 0.990) for 1–2 steps, then restore.
    - Increase `damping` if beliefs oscillate or fluctuate.
4. Stop when all U vectors are zero (solved) or when neither DE nor GOBP makes progress for several iterations.

## Integration Pattern

- Typical loop per block:
    1. `de.solve_step()` until DE makes no further progress.
    2. `gobp.solve_step()` once (or a small fixed number of times).
    3. Repeat 1–2 while progress > 0.
- Stopping criteria: solved (all U==0), or max iterations, or no progress for N rounds.

## Numerical Stability & Robustness

- Line belief `p = R/U` is clamped via odds to avoid 0/1 singularities; internally uses small epsilons.
- If `U == 0` on any line while attempting assignment, GobpSolver throws (contradiction); fix state upstream or
  adjust iteration order.
- When assigning 1, GobpSolver decrements R on all four lines; underflow throws.

## Debugging & Inspection

- The `Csm` data layer stores blended beliefs; you can sample it to visualize “heat” across the grid.
- Track progress counts from `solve_step()` to understand how parameter changes affect assignment rates.
- Verify residual invariants (0 ≤ R ≤ U) after each step; contradictions usually indicate upstream state issues.

## Quick Reference

- Safer but slower: `damping ~ 0.7–0.85`, `assign_confidence ~ 0.997–0.999`.
- Faster but riskier: `damping ~ 0.2–0.4`, `assign_confidence ~ 0.98–0.99`.
- Balanced defaults: `damping = 0.5`, `assign_confidence = 0.995`.
