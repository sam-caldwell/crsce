## B.63: Hybrid Combinator + DFS at S=127

B.62 established the combinator-algebraic solver at S=191 and identified load-bearing constraints.
B.62m demonstrated that the hybrid architecture (Phase I algebra + Phase II row DFS) works
mechanically but requires sufficient Phase I strength. B.63 returns to S=127 where the combinator
is proven stronger (B.60r: full algebraic solve, $C_r = 87\%$), rows are shorter (127 cells),
and DFS per row is tractable even at moderate Phase I determination.

### B.63a: Constraint Configuration Sweep at S=127

**Prerequisite.** B.60 series and B.62 load-bearing constraint analysis completed.

**Objective.** Systematically evaluate constraint family combinations at S=127 to find the
configuration that maximizes Phase I determination (solver power) at minimum $C_r$. The target
is $C_r < 90\%$ with Phase I determining enough cells that every row has $\leq 38$ unknowns
(70%+ determination), making Phase II DFS tractable.

**Design.** A/B comparison runs across multiple constraint pairings. Each configuration is tested
on MP4 blocks 0&ndash;5. The key metrics per configuration:
- $C_r$: compression ratio (must be $< 90\%$, target $< 80\%$ if achievable)
- Per-block $|D|$: total cells determined by Phase I
- Max unknowns per row: the worst-case row for Phase II
- Mean unknowns per row: average DFS difficulty

**Payload budget at S=127:**

Block size: $127^2 = 16{,}129$ bits.

| Target $C_r$ | Max payload (bits) |
|---------------|-------------------|
| 90% | 14,516 |
| 85% | 13,710 |
| 80% | 12,903 |
| 75% | 12,097 |

**Component costs at S=127 ($b = 7$):**

| Component | Bits | Notes |
|-----------|------|-------|
| LSM ($127 \times 7$) | 889 | Row integer sums (IntBound) |
| VSM ($127 \times 7$) | 889 | Column integer sums (IntBound) |
| DSM (253 diags, variable-width) | 1,271 | Diagonal integer sums (**load-bearing** per B.62k) |
| XSM (253 anti-diags, variable-width) | 1,271 | Anti-diagonal integer sums (redundant per B.62j) |
| LH (CRC-32, 127 rows) | 4,064 | Phase II row verification |
| VH (CRC-32, 127 cols) | 4,064 | GaussElim column axis |
| DH (CRC-32, shortest 64 diags) | 2,048 | GaussElim diagonal axis (partial coverage) |
| DH (CRC-32, all 253 diags) | varies | ~4,400 bits (hybrid-width) |
| XH (CRC-32, shortest 64 anti-diags) | 2,048 | GaussElim anti-diagonal axis (partial coverage) |
| rLTP CRC (1 sub-table, lines 1&ndash;16) | 192 | Interior cascade triggers (CRC-8 + CRC-16) |
| rLTP CRC (1 sub-table, lines 1&ndash;32) | 704 | Extended interior triggers |
| BH (SHA-256) | 256 | Final verification |
| DI | 8 | Disambiguation index |

**Configurations to test:**

| Config | Components | Est. $C_r$ | Hypothesis |
|--------|-----------|------------|------------|
| A1 | LSM + DSM + LH + VH + BH/DI | 69.0% | Baseline: geometric sums + row/col CRC only. No DH/XH/rLTP. |
| A2 | A1 + DH64 (CRC-32, 64 shortest diags) | 81.7% | Add diagonal CRC on short diags. |
| A3 | A1 + DH64 + XH64 | 94.4% | Add both diagonal axes (short coverage). |
| A4 | A1 + DH64 + 1 rLTP (lines 1&ndash;16) | 83.0% | Diagonal CRC + rLTP interior triggers, no XH. |
| A5 | A1 + DH64 + 1 rLTP (lines 1&ndash;32) | 86.1% | Extended rLTP, no XH. |
| A6 | A1 + DH32 (CRC-16, 32 diags) + XH32 (CRC-16) + 1 rLTP (1&ndash;16) | 77.5% | CRC-16 diags (cheaper) + rLTP. |
| A7 | LSM + DSM + LH + DH64 + 1 rLTP (1&ndash;16) + BH/DI | 77.0% | Drop VH, rely on rLTP + DH for column info. |
| A8 | LSM + DSM + LH + VH + DH128 + XH128 + BH/DI | ~88% | Closest to B.60r (the known full-solve config). |

Configurations A1&ndash;A8 span $C_r$ from 69% to 94%. Each trades payload bits for Phase I strength.
The sweep identifies the Pareto frontier: maximum determination at minimum $C_r$.

**Key design principles from B.62:**
- **DSM is load-bearing** (B.62k): always included. XSM is redundant (B.62j): excluded unless testing.
- **rLTP cross-sums are dead weight** (B.62g): only CRC hashes included, no integer sums.
- **LH is needed for Phase II** (row verification): always included in hybrid configs.
- **CRC width matters** (B.62i): CRC-8/16 on short lines is efficient; CRC-32 on long lines is costly but provides rank.

**Method.** For each configuration A1&ndash;A8: run on MP4 blocks 0&ndash;5 at S=127. Record per-block
$|D|$, per-row unknown count distribution (min, max, mean, P90), GaussElim cells, IntBound cells.
Report results as a comparison table. Identify the Pareto-optimal configuration for B.63b.

**Results.** Tested on MP4 blocks 0&ndash;5 (plus blocks 10, 20, 50, 100 for density profiling).

Block density profile at S=127:
- Block 0: low density (favorable, A1 determines 21.9%)
- Block 1: low density (favorable, A1 determines 24.2%)
- Block 2: very favorable (A1 achieves 100% BH-verified full solve)
- Block 3: moderate density (A1 determines 47.4%)
- Block 4: near-50% density (A1 determines 4.4%)
- Blocks 5+: 50% density (A1 determines 0.1%)

Phase I determination (%) by config and block:

| Config | Est. $C_r$ | Block 0 | Block 1 | Block 3 | Block 4 | Block 5 |
|--------|-----------|---------|---------|---------|---------|---------|
| A1 (LH+VH only) | ~69% | 21.9 | 24.2 | 47.4 | 4.4 | 0.1 |
| A2 (A1+DH64) | ~82% | **100** | **100** | **100** | 15.7 | 13.1 |
| A4 (A2+rLTP16) | ~85% | **100** | **100** | **100** | 16.5 | 13.9 |
| A6 (DH32/XH32 CRC-16+rLTP16) | ~78% | 46.1 | 55.6 | **100** | 16.5 | 13.9 |
| A7 (no VH, DH64+rLTP16) | ~52% | 37.1 | 50.8 | 56.9 | 16.5 | 13.9 |
| A8 (DH128+XH128, ~B.60r) | ~88% | **100** | **100** | **100** | 15.7 | 13.1 |

Block 2 omitted (100% BH-verified on every config including A1 &mdash; anomalously favorable).

**Key findings.**

1. **A2 (DH64 at $C_r \approx 82\%$) is the Pareto optimum for favorable blocks.** It achieves
   100% BH-verified full solve on blocks 0, 1, 2, and 3 &mdash; identical to A8 ($C_r \approx 88\%$)
   at 6% lower $C_r$. Adding rLTP (A4, A5) or XH (A3) provides no improvement on any block.

2. **50% density blocks are intractable for all configs.** Blocks 4&ndash;5 achieve 13&ndash;17%
   determination regardless of config. IntBound contributes zero cells. GaussElim determines only
   short-line CRC cells. Average unknowns per row: 106&ndash;121. Phase II DFS is impossible.

3. **A6 and A7 are too weak.** CRC-16 diagonal hashes (A6) and dropping VH (A7) lose the
   cascade on blocks 0 and 1. At 46&ndash;56% determination, rows average 56&ndash;99 unknowns
   &mdash; intractable for DFS.

4. **VH is load-bearing at S=127.** A7 (no VH) drops from 100% to 51&ndash;57% on blocks 0&ndash;1.
   VH provides the column-axis GaussElim equations that complete the cascade.

5. **The hybrid adds no value at S=127.** Favorable blocks (0&ndash;3) achieve full algebraic
   solves at $C_r \approx 82\%$. Unfavorable blocks (4+) have 106+ unknowns per row &mdash;
   intractable for any DFS. There is no intermediate regime where Phase I partially solves and
   Phase II handles the residual.

**Pareto frontier:**

| $C_r$ | Favorable blocks solved | 50% blocks solved | Config |
|-------|------------------------|-------------------|--------|
| ~69% | 1/5 (block 2 only) | 0/5 | A1 |
| ~78% | 2/5 (blocks 2, 3) | 0/5 | A6 |
| ~82% | 4/5 (blocks 0&ndash;3) | 0/5 | **A2** |
| ~88% | 4/5 | 0/5 | A8 |

**Conclusion.** At S=127, A2 (DH64 cascade, $C_r \approx 82\%$) is the Pareto-optimal Phase I
config: full BH-verified algebraic solve on 4/5 tested favorable blocks. However, this assessment
is based on only 10 blocks from a ~1,330-block file. The majority of blocks (video data at ~50%
density) achieve only 13% determination. A full-file assessment (B.63b) is required before
drawing conclusions about hybrid viability.

**Status: COMPLETE. A2 identified as Pareto-optimal Phase I config. Full-file assessment needed (B.63b).**

### B.63b: Full-File Decompression Assessment at S=127

**Prerequisite.** B.63a completed (A2 identified as Pareto-optimal Phase I config).

**Objective.** Run the A2 combinator (Phase I) on ALL ~1,330 blocks of the MP4 test file at S=127.
Classify every block by Phase I determination level and per-row unknown distribution. Then run the
full hybrid pipeline (Phase I + Phase II row-by-row DFS with LH CRC-32 verification, cross-row
backtracking, BH final verification) on blocks that Phase I does not fully solve. The goal is
complete file decompression &mdash; every block BH-verified &mdash; not partial success on
favorable blocks.

**Architecture.** Identical to B.62m but at S=127:

**Phase I:** CombinatorSolver solveCascade() with B.63a&rsquo;s optimal config. Determines $N$
cells algebraically.

**Phase II:** Process rows sequentially (sorted by unknowns ascending). For each row:
1. DFS over unknown cells with IntBound feasibility pruning.
2. LH CRC-32 verification at row completion (~256 valid candidates per row at 38 unknowns).
3. On success: continue to next row. On failure: backtrack to previous row, try next LH candidate.
4. After all rows complete: verify SHA-256 BH.
5. On BH failure: backtrack to last row, try next candidate. Propagate backtracking as needed.

At S=127, per-row DFS with 20&ndash;38 unknowns should complete in **seconds** (vs minutes at S=191).
Backtracking across ~256 LH candidates per row is tractable. The full search tree is bounded by
the product of LH candidates across rows, pruned by cross-constraint propagation.

**Expected runtime.** At 127 cells per row and $\leq 38$ unknowns, the per-row DFS with IntBound
pruning explores $O(2^{20})$ to $O(2^{38})$ states. With effective pruning (IntBound cuts branches
early), the practical exploration is orders of magnitude less. Estimated: 1&ndash;10 seconds per row
attempt, with ~256 candidates per row and ~127 rows to solve. Worst-case: hours. Expected: minutes
if cross-constraint propagation prunes early.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (hybrid solves all blocks) | All 6 blocks achieve $|D| = 16{,}129$, BH verified | The hybrid breaks the 50% density wall at S=127 under $C_r < 90\%$ |
| H2 (hybrid solves favorable blocks) | Block 1&ndash;2 solve; blocks 3&ndash;5 (50% density) stall | Phase I determination insufficient for 50% density row DFS |
| H3 (backtracking intractable) | Search runs for hours without converging | LH CRC-32 provides insufficient disambiguation; too many candidates per row |
| H4 (hybrid + re-invocation cascades) | After a few rows by DFS, combinator re-invocation solves remaining rows algebraically | The row-search + fixpoint cycle creates a cascading solve |

**Results (Phase I full-file sweep, A2 config, all 1,331 blocks).**

| Category | Blocks | % of file |
|----------|--------|-----------|
| Full solve (BH verified) | 5 | 0.4% |
| Partial ($> 50\%$) | 0 | 0% |
| Low ($1\text{--}50\%$) | 1,326 | 99.6% |
| Near-zero ($\leq 1\%$) | 0 | 0% |
| **Average determination** | **13.4%** | |

The 5 fully-solved blocks are the file header/metadata (first ~10 KB). The remaining 1,326 blocks
are video payload at ~50% density, each achieving ~13.1% determination (the GaussElim short-diagonal
floor). Sampled blocks at indices 100, 200, ..., 1300 all show identical 13.1% determination.

**Distribution is bimodal:** blocks are either 100% (favorable header) or ~13% (50% density payload).
Zero blocks fall in an intermediate range ($> 50\%$) where Phase I partially closes the matrix.

**Phase II assessment for the 1,326 unsolved blocks.** At 13.1% determination (2,112 cells on a
16,129-cell block), each row has on average $127 \times 0.869 \approx 110$ unknowns. Per-row DFS
at 110 unknowns is $O(2^{110})$ &mdash; intractable even with aggressive IntBound pruning.

**The 50% density wall at S=127.** The combinator fixpoint at 50% density hits the same structural
barrier observed at S=191 (B.62 series):
- IntBound contributes zero cells ($\rho \approx u/2$ on every line)
- GaussElim determines only cells on short lines ($\leq 32$ cells) where CRC rank equals line length
- Adding equations on longer lines increases GF(2) rank but every new pivot depends on free variables
- The 2,112-cell floor corresponds to the 253 shortest diagonals fully determined by CRC

**Open question: is the hybrid truly unable to help at 50% density?** The current Phase II approach
(row-by-row DFS) cannot handle 110 unknowns per row. However, alternative Phase II strategies
exist that have not been tested:

1. **Cell-by-cell DFS with constraint propagation** (the existing production DFS at S=511):
   instead of completing entire rows before verifying, assign cells one at a time with full
   propagation across all constraint lines. This leverages the 2,112 pre-determined cells as
   additional constraints that the DFS-from-scratch solver does not have.

2. **Combinator re-invocation after each cell** (not each row): if completing a row is intractable,
   completing a single cell and re-running the fixpoint may cascade. The combinator determined 2,112
   cells from CRC alone. Assigning one additional cell in the right location may tip a constraint
   line into forcing ($\rho = 0$ or $\rho = u$), cascading further assignments.

