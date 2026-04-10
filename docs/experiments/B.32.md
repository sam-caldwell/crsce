### B.32 Four-Direction Iterative Pincer — Diagonal and Anti-Diagonal Passes (Proposed)

#### B.32.1 Motivation

B.30 and B.31 attack the meeting band from two directions: top-down and bottom-up, both
along the row axis. The diagonal cross-sums (DSM, 1,021 lines) and anti-diagonal cross-sums
(XSM, 1,021 lines) carry constraint information along axes rotated 45° from the row/column
grid, but neither B.30 nor B.31 exploits this geometric diversity. After the top-down and
bottom-up passes plateau, the meeting band (~rows 178–332, ~79K cells) remains underdetermined.
Constraints along the row axis have been exhausted by the two row-serial passes; constraints
along the diagonal and anti-diagonal axes have been tightened only as a side effect of
row-serial cell assignments, not by directed traversal.

**The B.32 hypothesis:** by adding diagonal and anti-diagonal DFS passes that proceed
top-down through the row dimension, the solver can tighten constraint lines from four
geometric directions within a single cycle. Each completed cycle reduces the unknowns in
the meeting band; the cycle repeats until a full pass-cycle produces no progress. The
diagonal and anti-diagonal passes preserve SHA-1 row-hash verification because they
advance top-to-bottom through rows, allowing rows to complete and trigger hash checks.

#### B.32.2 The Four-Direction Structure

The B.32 solver executes a repeating four-pass cycle on a **single shared ConstraintStore**:

| Pass | Direction | Cell Selection | SHA-1 Order |
|------|-----------|---------------|-------------|
| **1. Top-down** | Rows 0 → 510 | Row-major (existing) | Rows 0, 1, 2, … |
| **2. Bottom-up** | Rows 510 → 0 | Reverse row-major (B.31) | Rows 510, 509, 508, … |
| **3. Diagonal top-down** | DSM[$s{-}1$ … $2(s{-}1)$], rows 0 → 510 | Cells on diagonals $d \in [510, 1020]$, row-ascending within each diagonal | Rows complete as last diagonal cell in each row is assigned |
| **4. Anti-diagonal top-down** | XSM[$s{-}1$ … $2(s{-}1)$], rows 0 → 510 | Cells on anti-diagonals $x \in [510, 1020]$, row-ascending within each anti-diagonal | Same: rows complete when last anti-diagonal cell is assigned |

**Cycle termination:** if a complete four-pass cycle produces zero newly forced cells and
zero newly completed rows across all four passes, the meeting band is irreducibly hard at
the current constraint density. The solver falls back to standard row-major DFS within
the remaining underdetermined cells.

**Plateau detection within each pass:** identical to B.31. When `stall_count` reaches
threshold $N$ (candidate: 511), the current pass declares a plateau and yields to the
next pass in the cycle. The pass does not resume until the next cycle iteration.

#### B.32.3 Geometric Coverage of the Diagonal and Anti-Diagonal Passes

**DSM[$s{-}1$ … $2(s{-}1)$] coverage.** Diagonal index $d = c - r + (s-1)$. The range
$d \in [510, 1020]$ corresponds to cells where $c \geq r$ — the upper-right triangle of
the matrix (including the main diagonal).

For meeting-band row $r$ ($178 \leq r \leq 332$), pass 3 assigns cells at columns
$c \in [r,\, 510]$:

| Row | Cells assigned by pass 3 | Count |
|-----|-------------------------|-------|
| 178 | $c \in [178, 510]$ | 333 |
| 255 | $c \in [255, 510]$ | 256 |
| 332 | $c \in [332, 510]$ | 179 |

**XSM[$s{-}1$ … $2(s{-}1)$] coverage.** Anti-diagonal index $x = r + c$. The range
$x \in [510, 1020]$ corresponds to cells where $c \geq 510 - r$.

For meeting-band row $r$, pass 4 assigns cells at columns $c \in [510 - r,\, 510]$.
The net new cells (not already assigned by pass 3) are in columns
$c \in [510 - r,\, r - 1]$, which is non-empty only when $510 - r < r$, i.e., $r > 255$:

