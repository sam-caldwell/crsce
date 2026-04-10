## B.41 Cross-Dimensional Hashing + Four-Direction Pincer (Diagnostic Complete &mdash; Infeasible for Depth)

### B.41.1 Motivation

The B.39a null-space analysis established that the minimum constraint-preserving swap for the full 8-family system is 1,528 cells, touching 11 rows and 492 columns. This structural result has a direct implication for hash design: per-row SHA-1 hashes alone exploit only the row dimension for pruning, even though the swap structure spans nearly the full column range (96% of columns). Adding column hashes would provide a second verification axis, enabling solver architectures that prune along both rows and columns.

The current per-row SHA-1 system stores 511 hashes (81,760 bits), representing 64.9% of the block payload. The solver verifies each row hash at row completion during row-serial DFS, providing 511 pruning events per block. However, no pruning mechanism exists for columns &mdash; the solver has no way to verify partial progress along the column axis. This limitation is why B.31 (alternating-direction pincer) produced null results: the bottom-up solver could verify row hashes on bottom rows, but a column-serial or left-right solver would have had no verification capability at all.

B.41 introduces **cross-dimensional hashing** &mdash; storing both row hashes and column hashes &mdash; and resurrects the B.31 alternating-direction pincer with four-direction capability (top-down, bottom-up, left-right, right-left). Column hashes give the left-right and right-left passes a pruning mechanism they previously lacked.

### B.41.2 Hash Construction

**Row hashes.** Each row hash covers a **pair** of adjacent rows. Row pair $p$ hashes the concatenation of rows $2p$ and $2p+1$ (1,022 data bits + 2 padding bits = 1,024 bits = 128 bytes) via SHA-1. For $S = 511$ rows, this produces $\lfloor 511/2 \rfloor = 255$ row-pair hashes, plus one singleton hash for row 510 (512 bits = 64 bytes). Total: 256 row hashes.

**Column hashes.** Each column hash covers a **pair** of adjacent columns. Column pair $q$ hashes the concatenation of columns $2q$ and $2q+1$ (1,022 data bits, serialized column-major + 2 padding bits = 1,024 bits = 128 bytes) via SHA-1. Total: 256 column hashes, with column 510 hashed solo.

**Hash message format.** Row-pair message: bits from row $2p$ (511 bits) concatenated with bits from row $2p+1$ (511 bits), plus 2 zero padding bits = 1,024 bits = 128 bytes. Column-pair message: bits from column $2q$ across all 511 rows (511 bits) concatenated with bits from column $2q+1$ across all 511 rows (511 bits), plus 2 zero padding bits = 1,024 bits = 128 bytes.

**Payload impact.**

| Component | Current | B.41 |
|-----------|---------|------|
| Row hashes | 511 &times; 160 = 81,760 bits | 256 &times; 160 = 40,960 bits |
| Column hashes | 0 | 256 &times; 160 = 40,960 bits |
| Hash total | 81,760 bits | 81,920 bits |
| Block payload | 125,988 bits | 126,148 bits |
| Compression ratio | 48.2% | 48.3% |

The payload is essentially unchanged (+160 bits, one additional hash).

### B.41.3 Collision Resistance

The minimum constraint-preserving swap (1,528 cells, 11 rows, 492 columns) must evade both row-pair and column-pair hashes. With 11 affected rows spread across a span of 401 rows, approximately 6 row-pair hashes are affected (some affected rows may share a pair, but the 401-row span makes this unlikely &mdash; expected shared pairs $\approx \binom{11}{2}/511 \approx 0.1$, so $k_{\text{row}} \approx 11$ row-pair hashes). With 492 affected columns, approximately 246 column-pair hashes are affected ($k_{\text{col}} \approx 246$).

$$P_{\text{collision}} = 2^{-160(k_{\text{row}} + k_{\text{col}}) - 256} = 2^{-160 \times 257 - 256} = 2^{-41,376}$$

Compared to the current system's $2^{-2,016}$, this is a $2^{39,360}$ improvement in collision resistance at essentially the same payload cost.

### B.41.4 Four-Direction Pincer Solver

B.41 extends the B.31 alternating-direction pincer from two directions (top-down, bottom-up) to four:

| Direction | Traversal | Hash verification trigger |
|-----------|-----------|--------------------------|
| Top-down (TD) | Row 0 &rarr; 510, left-to-right within row | Row-pair hash when both rows $2p$ and $2p+1$ are complete |
| Bottom-up (BU) | Row 510 &rarr; 0, left-to-right within row | Row-pair hash when both rows $2p$ and $2p+1$ are complete |
| Left-right (LR) | Column 0 &rarr; 510, top-to-bottom within column | Column-pair hash when both columns $2q$ and $2q+1$ are complete |
| Right-left (RL) | Column 510 &rarr; 0, top-to-bottom within column | Column-pair hash when both columns $2q$ and $2q+1$ are complete |

**Plateau trigger and direction switching.** When the current direction's `stall_count` reaches threshold $N$ (candidate: 1,022 &mdash; one full row-pair or column-pair of unconstrained branching), declare a plateau and switch to the next direction in the cycle TD &rarr; BU &rarr; LR &rarr; RL &rarr; TD. Reset `stall_count`.

**Frontier tracking.** Four frontiers are maintained:

- `topFrontier`: lowest row with any unassigned cell (advances downward)
- `bottomFrontier`: highest row with any unassigned cell (advances upward)
- `leftFrontier`: leftmost column with any unassigned cell (advances rightward)
- `rightFrontier`: rightmost column with any unassigned cell (advances leftward)

**Convergence.** When all four directions have plateaued without progress since the last switch, the remaining unassigned region (bounded by the four frontiers) is irreducibly hard. The solver falls back to standard row-serial DFS within this region.

**DI consistency.** As established in B.31.2, the enumeration order is determined by the decompressor's deterministic traversal. The compressor runs the same four-direction solver to discover DI. The alternating order is fully deterministic and reproducible.

### B.41.5 Why B.31 Failed and B.41 May Succeed

B.31 (two-direction pincer) produced null results. The B.39 n-dimensional analysis explains why: the bottom-up solver could verify row hashes but had no way to exploit column-axis information. The solver was alternating between two projections along the same axis (the row axis), gaining no new dimensional coverage.

B.41 adds column hashes, giving the LR and RL passes genuine verification capability along the column axis &mdash; a fundamentally different projection than the row axis. This provides cross-dimensional pruning: the TD/BU passes prune based on row structure, and the LR/RL passes prune based on column structure. The combination may tighten the meeting region more effectively than either axis alone.

The key hypothesis: if the meeting band's intractability is caused by the solver's inability to verify column-axis constraints (because no column hashes exist), then adding column hashes and column-serial passes will reduce the meeting band and increase depth.

### B.41.6 Sub-experiment B.41a: Full Paired Hashing + Four-Direction Pincer

**Hash configuration:** 256 row-pair hashes + 256 column-pair hashes (512 total, 81,920 bits).

**Solver:** Four-direction pincer (TD, BU, LR, RL) with plateau threshold $N = 1{,}022$.

**Metric:** Peak depth (cells assigned before exhaustive backtracking). Compare against baseline 96,672.

**Expected outcomes:**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Depth > 96,672 | Cross-dimensional pruning reduces the meeting band; four-direction pincer is effective |
| H2 (Neutral) | Depth &asymp; 96,672 | Column hashes provide no additional pruning power; meeting band is axis-independent |
| H3 (Regression) | Depth < 96,672 | Row-pair granularity (2 rows per hash) is coarser than per-row; the pruning loss outweighs the column-axis gain |

If H3 occurs, the regression isolates the cost of paired-row hashing. Compare against B.41b to distinguish the row-pair granularity effect from the column-hash benefit.

### B.41.7 Sub-experiment B.41b: Half-Hash Configuration + Four-Direction Pincer

**Hash configuration:** Hash only odd-indexed rows and odd-indexed columns:

- 256 row hashes: SHA-1 of row $2p+1$ for $p = 0, \ldots, 254$ (rows 1, 3, 5, ..., 509). Each hash covers a single row (511 bits + 1 padding bit = 512 bits = 64 bytes).
- 256 column hashes: SHA-1 of column $2q+1$ for $q = 0, \ldots, 254$ (columns 1, 3, 5, ..., 509). Each hash covers a single column (511 bits + 1 padding bit = 512 bits = 64 bytes).

**Payload impact:**