3. **Targeted cell search**: instead of processing rows sequentially, identify the single cell
   whose assignment would create the largest constraint cascade. This transforms Phase II from
   a row-completion problem into a cell-selection problem.

These alternatives represent fundamentally different hybrid architectures than the row-by-row DFS
tested in B.62m. They are not yet evaluated.

**Status: COMPLETE (Phase I sweep). 5/1,331 blocks fully solved. 1,326 blocks at 13% determination.
Full-file decompression not achieved. Multi-axis DFS approach proposed (B.63c).**

### B.63c: Multi-Phase Multi-Axis Solver at S=127

**Prerequisite.** B.63a (A2 config identified), B.63b (full-file Phase I sweep: 5/1,331 solved).

**Motivation.** B.63b showed that Phase I (combinator algebra) determines 2,112 cells (13%) on
50% density blocks, and the row-by-row DFS of B.62m cannot handle 110 unknowns per row. However,
the solver has verification hashes on FOUR geometric axes (LH for rows, VH for columns, DH for
diagonals, XH for anti-diagonals). Each axis provides an independent DFS search direction with
its own CRC-32 verification at line completion. By cycling through all four axes and returning to
the combinator after each axis makes progress, the solver can chip away at the unknowns from
multiple directions.

**Architecture.** Five phases, executed in a loop until no progress.

**Phase I &mdash; Combinator Algebra.** Run GaussElim + IntBound + CrossDeduce fixpoint to
convergence. Determines cells algebraically from CRC equations and integer constraints. This is
the existing `solveCascade()`. Output: $N$ determined cells.

**Phase II &mdash; Row DFS (LH verification).** For each row $r$ with unknowns: perform cell-by-cell
DFS over the unknown cells in the row, using IntBound feasibility pruning on all constraint lines
through each cell. When all unknowns in the row are assigned, verify LH (CRC-32). If LH passes,
accept the row. If LH fails, backtrack within the row. Process rows in order of fewest unknowns
first. If ANY row is solved, return to Phase I (the new assignments may cascade).

**Phase III &mdash; Column DFS (VH verification).** Same as Phase II but for columns. For each
column $c$ with unknowns: DFS over unknown cells in the column, IntBound pruning, verify VH
(CRC-32) at column completion. Process columns by fewest unknowns first. If any column is solved,
return to Phase I.

**Phase IV &mdash; Diagonal DFS (DH verification).** Same pattern for diagonals. For each
non-toroidal diagonal $d$ with unknowns: DFS over unknown cells on the diagonal, verify DH (CRC)
at diagonal completion. Short diagonals (length $\leq 32$) are the most tractable targets. Process
by fewest unknowns first. If any diagonal is solved, return to Phase I.

**Phase V &mdash; Anti-Diagonal DFS (XH verification).** Same pattern for anti-diagonals using
XH verification.

**Outer loop.** Repeat Phases I &rarr; II &rarr; III &rarr; IV &rarr; V until a full iteration
produces zero new determined cells. At each phase transition, log:
- Phase number and axis
- Number of lines attempted
- Number of lines solved
- Total cells determined before and after
- Whether progress was made (triggers restart to Phase I)

**Key insight: cross-axis coupling.** Solving a row determines 127 cells. Those cells participate
in 127 columns, up to 127 diagonals, and up to 127 anti-diagonals. Each solved row reduces unknown
counts on hundreds of cross-axis lines. A diagonal that had 110 unknowns before Phase II might have
80 after several rows are solved &mdash; and after Phase III solves some columns, that diagonal
might drop to 40 unknowns, making Phase IV tractable.

The multi-axis cycling creates a feedback loop: rows help columns, columns help diagonals,
diagonals help anti-diagonals, and all of them feed back into the combinator (Phase I) which
may find new GaussElim pivots from the tightened constraints.

**Configuration.** LSM + VSM + DSM + LH + VH + DH64 + XH64 + 1 rLTP (lines 1&ndash;16) + BH/DI.
All four hash axes present for verification. $C_r \approx 88.8\%$ (14,325 / 16,129 bits). XSM
integer sums dropped (redundant per B.62j). This configuration and $C_r$ applies to all
subsequent experiments (B.63d through B.63g) which differ only in search strategy.

Previously estimated $C_r \approx 94\%$ (DH64 + XH64 adds anti-diagonal coverage
not present in the A2 baseline; needed for Phase V).

Actually, A3 from B.63a provides both DH64 and XH64. If $C_r$ is too high, DH and XH can be
trimmed to fewer diagonals or use CRC-16 instead of CRC-32.

**DFS tractability per axis.** A line is DFS-tractable when its unknown count is $\leq 40$.
At 13% initial determination:
- Rows: 127 cells, ~110 unknowns. NOT tractable initially.
- Columns: 127 cells, ~110 unknowns. NOT tractable initially.
- Short diagonals: length 1&ndash;32, 0&ndash;28 unknowns. **TRACTABLE** (many already fully
  determined by Phase I CRC).
- Medium diagonals: length 33&ndash;64, ~29&ndash;56 unknowns. **MARGINAL** after Phase I.
- Long diagonals: length 65+, ~57&ndash;110 unknowns. NOT tractable initially.

The expected entry point is **Phase IV on short/medium diagonals**. Phase I already determines
the shortest diagonals. Phase IV attempts the next tier (length 33&ndash;64) which may have
28&ndash;56 unknowns after Phase I. If even a few medium diagonals are solved, Phase I re-invocation
may cascade, reducing unknowns on adjacent rows/columns, making Phases II/III tractable.

**Logging.** Each phase iteration outputs a JSON record:

```json
{
  "round": 1,
  "phase": "IV",
  "axis": "diagonal",
  "lines_attempted": 64,
  "lines_solved": 12,
  "cells_before": 2112,
  "cells_after": 2844,
  "progress": true
}
```

All records accumulated into `tools/b63c_log.json` for post-analysis.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (multi-axis solves all) | Block reaches $|D| = 16{,}129$, BH verified | The four-axis cycling creates sufficient cross-coupling to close the matrix |
| H2 (partial progress) | $|D|$ increases beyond 2,112 but does not reach 100% | Multi-axis DFS extends determination but eventually stalls; identifies the limiting axis |
| H3 (diagonal entry only) | Phase IV solves diagonals, cascades into Phase I, but Phases II/III still intractable | Short diagonals are the only DFS-tractable entry point; row/column unknowns remain too high |
| H4 (no progress) | Zero lines solved across all phases | Even short/medium diagonals cannot be completed by DFS at 50% density |

**Method.**

(a) Implement `solveHybrid()` as a multi-phase loop with per-axis DFS and combinator re-invocation.
Each axis DFS uses the existing `assignCell`/`unassignCell` infrastructure with IntBound feasibility
checking. Line verification uses the appropriate CRC hash (LH for rows, VH for columns, DH for
diagonals, XH for anti-diagonals).

(b) Run on one 50% density block (block 5) first. Record per-phase logs. Assess whether any axis
makes progress.

(c) If progress is made, run on all 1,326 unsolved blocks and record solve rates.

**Implementation notes.** Per-line DFS uses a 50M node limit to prevent infinite search on
intractable lines. The DFS unknown limit is set to 127 (full row length) since the node limit
provides the time bound. All four CRC axes are precomputed at construction: LH (CRC-32 per row),
VH (CRC-32 per column), DH (graduated CRC per diagonal), XH (graduated CRC per anti-diagonal).

**Results (block 5, 50% density).**

| Round | Phase | Lines solved | Cells gained | Total determined |
|-------|-------|-------------|-------------|-----------------|
| 1 | I (combinator) | &mdash; | 2,248 | 2,248 (13.9%) |
| 1 | II (rows, LH) | 0 | 0 | 2,248 |
| 1 | III (cols, VH) | 0 | 0 | 2,248 |
| 1 | IV (diags, DH) | 0 | 0 | 2,248 |
| 1 | **V (anti-diags, XH)** | **2** | **+157** | **2,405 (14.9%)** |
| 2 | I (combinator) | &mdash; | 0 | 2,405 |
| 2 | II (rows, LH) | 0 | 0 | 2,405 |
| 2 | III (cols, VH) | 0 | 0 | 2,405 |
| 2 | IV (diags, DH) | 0 | 0 | 2,405 |
| 2 | V (anti-diags, XH) | 0 | 0 | 2,405 |
| | **Stalled** | | | **2,405 / 16,129 (14.9%)** |

**Outcome: H2 (partial progress).**

1. **First-ever progress beyond the short-diagonal floor on a 50% density block.** Phase V
   (anti-diagonal DFS with XH CRC verification) solved 2 anti-diagonals, gaining 157 cells.
   The pure combinator floor at 50% density was 2,248 cells (B.63a A4); B.63c reached 2,405
   (+7.0%). This is the first time ANY solver in the B.60&ndash;B.63 series has extended
   determination on a 50% density block beyond the algebraic floor.

2. **The progress came from anti-diagonals, not diagonals.** Phase IV (diagonal DFS with DH)
   solved zero lines. Phase V (anti-diagonal DFS with XH) solved 2. This asymmetry mirrors
   the DSM/XSM asymmetry from B.62k/l but inverted &mdash; here the anti-diagonal axis provided
   the DFS entry point while the diagonal axis did not.

3. **Stalled after 1 round of progress.** The 157 new cells from the 2 solved anti-diagonals
   did not reduce unknown counts on other axes enough to open new lines. Phase I re-invocation
   found no new GaussElim pivots. All subsequent phases found zero solvable lines.

4. **Row and column DFS never fired.** At 14.9% determination, rows and columns still have
   ~108 unknowns each. The node limit (50M) prevents these from consuming time, but the DFS
   cannot solve them. Cross-axis coupling from 2 anti-diagonals is insufficient to bring
   row/column unknowns below the tractable threshold.

5. **The node limit (50M) is effective.** Without it, the solver ran for 12+ hours stuck on a
   single diagonal. With it, the full 5-phase cycle completes in ~10 minutes, allowing the
   cycling mechanism to operate.

**Analysis: what would it take to sustain progress?**

The 2 solved anti-diagonals had favorable unknown counts (likely $\leq 30$) after Phase I.
The remaining anti-diagonals have 40&ndash;110 unknowns, exceeding the practical DFS threshold
even with the 50M node limit. To sustain the multi-axis cycling:

- Each round must solve enough lines to reduce unknowns on cross-axis lines by 10&ndash;20 cells.
- At 127 cells per row, reducing from 108 to 88 unknowns requires ~20 additional determined cells
  per row. That requires solving ~20 cross-axis lines that pass through each row &mdash; far more
  than the 2 anti-diagonals achieved.

The cross-axis coupling coefficient is too low: 2 solved anti-diagonals with 157 cells spread
across 127 rows averages ~1.2 cells per row. Rows need ~20 additional cells each.

**Significance.** B.63c demonstrates that multi-axis DFS can extend determination beyond the
algebraic floor at 50% density. The mechanism works &mdash; Phase V found solvable lines that
no single-axis approach could find. But the coupling between axes is too weak for sustained
progress at this density. Stronger Phase I (more rLTP lines, higher $C_r$) or more DFS-tractable
line families (shorter lines, lower unknown counts) would be needed to sustain the cycling.

**Status: COMPLETE. H2 confirmed: +157 cells (14.9%) on 50% density block. First progress beyond algebraic floor. Stalled after 1 round.**

### B.63d: Residual-Guided Cell-by-Cell DFS at S=127

**Prerequisite.** B.63c completed (multi-axis cycling gains +157 cells but stalls).

**Motivation.** B.63c&rsquo;s line-completion DFS requires solving ALL unknowns on a line before
CRC verification. At 50% density, most lines have 50&ndash;110 unknowns &mdash; intractable as a
single DFS problem. But each unknown cell participates in 4+ constraint lines (row, column,
diagonal, anti-diagonal, rLTP). The residuals ($\rho$, $u$) on those lines collectively constrain
the cell. Assigning a SINGLE cell and propagating via IntBound may tip other lines into forcing
($\rho = 0$ or $\rho = u$), cascading additional assignments without search.

B.63d changes the search granularity from line-completion to cell-assignment. Instead of solving
a 110-unknown row in one DFS, assign one cell at a time, propagate, and let the constraint system
do the work. This is the same architecture as the production DFS solver (S=511) but seeded with
the combinator&rsquo;s 2,248 pre-determined cells.

**Architecture.**

**Phase I:** Combinator algebra (solveCascade). Determines ~2,248 cells on 50% density blocks.

**Phase II:** Cell-by-cell DFS with residual-guided ordering and IntBound propagation.

1. **Score unknown cells.** For each unknown cell, compute a constraint score based on its
   participating lines. Cells on lines with few remaining unknowns ($u$ close to 0) are most
   likely to trigger IntBound forcing when assigned. Score = $\sum_{L \ni \text{cell}} 1/u(L)$.

2. **Select the highest-scoring cell** (most constrained).

3. **Branch: try 0, then 1.** For the selected cell, assign value 0. Check IntBound feasibility
   on all lines through the cell ($0 \leq \rho' \leq u'$). If feasible:

4. **Propagate.** Scan all constraint lines. For any line with $\rho = 0$: force all remaining
   unknowns to 0. For any line with $\rho = u$: force all remaining unknowns to 1. Each forced
   cell triggers further propagation on its lines. Continue until quiescence or infeasibility.

5. **Verify at boundaries.** When a row completes ($u_{\text{row}} = 0$), verify LH (CRC-32).
   If LH fails, the current search path is wrong &mdash; backtrack.

6. **Backtrack on infeasibility.** If propagation detects $\rho < 0$ or $\rho > u$ on any line,
   or LH verification fails: undo all assignments from step 3 onward, try value 1 for the
   selected cell. If both values fail, backtrack to the previous cell decision.

7. **Verify at completion.** When all 16,129 cells are assigned, verify BH (SHA-256). If BH
   fails, backtrack.

8. **Track search state.** Log each decision (cell, value, depth), propagation cascades,
   LH verifications, and backtracks.

**Key difference from B.63c:** B.63c required completing an entire line (50&ndash;110 unknowns)
before ANY verification. B.63d gets verification feedback after EACH cell assignment via IntBound
propagation, and at row boundaries via LH. The search tree is deeper (one level per unknown cell)
but much narrower (propagation prunes aggressively).

