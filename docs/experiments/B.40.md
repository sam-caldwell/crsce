## B.40 Hash-Failure Correlation Harvesting + Within-Row Priority Branching (Completed &mdash; Null)

### B.40.1 Motivation

The solver reaches a consistent plateau at depth ~96,672 (row ~189 of 511). At the plateau, the
DFS backtracks repeatedly through the same row range. Every SHA-1 failure at a row boundary
discards the entire row's assignment and explores an alternative. Currently no information from
failed paths is retained — each failure is treated as independent.

However, SHA-1 failures at the plateau are not random: the same row range consistently fails,
and the correlation between specific cell assignments and hash failure is a structural property
of the constraint system at that depth. This exploitable structure has never been measured.

### B.40.2 Hypothesis

SHA-1 failures at the plateau are systematically correlated with specific cells within the
failing row. Certain cells (those most tightly coupled to column, LTP, and cross-family
constraints) contribute disproportionately to hash mismatches. If these "high-correlation" cells
are branched on first within the row, SHA-1 pruning triggers earlier in the row evaluation,
reducing the search space for subsequent cells and compressing the effective branching factor
within the plateau row range.

This is distinct from B.26b (within-row MRV by constraint uncertainty): B.26b used `u` values
(remaining unknowns per constraint line) as a proxy for importance; B.40 uses observed SHA-1
failure correlation — a signal that directly measures which cells are responsible for
unsatisfiable paths, rather than which cells are most constrained.

### B.40.3 Method

**Phase 1 — Profiling run (no behavior change):**

Add a `HashFailureTracker` to `RowDecomposedController`. For each SHA-1 failure at row `r`:

1. Record the `1`-assigned cells in row `r` at the time of failure.
2. Accumulate a per-row, per-column failure count: `fail_count[r][c]` incremented each time
   cell `(r,c)=1` at a SHA-1 failure.
3. Also accumulate success count: `pass_count[r][c]` when SHA-1 passes with `(r,c)=1`.
4. Run a fixed profiling budget (e.g., 20M iterations at plateau) and emit correlation
   statistics to a JSON file.

**Correlation score per cell:**

```
phi[r][c] = (fail_count[r][c] / total_fail[r]) - (pass_count[r][c] / total_pass[r])
```

Positive `phi` means the cell appears more often in failing assignments than passing ones.

**Phase 2 — Priority-branching run:**

Load the pre-computed `phi` table. In `BranchingController`, within a row, order cells by
descending `phi` score (branch on high-correlation cells first). The row-serial outer order is
unchanged (rows are still processed 0→510 in sequence), preserving the SHA-1 pruning invariant
established in B.26a. Only the within-row cell ordering changes.

### B.40.4 Implementation Notes

- `HashFailureTracker`: lightweight array `uint32_t fail_count[511][511]`, `uint32_t pass_count[511][511]`
  (~1 MB stack or heap). Incremented only on SHA-1 events (rare relative to propagation steps).
- Profiling run: use existing `RowDecomposedController` with profiling enabled; emit
  `phi[r][c]` to JSON after the run. Requires ~20M iterations at plateau for statistical
  significance (failure events are common in this zone).
- Priority-branching run: load `phi` table at startup; `BranchingController::nextCell()` within
  a row sorts by `phi` (or uses a pre-sorted column-priority array per row).
- Row-serial order preserved: outer DFS loop advances rows 0→510 identically to current B.26c.

### B.40.5 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Plateau depth > 96,672 | Within-row phi correlation is a genuine pruning signal; proceed to Phase 2 full run |
| H2 (Neutral) | Depth ~96,672, iteration rate unchanged | Correlation exists but not at the right depth; try profiling only rows 150–200 |
| H3 (Regression) | Depth < 96,672 | Priority disrupts a beneficial order; revert; analyze phi distribution shape |
| H4 (Null — no correlation) | phi values near 0 for all cells | SHA-1 failures are independent of individual cell assignments; B.40 infeasible |

### B.40.6 Results (Completed &mdash; Null)

**Phase 1 profiling run.** The solver was run on a synthetic random 50%-density CSM block with B.31 direction switching disabled (top-down only) and `CRSCE_B40_PROFILE` enabled. After 200M+ DFS iterations at peak depth 85,694 (row ~167):

- Total SHA-1 failures: **0**
- Total SHA-1 passes: **0**
- Hash mismatches: **0**

**Root cause: constraint exhaustion, not hash failure.** The plateau is NOT caused by SHA-1 verification failures. No meeting-band row ever reaches $u(\text{row}) = 0$ (fully assigned) during top-down DFS. The solver backtracks due to constraint infeasibility ($\rho < 0$ or $\rho > u$ on some constraint line) long before any row in the meeting band becomes fully assigned. SHA-1 verification never triggers because the verification condition ($u = 0$) is never met.

This invalidates the B.40 hypothesis. The within-row priority branching by phi correlation is inapplicable because there is no phi signal to measure&mdash;the solver never generates SHA-1 events in the region where improvement is needed.

**B.40 outcome: H4 (null&mdash;no correlation).** SHA-1 failures are not the binding constraint at the plateau. The binding constraint is cross-sum feasibility, which exhausts before any row completes. B.40 Phase 2 (priority branching) is not attempted.

**Implication for B.41.** This finding initially appeared to strengthen the case for B.41 (cross-dimensional hashing + four-direction pincer), suggesting that if the row axis is exhausted, the column axis might not be. However, B.41 diagnostic instrumentation (column unknown counts at the plateau) reveals the opposite: at peak depth 86,125, the minimum nonzero column unknown count is **206** (vs 2 for rows), and only 8 of 511 columns are fully assigned. Columns are far LESS complete than rows at the plateau, with ~206 unassigned cells per column versus ~2 per row. Column-serial passes would need to assign ~206 cells per column before any column-hash verification could fire&mdash;dramatically worse than the row situation. The meeting band's constraint exhaustion is not axis-specific; it affects columns even more severely than rows because columns span the entire row range including the ~343 rows of the meeting band. **B.41's four-direction pincer is unlikely to improve depth.**

**Status: COMPLETED &mdash; NULL.**

---