| Component | B.41a | B.41b |
|-----------|-------|-------|
| Row hashes | 256 &times; 160 = 40,960 bits | 256 &times; 160 = 40,960 bits |
| Column hashes | 256 &times; 160 = 40,960 bits | 256 &times; 160 = 40,960 bits |
| Hash total | 81,920 bits | 81,920 bits |

Identical payload to B.41a, but the hash granularity differs: B.41b hashes individual rows/columns (finer verification per hash, but only half the rows and half the columns are verified).

**Solver:** Same four-direction pincer as B.41a. The TD and BU passes verify row hashes only at odd rows (1, 3, 5, ..., 509). The LR and RL passes verify column hashes only at odd columns (1, 3, 5, ..., 509). Even rows and columns have no hash verification and rely solely on cross-sum constraint propagation.

**Verification granularity comparison:**

| Property | B.41a (paired) | B.41b (odd-only) |
|----------|---------------|-----------------|
| Row hash granularity | 2 rows (1,022 bits) per hash | 1 row (511 bits) per hash |
| Column hash granularity | 2 columns (1,022 bits) per hash | 1 column (511 bits) per hash |
| Rows with hash verification | All 511 (via pairs) | 256 of 511 (odd only) |
| Columns with hash verification | All 511 (via pairs) | 256 of 511 (odd only) |
| TD pruning events | 256 (every 2 rows) | 256 (every other row) |
| LR pruning events | 256 (every 2 columns) | 256 (every other column) |

B.41b trades coverage (only half of rows/columns verified) for precision (each hash covers exactly one row or column, making failures more localizable). The unchecked even rows and columns rely on cross-sum propagation from their verified neighbors, which may provide sufficient indirect constraint.

**Expected outcomes:**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (B.41b > B.41a) | B.41b depth exceeds B.41a depth | Finer per-hash granularity matters more than full coverage; single-row hash pruning is more effective than paired-row hash pruning |
| H2 (B.41b &asymp; B.41a) | Depths within 5% | Coverage and granularity are equivalent at this scale; cross-sum propagation fills the gaps |
| H3 (B.41b < B.41a) | B.41b depth below B.41a | Full coverage matters; unchecked even rows/columns create exploitable gaps |

**Collision resistance (B.41b).** With 11 affected rows, approximately 6 land on odd rows (hashed). With 492 affected columns, approximately 246 land on odd columns (hashed). The collision probability is $2^{-160 \times (6 + 246) - 256} = 2^{-160 \times 252 - 256} = 2^{-40,576}$ &mdash; comparable to B.41a.

### B.41.8 Sub-experiment B.41c: Row/Column-Completion Look-Ahead (Conditional on B.41b H1)

**Prerequisite.** B.41b must achieve H1 (depth > 96,672). If B.41b is neutral or regressive, B.41c is not
attempted &mdash; the four-direction pincer does not provide the architectural context that makes
completion look-ahead valuable across both axes.

**Motivation.** B.17 proposed Row-Completion Look-Ahead (RCLA): when a row drops to $u \leq u_{\max}$
unknowns, enumerate all $\binom{u}{\rho}$ cardinality-feasible completions, check each against SHA-1,
and backtrack immediately if none pass. B.17 was never implemented because it is a constant-factor
optimization &mdash; it prunes the same infeasible subtrees that DFS would have pruned, just faster
&mdash; and does not change the depth ceiling in a row-serial-only solver.

B.41b changes the calculus. The four-direction pincer introduces column-serial passes (LR, RL) with
column-hash verification. **Column-Completion Look-Ahead (CCLA)** is the natural dual of RCLA: when
a column drops to $u \leq u_{\max}$ unknowns, enumerate all $\binom{u}{\rho}$ completions consistent
with the column's cardinality constraint, check each against the column hash, and backtrack immediately
if none pass. In B.41b, CCLA applies to the 256 odd columns that have hashes.

With both RCLA and CCLA active, all four passes benefit from look-ahead pruning:

| Pass | Look-ahead type | Hash verified | Trigger |
|------|----------------|---------------|---------|
| TD (top-down) | RCLA | Odd-row SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd row |
| BU (bottom-up) | RCLA | Odd-row SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd row |
| LR (left-right) | CCLA | Odd-column SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd column |
| RL (right-left) | CCLA | Odd-column SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd column |