**Key difference from production DFS:** The production solver starts from cell (0,0) and proceeds
in row-major order. B.63d starts from the combinator&rsquo;s partial solution and uses
residual-guided cell ordering (most-constrained-first). The 2,248 pre-determined cells tighten
all constraint lines before the search begins.

**Results (block 5, 50% density).**

Phase VI (cell-by-cell DFS) was reached after B.63c&rsquo;s multi-axis phases stalled at 2,405
cells (14.9%). The cell-by-cell DFS used residual-guided cell selection (most-constrained-first)
with IntBound propagation after each assignment and LH verification at row boundaries.

| Metric | Value |
|--------|-------|
| Starting determination (from Phase I + B.63c) | 2,405 (14.9%) |
| Peak determination | **3,682 (22.8%)** |
| DFS decisions (first 500, 0 backtracks) | +1,277 cells via propagation cascade |
| DFS plateau depth | ~660 cells |
| Total decisions (25 min) | ~19.3M |
| Total backtracks | ~19.3M |
| Final determination | 3,680 (22.8%) &mdash; **stuck at plateau** |

**Progression:**

| Decisions | Backtracks | Depth | Determined | % |
|-----------|-----------|-------|------------|---|
| 500 | 0 | 500 | 3,370 | 20.9% |
| 1,000 | 347 | 653 | 3,673 | 22.8% |
| 1,500+ | ~equal to decisions | ~660 | ~3,680 | 22.8% |
| 19,300,000 | 19,300,000 | ~661 | 3,681 | 22.8% |

**Outcome: plateau at 22.8%.**

1. **+1,275 cells beyond the multi-axis floor.** The cell-by-cell DFS with IntBound propagation
   extended determination from 14.9% to 22.8% in the first 500 decisions (milliseconds). The
   combinator&rsquo;s pre-determined cells tightened constraints enough for the DFS to cascade
   ~615 additional cells via IntBound forcing.

2. **Plateau at depth ~660.** After the initial 500 easy decisions, every subsequent decision
   backtracks almost immediately. The search oscillates at depth 650&ndash;662 with no viable
   path forward. This is the same constraint-exhaustion plateau observed in the production DFS
   at S=511 (depth ~96K out of 261K cells, i.e., ~37%).

3. **The DFS plateau is at 22.8% (S=127) vs 37% (S=511, production).** The lower percentage
   reflects the different constraint configuration (fewer LTP families, different C_r) and the
   combinator seed advantage. The production DFS at S=511 uses 4 LTP families + SHA-1 per row.

4. **19.3M decisions in 25 minutes with zero progress beyond depth 662.** The backtrack rate
   is ~1 backtrack per decision &mdash; pure thrashing. The search tree at depth 662 is
   completely exhausted within the IntBound pruning envelope.

**Analysis.** The cell-by-cell DFS achieves the same outcome as the production DFS: rapid
initial progress via propagation, then a hard plateau where constraint lines are too balanced
($\rho \approx u/2$) for IntBound to force. The residual-guided cell ordering helps reach
the plateau faster (500 decisions vs thousands) but does not change the plateau depth.

The fundamental limitation is that IntBound can only force cells on lines where $\rho = 0$ or
$\rho = u$. At 50% density, after the initial cascade, all remaining lines have $\rho$ well
within the interior of $[0, u]$. No single cell assignment tips any line into forcing. The
search must guess multiple cells correctly to create a forcing cascade, which requires
exploring $O(2^k)$ states where $k$ is the number of &ldquo;gap&rdquo; cells between forcing
events.

**Status: COMPLETE. Cell-by-cell DFS extends 14.9% &rarr; 22.8% (+7.9%), then plateaus.
Same constraint-exhaustion wall as production DFS. 19.3M decisions, zero progress beyond depth 662.**

### B.63f: CDCL (Conflict-Driven Clause Learning) at S=127

**Prerequisite.** B.63d completed (cell-by-cell DFS plateaus at 22.8%).

**Baseline.** B.63c multi-axis solver: 2,405 / 16,129 determined (14.9%). B.63d cell-by-cell DFS
extended to 3,682 (22.8%) then thrashed &mdash; 19.3M decisions with zero progress beyond depth 662.
The DFS backtracks chronologically (one level at a time) even when the root cause of infeasibility
is hundreds of levels earlier.

**Motivation.** B.63d&rsquo;s DFS thrashes because it backtracks chronologically. When a conflict
occurs at depth 662, the problematic decision may be at depth 50 &mdash; but the DFS tries all
$2^{612}$ combinations between depth 50 and 662 before reaching it. CDCL addresses this by:

1. **Conflict analysis:** When infeasibility is detected, analyze the implication graph to identify
   the SUBSET of decisions that caused the conflict (the conflict clause).
2. **Clause learning:** Record the conflict clause so the same combination is never tried again.
3. **Non-chronological backjumping:** Backtrack directly to the earliest decision in the conflict
   clause, skipping hundreds of irrelevant intermediate levels.
4. **Restarts:** Periodically restart the search from depth 0 using accumulated clauses to prune.

CDCL is the architecture used by modern SAT solvers (MiniSat, CaDiCaL) which routinely solve
problems with millions of variables. B.21 previously attempted CDCL at S=127 but on the RAW
constraint system (bit-blasting CRC equations into CNF, producing millions of clauses). B.63e
differs fundamentally: the CDCL operates on the INTEGER constraint system (IntBound on cross-sum
lines), not on bit-blasted CRC. The variables are cell values (0/1), and the constraints are
cardinality bounds ($0 \leq \rho \leq u$ per line). Conflict analysis identifies which cell
assignments collectively violated a cardinality constraint.

**Architecture.**

Starting from B.63c&rsquo;s 2,405 determined cells:

1. **Unit propagation** = IntBound forcing ($\rho = 0 \Rightarrow$ all 0; $\rho = u \Rightarrow$ all 1).
2. **Decision:** Select the most constrained unassigned cell. Assign a value.
3. **Propagation:** Run IntBound. If forcing cascades, record implications.
4. **Conflict detection:** If $\rho < 0$ or $\rho > u$ on any line, a conflict has occurred.
5. **Conflict analysis:** The conflict clause is the set of decision-level assignments on the
   conflicting line. For a line $L$ with cells $\{c_1, \ldots, c_n\}$ where the cardinality
   constraint is violated: the clause is $\neg(c_{d_1} = v_1 \wedge \ldots \wedge c_{d_k} = v_k)$
   where $d_1, \ldots, d_k$ are the decision cells (not propagated cells) on line $L$.
6. **Backjump:** Return to the second-highest decision level in the conflict clause.
7. **Learn:** Add the conflict clause to a clause database. Future search avoids this combination.
8. **LH verification:** At row completion, verify CRC-32. Treat LH failure as a conflict and
   learn the clause.
9. **Restart:** After $N$ conflicts, restart from depth 0. Learned clauses persist across restarts.

**Key metric.** The backjump distance: how many levels does CDCL skip compared to chronological
backtracking? If the average backjump is 100+ levels, CDCL should escape the depth-662 plateau
that trapped B.63d&rsquo;s DFS.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (CDCL breaks plateau) | Determination exceeds 22.8%, potentially reaches 100% | Non-chronological backjumping escapes the local trap; clause learning prunes exponentially |
| H2 (CDCL extends modestly) | 22.8% &lt; determination &lt; 50% | Backjumping helps but clause database grows too large or conflicts remain shallow |
| H3 (CDCL same as DFS) | Determination &asymp; 22.8% | Conflicts at the plateau are caused by the IMMEDIATE decision (depth 661&ndash;662), not distant ones; backjumping gains nothing |

**Method.** Implement a CDCL solver operating on the IntBound constraint system within
`CombinatorSolver`. Track decision levels and propagation reasons per cell. On conflict, analyze
the conflicting IntLine to produce a clause, backjump non-chronologically, and learn the clause.
Restarts every 10K decisions with learned clauses persisting. 200 restarts, 5M total decisions.

**Results (block 5, 50% density).**

| Metric | B.63e Run 2 (random restarts) | B.63f (CDCL) |
|--------|------------------------------|-------------|
| Best peak | **15,453 (95.8%)** | 15,230 (94.4%) |
| Mean peak | **14,875 (92.2%)** | 14,590 (90.4%) |
| Decisions/restart | 5,000,000 | 10,000 |
| Restarts | 1,000 | 200 |
| Total decisions | 5,000,000,000 | 2,000,000 |
| Clauses learned | N/A | **0** |
| Conflicts | N/A | 1,368,773 |

**Outcome: H3 (CDCL same as DFS) &mdash; implementation gap.**

1. **Zero clauses learned.** The conflict analysis collected ALL assigned cells on the conflicting
   IntLine (~100 cells at 90%+ determination). Every clause exceeded the 20-literal size limit.
   Without clause learning, the CDCL reduces to a plain random-restart DFS with backjumping.

2. **Smaller restart budget hurt performance.** The 10K-decision restart interval (needed to
   exercise restarts with clause accumulation) gave each restart less exploration time than
   B.63e&rsquo;s 500K&ndash;5M budget. Peak determination was lower (94.4% vs 95.8%).

3. **The backjumping mechanism fired but was ineffective.** Non-chronological backjumping skipped
   intermediate levels, but without clause learning to prune future exploration, the same
   conflicts were encountered repeatedly.

**Root cause:** The conflict analysis is too coarse. Collecting all assigned cells on the
conflicting line produces clauses of size ~100. A proper first-UIP (Unique Implication Point)
resolution scheme would trace propagated cells back through their IntBound reasons to identify
the minimal set of DECISION cells that caused the conflict, producing clauses of size 5&ndash;15.
This requires a full implication graph traversal that the current implementation does not perform.

**Required for effective CDCL:**
- First-UIP conflict analysis: resolve propagated cells to their decision antecedents
- Two-watched-literal scheme for efficient clause propagation
- Activity-based variable selection (VSIDS) instead of random ordering
- Clause database management (deletion of large/inactive clauses)

These are standard SAT solver components but represent substantial engineering beyond the
current research prototype. The B.63e random-restart approach achieves 95.8% without clause
learning and is the better baseline for further work.

**Run 2: First-UIP resolution (500 restarts, 100K decisions each, clause limit 50).**

| Metric | Run 1 (coarse analysis) | Run 2 (first-UIP) |
|--------|------------------------|--------------------|
| Best peak | 15,230 (94.4%) | 15,230 (94.4%) |
| Mean peak | 14,590 (90.4%) | 14,527 (90.0%) |
| Clauses learned | 0 | **0** |
| Restarts | 200 | 500 |

First-UIP resolution did not help. Zero clauses learned even with proper resolution.

**Root cause: cardinality constraint antecedents are inherently large.** When IntBound forces
cell $c$ to 0 via $\rho = 0$ on line $L$, the &ldquo;reason&rdquo; is ALL 1-valued cells on $L$
(&sim;64 cells on a 127-cell line at 50% density). Resolution replaces one large antecedent
with another from a different line. The first-UIP clause still contains 50&ndash;100 literals
because every resolution step adds as many literals as it removes.

This is a known limitation of standard CDCL on cardinality constraints. Boolean clauses have
2&ndash;3 literals, so resolution produces small learned clauses. Cardinality constraints
involve $O(s)$ cells per line, so learned clauses are $O(s)$. Specialized techniques
(pseudo-Boolean reasoning, cutting planes) are needed for effective learning on cardinality
constraints, but these are a fundamentally different solver architecture.

**Status: COMPLETE. CDCL ineffective on IntBound cardinality constraints &mdash; antecedents
too large for standard clause learning. First-UIP resolution confirmed insufficient.
Random restarts (B.63e: 95.8%) remain the best approach.**

### B.63g: Beam Search (Bounded BFS) at S=127

**Prerequisite.** B.63d completed.

**Baseline.** B.63c: 2,405 cells (14.9%). B.63d DFS: 3,682 cells (22.8%), plateaued.

**Motivation.** B.63d&rsquo;s DFS commits to a single path and backtracks when it fails. It found
ONE path to depth ~660, then thrashed. But there may be MANY paths to depth ~660 with different
cell assignments. Some of those alternative paths may extend deeper &mdash; the DFS never
discovers them because it&rsquo;s trapped in the chronological backtracking neighborhood of the
first path it found.

Beam search explores $K$ paths in parallel. At each depth, it maintains the top-$K$ partial
assignments ranked by a heuristic (e.g., maximum constraint tightness, minimum total residual
imbalance). The beam width $K$ controls the trade-off between exploration breadth and memory.

**Architecture.**

Starting from B.63c&rsquo;s 2,405 determined cells:

1. **Initialize beam** with a single state: the combinator&rsquo;s partial solution.
2. **Select the most constrained unknown cell** across all beam states (or per-state).
3. **Expand:** For each state in the beam, branch on the selected cell (value 0 and value 1).
   Run IntBound propagation on each branch. Discard infeasible branches.
4. **Score** each surviving state by a heuristic:
   - Total determined cells (more = better)
   - Sum of $|\rho - u/2|$ across all lines (larger = more forcing potential)
   - Number of lines with $\rho = 0$ or $\rho = u$ (more = imminent cascades)
5. **Prune:** Keep only the top-$K$ states. Discard the rest.
6. **Repeat** until a state reaches 100% or the beam is empty.
7. **LH verification** at row completion (discard states that fail LH).
8. **BH verification** at full completion.

**Memory:** Each state stores `cellState_[]` (16 KB) + `intLines_` rho/u (~10 KB). At $K = 1{,}000$
beams: ~26 MB. At $K = 10{,}000$: ~260 MB. Both tractable.

**Key metric.** Maximum determination achieved across any beam state after $N$ expansion steps.
Compare to B.63d&rsquo;s 22.8% ceiling.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (beam breaks plateau) | Any beam state exceeds 22.8%, potentially reaches 100% | Multiple-path exploration finds a viable extension that DFS missed |
| H2 (beam explores wider but not deeper) | All beam states plateau at ~22.8% | The plateau is not path-dependent; all paths converge to the same depth |
| H3 (beam collapses) | The beam quickly narrows to 1 state (all alternatives pruned) | IntBound propagation is deterministic enough that most branches are equivalent |

**Method.** Implement beam search within `CombinatorSolver`. Beam width $K = 1{,}000$.
Score by total determined cells. Branch on most-constrained cell. Run on block 5.

**Results (block 5, 50% density, K=1,000).**

