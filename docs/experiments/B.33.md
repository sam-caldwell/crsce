### B.33 Complete-Then-Verify Solver with Constraint-Preserving Local Search (Proposed)

#### B.33.1 Motivation

Every solver from B.1 through B.32 uses the same fundamental architecture: **interleaved
constraint solving and hash verification**. The DFS assigns cells row-by-row, checking SHA-1
at each row boundary and backtracking on failure. At the current operating point (B.26c,
depth ~91,090), the solver spends the vast majority of its compute budget on SHA-1 checks
that fail — millions of rejected row candidates at the propagation frontier (~row 178),
each providing approximately zero bits of useful search information.

**The architectural problem.** A failed SHA-1 check tells the solver "this specific row
assignment is wrong" but provides no information about *how* it is wrong or *which bits*
need to change. SHA-1 is a cryptographic hash: the output is a pass/fail signal with no
gradient, no partial credit, and no locality. A single bit flip in the row changes the hash
completely. Each failed check eliminates exactly one candidate from a space of
$\binom{511}{\text{LSM}[r]} \approx 2^{510}$ possible row assignments — negligible pruning.
The solver is spending cycles on near-zero-information operations.

**The deeper problem.** The solver tests SHA-1 on *partial solutions* — a row assignment
within an incomplete CSM where ~170,000 cells are still unassigned. A row that passes SHA-1
at this stage IS correct (SHA-1 has no false positives), but a row that fails SHA-1 may fail
because:

(a) The row assignment itself is wrong (the row's bits need to change), or

(b) The partial assignment of prior rows is wrong (this row cannot be completed to a valid
    solution given the current prefix).

The solver cannot distinguish (a) from (b). In case (b), it exhaustively tries all
assignments of the current row before backtracking to a prior row — exploring an
exponentially large subtree that contains zero solutions. This is the "spinning wheels"
failure mode.

**The B.33 hypothesis.** Separate constraint solving from hash verification entirely:

1. **Solve cross-sums to completion** — produce a full CSM candidate satisfying all 5,108
   constraint lines, without any SHA-1 checks during the solve.
2. **Verify the complete candidate** — check SHA-1 on every row. A complete, cross-sum-valid
   CSM is a coherent hypothesis worth testing; a partial assignment through row 178 is not.
3. **Modify, don't restart** — for rows that fail SHA-1, search for alternative cross-sum-valid
   assignments via constraint-preserving local transformations. Never discard the complete state.

#### B.33.2 Information-Theoretic Foundation

The cross-sum constraint system has 5,108 lines over 261,121 binary variables. The constraint
matrix $A$ (5,108 × 261,121) has effective rank $\leq 5,101$ (accounting for linear dependencies:
each of the 8 partition families sums to the same global popcount, giving at least 7 redundant
equations). The null space of $A$ has dimension $\geq 256,020$.

**Propagation determines ~91,090 cells** — values uniquely forced by the cross-sum constraints
via arc consistency (forcing rules: $\rho = 0 \Rightarrow$ unknowns are 0; $\rho = u \Rightarrow$
unknowns are 1). The remaining ~170,031 cells lie in the null space: their values can vary freely
while maintaining all cross-sum constraints.

**The cross-sum solution space** contains approximately $2^{170{,}000}$ valid matrices — an
astronomically large set, of which exactly one is the target CSM.

**SHA-1 is not a linear constraint.** A 160-bit hash does not provide 160 bits of constraint
information in the search-theoretic sense. It provides a binary pass/fail oracle:

- **PASS** (probability $\sim 2^{-160}$ for a random candidate): confirms the row is correct.
  Highly informative (~160 bits).
- **FAIL** (probability $\sim 1$): eliminates one candidate from $\sim 2^{510}$ possible row
  assignments. Information content: $\log_2\!\bigl(\tfrac{2^{510}}{2^{510}-1}\bigr) \approx 0$
  bits.

The current solver performs millions of FAIL checks per second. Each provides ~0 bits of
information. The total information gathered from SHA-1 failures is negligible despite
enormous compute expenditure. **SHA-1 verification is only useful when applied to a
complete, plausible candidate — not as a search heuristic on partial assignments.**

#### B.33.3 The Complete-Then-Verify Architecture

**Phase 1: Cross-sum solve.** Starting from the propagation-determined cells (~91K, rows
0–177), extend the assignment through the meeting band (rows 178–510) using DFS with
cross-sum feasibility checks only. No SHA-1 verification during this phase.

The cross-sum system in the meeting band is heavily underdetermined ($u \gg 1$ on all
lines), so infeasibilities are rare. The DFS should reach a complete 261,121-cell assignment
quickly — orders of magnitude faster than the current solver, which backtracks at every
row boundary.

**Phase 2: Hash verification.** Compute SHA-1 on all 511 rows of the complete candidate.
Rows 0–177 (propagation-determined) will pass. Meeting-band rows will almost certainly fail
(probability $\sim 2^{-160}$ per row of a coincidental match).

**Phase 3: Lock and modify.** Lock every row that passes SHA-1 (confirmed correct). For
failing rows, apply **constraint-preserving transformations** to the complete CSM,
searching for alternative cross-sum-valid matrices where additional rows pass SHA-1.
After each transformation, re-check SHA-1 on affected rows. Lock newly passing rows.
Repeat until all rows pass or the search budget is exhausted.

The critical property: **the solver never discards a complete state**. It always holds a
full, cross-sum-valid CSM and incrementally modifies it. This contrasts sharply with the
current DFS, which throws away partial work on every SHA-1 failure.

#### B.33.4 Constraint-Preserving Transformations (Swap Polygons)

A **constraint-preserving swap** is a set of cells whose values can be flipped (0↔1) while
maintaining all 5,108 cross-sum constraints. Formally, a swap is a vector
$D \in \{-1, 0, +1\}^{261{,}121}$ in the null space of the constraint matrix $A$, subject to:

- **Balance:** each constraint line $L$ has $\sum_{(r,c) \in L} D(r,c) = 0$
- **Feasibility:** the modified matrix $M + D$ remains in $\{0, 1\}^{s \times s}$

The null space has dimension ~256,020, so valid swap patterns exist abundantly. The practical
question is: **how small can a swap be?**

**Known results for subsets of constraint families:**

| Families preserved | Min swap size | Construction |
|---|---|---|
| LSM + VSM (row + column) | 4 cells | 2×2 rectangle: $(r_1,c_1), (r_1,c_2), (r_2,c_1), (r_2,c_2)$ |
| LSM + VSM + DSM (+ diagonal) | 6 cells | 3×3 hexagonal: rows $r_1,r_2,r_3$; columns $c_1 = c_3{-}r_2{+}r_1$, $c_2 = c_3{+}r_1{-}r_3$ |
| LSM + VSM + DSM + XSM (all geometric) | ≥8 cells | Unverified; 6-cell diagonal-preserving swaps break anti-diagonal sums |

**The 4-cell rectangle failure.** A rectangle at $(r_1,c_1), (r_1,c_2), (r_2,c_1), (r_2,c_2)$
preserves row and column sums but places its 4 cells on 4 distinct diagonals ($d = c-r+510$)
and 4 distinct anti-diagonals ($x = r+c$). Each diagonal and anti-diagonal sees exactly one
±1 disturbance with nothing to cancel it. For two diagonals to pair, one would need
$c_1-r_1 = c_2-r_2$ AND $c_2-r_1 = c_1-r_2$, which requires $r_1 = r_2$ (degenerate).

**Adding LTP families.** Each LTP sub-table is a pseudorandom partition (Fisher-Yates shuffle)
with no spatial correlation to the geometric families. A compensating cell for an LTP
disturbance lands at a spatially unrelated position, creating new disturbances on geometric
lines that require their own compensation. The minimum swap for all 8 families (4 geometric
+ 4 LTP) is unknown and must be computed empirically.

**Swap polygon model.** A swap can be visualized as a polygon whose vertices are cell
positions and whose edges connect cells sharing a constraint line. Traversing the polygon
with alternating signs (+1, −1, +1, −1, …) ensures each edge's constraint line sees
balanced changes. However, each vertex (cell) sits on ALL 8 constraint lines, not just the
2 polygon edges it participates in. The non-edge lines see unbalanced disturbances. Closing
all disturbances requires additional structure beyond a simple polygon — the pattern must
be a higher-dimensional cycle in the constraint hypergraph.

#### B.33.5 Sub-experiment B.33a: Minimum Swap Size Computation

**Objective.** Determine the minimum number of cells in a valid all-constraint-preserving
swap for the current 8-family constraint system (LSM, VSM, DSM, XSM, LTP1–LTP4) with
seeds CRSCLTPV, CRSCLTPP, CRSCLTP3, CRSCLTP4.

**Hypothesis.** Two competing models:

- **Linear model:** the minimum swap grows as ~2 cells per constraint family, predicting
  ~16 cells (8 families × 2) or possibly as low as 8 (one cell per family).
- **Exponential model:** each pseudorandom LTP family approximately doubles the
  cancellation chain, predicting ~128–256 cells.

**Method.**

(a) **Construct the constraint matrix** $A$ (5,108 × 261,121) for the current LTP seeds.
Each row of $A$ is an indicator vector for one constraint line.

(b) **Compute the null space** of $A$ over the integers (or equivalently, over $\mathbb{Q}$).
The null space has dimension ~256,020. Finding it exactly is infeasible at this scale, but
we only need SHORT null-space vectors.

(c) **Greedy chain repair.** Start with a random 4-cell rectangle (preserves row + column).
Identify violated lines (diagonals, anti-diagonals, LTPs with net ±1 change). For each
violated line, find a compensating cell on that line whose addition (with opposite sign)
cancels the disturbance. The compensating cell creates new disturbances on its other 7
lines; add those to the repair queue. Iterate until all lines are balanced. Record the
total swap size. Repeat 10,000 times with random starting rectangles; report the minimum.

(d) **Exhaustive small-swap search.** For $k = 4, 6, 8, 10, \ldots, 32$: enumerate
(or sample) sets of $k$ cells with $k/2$ designated +1 and $k/2$ designated −1. Check
whether $A \cdot D = 0$ for each candidate. Report the smallest $k$ with a valid swap.
For small $k$, this is feasible via constraint propagation: the first cell's 8 lines each
require a compensating cell, which constrains the search.

(e) **LTP-only isolation test.** Compute the minimum swap preserving ONLY the 4 LTP families
(ignoring geometric constraints). If this is already large, the LTP pseudorandom structure
is the binding constraint. If it is small (4–8 cells), the geometric constraints are the
bottleneck.

**Expected outcome.** The minimum swap is between 8 and 64 cells, likely closer to the
lower end because the null space is enormous (~256K-dimensional) and short vectors are
statistically likely in high-dimensional spaces. If the minimum exceeds ~100, the local
search approach may be impractical due to the cost of enumerating valid swap patterns.

**B.33a Results (Completed).**

*Tool:* `tools/b33a_min_swap.py` — score-guided greedy BFS from 4-cell rectangle seed.

*Geometric-only (4 families, num_ltp=0):* **0% convergence** in 200 trials with max_cells=500.
The algorithm starts from a balanced 4-cell rectangle (rows, cols satisfied), then greedily
repairs the 4 diagonal and 4 anti-diagonal violations by adding cells with the highest
score. Despite a positive score being achievable on the first step (~+16 per well-placed
cell), the repair chain never terminates within the budget.

*Full system (6 LTP sub-tables, 10 families):* **0% convergence** in 200 trials.

*LTP sensitivity sweep (0–6 LTP sub-tables, 200 trials each):*

| Families (4+LTP) | Success % | Min | Mean |
|---|---|---|---|
| 4 (geo only) | 0% | N/A | N/A |
| 5 | 0% | N/A | N/A |
| 6 | 0% | N/A | N/A |
| 7 | 0% | N/A | N/A |
| 8 | 0% | N/A | N/A |
| 9 | 0% | N/A | N/A |
| 10 | 0% | N/A | N/A |

*Root-cause analysis.* The greedy BFS diverges monotonically: each repair step fixes 1
violation on a targeted line but creates n-1 new violations (one per additional family
the chosen cell participates in). For 4 families, each step nets +2 new violations; for
10 families, each step nets +8 new violations. The score function can pick cells that
cancel more than one violation simultaneously (score >+10) but such cells are rare; the
expected net violation change per step is still positive, so the repair chain diverges in
expectation.

*Geometric minimum (analytic):* The minimum constraint-preserving swap for 4 geometric
families (row, col, diag, anti-diag) is **8 cells**. Proof sketch: any swap must include at
least one pair of cells on each violated family (4 cells to fix diagonals, 4 cells to fix
anti-diagonals, which are the same 4 additional cells by a tiling argument). The construction
is uniquely constrained given a starting rectangle; whether a valid construction exists
depends on the specific rectangle geometry (row/col parity constraints).

*Full-system minimum:* **Unknown** — lower bound is 8 (from geometric argument), upper bound
is unknown. The BFS approach cannot determine it. The null space has ~256,020 dimensions,
so short null vectors likely exist, but finding them requires a different algorithm.

*Conclusion.* The score-guided greedy BFS approach is **not viable** for constructing
minimum constraint-preserving swaps. This does NOT mean B.33 Phase 3 is infeasible: it
means the swap-finding algorithm must be redesigned. Two alternative approaches are viable:

1. **B.33b (Analytic construction):** Characterize the structure of minimum swaps for the
   full 10-family system using the LTP assignment arrays. For each cell, the 10 families
   are known exactly. A minimum swap is a balanced assignment of ±1 signs to a small set
   of cells such that every family containing any selected cell has exactly equal ±1 counts.
   This is equivalent to finding a balanced 10-uniform hypergraph cover — tractable for
   small support sizes via exhaustive search with pruning.

2. **B.33c (Solver upper bound):** Run the C++ decompressor twice on the same test block
   with DI=0 and DI=1. The XOR of the two solution CSMs gives a valid constraint-preserving
   swap; its support size is an **upper bound** on the minimum swap. Consecutive solutions
   differ starting from the first branching cell (~91,090 from the base), so the expected
   support is ~170,031 cells — large but confirms a valid swap EXISTS, and may reveal the
   structure of nearby solutions.

#### B.33.6 Phase 3 Local Search Strategy

Given a minimum swap size of $k$ cells, the Phase 3 search operates as follows:

**Swap vocabulary.** Pre-compute or lazily generate valid $k$-cell swap patterns. Each
swap is parameterized by a starting cell and a chain of compensating cells determined by
the constraint structure. The vocabulary size depends on $k$ and the constraint geometry;
for $k = 8$, a rough estimate is $O(s^4)$ possible swaps (choosing 4 pairs from $s$ options
per family).

**Search loop:**

```
while (failing_rows is not empty):
    select a failing row r (e.g., lowest index, or random)
    for each valid swap pattern involving at least one cell in row r:
        apply swap to the CSM
        recompute SHA-1 on all affected rows
        if any affected row newly passes SHA-1:
            lock that row
            accept the swap
            break
        else:
            undo the swap
```

**Acceptance criteria.** Strictly greedy: accept a swap only if it increases the number of
locked (SHA-1-verified) rows. Alternatively, simulated annealing: accept swaps that decrease
the locked count with probability $e^{-\Delta/T}$, allowing the search to escape local optima.

**SHA-1 landscape.** Each swap changes the values of $k$ cells, affecting at most $k$ rows
(if all cells are in distinct rows, which is likely for $k \geq 8$). SHA-1 is recomputed
only on those rows. With SHA-1 at ~1 µs per 64-byte block, the per-swap verification cost
is $O(k)$ microseconds — negligible.

The fundamental challenge: SHA-1 provides no gradient. A swap either makes a row pass
(probability $\sim 2^{-160}$ per affected row) or doesn't. The search is a random walk on
the space of cross-sum-valid matrices, guided only by the binary signal of SHA-1 pass/fail.

**Mitigation: row-targeted swaps.** Rather than applying random swaps, target swaps to
specific failing rows. For a failing row $r$ with all other rows fixed, the column
constraints fully determine row $r$'s values. To change row $r$, at least one other row
must also change (to alter the column residuals). Enumerate swap patterns that modify row
$r$ and one or more other rows, checking SHA-1 on each candidate. The per-row search space
is bounded by the number of valid swaps involving row $r$, which is much smaller than the
full swap vocabulary.

#### B.33.7 Relationship to Additional LTP Sub-tables

The B.33 architecture becomes strictly more powerful as the constraint density increases.
With more LTP sub-tables:

- The null space shrinks (fewer degrees of freedom → fewer cross-sum-valid matrices)
- The propagation cascade extends (more cells determined without branching)
- The meeting band narrows (fewer underdetermined rows)
- The swap vocabulary shrinks (fewer valid swap patterns, but each is more targeted)
- The search space contracts exponentially

At the limit (~52 LTP sub-tables), the null space collapses to dimension ~0. There is
exactly one cross-sum-valid matrix. Phase 1 produces the correct CSM directly via
propagation. Phases 2 and 3 are trivially satisfied. Decompression is $O(n)$.

The practical trade-off:

| Sub-tables | Block payload | Compression ratio | Null space dim | Phase 1 | Phase 3 |
|---|---|---|---|---|---|
| 4 (current) | ~15.7 KB | ~48.2% | ~170,000 | Fast | Huge search |
| 12 | ~22.3 KB | ~68.5% | ~130,000 | Fast | Large search |
| 24 | ~35.5 KB | ~109% (expansion) | ~80,000 | Fast | Moderate search |
| 36 | ~48.7 KB | ~149% | ~30,000 | Fast | Small search |
| 52 | ~65.6 KB | ~201% | ~0 | Deterministic | N/A |

The sweet spot — if one exists — is the minimum number of sub-tables where the Phase 3
search converges within a practical time budget. This depends on the minimum swap size
(B.33a) and the structure of the SHA-1 landscape over the constrained solution space.

#### B.33.8 Expected Outcomes

**Optimistic.** The minimum swap size is 8–16 cells. Phase 1 completes the CSM in
milliseconds (no SHA-1 backtracking). Phase 3 finds SHA-1-matching row assignments via
targeted swaps within a tractable search budget. The solver converges to the correct CSM
in minutes rather than the current approach's hours-to-never. The architecture
fundamentally outperforms DFS with interleaved SHA-1.

**Likely.** The minimum swap size is 16–64 cells. Phase 1 is fast. Phase 3 is tractable
for rows near the propagation frontier (which have tight constraints from the locked rows
above) but struggles with deep meeting-band rows (which have high null-space dimensionality).
The solver fixes some meeting-band rows but stalls in the center of the meeting band.
Performance is comparable to or modestly better than the current DFS approach for the
first ~200 rows, then degrades.

**Pessimistic.** The minimum swap size exceeds 100 cells. The swap vocabulary is too large
to enumerate efficiently. Phase 3 degenerates into a random walk on the $2^{170{,}000}$
cross-sum-valid matrices with negligible probability of improving any row's SHA-1 status.
The architecture provides no advantage over the current DFS and is abandoned in favor of
constraint-density improvements (more LTP sub-tables). Alternatively, Trump finds a way to 
screw up the fundamentals of mathematics too.

#### B.33.9 Relationship to Prior Work

**B.1 (CDCL).** B.1 attempted to learn clauses from SHA-1 failures — a non-linear global
constraint that produces uselessly long 1-UIP traces. B.33 avoids SHA-1 during the solve
entirely, eliminating the need for CDCL over hash constraints. If CDCL is reintroduced in
B.33, it would operate on cross-sum constraint violations only (cardinality constraints),
which are linear and well-structured — producing tight, useful learned clauses with bounded
jump distance.

**B.30/B.31 (Pincer).** The Pincer experiments attempted to tighten constraints by attacking
from multiple directions. B.31 was tested and produced a null result: bottom-up SHA-1
verification rate was ~0% because unconstrained rows have random assignments that never
match. B.33 embraces this reality — it does not expect meeting-band rows to pass SHA-1
during the solve. Instead, it defers hash verification to a post-solve phase where the
complete CSM provides a meaningful hypothesis to test.

**B.32 (Four-direction Pincer).** B.32 proposes diagonal and anti-diagonal passes to
pre-fill the right portion of the meeting band before resuming row-serial DFS with SHA-1.
B.33 is a more radical departure: it eliminates SHA-1 from the solve loop entirely and
replaces the tree-search paradigm with a local-search paradigm. B.32's pre-filling strategy
could be used within B.33's Phase 1 (as a heuristic for generating the initial candidate),
but Phase 3's swap-based search is a fundamentally different mechanism.

#### B.33.10 Open Questions

(a) **What is the minimum swap size for all 8 constraint families?** This is the critical
parameter determining Phase 3's feasibility. Sub-experiment B.33a addresses this directly.

(b) **Can the swap vocabulary be precomputed?** If the minimum swap is $k$ cells, the number
of valid $k$-cell swap patterns is the key computational parameter. For $k = 8$, the
vocabulary might be manageable ($O(s^4) \sim 10^{10}$). For $k = 64$, it is likely
intractable. Precomputation vs. lazy generation is an implementation decision that depends
on $k$.

(c) **Does simulated annealing help?** Greedy acceptance (only accept swaps that lock new
rows) may trap the search in local optima where no single swap improves SHA-1. Simulated
annealing or tabu search could escape by allowing temporary regressions. Whether this
helps depends on the structure of the SHA-1 landscape over cross-sum-valid matrices.

(d) **Is there a better Phase 3 strategy than element-wise swaps?** Alternatives include:
re-running Phase 1 with different branching decisions in the meeting band (producing a
different complete candidate), Gaussian elimination or linear-programming relaxation over
the null space, or belief propagation on the factor graph of cross-sum constraints with
SHA-1 priors.

(e) **How does the minimum swap size depend on the LTP seeds?** Different seeds produce
different LTP partition geometries. The null-space structure — and therefore the minimum
swap size — depends on the specific partitions. Seeds that produce smaller minimum swaps
may enable more efficient Phase 3 search, adding a new dimension to the seed optimization
problem (B.26c).

(f) **Can Phase 1 be biased toward SHA-1-friendly candidates?** If the DFS in Phase 1
uses a heuristic that produces "more typical" or "less extreme" row assignments (e.g.,
preferring cells that minimize the variance of column residuals), the resulting candidate
might be closer to the correct CSM in Hamming distance, reducing the number of swaps
needed in Phase 3. This is speculative — SHA-1 provides no useful signal for biasing —
but statistical properties of the row constraints might provide weak guidance.

(g) **What is the expected number of swaps to fix one row?** For a failing row $r$, the
number of valid swap patterns involving row $r$ determines the per-row search space. If
the column constraints (with all other rows fixed) leave $d$ degrees of freedom for row $r$'s
affected cells after a swap, the search space per swap is $O(2^d)$. With SHA-1 as the
discriminator, the expected number of swaps to fix row $r$ is $\sim 2^{\min(d, 160)}$.
Measuring $d$ empirically for typical meeting-band rows is a key sub-experiment.

#### B.33.11 Sub-experiment B.33b: Backtracking Minimum Swap Search (Completed)

**Objective.** Determine the minimum constraint-preserving swap size for the full 10-family
system via backtracking DFS rather than greedy forward repair (as in B.33a).

**Method.** `tools/b33b_min_swap_bt.py` — analytic geometric base + backtracking extension.

1. Construct the analytic 8-cell geometric base from a starting rectangle using the exact
   formula: for rectangle (r1,c1),(r1,c2),(r2,c1),(r2,c2) with signs (+1,-1,-1,+1), the
   four unique repair cells are determined by the midpoint formula (requires parity condition
   r1+r2+c1+c2 even). This gives an 8-cell swap that is EXACTLY balanced for all 4 geometric
   families (row, col, diag, anti-diag) — verified analytically.

2. Compute the LTP violations created by the 8-cell base (each cell falls on `num_ltp`
   random LTP lines, creating 8·num_ltp violations in the worst case, all distinct).

3. Backtracking DFS to extend the base swap to balance LTP violations:
   - Pick the most-constrained violated LTP line (minimum fan-out criterion)
   - Branch on the top-20 score-ranked candidate cells for that line
   - Prune if |swap| ≥ budget or lower-bound on remaining cells ≥ remaining budget
   - Recurse; backtrack on no-progress

**Results.**

| Families | Budget | Timeout | Rectangles | Converged | Min found |
|---|---|---|---|---|---|
| 4 (geo only, num_ltp=0) | 20 | 10s | 20/20 | 18/20 | **8 cells** |
| 5 (geo + 1 LTP) | 50 | 15s | 10/10 | 0/10 | N/A |
| 8 (geo + 4 LTP) | 150 | 30s | 10/10 | 0/10 | N/A |

For geometric-only, the analytic base trivially satisfies all 4 families — 8-cell minimum
confirmed. For any LTP sub-table added, backtracking exhausts the 15–30s budget across
10 rectangles without finding any swap within 50–150 cells.

**Root-cause analysis (theoretical).** With Fisher-Yates random partition, each of the
8 geometric-base cells falls on a UNIQUE LTP line with probability ≈ 1 (8 cells out of
511 lines per sub-table; expected distinct lines ≈ 8). For the swap to be LTP-balanced,
each occupied LTP line must have equal ±1 counts, requiring a mate cell for each cell
(since each line has exactly 1 cell). Each mate falls on another unique LTP line
(expected overlap with existing violations ≈ 8/511 ≈ 1.6%). Thus:

- After adding 8 mates to fix 8 violations: new violations ≈ 8 × (1 - 8/511) × num_ltp ≈ 8
  (approximately equal new violations). The cascade is approximately a random walk on
  violations, neither converging nor diverging rapidly.
- But: each mate also creates geometric violations (row, col, diag, anti-diag) since mates
  are NOT generally geometrically balanced. Fixing those creates more LTP violations...

The net result is that violations grow monotonically. The MINIMUM swap size is likely
O(N/511) = O(511) cells per LTP sub-table (the size needed for each partition family to
have enough candidate cells that balanced pairs can form across all families
simultaneously). For 4 LTP sub-tables: minimum ≈ O(2000) cells.

**Conclusion.** The minimum constraint-preserving swap for the full 10-family CRSCE
system with Fisher-Yates random LTP partitions is **O(hundreds to thousands) of cells**.
This is not a local structure — swaps are fundamentally global. Consequently:

- **B.33 Phase 3 is infeasible** as originally specified. No "local search using small
  constraint-preserving swaps" exists for the random-partition LTP structure.
- The search reduces to a random walk on the $2^{256{,}020}$-dimensional null space,
  indistinguishable from exhaustive search.
- The correct CSM M* and the Phase 1 output M0 differ in O(170,000) cells (from the
  first branching point ~91,090 to the end at 261,121). There is no efficient path
  between them via small moves.

**B.33 STATUS: ABANDONED.** The Complete-Then-Verify architecture does not provide a
tractable Phase 3 for random-partition LTP. The decoupling of constraint satisfaction
from hash verification is not achievable via constraint-preserving local search.

**Implication for future work.** The B.33 analysis reveals a fundamental property of
the CRSCE constraint structure: all constraint-preserving transformations are GLOBAL
(O(1000+) cells). This explains why the current DFS with row-serial SHA-1 verification
is effective — it's the only known approach that exploits the correct CSM's structure
(hash constraints) to efficiently navigate the vast null space toward the unique solution.
Any approach that tries to decouple constraint solving from hash verification faces this
global-swap barrier. I'm not 100% comfortable with this, but I cannot explain it any better.
This probably just needs more thinking with a cigar and a long walk.