Even-indexed rows and columns (unhashed in B.41b) cannot trigger look-ahead &mdash; they rely solely
on cross-sum constraint propagation, as in the base B.41b design.

**Phase 1 &mdash; Instrumentation (no behavior change).**

Before implementing look-ahead, instrument the B.41b solver to log the distribution of $u$ and $\rho$
at the moment each hash verification fires (both row and column hashes). This answers B.17's open
question B.17.8(a) in the four-direction context:

- If $u$ is typically near 0 at hash verification (propagation forces most cells before the hash
  fires), look-ahead has no room to trigger and B.41c is moot.
- If $u$ is typically 5&ndash;20 at hash verification (propagation stalls before the row or column is
  complete, leaving cells for branching), look-ahead can trigger and is worth implementing.

The instrumentation is ~10 lines of logging in `RowDecomposedController` and imposes negligible
runtime overhead.

**Phase 2 &mdash; RCLA + CCLA implementation.**

If Phase 1 shows $u \geq 5$ at hash verification for a significant fraction of plateau-band rows
and columns:

1. **Triggering.** Invoke look-ahead when $\binom{u}{\rho} \leq C_{\max}$ (candidate: $C_{\max} = 500$,
   corresponding to ~100 $\mu$s of SHA-1 work). The threshold adapts naturally to the residual:
   extreme $\rho$ (near 0 or $u$) allows larger $u$ values; $\rho \approx u/2$ requires smaller $u$.
   Use a precomputed $\binom{u}{\rho}$ lookup table ($\leq 1$ MB).

2. **Cross-line pre-filtering (B.17.3).** Before checking SHA-1, filter each candidate completion
   against cross-line feasibility (column, diagonal, anti-diagonal, and LTP residuals for RCLA;
   row, diagonal, anti-diagonal, and LTP residuals for CCLA). Cost: $O(8u)$ per candidate. This
   may reduce the SHA-1 candidate count by 2&ndash;10&times; in the plateau band, where cross-line
   residuals are moderately constrained.

3. **DI consistency.** Look-ahead is purely deductive and deterministic (B.17.6). It prunes only
   provably infeasible subtrees. The enumeration order (lexicographic over the $u$ unknown positions)
   is fixed. The solver visits the same solutions in the same order, but backtracks sooner on doomed
   rows and columns. DI semantics are preserved.

**Cost-benefit estimate.**

| Metric | Without look-ahead | With look-ahead (u=10, &rho;=5) |
|--------|-------------------|-------------------------------|
| Cost per infeasible row/column | $\binom{10}{5} \times 10 \times 3\mu s \approx 7.5$ ms (DFS) | $\binom{10}{5} \times 1\mu s \approx 250\mu s$ (enumerate) |
| Speedup per infeasible detection | &mdash; | ~30&times; |
| Depth improvement | &mdash; | None (same subtrees pruned) |
| Iteration rate improvement | &mdash; | Higher (more iterations per wall-clock second) |

Look-ahead is a constant-factor optimization: it increases the iteration rate in the plateau band,
allowing the solver to explore more of the search space per unit time. It does not change the depth
ceiling directly. However, in a time-bounded decompression context, faster iteration translates to
deeper effective exploration.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | B.41c iteration rate &gt; B.41b iteration rate by &geq; 20% in plateau band | Look-ahead triggers frequently; cross-line pre-filtering effective; worth keeping |
| H2 (Neutral) | Iteration rate within 5% | Look-ahead triggers rarely ($u$ near 0 at hash verification); Phase 1 instrumentation was correct to gate Phase 2 |
| H3 (Regression) | Iteration rate decreases | Look-ahead overhead (enumeration + SHA-1) exceeds DFS overhead for the observed $u$ distribution; revert |

### B.41.9 Implementation Notes

Both B.41a and B.41b require:

1. **Format change:** Add column hashes to the block payload (Section 12). Increment format version. Column hash messages are serialized column-major: for column $c$, the message is the sequence of bits $M[0][c], M[1][c], \ldots, M[510][c]$.

2. **Compressor change:** Compute column hashes after CSM construction, serialize into payload.