| Step | Beam size | Best det. | Worst det. | Best ever |
|------|-----------|-----------|------------|-----------|
| 100 | 1,000 | 2,583 (16.0%) | 2,583 (16.0%) | 2,583 |
| 200 | 1,000 | 2,790 (17.3%) | 2,771 (17.2%) | 2,790 |
| 300 | 1,000 | 2,989 (18.5%) | 2,969 (18.4%) | 2,989 |
| 500 | 1,000 | 3,295 (20.4%) | 3,295 (20.4%) | 3,295 |
| 700 | 1,000 | 3,609 (22.4%) | 3,609 (22.4%) | 3,609 |
| 762 | **0** | &mdash; | &mdash; | **3,682 (22.8%)** |

**Outcome: H3 (beam collapses).**

1. **Peak: 3,682 (22.8%) &mdash; identical to B.63d single-path DFS.** The beam search provides
   zero benefit over a single DFS path. Both achieve exactly the same plateau.

2. **All 1,000 states converge.** By step 500, best=worst=3,295 &mdash; every beam state has
   the same determination. IntBound propagation is deterministic: given the same cell assignment
   at step $k$, propagation produces the same cascaded result in all states. Branching on one
   cell generates two candidates, but after propagation they converge to nearly identical states
   that receive the same score. The beam carries 1,000 copies of essentially the same solution.

3. **Beam empties at step 762.** At 22.8% determination, BOTH branches (0 and 1) fail feasibility
   for the next cell &mdash; all 1,000 identical states hit the same wall simultaneously.
   The beam provides no diversity to escape.

4. **IntBound propagation homogenizes the beam.** This is the fundamental limitation. Each cell
   assignment triggers a cascade that determines dozens of additional cells deterministically.
   The cascade overwhelms the single-cell branch difference. By the next branching step,
   the two branches have been homogenized by propagation into nearly identical states.

**Why random restarts succeed where beam search fails.** Random restarts (B.63e) work because
each restart uses a DIFFERENT cell ordering, which produces DIFFERENT propagation chains from
the start. The beam search branches within a single ordering &mdash; all branches follow the
same propagation path and converge. Restart diversity is across orderings (macroscopic);
beam diversity is across single-cell branches (microscopic, erased by propagation).

**Comparison:**

| Strategy | Peak det. | Mechanism |
|----------|-----------|-----------|
| B.63d (single DFS, MCF) | 22.8% | Single path, MCF ordering |
| **B.63g (beam K=1000)** | **22.8%** | 1000 paths converge to same plateau |
| B.63e (random restarts) | **95.8%** | Different orderings reach different plateaus |

**Status: COMPLETE. H3 confirmed: beam collapses due to IntBound propagation homogenization.
Peak identical to single-path DFS (22.8%). Random restarts (95.8%) confirmed superior.**

### B.63e: Random Restarts at S=127

**Prerequisite.** B.63d completed.

**Baseline.** B.63c: 2,405 cells (14.9%). B.63d DFS: 3,682 cells (22.8%), plateaued.

**Motivation.** B.63d reached depth ~660 in 500 decisions using residual-guided cell ordering
(most-constrained-first). The cell ordering determines the path through the search tree. Different
orderings explore different paths. If the plateau depth varies by ordering, the best ordering
among many restarts may reach deeper than any single ordering.

Random restarts are the simplest meta-strategy: run the DFS from the combinator base with a
different random cell ordering, collect the plateau depth, and repeat. This tests whether the
22.8% plateau is ordering-dependent or structural.

**Architecture.**

For $R$ restarts:

1. Start from B.63c&rsquo;s 2,405 determined cells.
2. Generate a random permutation of the ~13,724 unknown cells.
3. Run the cell-by-cell DFS using the random permutation as the cell ordering (instead of
   most-constrained-first). Apply IntBound propagation after each assignment.
4. Record the plateau depth (where backtracking begins) and total determined cells.
5. Restore to B.63c&rsquo;s state. Repeat with a new random permutation.

After $R$ restarts, report:
- Distribution of plateau depths (min, max, mean, P90)
- Best determination achieved across all restarts
- Whether any restart broke the 22.8% barrier
- Correlation between cell ordering properties and plateau depth

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (depth varies, some deeper) | Max plateau depth $>$ 700; some restarts reach $> 25\%$ | The plateau is ordering-dependent; there exist good orderings that extend deeper |
| H2 (depth is invariant) | All restarts plateau at ~660 $\pm$ 20 | The plateau is structural (constraint-exhaustion), not ordering-dependent |
| H3 (some restarts solve fully) | Any restart reaches 100% | The correct path exists but the original ordering missed it; random search finds it |

**Method.** Implement restart loop within `solveHybrid`. Each restart saves/restores the B.63c
base state and uses a seeded RNG for the cell permutation. 500K decisions max per restart.
Run $R = 100$ restarts on block 5 (50% density).

**$C_r = 88.8\%$ (shared across B.63c&ndash;g; configuration differs only in search strategy).**

**Run 1 results (block 5, 50% density, 100 restarts, 500K decisions/restart).**

| Metric | MCF (restart 0) | Random restarts (1&ndash;99) |
|--------|-----------------|----------------------------|
| Strategy | Most-constrained-first | Random cell permutation |
| Peak determination | **13,044 (80.9%)** | **mean 14,921 (92.5%), max 15,340 (95.1%)** |
| Plateau depth | 6,619 | mean ~6,800 |
| Decisions | 11,161 | mean ~8,800 |
| Backtracks | 4,542 | mean ~2,000 |
| Propagated cells | 1,138,963 | mean ~280,000 |
| Solved (100%) | No | No (0/99) |

Peak determination distribution (100 restarts):
- Minimum: 13,044 (80.9%) &mdash; the MCF ordering (worst)
- P10: ~14,500 (89.9%)
- Mean: 14,889 (92.3%)
- P90: ~15,200 (94.2%)
- Maximum: **15,340 (95.1%)** (restart 16) &mdash; **789 cells short of full solve**

**Outcome: H1 (depth varies, dramatically deeper).**

1. **The plateau is ordering-dependent, not structural.** Random cell orderings consistently
   reach 90&ndash;95% determination vs the MCF ordering&rsquo;s 80.9%. The 22.8% plateau
   reported in B.63d was an artifact of the MCF ordering combined with a limited decision
   budget &mdash; with 500K decisions, even MCF reaches 80.9%.

2. **Random orderings outperform most-constrained-first.** MCF is the WORST ordering tested.
   It commits to locally-optimal decisions that are globally suboptimal, creating early conflicts
   that require extensive backtracking (4,542 backtracks vs random&rsquo;s ~2,000). Random
   orderings avoid this systematic bias.

3. **95.1% on a 50% density block.** The best restart determined 15,340 / 16,129 cells &mdash;
   789 cells short of a full solve. This is a transformative result: B.63a showed the combinator
   alone determines 13.9% on this block. Random-restart DFS with IntBound propagation extends
   this to 95.1% &mdash; a **6.8x improvement**.

4. **No restart achieved 100%.** All 100 restarts stalled between 80.9% and 95.1%. The last
   ~800 cells resist all orderings. These are likely cells in the matrix interior where all
   constraint lines are deeply balanced and no cell assignment triggers forcing.

5. **The propagation cascade is the mechanism.** Each restart makes ~8,800 decisions but
   IntBound propagation determines ~280,000 cells (32x amplification). The DFS assigns a cell,
   propagation forces hundreds more, creating a virtuous cycle. Random orderings produce more
   effective early cascades because they distribute decisions across the matrix rather than
   concentrating on locally-constrained cells.

**Significance.** B.63e demonstrates that the 50% density wall is NOT a fundamental barrier for
cell-by-cell DFS with IntBound propagation. The combinator+DFS hybrid determines 95% of cells
on a 50% density block. The remaining 5% (~789 cells) is the true residual. With more restarts,
better orderings, or targeted search on the residual, a full solve may be achievable.

**Run 2 results (block 5, 50% density, 1,000 restarts, 5M decisions/restart).**

| Metric | Run 1 (100 &times; 500K) | Run 2 (1,000 &times; 5M) |
|--------|------------------------|--------------------------|
| Best peak | 15,340 (95.1%) | **15,453 (95.8%)** |
| Mean peak | 14,889 (92.3%) | 14,875 (92.2%) |
| P10 | 14,500 (89.9%) | 14,594 (90.5%) |
| P90 | 15,200 (94.2%) | 15,143 (93.9%) |
| Min (MCF) | 13,044 (80.9%) | 13,044 (80.9%) |
| Cells remaining (best) | 789 | **676** |
| Full solves | 0 / 100 | **0 / 1,000** |

10x more restarts and 10x larger decision budget improved the best peak by only 113 cells
(95.1% &rarr; 95.8%). The mean and distribution shape are nearly identical to Run 1. The larger
decision budget (5M vs 500K) did not help &mdash; restarts still plateau at the same depth.

**The last ~676 cells are a structural residual.** All 1,000 random orderings converge to the
same ~95% ceiling. This is no longer an ordering problem. The residual cells are on constraint
lines where $\rho$ is deeply interior to $[0, u]$ regardless of which path the DFS takes.
Random restarts cannot close this gap.

**Comparison to B.63d baseline:**

| Experiment | Strategy | Peak det. | Cells from solve |
|-----------|----------|-----------|-----------------|
| B.63c (multi-axis) | Line DFS | 2,405 (14.9%) | 13,724 |
| B.63d (cell DFS, MCF) | MCF, 19.3M decisions | 3,682 (22.8%) | 12,447 |
| B.63e Run 1 | 100 random restarts | 15,340 (95.1%) | 789 |
| **B.63e Run 2** | 1,000 random restarts | **15,453 (95.8%)** | **676** |

**Status: COMPLETE. Run 1 + Run 2 both confirm H1: plateau is ordering-dependent (80.9&ndash;95.8%).
But a structural residual of ~676 cells (4.2%) resists all 1,000 orderings. Random restarts
alone cannot achieve a full solve. CDCL (B.63f) or beam search (B.63g) needed for the residual.**

### B.63h: Large-Scale Random Restart Census at S=127

**Prerequisite.** B.63e completed (95.8% best across 1,000 restarts).

**$C_r = 88.8\%$.**

**Motivation.** B.63e Run 2 tested 1,000 restarts and found the peak at 95.8% (15,453 cells).
The distribution ranged from 80.9% to 95.8% with mean 92.2%. But 1,000 samples may not capture
the tail of the distribution. With 10,000 restarts, we sample the tail 10x more thoroughly.
If ANY ordering can reach 100%, 10,000 trials are more likely to find it.

**Hypothesis.** Some orderings produce dramatically better results than others. The distribution
may have a long right tail &mdash; a small fraction of orderings that reach 96&ndash;100%.
Identifying the properties of high-performing orderings could inform a targeted search strategy.

**Method.** Run 10,000 random restarts on block 5 (50% density) with 5M decisions per restart.
For each restart, record:
- Restart index and RNG seed
- Peak determination (cells and %)
- Plateau depth (search decisions before stalling)
- Total decisions and backtracks
- Whether 100% was achieved (solved)

Output to `tools/b63h_results.json` for post-analysis. Report the full distribution: min, P1, P5,
P10, P25, P50, P75, P90, P95, P99, max.

Additionally, run on multiple blocks (5, 10, 50, 100) to assess whether the distribution shape
varies with block density.

**Results (block 5, 50% density, 10,001 restarts, 5M decisions/restart).**

| Percentile | Peak det. | % of 16,129 |
|-----------|-----------|------------|
| Min | 13,793 | 85.5% |
| P1 | 14,342 | 88.9% |
| P5 | 14,513 | 90.0% |
| P10 | 14,599 | 90.5% |
| P25 | 14,740 | 91.4% |
| P50 (median) | 14,886 | 92.3% |
| P75 | 15,021 | 93.1% |
| P90 | 15,134 | 93.8% |
| P95 | 15,202 | 94.2% |
| P99 | 15,315 | 94.9% |
| **Max** | **15,546** | **96.4%** |
| Mean | 14,875 | 92.2% |
| Solved (100%) | **0 / 10,001** | |

**Comparison across B.63e/h runs:**

| Run | Restarts | Best peak | Cells remaining | Improvement |
|-----|----------|-----------|-----------------|-------------|
| B.63e Run 1 | 100 | 15,340 (95.1%) | 789 | &mdash; |
| B.63e Run 2 | 1,000 | 15,453 (95.8%) | 676 | +113 |
| **B.63h** | **10,000** | **15,546 (96.4%)** | **583** | +93 |

**Findings.**

1. **Diminishing returns.** Each 10x increase in restarts gains ~100 fewer cells: +113 (100&rarr;1K),
   +93 (1K&rarr;10K). Extrapolating: 100K restarts might gain ~70 more cells (to ~96.8%).
   A full solve (100%) would require an astronomically lucky ordering.

2. **The distribution is stable.** Mean=92.2% across all three runs (identical). The shape
   (min/median/max) shifts only at the extreme tail. The core mechanism (IntBound propagation
   cascade from random orderings) has a well-defined ceiling.

3. **583 cells remain as the structural residual.** These cells are on constraint lines where
   $\rho$ is deeply interior to $[0, u]$ regardless of ordering. No amount of random restarts
   can close this gap &mdash; the gap is a property of the constraint system at this $C_r$,
   not of the search strategy.

4. **No restart achieved 100%.** Zero out of 10,001 trials. The probability of a random ordering
   reaching 100% is effectively zero at this constraint configuration.

*Multi-block runs not yet completed.*

**Status: COMPLETE (block 5). 10,001 restarts: max 96.4%, 0 solves. 583-cell structural residual.
Diminishing returns confirmed.**

### B.63i: Unified Information Budget at S=127

**Prerequisite.** B.63a&ndash;h completed. All constraint families characterized empirically.

**Motivation.** The B.60&ndash;B.63 research programme has produced extensive per-experiment data on
which constraints contribute what, but no unified accounting of how the payload bits decompose into
functional roles. The payload at the B.63c configuration ($C_r = 88.8\%$) contains 14,325 bits.
The CSM contains 16,129 bits. The solver determines 2,248 cells algebraically (Phase I), extends
to 2,405 via multi-axis DFS (Phase III&ndash;V), and reaches 15,453 (95.8%) via random restarts.
But 676 cells resist everything.

An information budget answers: for each payload bit, what does it buy? How many bits go to algebraic
determination, how many to search pruning, how many to verification, and how many are redundant?
This accounting identifies where the payload is efficiently spent and where bits are wasted, informing
future constraint design.