| Row | New cells from pass 4 | Count |
|-----|----------------------|-------|
| 178 | None (pass 3 already covers $[178, 510] \supseteq [332, 510]$) | 0 |
| 255 | None (pass 3 covers $[255, 510]$ = pass 4's $[255, 510]$) | 0 |
| 332 | $c \in [178, 331]$ | 154 |

**Union of passes 3 + 4:** for meeting-band row $r$, columns $c \in [\min(r,\, 510{-}r),\, 510]$
are assigned. The complementary **left portion** — columns $c \in [0,\, \min(r,\, 510{-}r) - 1]$ —
remains unassigned after both diagonal passes. This left portion contains:

- Row 178: 178 cells in columns $[0, 177]$
- Row 255: 255 cells in columns $[0, 254]$
- Row 332: 178 cells in columns $[0, 177]$

#### B.32.4 SHA-1 Preservation

A key concern from the column-major variant (discussed prior to B.32) was loss of SHA-1
row-hash verification. B.32 avoids this by construction:

**Passes 3 and 4 proceed top-down through the row dimension.** Within each diagonal (or
anti-diagonal), cells are assigned in ascending row order. As the passes progress, cells
accumulate in each meeting-band row from right to left. When any row's last unassigned
cell is filled — whether by direct assignment, propagation, or a combination — SHA-1
verification fires immediately via the existing `u(row_r) == 0` trigger.

The diagonal and anti-diagonal passes do not complete rows as quickly as row-serial
passes (they spread assignments across many rows simultaneously rather than filling one
row at a time). However, they do not prevent row completion — they merely defer it.
Combined with the assignments from passes 1 and 2 (which fully completed the top and
bottom rows), the diagonal passes fill the right portion of each meeting-band row,
leaving fewer cells for the subsequent cycle's row-serial passes to resolve before
SHA-1 fires.

**B.26a is not applicable.** B.26a's MRV cell selection fragmented row completion during
the *primary* solve, preventing SHA-1 from ever firing efficiently. B.32's diagonal
passes operate *after* the row-serial passes have plateaued, as a supplementary
mechanism. The row-serial passes remain the primary SHA-1-verified solve; the diagonal
passes pre-fill portions of meeting-band rows to reduce the branching cost of the
subsequent row-serial cycle.

#### B.32.5 The Left-Portion Blind Spot

The DSM[$s{-}1$ … $2(s{-}1)$] and XSM[$s{-}1$ … $2(s{-}1)$] passes cover cells where
$c \geq \min(r,\, 510{-}r)$. Cells in the left portion ($c < \min(r,\, 510{-}r)$) lie on
diagonal lines DSM[$0$ … $s{-}2$] and anti-diagonal lines XSM[$0$ … $s{-}2$], which are
outside the targeted index ranges.

The left-portion cells receive **no direct assignments** from passes 3 or 4. Their
constraint tightening is purely indirect:

1. **LTP cross-pollination.** Each right-portion cell assigned by passes 3/4 updates four
   LTP constraint lines. LTP lines are spatially random (Fisher-Yates shuffle), spanning
   both left and right portions of the meeting band. A right-portion assignment on an LTP
   line that includes left-portion cells reduces $u$ on that line, potentially pushing it
   toward forcing ($u = 1$).

2. **Diagonal bleed-through.** Diagonals in DSM[$0$ … $s{-}2$] pass through both the
   assigned top/bottom regions (from passes 1/2) and the meeting band. Right-portion
   assignments from passes 3/4 are not on these diagonals (by definition), but the LTP
   cross-pollination may force cells that *are* on these lower diagonals, cascading
   indirectly.

3. **Column constraint tightening.** For right-portion columns ($c > 255$), passes 3/4
   assign many meeting-band cells, heavily consuming the column budget. For left-portion
   columns ($c < 178$), passes 3/4 provide zero meeting-band assignments — these columns
   see only the top-down and bottom-up contributions (~356 of 511 cells assigned, $u \approx
   155$). Column forcing in the left portion depends entirely on the row-serial passes.

**The cycle's second iteration resolves the blind spot.** When the top-down pass resumes
on cycle 2, it enters the meeting band with the right portion already assigned. For each
meeting-band row $r$, only $\min(r,\, 510{-}r)$ cells remain — the left portion. Row 178
needs 178 branching decisions instead of 511 before SHA-1 fires. Row 255 needs 255 instead
of 511. This is a **2–3× reduction in branching cost per row**, yielding correspondingly
faster SHA-1 feedback and cheaper backtracking.

#### B.32.6 Constraint Density After One Full Cycle

After the first complete four-pass cycle, a typical remaining cell in the left portion
(e.g., $r = 255, c = 100$) has the following constraint profile:

| Constraint | Assigned | Unknown ($u$) | Near forcing? |
|------------|----------|---------------|---------------|
| Row 255 (LSM) | 256 (right portion) | 255 | No |
| Column 100 (VSM) | ~356 (top + bottom) | ~155 | No |
| Diagonal $d = 355$ (DSM) | ~200 (top + bottom + right portion of meeting band) | ~100 | No |
| Anti-diag $x = 355$ (XSM) | ~200 | ~100 | No |
| LTP lines | ~350 (all passes) | ~160 | Unlikely |

No individual line is near $u = 1$, so propagation alone does not resolve the left portion.
**The value of B.32 is not that the diagonal passes solve the meeting band directly — it is
that they halve the branching work for the subsequent row-serial cycle.** Faster SHA-1
feedback on the resumed top-down pass is the primary benefit mechanism.

On subsequent cycles, each pass enters with a progressively tighter constraint network.
The cycle converges when either: (a) all cells are assigned and verified, or (b) a complete
cycle produces no new forced cells, indicating the remaining cells are irreducibly hard.

#### B.32.7 Proposed Implementation

**New state (local to `enumerateSolutionsLex`):**

```cpp
enum class PassDirection : std::uint8_t {
    TopDown,          // Pass 1: row-major, rows 0 → 510
    BottomUp,         // Pass 2: reverse row-major, rows 510 → 0
    DiagonalTopDown,  // Pass 3: DSM[s-1..2(s-1)], row-ascending
    AntiDiagTopDown   // Pass 4: XSM[s-1..2(s-1)], row-ascending
};

PassDirection currentPass     = PassDirection::TopDown;
std::uint32_t passStallCount  = 0;      // branching decisions since last forced cell
std::uint32_t kPassStallMax   = kS;     // plateau threshold (511)
std::uint64_t cycleProgress   = 0;      // forced cells + completed rows in current cycle
std::uint8_t  cycleCount      = 0;      // total completed cycles
```

**Precompute diagonal and anti-diagonal cell orderings:**

```cpp
// Pass 3 ordering: for each diagonal d in [kS-1, 2*(kS-1)], cells sorted by ascending row.
// Only meeting-band cells (those still unassigned after passes 1/2) are included.
auto diagOrder = computeDiagCellOrder(kS - 1, 2 * (kS - 1), SortOrder::RowAscending);

// Pass 4 ordering: for each anti-diagonal x in [kS-1, 2*(kS-1)], cells sorted by ascending row.
auto antiDiagOrder = computeAntiDiagCellOrder(kS - 1, 2 * (kS - 1), SortOrder::RowAscending);
```

**Pass cycling logic:**

```cpp
// After current pass plateaus:
passStallCount = 0;
switch (currentPass) {
    case PassDirection::TopDown:        currentPass = PassDirection::BottomUp;       break;
    case PassDirection::BottomUp:       currentPass = PassDirection::DiagonalTopDown; break;
    case PassDirection::DiagonalTopDown: currentPass = PassDirection::AntiDiagTopDown; break;
    case PassDirection::AntiDiagTopDown:
        // End of cycle — check convergence
        if (cycleProgress == 0) {
            // No progress in entire cycle: fall back to row-major DFS
            currentPass = PassDirection::TopDown;
            // ... enter fallback mode ...
        } else {
            currentPass = PassDirection::TopDown;
            cycleProgress = 0;
            ++cycleCount;
        }
        break;
}
```

**Cell selection by pass:**

```cpp
switch (currentPass) {
    case PassDirection::TopDown:         activeOrder = &cellOrder;        break;
    case PassDirection::BottomUp:        activeOrder = &bottomOrder;      break;
    case PassDirection::DiagonalTopDown:  activeOrder = &diagOrder;        break;
    case PassDirection::AntiDiagTopDown:  activeOrder = &antiDiagOrder;    break;
}
```

**SHA-1 verification** is unchanged: row $r$ verifies when $u(\text{row}_r) = 0$, regardless
of which pass completed the row. The existing `IHashVerifier` interface requires no
modification.

**Unified undo stack:** identical to B.31. The undo stack records $(r, c)$ pairs regardless of
which pass made the assignment. Backtracking across pass boundaries correctly restores the
ConstraintStore via the existing assign/unassign machinery.

**DI consistency:** as in B.31, the compressor runs the same four-pass cycle as the
decompressor. The DI is the zero-based position of the correct solution in the four-pass
enumeration order. This order is deterministic and reproducible; DI consistency between
compression and decompression is guaranteed by code identity.

#### B.32.8 Expected Outcomes

**Optimistic.** The diagonal and anti-diagonal passes complete their respective cascades,
assigning ~50–65% of meeting-band cells in the right portion. LTP cross-pollination forces
a significant fraction of left-portion cells. When the top-down pass resumes on cycle 2,
most meeting-band rows need only 50–100 branching decisions before SHA-1 fires. The faster
SHA-1 feedback dramatically reduces backtracking. The cycle converges in 2–3 iterations.
Effective branching space falls from ~170K cells to ~5–10K. Wall-clock decompression time
decreases by 1–2 orders of magnitude compared to B.31's two-direction Pincer.

**Likely.** The diagonal and anti-diagonal passes fill the right portion of the meeting band
but the left portion remains largely underdetermined (LTP cross-pollination forces few cells).
The top-down pass on cycle 2 benefits from the 2–3× reduction in per-row branching cost:
row 178 needs 178 guesses instead of 511 before SHA-1 fires, catching bad branches ~3×
sooner. Net speedup over B.31 is modest but real — perhaps 1.5–3× in wall time. The
improvement concentrates at the meeting-band edges (rows 178 and 332, where the left portion
is smallest) and is weakest at the center (row 255, where 255 cells still need branching).

**Pessimistic.** The diagonal and anti-diagonal passes thrash in the meeting band: without
per-row SHA-1 verification during the diagonal sweeps themselves, the solver makes incorrect
branching decisions that propagate incorrect assignments into the right portion. When the
top-down pass resumes, the pre-filled right portion is wrong, causing immediate SHA-1
failures and full backtracking through the diagonal pass assignments. The four-pass cycle
overhead exceeds the constraint-tightening benefit. Performance degrades to below B.31.

#### B.32.9 Relationship to B.30 and B.31

B.32 extends the Pincer hypothesis from two directions (B.31) to four, adding constraint
tightening from the diagonal and anti-diagonal axes:

| | B.31 (Two-direction) | B.32 (Four-direction) |
|---|---|---|
| Passes per cycle | 2 (top-down, bottom-up) | 4 (top-down, bottom-up, diagonal, anti-diagonal) |
| Constraint axes exploited | Row (primary), col/diag/anti-diag/LTP (via propagation) | Row + diagonal + anti-diagonal (directed), col/LTP (via propagation) |
| Meeting-band pre-fill | None (row-serial only) | Right portion (~50–65% of meeting-band cells) |
| SHA-1 preservation | Full (both passes are row-serial) | Full (all passes proceed top-down in row dimension) |
| Per-row branching cost (cycle 2) | 511 cells/row | $\min(r, 510{-}r)$ cells/row (178–255) |
| Implementation cost | ~150–250 lines over baseline | ~300–400 lines over baseline |

B.32 is implemented after B.31 validates the two-direction Pincer. If B.31 shows that the
meeting band shrinks significantly from top-down + bottom-up alternation alone, the marginal
value of diagonal/anti-diagonal passes may be small. Conversely, if B.31 leaves a substantial
meeting band (~100+ rows), B.32's per-row branching reduction becomes the primary mechanism
for further progress.

#### B.32.10 Open Questions

(a) **Do the diagonal passes thrash without per-row SHA-1?** The diagonal and anti-diagonal
passes assign cells across many rows simultaneously. A bad branching decision on diagonal $d$
affects cells in many rows, but SHA-1 cannot reject any individual row until it completes.
The effective pruning during passes 3/4 relies on constraint-line feasibility checks
($0 \leq \rho \leq u$) and diagonal sum verification (when $u(d) = 0$, the accumulated
sum must equal DSM[$d$]). These are weaker than SHA-1 but may suffice for the partially
pre-constrained meeting band.

(b) **Should passes 3 and 4 target DSM[$0$ … $s{-}2$] and XSM[$0$ … $s{-}2$] instead (or
additionally)?** The current design targets the upper-right diagonals, leaving a left-portion
blind spot. Adding lower-diagonal passes (DSM[$0$ … $s{-}2$], XSM[$0$ … $s{-}2$]) would
cover the left portion directly, making it a six-pass or eight-pass cycle. The trade-off is
implementation complexity vs. coverage completeness.

(c) **What is the optimal diagonal traversal order within each pass?** Candidates include:
processing diagonals shortest-first (exploiting early sum verification on short diagonals),
longest-first (maximizing propagation cascade breadth), or by residual tightness (diagonals
nearest to $\rho = 0$ or $\rho = u$ first). The choice affects how quickly the passes trigger
forced cells and row completions.

(d) **Can seeds be jointly optimized for four-direction depth?** The B.26c seed-search
infrastructure optimized seeds for the forward (top-down) cascade. B.32 introduces two
additional pass directions whose effectiveness may depend on which LTP lines are loaded
in the right vs. left portions of the meeting band. A four-direction objective
$\text{depth}_{\text{fwd}} + \text{depth}_{\text{rev}} + \text{depth}_{\text{diag}} +
\text{depth}_{\text{anti-diag}}$ is a higher-dimensional search problem.

(e) **What is the convergence rate of the cycle?** Each cycle tightens constraints from
four directions; diminishing returns are expected. Empirical measurement of cells-forced
per cycle will determine whether the iteration converges in 2–3 cycles (useful) or requires
10+ cycles (overhead-dominated).

(f) **Is the right-portion pre-fill correct often enough?** The diagonal and anti-diagonal
passes branch without SHA-1 verification. If the branching decisions in passes 3/4 have a
high error rate, the pre-filled right portion will be mostly wrong, and cycle 2's top-down
pass will waste time backtracking through incorrect diagonal assignments rather than
benefiting from the pre-fill. The hash-mismatch rate during the diagonal passes is the
key diagnostic.

---