3. **Decompressor/solver change:** Implement four-direction DFS in `RowDecomposedController`. Add column-serial cell ordering. Add column-hash verification at column-pair (B.41a) or odd-column (B.41b) completion. Track four frontiers.

4. **LTP table:** Use the B.38-optimized table (`b38e_t31000000_best_s137.bin`) for both experiments to isolate the hash/solver effect from LTP geometry.

B.41c additionally requires:

5. **Phase 1 instrumentation:** Log $u$ and $\rho$ at each hash verification event (row and column) in the B.41b solver. Emit to JSON for offline analysis. ~10 lines in `RowDecomposedController`.

6. **Phase 2 look-ahead:** Precomputed $\binom{u}{\rho}$ lookup table ($u, \rho \leq 511$). Row-completion enumerator and column-completion enumerator sharing common combinatorial generation logic. Cross-line pre-filter checking 8 constraint lines per candidate. ~100&ndash;150 lines total.

### B.41.10 Relationship to Prior Work

**B.31 (Alternating-Direction Pincer: Null).** B.41 is a direct successor to B.31, addressing the root cause of its failure: the absence of column-axis verification. B.31 alternated between top-down and bottom-up passes along the same (row) axis, gaining no new dimensional coverage. B.41 adds column hashes and column-serial passes, providing genuine cross-dimensional pruning.

**B.39a (Null-Space Analysis).** The minimum swap structure (11 rows, 492 columns) directly motivates the column-hash design: the 96% column coverage of the lightest swap means column hashes are maximally effective at detecting constraint-preserving perturbations. The B.39a result also explains why per-row-only hashing leaves the column dimension unpoliced.

**B.32 (Four-Direction Iterative Pincer: Proposed).** B.32 proposed a four-direction pincer but was never implemented because B.31 (its two-direction prerequisite) produced null results. B.41 supersedes B.32 by addressing the verification gap that made B.31 ineffective.

**B.17 (Row-Completion Look-Ahead: Proposed).** B.41c generalizes B.17's RCLA to both row and column axes, exploiting B.41b's column hashes for Column-Completion Look-Ahead (CCLA). B.17 was never implemented as a standalone experiment because it is a constant-factor optimization that does not change the depth ceiling in a row-serial-only solver. In the four-direction pincer context, CCLA gives the LR/RL passes a pruning acceleration mechanism that did not previously exist.

### B.41.10 Diagnostic Results (Completed &mdash; Infeasible for Depth)

Before implementing the full four-direction solver and format changes, diagnostic instrumentation was added to measure column completion at the plateau. The solver was run on a synthetic random 50%-density CSM block using the B.38-optimized LTP table.

**Column vs. row unknown counts at peak depth 86,125:**

| Metric | Row axis | Column axis |
|--------|----------|-------------|
| Min nonzero unknown | 2 | 206 |
| Axes fully complete | 168 / 511 | 8 / 511 |
| Cells needed for next verification event | 2 | 206 |

**Interpretation.** Columns are **far less complete** than rows at the plateau. The minimum column unknown count is 206, meaning even the closest-to-completion column has 206 unassigned cells. Column-serial passes would need to assign ~206 cells before any column-hash verification could fire&mdash;approximately 100x worse than the row situation (where some rows are within 2 cells of completion).

**Root cause.** Columns span all 511 rows, including the ~343 rows of the meeting band (rows ~168-510). A column cannot complete until its meeting-band cells are assigned. Since the meeting band is precisely the region where constraint propagation has exhausted, columns inherit the meeting band's intractability in full. Row-serial DFS at least completes rows in the propagation zone (rows 0-167); column-serial DFS cannot complete ANY column until the meeting band is traversed, which is the problem it was supposed to solve.

**Conclusion.** The B.41 four-direction pincer will not improve solver depth. The meeting band's constraint exhaustion is not axis-specific&mdash;it affects columns more severely than rows. B.41's column hashes remain valuable for collision resistance (&sect;B.41.3 shows $2^{39,360}$ improvement in collision resistance at the same payload cost), but the solver architecture does not benefit from column-serial passes.

**B.41a/B.41b sub-experiments: NOT ATTEMPTED** (infeasible per diagnostic result).

**Status: DIAGNOSTIC COMPLETE &mdash; INFEASIBLE FOR DEPTH IMPROVEMENT. Collision resistance value retained.**

---