**Definitions.**

The information budget partitions the payload into four functional categories:

1. **Algebraic determination bits** &mdash; payload bits that translate into determined cells via
   GaussElim + IntBound fixpoint (Phase I). Measured by: cells determined per payload bit.

2. **Search pruning bits** &mdash; payload bits that constrain the DFS search tree without directly
   determining cells. These reduce the branching factor and backtracking cost in Phases II&ndash;V.
   Measured by: reduction in search decisions per payload bit.

3. **Verification bits** &mdash; payload bits used solely for pass/fail checking (LH CRC-32 for row
   verification, BH SHA-256 for block verification). These do not determine cells or prune search;
   they confirm correctness after the search commits to a candidate. Measured by: false-positive
   filter rate (candidates rejected per verification check).

4. **Redundant bits** &mdash; payload bits that duplicate information already provided by other
   components. Dropping them costs zero cells, zero pruning, and zero verification power.
   Identified by: B.62 ablation experiments (e.g., LH redundant per B.62e, XSM redundant per B.62j,
   rLTP cross-sums redundant per B.62g).

**Method.**

**(a) Per-component accounting.** For each payload component at S=127 ($C_r = 88.8\%$, B.63c config),
compute:

| Component | Bits | GF(2) equations | IntBound lines | Cells determined (Phase I) | Role |
|-----------|------|-----------------|----------------|---------------------------|------|
| LSM ($127 \times 7$) | 889 | 126 (parity) | 127 | TBD | Algebraic + IntBound |
| VSM ($127 \times 7$) | 889 | 126 (parity) | 127 | TBD | Algebraic + IntBound |
| DSM (253 diags, var-width) | 1,271 | 252 (parity) | 253 | TBD | **Load-bearing** (B.62k) |
| LH (CRC-32, 127 rows) | 4,064 | ~4,032 | 0 | TBD | Verification (Phase II) |
| VH (CRC-32, 127 cols) | 4,064 | ~4,032 | 0 | TBD | **Load-bearing** (B.63a A7) |
| DH64 (CRC-32, 64 diags) | 2,048 | ~2,032 | 0 | TBD | Algebraic (short-diag cascade) |
| XH64 (CRC-32, 64 anti-diags) | 2,048 | ~2,032 | 0 | TBD | Algebraic + Phase V entry |
| rLTP CRC (1 sub, lines 1&ndash;16) | 192 | ~188 | 0 | TBD | Interior cascade triggers |
| BH (SHA-256) | 256 | 0 | 0 | 0 | Verification (final) |
| DI | 8 | 0 | 0 | 0 | Enumeration index |
| **Total** | **14,325** (est.) | **~12,820** | **507** | **2,248** | |

**(b) Ablation analysis.** Using B.62/B.63 experimental results, determine the marginal contribution
of each component by measuring the determination loss when it is removed:

| Component removed | Determination (Phase I) | Delta | Bits saved | Bits/cell lost |
|-------------------|------------------------|-------|------------|----------------|
| LH | No change (B.62e) | 0 | 4,064 | $\infty$ (redundant for Phase I) |
| XSM (if present) | No change (B.62j) | 0 | 1,271 | $\infty$ (redundant) |
| rLTP cross-sums | No change (B.62g) | 0 | varies | $\infty$ (redundant) |
| VH | Block 0: 100% &rarr; 37% (B.63a A7) | &minus;10,160 | 4,064 | 0.40 |
| DSM | Block 1: 100% &rarr; 9.4% (B.62k) | &minus;33,068 | 1,271 (at S=191) | 0.04 |
| DH64 | Block 0: 100% &rarr; 22% (B.63a A1 vs A2) | ~&minus;12,600 | 2,048 | 0.16 |

**(c) Information efficiency ranking.** Compute bits per determined cell for each component:

$$\text{efficiency}(C) = \frac{\text{cells determined with } C - \text{cells determined without } C}{\text{bits}(C)}$$

High efficiency = each payload bit buys many determined cells. Low efficiency = bits wasted.

**(d) Functional decomposition of the 14,325-bit payload.**

Map each bit to its primary functional role:

| Role | Bits | % of payload | Contribution |
|------|------|-------------|-------------|
| Algebraic determination | TBD | TBD | Phase I: 2,248 cells (13.9%) |
| Search pruning | TBD | TBD | Phase II&ndash;V: DFS from 2,248 to 15,453 (95.8%) |
| Verification | ~4,320 | ~30% | LH (4,064) + BH (256): row/block pass/fail |
| Redundant | TBD | TBD | Zero-contribution components |
| Enumeration overhead | 8 | 0.06% | DI |

**(e) The 676-cell residual analysis.** For the 676 cells that resist all 1,000 random restarts
(B.63e Run 2):

- Map their positions in the CSM. Are they clustered or scattered?
- For each residual cell, compute the constraint line statistics: what are $\rho$ and $u$ on every
  line passing through that cell? How far is each line from forcing ($\rho = 0$ or $\rho = u$)?
- Compute the information deficit: how many additional GF(2) equations or IntBound forcings would
  be needed to determine these cells? Is the deficit concentrated on specific axes?
- Estimate the payload cost to close the deficit: what constraint family additions (more rLTP lines,
  wider DH/XH coverage, alternative hash axes) would provide the missing equations, and at what
  $C_r$ cost?

**(f) Shannon lower bound comparison.** The CSM at 50% density has maximal binary entropy:
$H = 16{,}129$ bits. Shannon&rsquo;s source coding theorem says lossless compression of a uniform
random 16,129-bit string requires at least 16,129 bits. The payload is 14,325 bits ($C_r = 88.8\%$).
This means the payload does NOT carry enough information to uniquely identify the CSM &mdash;
consistent with the observation that Phase I determines only 13.9%. The remaining 86.1% is
recovered by search (exploiting the constraint structure), not by stored information.

Decompose:
- **Stored information:** 14,325 payload bits.
- **Computed information (Phase I):** GaussElim + IntBound derive $N$ additional determined bits
  from the stored 14,325 bits. This is information amplification via algebraic reasoning &mdash;
  the constraints encode more than their bit count through cross-constraint interactions.
- **Searched information (Phase II&ndash;V):** The DFS + random restarts explore the search tree,
  using the constraint structure (IntBound pruning) and verification (LH CRC-32) to reject wrong
  paths. The search does not add stored information; it selects the correct solution from the
  constrained space.
- **Missing information (residual):** The 676 cells that no search strategy can determine within
  practical time. These represent the gap between the constraint system&rsquo;s practical resolving
  power and the CSM&rsquo;s information content.

The information budget quantifies each component:

$$16{,}129 = \underbrace{14{,}325}_{\text{stored}} + \underbrace{N_{\text{amplified}}}_{\text{algebra}} + \underbrace{N_{\text{searched}}}_{\text{DFS}} - \underbrace{N_{\text{redundant}}}_{\text{wasted}} - \underbrace{676}_{\text{residual gap}}$$

Solving for the unknowns is the objective of this experiment.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (verification-dominated) | $> 40\%$ of payload is verification bits (LH + BH) | The payload is heavily invested in pass/fail checking rather than determination; reallocating to more algebraic constraints could close the 676-cell gap |
| H2 (efficiently allocated) | Each functional category contributes proportionally; no large waste | The current payload is near-optimal; the 676-cell residual requires genuinely new information (higher $C_r$ or new constraint families) |
| H3 (residual is axis-limited) | The 676 cells cluster on specific constraint axes (e.g., all on long diagonals with $\rho \approx u/2$) | Targeted investment in that axis (more CRC, shorter lines) could close the gap at modest $C_r$ cost |
| H4 (residual is diffuse) | The 676 cells are scattered uniformly with no axis preference | No targeted fix exists; only global equation density improvement (more families, higher $C_r$) can close the gap |

**Method.**

(a) Implement the per-component ablation measurements: for each component in the B.63c config,
run with that component removed and record the Phase I determination loss. Use B.62 results
directly where applicable (LH, VH, DSM, XSM ablations already measured).

(b) For components not yet ablated at S=127 (LSM, VSM, DH64, XH64, rLTP), run individual
ablation experiments: remove one component, measure Phase I determination, compute marginal
contribution.

(c) Run the 676-cell residual analysis on the best B.63e restart (seed that reached 15,453 cells).
Extract the 676 undetermined cell positions and their per-line constraint statistics.

(d) Compute the full information budget table and the Shannon comparison.

(e) Report all results in a unified table suitable for guiding B.64+ constraint design.

**Results.**

**(a) Per-component ablation (Phase I determination, block 5 &mdash; 50% density):**

| Config | Det. | &Delta; from full | Component removed | Bits saved | Cells/bit |
|--------|------|-----------------|-------------------|------------|-----------|
| b63c (full) | 2,405 | &mdash; | &mdash; | &mdash; | &mdash; |
| no LH | 2,248 | &minus;157 | LH (4,064 bits) | 4,064 | 0.039 |
| no VH | 2,248 | &minus;157 | VH (4,064 bits) | 4,064 | 0.039 |
| no rLTP | 2,112 | &minus;293 | rLTP (192 bits) | 192 | **1.526** |
| no DH/XH | 149 | &minus;2,256 | DH+XH (~2,432 bits) | 2,432 | **0.928** |
| A1 (no DH/XH/rLTP) | 13 | &minus;2,392 | DH+XH+rLTP | 2,624 | 0.912 |

**(a&rsquo;) Per-component ablation (Phase I determination, block 1 &mdash; favorable density):**

| Config | Det. | &Delta; from full | Component removed | Cells/bit |
|--------|------|-----------------|-------------------|-----------|
| b63c (full) | 16,129 | &mdash; | &mdash; | &mdash; |
| no LH | 12,056 | &minus;4,073 | LH (4,064 bits) | **1.002** |
| no VH | 8,190 | &minus;7,939 | VH (4,064 bits) | **1.953** |
| no rLTP | 16,129 | 0 | rLTP (192 bits) | 0 |
| no DH/XH | 4,025 | &minus;12,104 | DH+XH (~2,432 bits) | **4.976** |
| A1 | 3,897 | &minus;12,232 | DH+XH+rLTP | 4.663 |

**(b) Information efficiency ranking (cells determined per payload bit):**

| Component | Bits | Block 5 cells/bit | Block 1 cells/bit | Role |
|-----------|------|-------------------|-------------------|------|
| **DH+XH** | ~2,432 | **0.928** | **4.976** | Primary algebraic engine |
| **rLTP** | 192 | **1.526** | 0 | Interior cascade triggers (50% density only) |
| VH | 4,064 | 0.039 | **1.953** | Column GaussElim (critical for favorable blocks) |
| LH | 4,064 | 0.039 | **1.002** | Row GaussElim + Phase II verification |
| DSM | 1,531 | load-bearing | load-bearing | IntBound cascade trigger (B.62k) |
| LSM+VSM | 1,778 | not ablated | not ablated | IntBound (likely low contribution) |
| BH | 256 | 0 (verification only) | 0 | Final block verification |
| DI | 8 | 0 (disambiguation) | 0 | Enumeration index |

**(c) Functional decomposition of the 14,325-bit payload:**

| Role | Bits | % of payload | Contribution |
|------|------|-------------|-------------|
| **Algebraic determination** (DH+XH+rLTP) | ~2,624 | 18.3% | Phase I: 2,248&ndash;2,405 cells |
| **Search pruning** (DSM+LSM+VSM IntBound) | ~3,309 | 23.1% | DFS propagation cascade (14.9%&rarr;96.4%) |
| **Dual role** (LH+VH: GaussElim + verification) | ~8,128 | 56.7% | GaussElim on favorable blocks; LH for row verification |
| **Verification only** (BH) | 256 | 1.8% | Block-level pass/fail |
| **Overhead** (DI) | 8 | 0.1% | Disambiguation |

**(d) Key findings.**

| Component | Bits  | Efficiency (cells/bit, 50%) | Efficiency (cells/bit, favorable) |
| --------- | ----- | --------------------------- | --------------------------------- |
| DH+XH     | 2,432 | 0.928                       | 4.976                             |
| rLTP      | 192   | 1.526                       | 0                                 |
| VH        | 4,064 | 0.039                       | 1.953                             |
| LH        | 4,064 | 0.039                       | 1.002                             |

1. **DH/XH is the most efficient algebraic component.** At 0.928 cells/bit on 50% density and
   4.976 cells/bit on favorable blocks, diagonal CRC equations provide the highest return per
   payload bit. The cascade from short-diagonal CRC is the primary algebraic mechanism.

2. **rLTP is the most efficient 50% density component.** At 1.526 cells/bit (192 bits for +293
   cells), the variable-length rLTP CRC provides the best marginal return at 50% density. However,
   it provides zero additional cells on favorable blocks (already fully solved by DH/XH+VH).

3. **LH is NOT redundant at S=127.** Unlike S=191 (B.62e: zero loss), dropping LH at S=127
   loses 157 cells on 50% blocks (the multi-axis anti-diagonal solves depend on LH row equations)
   and 4,073 cells on favorable blocks. LH has dual role: GaussElim equations for Phase I and
   CRC-32 verification for Phase II row search.

4. **VH is the most expensive load-bearing component.** At 4,064 bits (28.4% of payload), VH
   provides only 0.039 cells/bit on 50% density but 1.953 cells/bit on favorable blocks.
   Dropping VH collapses favorable-block determination from 100% to 50.8%.

5. **56.7% of the payload (LH+VH) serves dual algebraic+verification roles.** These 8,128 bits
   provide GaussElim equations AND pass/fail checking. They cannot be cleanly classified as
   &ldquo;algebraic&rdquo; or &ldquo;verification&rdquo; &mdash; they serve both functions.

**(e) Shannon comparison.**

Block size: 16,129 bits. Payload: 14,325 bits ($C_r = 88.8\%$).

At 50% density, the CSM has maximal entropy: $H = 16{,}129$ bits. The payload carries 14,325
bits &mdash; an 1,804-bit deficit from the Shannon limit. Yet the solver determines up to 96.4%
of cells (15,546 out of 16,129 = 15,546 bits of information recovered from 14,325 stored bits).

Information amplification: $15{,}546 / 14{,}325 = 1.085\times$ &mdash; the constraint structure
amplifies the stored information by 8.5%. This amplification comes from the cross-constraint
interactions: each cell participates in 4+ constraint lines, creating redundancy that the solver
exploits via IntBound propagation.

The 583-cell residual ($583 / 16{,}129 = 3.6\%$) represents information that cannot be recovered
by either algebra or search at this payload size. To close it, we must find a more efficient constraint encoding that amplifies the existing 14,325 bits further.

**(f) The 583-cell residual.** *(Updated from B.63h: 583 cells, not 676.)*

These cells are on constraint lines where $\rho$ is deeply interior to $[0, u]$ at the plateau.
The DFS cannot tip any line into forcing because each line has many remaining unknowns with
balanced residuals. The residual is not axis-specific &mdash; it spans all four geometric axes
and the rLTP lines uniformly.

**Status: COMPLETE. DH/XH most efficient (0.93&ndash;4.98 cells/bit). rLTP most efficient at 50%
density (1.53 cells/bit). 56.7% of payload is dual algebraic+verification (LH+VH).
583-cell residual requires better information amplification from the existing 14,325 bits.**

### B.63j: GaussElim + Partial CRC Verification During DFS at S=127

**Prerequisite.** B.63e (random restarts reach 96.4%), B.63i (information budget).

**$C_r = 88.8\%$ (unchanged &mdash; zero additional payload bits).**

**Motivation.** B.63e&rsquo;s random restart DFS reaches 96.4% using ONLY IntBound propagation
during Phase II search. IntBound forces cells when $\rho = 0$ or $\rho = u$ on a constraint line.
At the 96.4% plateau, all remaining lines have $\rho$ in the interior of $[0, u]$ &mdash; IntBound
cannot fire, and the DFS stalls.

However, the GF(2) system (built from CRC hash equations) contains ~12,820 equations that are
exploited ONLY in Phase I. During Phase II, these equations sit idle. GaussElim does NOT require
$\rho = 0$ or $\rho = u$ &mdash; it determines cells by finding single-variable pivots in the
reduced row echelon form. If the DFS assigns a cell and GaussElim is re-run on the reduced system,
new pivots may emerge that IntBound alone cannot find.

Additionally, CRC hash verification (LH, VH, DH, XH) is currently checked only at line completion
(when all cells on a line are assigned). But CRC is linear over GF(2): after each cell assignment,
the GF(2) equations involving that cell can be propagated, potentially revealing inconsistencies
BEFORE the line completes. This provides earlier pruning of wrong branches.

B.63j adds both mechanisms to the Phase II random restart DFS:

1. **GaussElim propagation after each DFS decision.** After assigning a cell and running IntBound,
   also propagate the assignment through the GF(2) system and run GaussElim to find new pivots.
   Any newly determined cells are assigned and their IntBound implications propagated. This creates
   a combined IntBound + GaussElim cascade at every decision point.

2. **Partial CRC consistency checking.** After each cell assignment, check whether the current
   partial assignments on each affected CRC line are consistent with the expected CRC. If a partial
   assignment is already inconsistent (the GF(2) equations involving assigned cells on the line
   are contradictory), prune the branch immediately without waiting for line completion.

**Architecture.**

Starting from B.63c&rsquo;s 2,405 determined cells, run random restart DFS as in B.63e but with
an augmented propagation loop:

```
for each DFS decision (assign cell c to value v):
    1. assignCell(c, v)
    2. propagateIntBound()          — existing: force cells on lines with rho=0 or rho=u
    3. propagateGF2({c, v})         — NEW: substitute c=v into GF(2) system
    4. runGaussElim()               — NEW: find new single-variable pivots
    5. for each newly determined cell d:
         assignCell(d, value)
         propagateIntBound()        — cascade IntBound from GaussElim-determined cells
         propagateGF2({d, value})
    6. checkPartialCRC()            — NEW: verify GF(2) consistency on affected lines
    7. if infeasible: backtrack
```

**Cost analysis.** GaussElim on a ~12K-row GF(2) system with ~16K columns (at S=127,
$\text{kWordsPerRow} = 253$ uint64 words): each pass is $O(R \times W)$ where $R$ is the number
of active rows and $W$ is the word count. At $R = 12{,}000$ and $W = 253$: ~3M word operations
per pass, ~10ms on modern hardware. With ~7,000 decisions per restart: ~70 seconds per restart.
At 100 restarts: ~2 hours. Tractable.

The partial CRC check is cheaper: for each affected CRC line, check whether the assigned cells'
GF(2) equations are consistent. This is a subset of GaussElim restricted to the equations on
the affected lines. Cost: negligible compared to full GaussElim.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (GaussElim breaks plateau) | Best restart exceeds 96.4%, potentially reaches 100% | GaussElim finds pivots that IntBound misses; the combined cascade closes the residual |
| H2 (modest improvement) | 96.4% &lt; best &lt; 99% | GaussElim determines some additional cells but the residual shrinks rather than vanishes |
| H3 (no improvement) | Best &asymp; 96.4% | At the plateau, the GF(2) system is fully reduced and has no new pivots regardless of which cells the DFS assigns |
| H4 (faster convergence) | Same peak but fewer decisions/backtracks per restart | Partial CRC pruning rejects wrong branches earlier, reducing wasted exploration |

**Prediction.** H2 or H4 is most likely. The GF(2) system after Phase I has rank ~8,000 out of
~12,800 equations. At 96.4% determination (~15,500 assigned cells), the reduced system has
~600 unknowns. GaussElim on 600 unknowns with 12,800 equations should find pivots that IntBound
cannot &mdash; the rank-to-unknown ratio is 21:1, highly overdetermined. The question is whether
the pivots resolve any of the specific 583 residual cells.

**Method.**

(a) Modify `solveHybrid()` Phase VI to run GaussElim after each DFS decision and IntBound
propagation cycle. Reuse the existing `propagateGF2()` and `gaussElim()` methods.

(b) Add partial CRC consistency check: after propagating a cell, verify that no CRC line
containing that cell has become inconsistent in the GF(2) system.

(c) Run 100 random restarts on block 5 (50% density). Report per-restart: peak determination,
decisions, backtracks, GaussElim-determined cells, CRC pruning events.

(d) Compare to B.63e (IntBound-only) and B.63h (10K IntBound-only restarts) using standardized
metrics at $C_r = 88.8\%$.

**Results (block 5, 50% density, 100 restarts, 5M decisions/restart, GE every 500 decisions).**

| Metric | B.63e (IntBound only) | B.63j (GaussElim checkpoint) |
|--------|----------------------|------------------------------|
| Peak determination | 96.4% (best of 10K) | **100% (every restart)** |
| Restarts reaching 100% | 0 / 10,000 | **100 / 100** |
| BH-verified solves | 0 | **0** (920 BH failures) |
| Avg BH failures/restart | N/A | ~9 |
| Avg decisions/restart | ~8,800 | ~8,500 |
| GaussElim-forced cells/restart | 0 | ~35,000 |
| CRC prunes/restart | 0 | ~13 |

**Outcome: 100% determination achieved on every restart. Zero BH-verified solves.**

1. **GaussElim checkpoint closes the 583-cell residual.** Every restart reaches 100% determination
   &mdash; the first time any solver has fully assigned all cells on a 50% density block.
   GaussElim contributes ~35,000 cells per restart (vs ~2,248 in Phase I alone). The checkpoint
   every 500 decisions amplifies DFS guesses through the GF(2) system, creating cascades that
   IntBound alone cannot produce.

2. **All 100% completions are WRONG.** 920 BH failures across 100 restarts (~9 per restart).
   The DFS + GaussElim finds constraint-consistent completions that satisfy IntBound, GF(2)
   equations, and partial CRC, but not BH (SHA-256). With ~4,500 DFS decisions each having
   ~2 options, and GaussElim deterministically propagating each, there are many consistent
   completions. BH is the only discriminator.

3. **The bottleneck shifted from determination to disambiguation.** B.63e&rsquo;s problem was
   reaching 100% (stuck at 96.4%). B.63j reaches 100% trivially. The new problem is finding
   the ONE correct completion among many consistent ones. This is a fundamentally different
   challenge: search-space navigation toward a specific SHA-256 target.

4. **Partial CRC pruning fires but is insufficient.** ~13 CRC prunes per restart (vs ~9 BH
   failures). The partial CRC check detects some inconsistencies early but most wrong paths
   pass partial CRC and fail only at BH.

**Timing (100 restarts, block 5).**

| Metric | Value |
|--------|-------|
| Mean time per restart | **39.6 seconds** |
| Min / Max restart time | 39.6s / 41.3s |
| Total wall-clock (100 restarts) | **66.7 minutes** |
| Time per candidate (100% completion) | **~4.4 seconds** |
| BH verification time | &lt;1ms |
| Total candidates tested | 920 |
| BH-verified solves | **0** |

**Candidate uniqueness analysis (5-restart diagnostic run).**

Each BH-failing candidate was verified to be a DISTINCT wrong completion:

| Candidate | Hamming from original | Hamming from previous | SHA-256 prefix |
|-----------|----------------------|----------------------|----------------|
| 1 | 6,888 (42.7%) | &mdash; | d9285dfc |
| 2 | 6,898 (42.8%) | 4,480 | 70ab43cc |
| 3 | 6,826 (42.3%) | 4,028 | ce575422 |
| 4 | 6,846 (42.4%) | 3,354 | 0c5f3072 |
| 5 | 6,784 (42.1%) | 2,290 | 38cd1a7a |
| 6 | 6,782 (42.0%) | 984 | d7c4e9d8 |
| 7 | 6,810 (42.2%) | 558 | ffb93359 |
| 8 | 6,798 (42.1%) | 640 | ffc526bb |
| 9 | 6,824 (42.3%) | 754 | 071c00ad |

All SHA-256 prefixes are distinct &mdash; every candidate is a genuinely different completion.
The pattern is consistent across all restarts tested.

5. **Every candidate is ~42% wrong (6,800 / 16,129 cells).** A few wrong DFS guesses cascade
   through GaussElim into ~6,800 incorrect cells &mdash; nearly half the matrix. The wrong
   guesses propagate deterministically through the linear GF(2) system, producing completions
   that are far from the correct answer.

6. **Consecutive candidates converge within a restart.** The Hamming distance from the previous
   candidate drops from 4,480 (candidate 2) to ~600&ndash;800 (candidates 7&ndash;9). Later
   candidates share more cells because the DFS backtracks only a few levels between them,
   exploring nearby branches of the search tree.

7. **Cross-restart candidates are maximally diverse.** Different random orderings produce
   unrelated completions (different SHA-256 prefixes, ~4,500 Hamming distance between restart 0
   and restart 1 candidates).

**Analysis: why are there so many wrong completions?**

The constraint system (IntBound cardinality + GF(2) CRC equations) is UNDERDETERMINED at $C_r =
88.8\%$. The payload carries 14,325 bits but the CSM has 16,129 bits of information. The 1,804-bit
deficit means there are $\approx 2^{1{,}804}$ solutions consistent with all stored constraints.
BH (SHA-256, 256 bits) narrows this to $\approx 2^{1{,}548}$ solutions. LH (CRC-32 per row,
4,064 bits) provides additional discrimination but CRC-32 has only 32 bits per row.

The DFS finds solutions efficiently because GaussElim makes the system easy to solve once enough
cells are guessed. But the correct solution is one of $\approx 2^{1{,}548}$ consistent ones.
Random search through this space by backtracking on BH failure has probability $\approx 2^{-1{,}548}$
per attempt &mdash; astronomically unlikely.

The ~42% Hamming distance from the correct answer is consistent with the information deficit:
with 1,804 bits of missing information, GaussElim propagates each wrong bit into $\sim 1{,}804
\times 16{,}129 / 14{,}325 \approx 2{,}030$ additional wrong cells via linear dependency chains.
The total ~6,800 wrong cells reflects the GF(2) system&rsquo;s error amplification factor.

**The path forward:** The constraint system needs more discriminating power to narrow the solution
space. Either:
- Higher $C_r$ (more payload bits to reduce the information deficit)
- More efficient constraints (higher rank per payload bit)
- BH-guided search (use SHA-256 structure to navigate toward the correct solution &mdash; but
  SHA-256 is cryptographic and does not expose exploitable structure)

**Status: COMPLETE. 100% determination achieved (first ever on 50% density). Zero BH-verified
solves (920 BH failures, all distinct candidates, each ~42% wrong). Bottleneck: 1,804-bit
information deficit at $C_r = 88.8\%$. Time per candidate: 4.4 seconds.**

### B.63k: Payload Trade-Off Sweep &mdash; Replace Low-Efficiency Components with rLTP

**Prerequisite.** B.63i (information budget), B.63j (GaussElim checkpoint proves 100% reachable).

**$C_r \approx 87\%$ target (all configurations).**

**Motivation.** B.63i showed rLTP is the most efficient component at 50% density (1.53 cells/bit)
while LH and VH are 39x less efficient (0.039 cells/bit). B.63j showed the solver reaches 100%
determination but finds ~$2^{1{,}548}$ wrong completions due to the GF(2) null space. The null
space dimension equals $kN - \text{rank}$. Trading low-rank-per-bit components (LH, VH) for
high-rank-per-bit components (rLTP) at the same $C_r$ SHRINKS the null space, reducing the number
of consistent-but-wrong completions.

Each rLTP sub-table (lines 1&ndash;16, CRC-8 + CRC-16) costs 192 bits and places short
fully-determined constraint lines at a unique spiral center in the matrix interior. Short-line
CRC equations are highly linearly independent because they couple small, non-overlapping cell
subsets.

**Trade-off configurations.** Each configuration trades one or more low-efficiency components
for rLTP sub-tables, keeping $C_r \approx 87\%$. All configurations retain DSM (load-bearing),
DH64+XH64 (most efficient algebraic), BH+DI (required), and the GaussElim checkpoint mechanism
from B.63j.

| Config | Dropped | Bits freed | rLTPs added | Total rLTPs | $C_r$ (est.) |
|--------|---------|-----------|-------------|-------------|-------------|
| K0 (baseline = b63c) | &mdash; | 0 | 0 | 1 | 88.8% |
| K1 | LH (4,064) | 4,064 | +21 | 22 | 88.6% |
| K2 | VH (4,064) | 4,064 | +21 | 22 | 88.6% |
| K3 | LSM+VSM (1,778) | 1,778 | +9 | 10 | 87.8% |
| K4 | LH + LSM+VSM (5,842) | 5,842 | +30 | 31 | 87.0% |
| K5 | VH + LSM+VSM (5,842) | 5,842 | +30 | 31 | 87.0% |
| K6 | LH + VH (8,128) | 8,128 | +42 | 43 | 87.1% |

rLTP spiral centers for $N$ sub-tables: distribute on a grid across the $127 \times 127$ matrix.
For $N$ centers, use $(\lfloor 127 i / \lceil \sqrt{N} \rceil \rfloor, \lfloor 127 j / \lceil
\sqrt{N} \rceil \rfloor)$ for $i, j \in \{0, \ldots, \lceil \sqrt{N} \rceil - 1\}$.

**What each trade costs and gains:**

| Config | Loses | Gains |
|--------|-------|-------|
| K1 (drop LH) | Row CRC verification during DFS; 157 Phase I cells | 21 interior spirals; ~4,000 GF(2) equations on short independent lines |
| K2 (drop VH) | Column GaussElim (critical for favorable blocks: &minus;7,939 cells on block 1) | Same as K1 |
| K3 (drop LSM+VSM) | Row/column IntBound (0 contribution on block 1 per B.62k) | 9 interior spirals |
| K4 (drop LH+LSM+VSM) | Row verification + row/col IntBound | 30 spirals; massive GF(2) rank injection |
| K5 (drop VH+LSM+VSM) | Column GaussElim + row/col IntBound | 30 spirals |
| K6 (drop LH+VH) | All row/column CRC GaussElim + verification | 42 spirals; maximum GF(2) rank injection |

**Method.**

(a) Implement configs K0&ndash;K6 with dynamically placed rLTP spiral centers.

(b) For each config, run Phase I (combinator solveCascade) on ALL 1,331 blocks of the MP4
test file. Record per-block: determined cells, GF(2) rank, Phase I time.

(c) For each config, run the B.63j GaussElim-checkpoint DFS (5 restarts) on a sample of blocks:
block 5 (50% density), block 1 (favorable), block 100 (50% density). Record per-block: peak
determination, candidates found, BH failures, time per candidate.

(d) Report a comparison table: for each config and block type, Phase I determination, DFS peak,
GF(2) rank, null space dimension, candidates per restart, and $C_r$.

**Key metric.** The null space dimension $= kN - \text{rank}$. Lower is better. If any config
achieves null space dimension $< 256$ (the BH discrimination power), the correct solution
becomes findable by enumeration.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (rLTP rank dominates) | K4 or K6 achieves highest GF(2) rank and fewest BH failures | Short-line rLTP equations provide more independent rank than LH/VH on long lines |
| H2 (VH is irreplaceable) | K2/K5/K6 regress on favorable blocks despite more rLTPs | VH&rsquo;s column-axis coverage provides rank that rLTP spirals cannot replace |
| H3 (LH trade is free) | K1 matches or exceeds K0 on all blocks | LH is purely redundant at S=127 when sufficient rLTP coverage exists (paralleling B.62e at S=191) |
| H4 (null space collapses) | Any config achieves null space $< 256$ | BH enumeration becomes tractable; full decompression achievable at $C_r \approx 87\%$ |

**Results (20 sampled blocks &times; 7 configs, Phase I only).**

Block 5 (50% density) &mdash; Phase I determination vs GF(2) rank:

| Config | Dropped | rLTPs | Det% | GE cells | Rank | Null space | BH solves (all blocks) |
|--------|---------|-------|------|----------|------|-----------|----------------------|
| **K0** (baseline) | &mdash; | 1 | 13.9 | 2,248 | **9,585** | **6,544** | 4 |
| K1 | LH | 22 | 29.8 | 4,811 | 6,324 | 9,805 | 2 |
| **K2** | VH | 22 | 30.8 | 4,973 | 9,122 | 7,007 | **4** |
| K3 | LSM+VSM | 10 | 21.6 | 3,484 | 9,413 | 6,716 | 4 |
| K4 | LH+LSM+VSM | 31 | 36.0 | 5,813 | 6,342 | 9,787 | 3 |
| **K5** | VH+LSM+VSM | 31 | **37.7** | 6,074 | 8,978 | 7,151 | **4** |
| K6 | LH+VH | 43 | **47.1** | 7,596 | 6,068 | **10,061** | 4 |

Block 1 (favorable) &mdash; Phase I determination:

| Config | Det% | Rank | BH |
|--------|------|------|----|
| K0 | 100.0 | 4,147 | true |
| K1 | 82.4 | 2,745 | false |
| **K2** | 100.0 | 5,339 | **true** |
| K3 | 100.0 | 6,661 | true |
| K4 | 81.3 | 2,831 | false |
| **K5** | 100.0 | 4,781 | **true** |
| K6 | 100.0 | 2,107 | true |

Average 50% density determination (blocks 5+):

| Config | 50% Avg Det% | 50% Avg Rank |
|--------|-------------|-------------|
| K0 | 15.4 | 9,464 |
| K1 | 31.4 | 6,263 |
| K2 | 32.4 | 8,986 |
| K3 | 21.8 | 9,396 |
| K4 | 36.2 | 6,324 |
| K5 | 37.9 | 8,958 |
| K6 | 49.8 | 5,763 |

**Outcome: determination and rank are inversely correlated.**

1. **The hypothesis was partially wrong.** More rLTPs dramatically increase Phase I determination
   (K6: 47.1% vs K0: 13.9%, a 3.4&times; improvement). But they DECREASE GF(2) rank and INCREASE
   the null space. rLTP short-line equations are efficient at cell determination (1.53 cells/bit)
   but highly linearly dependent (low rank/bit). LH/VH long-line equations provide less
   determination but more independent rank.

2. **Determination vs disambiguation trade-off.** B.63e&rsquo;s random restart DFS benefits from
   higher Phase I determination (more cells determined = tighter constraints for DFS). B.63j&rsquo;s
   disambiguation problem benefits from lower null space (fewer consistent completions). These
   goals conflict: K6 maximizes determination but has the worst null space (10,061); K0 minimizes
   null space but has the worst determination.

3. **Dropping LH hurts favorable blocks.** K1 and K4 lose BH-verified solves on favorable blocks
   (82.4% and 81.3% on block 1). LH is load-bearing for favorable-density cascade closure.
   Dropping VH (K2, K5) preserves all favorable-block solves.

4. **K5 is the best compromise.** Drops VH+LSM+VSM, adds 30 rLTPs. Achieves 37.7% Phase I
   determination on 50% blocks (2.7&times; K0), rank 8,978 (null space 7,151), and preserves all
   4 favorable-block BH solves. $C_r \approx 87\%$.

5. **K6 is the determination champion.** 47.1% on 50% blocks with 42 rLTPs. But null space
   10,061 is 53% larger than K0&rsquo;s 6,544, making disambiguation harder. However, for
   B.63e-style random restart DFS (where determination is the bottleneck), K6 would push the
   plateau from ~96% significantly higher.

**Analysis: why does rank decrease with more rLTPs?**

Each rLTP sub-table (lines 1&ndash;16) adds $8 \times 8 + 8 \times 16 = 192$ CRC equations.
But the 16 graduated lines cover only $\sum_{k=1}^{16} k = 136$ cells per sub-table. With 42
sub-tables, the total cells covered by short lines is $42 \times 136 = 5{,}712$ &mdash; many
cells are covered by MULTIPLE sub-tables. The overlapping CRC equations on the same cells are
linearly dependent. In contrast, LH equations cover disjoint rows (each row&rsquo;s 127 cells
appear in exactly one LH equation set), producing high independent rank.

**Implication for B.63j.** The disambiguation problem (finding the correct completion among
$\sim 2^{\text{null space}}$ candidates) gets HARDER with more rLTPs. The null space dimension
is the fundamental barrier, not Phase I determination. No rLTP trade can reduce the null space
below ~6,500 at $C_r \approx 87\%$.

**Status: COMPLETE. K5 (drop VH+LSM+VSM, +30 rLTPs) is the Pareto optimum: 37.7% Phase I
determination, rank 8,978, all favorable BH solves preserved. Determination and rank are
inversely correlated — more rLTPs help DFS but hurt disambiguation.**

### B.63l: Per-rLTPh Incremental GF(2) Pruning During DFS

**Prerequisite.** B.63j (GaussElim checkpoint: 100% det, 0 BH solves), B.63k (trade-off sweep).

**$C_r = 88.8\%$ (K0 baseline &mdash; unchanged payload).**

**Motivation.** B.63j runs GaussElim every 500 DFS decisions as a checkpoint. This catches
contradictions in the global GF(2) system but is coarse-grained: 500 wrong decisions can
accumulate before the next check. B.63k showed that adding more rLTP equations (for GaussElim)
hurts the null space.

B.63l takes a different approach: use the rLTPh CRC hash equations for **incremental per-line
GF(2) consistency checking** at every cell assignment. Each rLTP line of length $L$ with CRC-$W$
produces $W$ GF(2) equations that form a small sub-matrix. When the DFS assigns a cell on that
line, the sub-matrix partially reduces. If the reduction produces a contradiction (a GF(2) row
with all variable bits zero but target bit one), the current DFS assignment is provably wrong
&mdash; prune immediately.

This is **partial hash pruning via combinator algebra**: the rLTPh CRC equations participate in
the algebraic system, and contradictions are detected incrementally per line rather than globally
per checkpoint. The pruning fires at every cell assignment at negligible cost ($O(W \times
\text{lines per cell})$ per assignment).

**Architecture.**

For each rLTP sub-table, precompute the per-line CRC generator sub-matrix: for line $k$ of
length $L_k$ with CRC-$W_k$, store the $W_k \times L_k$ binary matrix $G_k$ and the target
vector $t_k = \text{CRC}(m_k) \oplus c_0$. These are already computed in `addRLTPAt`.

During Phase II DFS, after each cell assignment $(f, v)$:

1. For each rLTP line containing cell $f$:
   a. Substitute $v$ into the line&rsquo;s $W$ GF(2) equations (XOR column $f$ from each equation
      if $v = 1$; clear column $f$).
   b. Check if any equation has all variable bits zero: if target bit is also zero, equation is
      satisfied (remove). If target bit is one, **contradiction** &mdash; prune this branch.
2. This is equivalent to maintaining a reduced GF(2) sub-matrix per rLTP line incrementally.

On backtrack: restore the per-line sub-matrices from snapshot (small: $W \times \lceil L/64 \rceil$
words per line, ~16-32 bytes per line, ~few KB total per snapshot).

**Key advantage over B.63j global checkpoints:** Contradictions are detected at the FIRST wrong
cell assignment, not 500 decisions later. This prunes wrong branches before GaussElim cascades
the error into thousands of incorrect cells. The ~42% Hamming distance problem from B.63j
(each wrong candidate has ~6,800 wrong cells) should be dramatically reduced because errors
are caught early.

**Key advantage over B.63k rLTP-as-GF2-equations:** The per-line sub-matrices are NOT added to
the global GF(2) system. The global system&rsquo;s rank and null space are unchanged from K0
(rank 9,585, null space 6,544). The rLTPh pruning is a DFS-only mechanism that does not
inflate the null space.

**Configuration.** K0 baseline (LSM + VSM + DSM + LH + VH + DH64 + XH64 + BH/DI) with rLTPh
per-line pruning on $N$ spiral sub-tables. The rLTPh sub-tables are NOT added to the combinator&rsquo;s
GF(2) system &mdash; they exist only as DFS pruning oracles.

Test configurations:

| Config | rLTPh sub-tables | Payload bits for rLTPh | $C_r$ |
|--------|-----------------|----------------------|-------|
| L0 | 1 (center) | 192 | 90.0% |
| L1 | 10 (grid) | 1,920 | 100.7% &mdash; over budget |
| L0a | 1 + GaussElim checkpoint (B.63j) | 192 | 90.0% |

Wait &mdash; the rLTPh CRC values must be STORED in the payload for the decompressor to verify
against. Each sub-table costs 192 bits. At $C_r = 88.8\%$ (K0), there is 1,804 bits of headroom.
We can fit $\lfloor 1{,}804 / 192 \rfloor = 9$ additional rLTPh sub-tables (total 10) at
$C_r = 89.9 + 9 \times 1.2 = 99.6\%$. To stay under 90%, we can add 1 sub-table ($C_r = 90.0\%$).

Revised test:

| Config | Base | rLTPh pruning tables | $C_r$ |
|--------|------|---------------------|-------|
| L0 | K0 | 0 (no rLTPh, B.63j checkpoint only) | 88.8% |
| L1 | K0 | 1 rLTPh at (63,63) | 90.0% |
| L5 | K0 minus VH (saves 4,064) | 5 rLTPh (grid) | 84.9% |
| L10 | K0 minus VH | 10 rLTPh (grid) | 90.8% |
| L20 | K0 minus LH&minus;VH (saves 8,128) | 20 rLTPh (grid) | 86.3% |

The rLTPh sub-tables are used ONLY for incremental per-line GF(2) pruning during DFS. They are
NOT added to the Phase I GaussElim system. Phase I uses only the base config&rsquo;s equations.

**Method.**

(a) Implement per-line GF(2) sub-matrix tracking. For each rLTPh line, store a compact
$W \times \lceil L/64 \rceil$ binary matrix and target vector. On cell assignment: propagate
into affected lines. On contradiction: signal infeasibility to the DFS.

(b) Integrate into the B.63j GaussElim-checkpoint DFS. The per-line pruning fires at every
assignment; the global GaussElim checkpoint fires every 500 decisions. Both mechanisms coexist.

(c) Run 100 random restarts on block 5 (50% density) with config L1. Record per-restart: peak
determination, BH failures, CRC prunes, time per candidate. Compare to B.63j (no rLTPh pruning).

(d) If L1 shows improvement, test L5/L10/L20 and report the Pareto frontier.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (rLTPh prunes wrong branches early) | Fewer BH failures per restart (&lt;9) or BH-verified solve | Per-line pruning catches errors before GaussElim propagates them; fewer wrong candidates |
| H2 (rLTPh has negligible effect) | Same ~9 BH failures per restart | The DFS errors occur on cells NOT covered by the rLTPh lines; pruning misses them |
| H3 (more rLTPh = BH solve) | L10 or L20 achieves BH-verified solve | Sufficient per-line coverage prunes ALL wrong branches, leaving only the correct completion |

**Results (block 5, 50% density, 100 restarts, rLTPh on 16 lines at center (63,63)).**

| Metric | B.63j (no rLTPh) | B.63l (16-line rLTPh) |
|--------|-----------------|----------------------|
| Peak det. | 100% (every restart) | 100% (every restart) |
| BH failures/restart | ~9 | ~9 (unchanged) |
| rLTPh prunes | N/A | **0** (all 42 restarts) |
| Time/restart | 40s | 40s (unchanged) |

**Outcome: H2 (negligible effect). Zero rLTPh prunes across 42 restarts.**

The 16 short rLTPh lines (lengths 1&ndash;16) are fully determined by Phase I before the DFS
starts. The per-line GF(2) consistency check never encounters a partially-assigned line with a
contradiction because all cells on those lines are already correct. The DFS errors occur on cells
participating in LONGER constraint lines (rows, columns, long diagonals) that the 16-line rLTPh
does not cover.

**Root cause:** The rLTPh pruning only fires when ALL variable bits in an equation are eliminated
(all cells on the line assigned). For short lines already solved by Phase I, this condition is
met trivially with correct values. For the lines where the DFS guesses (length 64&ndash;127),
the rLTPh would need to cover those lengths &mdash; but at 32 bits per CRC-32 line, covering
127-cell lines costs the same as LH (which is already in the payload).

**Implication:** Per-line incremental GF(2) pruning requires coverage on the SAME lines where
the DFS makes errors. Short interior spiral lines (the rLTP sweet spot) are not where errors
occur. The errors occur on the long geometric lines (rows, columns, full diagonals) that are
already covered by LH, VH, DH, XH. The existing CRC hashes on those lines already provide the
GF(2) equations; the issue is that the equations are processed globally (GaussElim) rather than
incrementally per line.

**Status: COMPLETE. H2 confirmed. rLTPh on short lines provides zero pruning. DFS errors occur
on long lines already covered by LH/VH/DH/XH.**

### B.63m: Incremental Per-Line GF(2) Pruning on LH/VH/DH/XH During DFS

**Prerequisite.** B.63j (GaussElim checkpoint: 100% det, ~9 BH failures/restart), B.63l (rLTPh
on short lines: 0 prunes).

**$C_r = 88.8\%$ (K0 baseline &mdash; zero additional payload).**

**Motivation.** B.63l proved that per-line GF(2) pruning on SHORT rLTP lines provides zero value
because those lines are solved before the DFS starts. The DFS errors occur on LONG lines &mdash;
rows (127 cells, covered by LH CRC-32), columns (127 cells, VH CRC-32), and diagonals (up to
127 cells, DH/XH CRC). These CRC equations are already in the payload and already used in Phase I
GaussElim. But during Phase II DFS, they are only checked:
- **Globally** every 500 decisions (B.63j GaussElim checkpoint)
- **At line completion** (LH row verification when all 127 cells assigned)

Neither catches errors early. A wrong DFS guess at decision $d$ propagates through GaussElim at
checkpoint $d + 500$, by which point ~6,800 cells are wrong.

B.63m applies the SAME per-line incremental GF(2) consistency check from B.63l, but on the
EXISTING LH/VH/DH/XH equations rather than new rLTPh lines. After each cell assignment, check
the 32 LH equations for the affected row, the 32 VH equations for the affected column, and the
DH/XH equations for the affected diagonals. If any fully-reduced equation has target $\neq 0$,
the current assignment is provably wrong &mdash; prune immediately.

**Architecture.**

Precompute per-line GF(2) sub-matrices for LH, VH, DH, XH (already computed during
`addLH()`/`addVH()`/`addDHRange()`/`addXHRange()`). For each line, store:
- The $W$ equations as $(W \times L)$ binary sub-matrix (which cells have nonzero coefficient)
- The target vector ($W$ bits, derived from the CRC of the original line)
- A cell-to-line mapping (which LH/VH/DH/XH lines pass through each cell)

During Phase II DFS, after assigning cell $(f, v)$:

```
for each LH/VH/DH/XH line L containing cell f:
    for each of L's W GF(2) equations:
        if all variable cells on this equation are assigned:
            compute XOR of assigned values
            if XOR != target: CONTRADICTION → prune branch
```

This is the same check as B.63l but on lines of length 64&ndash;127 that the DFS actually
operates on. The check costs $O(W \times \text{lines per cell})$ per assignment:
- LH: 32 equations, 1 row per cell = 32 ops
- VH: 32 equations, 1 column per cell = 32 ops
- DH: 8&ndash;32 equations (hybrid width), 1 diagonal per cell = up to 32 ops
- XH: same as DH
- Total: ~128 ops per cell assignment. Negligible.

**Key difference from B.63l:** The lines being checked are the LONG geometric lines (127 cells)
where the DFS makes errors. These equations have many variables, so the &ldquo;all variables
assigned&rdquo; condition triggers only when the line is NEARLY complete. This means pruning
fires late &mdash; but still earlier than B.63j&rsquo;s 500-decision checkpoint.

**Key difference from B.63j checkpoint:** The per-line check is incremental (every assignment)
and line-local (only checks the affected lines). B.63j rebuilds the ENTIRE GF(2) system from
scratch every 500 decisions. B.63m checks only ~4 lines per assignment but at every assignment.

**Expected pruning profile.** On a 127-cell row with LH CRC-32: the 32 GF(2) equations each
involve ~64 cells (half the row, on average). An equation becomes fully resolved when all ~64
variable cells are assigned. During the DFS, a row&rsquo;s cells are assigned gradually. The
equations start firing contradictions when ~100&ndash;120 of the 127 cells are assigned (the
last 7&ndash;27 cells resolve the remaining equations). This is much earlier than waiting for
all 127 cells (LH row completion check).

**Configurations.**

| Config | Per-line pruning on | Payload | $C_r$ |
|--------|-------------------|---------|----|
| M0 (= B.63j) | None (GaussElim checkpoint only) | K0 | 88.8% |
| M1 | LH only (127 rows &times; 32 eq) | K0 | 88.8% |
| M2 | LH + VH (127 rows + 127 cols) | K0 | 88.8% |
| M3 | LH + VH + DH + XH (all axes) | K0 | 88.8% |

**Method.**

(a) Precompute per-line GF(2) sub-matrices for LH, VH, DH64, XH64 during solver construction.
Store cell-to-CRC-line mappings for each axis.

(b) Integrate per-line incremental consistency check into the DFS loop alongside IntBound
propagation and the GaussElim checkpoint.

(c) Run M3 (all axes) with 100 random restarts on block 5. Record per-restart: BH failures,
per-line prune events (broken down by axis: LH prunes, VH prunes, DH prunes, XH prunes),
decisions, backtracks, time.

(d) Compare to B.63j (M0) and B.63l using standardized metrics.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (long-line pruning catches errors) | rltphPrunes &gt; 0; fewer BH failures per restart | Per-line GF(2) checks on rows/columns catch DFS errors before GaussElim propagates them |
| H2 (pruning fires but late) | rltphPrunes &gt; 0 but same BH failure count | Pruning detects errors only when a line is nearly complete &mdash; too late to prevent wrong candidates |
| H3 (BH-verified solve) | Any restart achieves BH verification | Per-line pruning on all 4 axes eliminates enough wrong branches to navigate to the correct completion |
| H4 (no pruning fires) | rltphPrunes = 0 | Even on long lines, no single equation becomes fully resolved before the GaussElim checkpoint fires |

**Results (block 5, 50% density, partial: 12+ restarts so far).**

510 CRC pruning lines (127 LH + 127 VH + 128 DH + 128 XH), 15,040 equations total.

| Metric | B.63j (checkpoint only) | B.63m (per-line LH/VH/DH/XH) |
|--------|------------------------|------------------------------|
| Decisions/restart | ~8,500 | **~12,800 (+50%)** |
| Backtracks/restart | ~1,900 | **~5,700 (+200%)** |
| CRC prunes/restart | 0 | **~6,300** |
| BH failures/restart | ~9 | **~16 (+78%)** |
| Time/restart | 40s | **~75s (+88%)** |
| BH-verified solves | 0 | **0** |

**Outcome: H2 (pruning fires but makes things worse).**

1. **Per-line CRC pruning fires heavily: ~6,300 prunes per restart.** The LH/VH/DH/XH
   equations detect contradictions during the DFS, confirming that wrong branches are being
   caught at the per-line level.

2. **But the search gets WORSE, not better.** More decisions (+50%), more backtracks (+200%),
   more BH failures (+78%), slower (+88%). The per-line pruning rejects some wrong paths
   early but forces the DFS to explore MORE alternative paths. Each pruned branch opens
   another branch that is also wrong. The search tree is wider, not narrower.

3. **The fundamental problem persists.** The pruning catches individual equation contradictions
   but does not reduce the solution space. The null space dimension is unchanged (6,544).
   There are still $\sim 2^{6{,}544}$ GF(2)-consistent completions. Per-line pruning filters
   some that are not per-line-consistent, but the surviving candidates are still astronomically
   numerous.

4. **CRC pruning accumulates during each restart.** The prune count grows throughout the
   search: 12 at 4,500 decisions, 760 at 7,000, 6,011 at completion. The pruning rate increases
   as more cells are assigned (more equations become fully resolved). But this is too late &mdash;
   by the time enough equations resolve to prune effectively, GaussElim has already cascaded
   the errors.

**Analysis.** The per-line incremental GF(2) check is a correct optimization &mdash; it detects
contradictions earlier than the 500-decision global checkpoint. But &ldquo;earlier&rdquo; is
relative: a CRC equation on a 127-cell row resolves only when ~100+ cells are assigned, which
happens deep in the DFS (after ~4,000 decisions). By that point, GaussElim has already cascaded
the error. The pruning catches it slightly earlier than BH verification at 100%, but not early
enough to prevent the 6,800-cell error propagation.

**For pruning to help disambiguation**, it would need to fire within the first ~100 DFS
decisions &mdash; before GaussElim propagates errors widely. This requires equations on SHORT
lines (where all cells are assigned early). But B.63l showed short rLTP lines are already
solved by Phase I. The constraint system has a gap: short lines are solved algebraically
(no pruning needed) and long lines resolve too late for pruning to help.

**Final results (100 restarts complete).**

| Metric | B.63j (checkpoint only) | B.63m (per-line pruning) | Change |
|--------|------------------------|-------------------------|--------|
| Decisions/restart | ~8,500 | ~13,200 | +55% |
| Backtracks/restart | ~1,900 | ~6,246 | +229% |
| CRC prunes/restart | 0 | **~6,928** | new |
| BH failures total | 920 | **1,754** | +91% |
| BH failures/restart | ~9 | **~17** | +89% |
| Time/restart | 40s | **77s** | +93% |
| Total time | 67 min | **130 min** | +94% |
| BH-verified solves | 0 / 100 | **0 / 100** | same |

**Status: COMPLETE. Per-line LH/VH/DH/XH pruning fires ~6,928 times per restart but INCREASES
BH failures (+89%) and search time (+93%). Pruning fires too late in the DFS to prevent error
cascading through GaussElim. The structural gap persists: short lines are solved algebraically,
long lines resolve too late. 0 BH-verified solves across 1,754 BH failures.**

### B.63n: CRC-64 Validation at S=127

**Prerequisite.** B.63j (GaussElim checkpoint: 100% det, 0 BH solves at CRC-32).

**$C_r > 100\%$ (NOT compression &mdash; validation experiment only).**

**Motivation.** B.63j showed the solver reaches 100% determination but finds only wrong
completions due to a 6,544-dimension null space at CRC-32. CRC-64 doubles the GF(2) equations
per line, potentially shrinking the null space. This experiment validates whether CRC-64
provides enough independent rank for BH-verified disambiguation.

**IMPORTANT: $C_r > 100\%$.** At $S = 127$, CRC-64 on LH (8,128 bits) + VH (8,128 bits) alone
exceeds the 16,129-bit block size. This configuration EXPANDS rather than compresses. The result
validates the CRC-64 mechanism but does NOT demonstrate compression. Any configuration achieving
$C_r > 100\%$ has more payload bits than data bits &mdash; it trivially carries enough information
to uniquely specify the CSM. The BH-verified solve at $C_r > 100\%$ is expected, not a
breakthrough.

**Configuration.** K0 baseline with all CRC widths upgraded to CRC-64: LH (CRC-64), VH (CRC-64),
DH64/XH64 tier 4 (CRC-64), rLTP tier 4+ (CRC-64). DSM + LSM + VSM + BH + DI unchanged.

**Results (block 5, 50% density).**

| Metric | B.63j (CRC-32, $C_r = 88.8\%$) | B.63n (CRC-64, $C_r > 100\%$) |
|--------|-------------------------------|-------------------------------|
| GF(2) equations | ~18,232 | ~30,424 |
| Rank | 9,585 | **13,489** |
| Null space | 6,544 | **2,640** |
| Phase I det. | 2,248 (13.9%) | **16,129 (100%)** |
| GaussElim cells | 2,248 | **16,129** |
| IntBound cells | 0 | 0 |
| DFS needed | Yes | **No** |
| Correct | N/A (wrong completions) | **true** |
| BH verified | No (920 failures) | **true** |

**Outcome: BH-verified full solve. Expected at $C_r > 100\%$.**

1. **GaussElim alone solves the entire block.** All 16,129 cells determined algebraically in
   Phase I. Zero IntBound, zero DFS, zero search. The CRC-64 equations provide enough rank
   (13,489) that GaussElim finds a single-variable pivot for every cell.

2. **Null space reduced 60% (6,544 &rarr; 2,640).** CRC-64 doubles equation count and adds
   ~3,904 rank. The null space of 2,640 is still large in absolute terms but the system is
   sufficiently overdetermined for GaussElim to close without search.

3. **This result is expected and does NOT validate compression.** At $C_r > 100\%$, the payload
   carries MORE bits than the CSM. A payload larger than the data trivially contains enough
   information for unique reconstruction. The result confirms that CRC-64 GF(2) equations are
   properly implemented and produce correct algebraic solutions, but says nothing about whether
   CRSCE can compress data.

4. **The value of B.63n is the rank measurement.** Rank 13,489 at CRC-64 vs 9,585 at CRC-32
   shows that wider CRC provides +3,904 additional independent rank (+40.7%). This empirical
   scaling factor informs B.64a: at what $S$ does CRC-64 provide enough rank within $C_r < 80\%$?

**Status: COMPLETE. BH-verified full solve at $C_r > 100\%$ (expected &mdash; not compression).
CRC-64 provides +40.7% rank over CRC-32. Null space 2,640. Result validates CRC-64
implementation and provides scaling data for B.64a.**

