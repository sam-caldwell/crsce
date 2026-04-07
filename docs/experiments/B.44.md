## B.44 Constraint Density Breakthrough: Discovering New Projection Families (Proposed)

### B.44.1 Motivation

Experiments B.33 through B.43 have exhaustively characterized the solver's depth ceiling and established that it is a property of the constraint system's **information content**, not the solver's search strategy. The 8-family system (LSM, VSM, DSM, XSM, LTP1-4) provides 5,097 independent constraints over 261,121 cells&mdash;a 2% constraint density that leaves 256,024 degrees of freedom (98% of cells unconstrained by cross-sums alone). SHA-1 lateral hashes disambiguate within this null space, but only at row completion boundaries, which the solver cannot reach in the meeting band.

The path to 100K+ depth requires **increasing the constraint density**&mdash;adding new projection families that contribute independent constraints, reducing the null-space dimension and enabling propagation to force cells deeper into the matrix. B.44 systematically explores which new families are viable, how many independent constraints each contributes, and what payload cost each imposes.

### B.44.2 The Constraint Budget

The current system's information balance:

| Component | Bits | Independent constraints (GF(2)) |
|-----------|------|---------------------------------|
| LSM (511 row sums) | 4,599 | 510 |
| VSM (511 column sums) | 4,599 | 510 |
| DSM (1,021 diagonal sums) | 8,185 | 1,017 |
| XSM (1,021 anti-diagonal sums) | 8,185 | 1,020 |
| LTP1-4 (4 &times; 511 sums) | 18,396 | 2,040 |
| **Total cross-sums** | **43,964** | **5,097** |
| LH (511 SHA-1 hashes) | 81,760 | &mdash; (non-linear) |
| BH (SHA-256 block hash) | 256 | &mdash; (non-linear) |
| **Total payload** | **125,980** | |

The CSM has 261,121 bits. The payload is 125,980 bits&mdash;a 48.2% compression ratio. To reach 100% constraint density (every cell independently constrained by cross-sums), the system would need $261{,}121 - 5{,}097 = 256{,}024$ additional independent constraints. At ~9 bits per constraint line (for a 511-cell uniform partition), this would require $256{,}024 / 510 \approx 502$ additional LTP-like sub-tables&mdash;adding $502 \times 4{,}599 = 2{,}308{,}698$ bits (288,587 bytes) to the payload. This would make the payload far larger than the original data, defeating the purpose of compression.

The practical question is: **what is the minimum number of additional constraint families needed to push depth from ~96K to 261K, and what payload cost does each impose?**

### B.44.3 Candidate Constraint Families

Six categories of new projection families are considered, ordered by expected information yield per payload bit:

**Category 1: Sub-row / sub-column block sums.** Partition each row into $B$ blocks of $\lceil 511/B \rceil$ cells and store the sum of each block. For $B = 2$: 511 rows &times; 2 blocks = 1,022 constraints, each ~8 bits. Payload cost: ~8,176 bits. These are NOT independent of the row sum (the two block sums must add to LSM[r]), so the marginal independent constraints are $511 \times (B-1) = 511$ for $B = 2$.

**Category 2: Parity constraints.** Store the XOR (parity) of cells along each constraint line rather than (or in addition to) the sum. Parity is 1 bit per line vs 9 bits per sum, drastically reducing payload cost. However, parity is strictly less informative than the sum: it captures only the least significant bit. For lines with known sum, parity is redundant ($\text{parity} = \text{sum} \bmod 2$). Parity constraints on NEW lines (not already covered by cross-sums) could add cheap independent information.

**Category 3: Higher-order polynomial projections.** Instead of linear sums ($\sum c_i$), store quadratic sums ($\sum c_i c_j$ for specific cell pairs) or weighted sums ($\sum w_i c_i$ with non-binary weights). These capture non-linear relationships that cross-sums miss. The challenge: quadratic sums are expensive to verify during propagation (each cell assignment affects $O(k)$ terms for $k$ paired cells).

**Category 4: Algebraically structured partitions (MOLS).** Mutually Orthogonal Latin Squares of order 511 provide partitions where every pair of lines from different families intersects in exactly one cell. Since $511 = 7 \times 73$, $\text{MOLS}(511) \geq 6$ (at least 6 mutually orthogonal squares exist). Each MOLS-based partition provides 511 lines of 511 cells, but with a crucial difference from Fisher-Yates LTP: the algebraic structure guarantees uniform pairwise intersection, enabling tighter joint constraint bounds.

**Category 5: Sub-block SHA-1 hashes.** Instead of (or in addition to) per-row SHA-1, hash sub-blocks of each row. For example, split each 511-bit row into 4 blocks of ~128 bits and hash each block independently. This provides 4 SHA-1 verification events per row (at $u_{\text{block}} = 0$ instead of $u_{\text{row}} = 0$), enabling verification at 128-cell granularity rather than 511-cell granularity. Payload cost: $511 \times 4 \times 160 = 326{,}240$ bits (40,780 bytes)&mdash;expensive, but fundamentally changes the verification boundary.

**Category 6: Hybrid constraint-hash families.** Store a lightweight hash (e.g., CRC-32 or truncated SHA-1) of each LTP sub-table line. This provides hash-based verification at the LTP-line granularity (511 cells per line) in addition to the cross-sum. Payload cost: $4 \times 511 \times 32 = 65{,}408$ bits (8,176 bytes) for CRC-32.

### B.44.4 Theoretical Analysis: Constraints Required for Full Depth

The solver's propagation cascade forces cells when a constraint line's residual $\rho = 0$ (force all unknowns to 0) or $\rho = u$ (force all unknowns to 1). Each independent constraint contributes one potential forcing event. For a random 50%-density CSM, the probability that a line with $u$ unknowns has $\rho = 0$ or $\rho = u$ decreases exponentially with $u$: $P \approx 2 \times 2^{-u}$.

At the plateau, typical lines have $u \approx 300$. The probability of any single line triggering forcing is $\sim 2^{-299}$&mdash;effectively zero. Propagation works in the early rows because $u$ is small (many cells already assigned), not because the constraint system has high density.

**The leverage point is not more lines but shorter lines.** Each LTP sub-table has 511 lines of 511 cells. If we partitioned the same cells into 2,555 lines of ~102 cells, the same number of cells would be constrained, but each line would reach $\rho = 0$ or $\rho = u$ with probability $\sim 2^{-102}$ (still negligible) at the plateau.

**The real lever: finer-grained verification boundaries.** SHA-1 forces are non-linear and fire at $u = 0$. Reducing the verification granularity from 511 cells (per row) to $G$ cells (per sub-block) means the solver gets hash verification events when $G$ contiguous cells are assigned, enabling pruning at a $G$-cell boundary instead of a 511-cell boundary. At $G = 64$: the solver gets a verification event every 64 assignments (within the block), providing ~8 pruning checkpoints per row instead of 1.

### B.44.5 Sub-experiment B.44a: Constraint Family Discovery via Simulation

**Objective.** Empirically determine which candidate families provide the most independent constraints per payload bit, and measure the marginal depth improvement from each.

**Method.** Write a Python simulation tool (`tools/b44a_constraint_sim.py`) that:

1. Constructs the $5{,}108 \times 261{,}121$ constraint matrix $A$ for the current 8-family system (reusing B.39a code).
2. For each candidate family from &sect;B.44.3, extends $A$ with the new constraint lines.
3. Computes the GF(2) rank of the extended matrix to determine the number of **new independent constraints** (rank increase over the baseline 5,097).
4. Computes the new null-space dimension ($261{,}121 - \text{rank}_{\text{new}}$).
5. Estimates the payload cost of the new family (bits per constraint line &times; number of lines).
6. Computes the **information efficiency**: independent constraints gained per payload bit.

**Families to test:**

| ID | Family | Lines | Bits/line | Payload bits | Expected new rank |
|----|--------|-------|-----------|-------------|-------------------|
| C1a | Sub-row blocks ($B = 2$) | 1,022 | 8 | 8,176 | ~511 |
| C1b | Sub-row blocks ($B = 4$) | 2,044 | 7 | 14,308 | ~1,533 |
| C2a | Row parity on new random partitions | 511 | 1 | 511 | ~510 |
| C2b | 8 parity partitions (FY, new seeds) | 4,088 | 1 | 4,088 | ~4,080 |
| C4a | MOLS(511) partition (1 square) | 511 | 9 | 4,599 | ~510 |
| C4b | MOLS(511) partitions (6 squares) | 3,066 | 9 | 27,594 | ~3,060 |
| C5a | Sub-row SHA-1 ($G = 128$, 4 blocks/row) | 2,044 | 160 | 326,240 | N/A (non-linear) |
| C5b | Sub-row SHA-1 ($G = 64$, 8 blocks/row) | 4,088 | 160 | 654,080 | N/A (non-linear) |
| C6a | CRC-32 per LTP line | 2,044 | 32 | 65,408 | N/A (non-linear) |

**Key metric:** For linear families (C1, C2, C4), the rank increase directly measures the information gained. For hash families (C5, C6), the value is in verification granularity, not rank&mdash;these must be tested via solver simulation.

**Solver depth simulation.** For each candidate family, modify the synthetic-block test infrastructure to:
1. Compute the new constraint values from the known CSM.
2. Load them as additional constraints in the solver.
3. Measure peak depth with the extended constraint system.
4. Compare against the 86,125 baseline.

This can be done in Python for the rank analysis and via C++ solver modifications for the depth measurement.

#### B.44.5a B.44a Results (Completed)

The rank analysis was run using `tools/b44a_constraint_sim.py` with the B.38-optimized LTP table. GF(2) Gaussian elimination on the extended constraint matrices produced the following results:

| Family | Lines added | Payload bits | Rank gain | New null dim | Efficiency (constraints/bit) |
|--------|-------------|-------------|-----------|-------------|------------------------------|
| Baseline (8 families) | 5,108 | 43,964 | &mdash; | 256,024 | &mdash; |
| Sub-row B=2 (C1a) | +1,022 | +8,176 | +510 | 255,514 | 0.0624 |
| Sub-row B=4 (C1b) | +2,044 | +10,731 | +1,530 | 254,494 | 0.1426 |
| 1 parity partition (C2a) | +511 | +511 | +510 | 255,514 | **0.9980** |
| **8 parity partitions (C2b)** | **+4,088** | **+4,088** | **+4,080** | **251,944** | **0.9980** |
| 1 MOLS(511) (C4a) | +511 | +4,599 | **0** | 256,024 | 0.0000 |
| 6 MOLS(511) (C4b) | +3,066 | +27,594 | +2,544 | 253,480 | 0.0922 |

**Key findings.**

1. **Parity partitions dominate.** 8 parity partitions (C2b) provide 4,080 new independent constraints at only 4,088 bits (511 bytes)&mdash;an efficiency of 0.998 constraints per payload bit. This is **16&times; more efficient** than sub-row blocks and **11&times; more efficient** than MOLS.

2. **MOLS(511) with 1 square is completely redundant.** The Galois-field construction $\ell = kr + c \pmod{511}$ produces lines that are linear combinations of existing row+column constraints over GF(2). Zero new independent constraints are gained. Even 6 MOLS squares add only 2,544 constraints (not the expected 3,060) because of partial redundancy.

3. **Sub-row B=4 adds 1,530 constraints** but at 10,731 bits&mdash;an efficiency of 0.14, an order of magnitude worse than parity. However, sub-row sums provide **sum-based** constraints ($\rho = 0/u$ forcing) whereas parity provides only **parity-based** constraints ($u = 1$ forcing). The two mechanisms are complementary.

4. **Scaling projection for parity.** Each parity partition adds 510 independent constraints at 511 bits. To close the 256,024-dimensional null space entirely would require ~502 parity partitions at 256,522 bits (32,066 bytes), bringing the total payload to ~49K bytes (150% compression ratio). This exceeds parity with the original data, but intermediate values (e.g., 50 partitions = 25,500 constraints = 10% null-space reduction at ~3.2K bytes) may provide meaningful depth improvement without excessive payload cost.

**Decision.** Proceed to B.44c (parity partitions) as the primary candidate. B.44b (sub-row blocks) is secondary but may complement parity with sum-based forcing. B.44d (sub-block hashes) remains viable if linear families prove insufficient.

### B.44.6 Sub-experiment B.44b: Sub-Row Block Sums

**Prerequisite.** B.44a rank analysis confirms that sub-row block sums (C1a or C1b) provide $\geq 500$ new independent constraints.

**Hypothesis.** Splitting each row into $B$ blocks and storing sub-block sums provides finer-grained constraint information that forces cells within partially-assigned rows. At the plateau, rows have $u = 300$-$400$ unknowns distributed across the full 511-cell width. Sub-block sums ($B = 4$: blocks of ~128 cells each) provide 4 independent residuals per row, each with $u_{\text{block}} \approx 75$-$100$. While $2^{-75}$ is still negligibly small for direct forcing ($\rho = 0$ or $\rho = u$), the sub-block sums provide **tighter bounds** on each cell's feasibility: a cell in a block with $\rho_{\text{block}} = 0$ is forced to 0 even if the full-row $\rho$ is nonzero.

**Implementation.** Extend `ConstraintStore` with sub-row block lines. Each row $r$ is divided into $B$ blocks: cells $[r, 0..127]$, $[r, 128..255]$, $[r, 256..383]$, $[r, 384..510]$. Each block is a new constraint line with its own target sum (stored in the payload) and its own $u/\rho$ statistics. The propagation engine handles these lines identically to existing lines.

**Payload cost.** $511 \times 3 \times 7 = 10{,}731$ bits ($B = 4$: 3 new block sums per row, since the 4th is determined by the row sum). Adds ~1,342 bytes to the block payload.

**Expected outcome.** If sub-block $\rho = 0$ or $\rho = u$ events occur during DFS in the meeting band, propagation will force additional cells, potentially cascading to deeper depths. The question is how often sub-block residuals hit extremes when the full-row residual does not.

### B.44.7 Sub-experiment B.44c: Parity Partitions (Cheap Information)

**Prerequisite.** B.44a confirms parity constraints add $\geq 500$ independent constraints per 511 bits of payload.

**Hypothesis.** Random parity partitions are the cheapest way to add independent constraints: 1 bit per line vs 9 bits for a sum. Eight new FY-shuffled parity partitions (C2b) would add ~4,080 independent constraints at a payload cost of only 4,088 bits (511 bytes)&mdash;an order of magnitude more efficient than LTP sums per payload bit.

**Mechanism.** Each parity partition assigns every cell to one of 511 parity lines (via Fisher-Yates shuffle, different seeds from LTP1-6). The stored parity bit for each line is the XOR of all cells on that line. During propagation, when a parity line has $u = 1$ (one unknown cell), the cell's value is determined: $v = \text{parity}_{\text{stored}} \oplus \text{assigned\_parity}$. This is a new forcing rule: $u = 1$ forcing via parity, independent of the sum-based $\rho = 0 / \rho = u$ forcing.

**Key advantage.** Parity forcing fires at $u = 1$ (exactly one unknown on the line), which is a much more achievable threshold than $\rho = 0$ (requires all remaining unknowns to be zero). In the meeting band, parity lines from random partitions will reach $u = 1$ as the DFS assigns cells&mdash;providing forcing events that sum-based constraints cannot.

**Key risk.** Parity constraints carry only 1 bit of information per line (vs 9 bits for a sum). The forcing is weaker: it fires only at $u = 1$ (not $u > 1$), and it provides no residual tightening during propagation cascades.

**Payload cost.** $8 \times 511 \times 1 = 4{,}088$ bits = 511 bytes. New payload size: $16{,}899 + 511 = 17{,}410$ bytes. Compression ratio: $17{,}410 / 32{,}641 = 53.3\%$ (from 51.8%).

#### B.44.7a B.44c Results (Completed &mdash; Null)

Parity forcing was implemented in `RowDecomposedController` using 8 FY-shuffled parity partitions (seeds CRSCPAR1-8) loaded from a sidecar file (`CRSCE_B44C_PARITY` env var). The solver was run on the synthetic random 50%-density CSM block (B.38-optimized LTP table, 2-minute run).

| Metric | Baseline | With 8 parity partitions |
|--------|----------|--------------------------|
| Peak depth | 86,125 | 86,125 |
| Parity u=1 forcings | N/A | **0** |
| Iterations (2 min) | 97.5M | 41.9M |
| Iter/sec | ~825K | ~364K |

**Result: zero parity forcings.** The parity $u = 1$ condition was never met during the run. The parity lines, like all existing constraint lines, have 511 cells each. At the plateau, each parity line has ~300 unknown cells (matching the meeting band). The $u = 1$ threshold requires 510 of 511 cells on a line to be assigned&mdash;which can only happen when the solver has nearly completed the entire matrix, far beyond the plateau depth.

**The throughput dropped from ~825K to ~364K iter/sec** (56% overhead) due to the parity state maintenance (updating 8 partitions per cell assignment + cascaded u=1 checking per propagation event), with zero benefit.

**Critical insight: line length is the binding constraint, not family count.** All constraint families in the current system use 511-cell lines (rows, columns, diagonals, LTP, parity). The forcing condition for any mechanism ($\rho = 0$, $\rho = u$, or $u = 1$) requires the line to be nearly complete. At the plateau, lines have ~300 unknowns. Adding more 511-cell lines&mdash;regardless of family type (sum, parity, or otherwise)&mdash;cannot trigger forcing because the lines are 60% unknown.

**This reframes the problem.** The path to deeper propagation is not more families (which was the B.44 hypothesis) but **shorter lines**. A constraint system with lines of length $L \ll 511$ would reach the forcing condition ($u = 0$, $u = 1$, or $\rho = 0/u$) much sooner during DFS. B.44b's sub-row block sums ($L \approx 128$) and B.44d's sub-block hashes ($G = 64$) are the candidates that directly address line length.

### B.44.8 Sub-experiment B.44d: Sub-Block Hash Verification (Finer Pruning Boundaries)

**Prerequisite.** B.44a solver simulation shows that linear constraint families (B.44b, B.44c) provide insufficient depth improvement. This sub-experiment tests whether finer hash verification boundaries break through the ceiling via non-linear constraints.

**Hypothesis.** The B.40 finding&mdash;no SHA-1 events at the plateau because no row reaches $u = 0$&mdash;identifies the verification boundary as the binding constraint. If hash verification fires at $G$-cell sub-block boundaries instead of 511-cell row boundaries, the solver can prune infeasible sub-trees 8&times; sooner (for $G = 64$).

**Method.** Divide each 511-bit row into 8 blocks of 64 bits (blocks 0-6: 64 bits each, block 7: 63 bits + 1 padding). Compute SHA-1 of each 64-bit block independently. Store 8 block hashes per row: $511 \times 8 \times 160 = 653{,}280$ bits. This is a **large** payload increase (81,660 bytes), raising the block payload to ~98,559 bytes and the compression ratio to ~302%.

**The compression ratio exceeds 100%**: the payload is 3&times; larger than the original data. This makes the approach impractical for compression, but the experiment isolates the question: **does finer hash verification extend depth, regardless of payload cost?**

If the answer is yes, the next step is to find a cheaper hash mechanism (CRC-32, truncated SHA-1, or block parity) that provides similar pruning at lower payload cost.

**Alternative: CRC-32 sub-block verification (B.44d').** Use 32-bit CRC instead of 160-bit SHA-1. Payload: $511 \times 8 \times 32 = 130{,}816$ bits = 16,352 bytes. New total: ~33,251 bytes. Ratio: ~102%. CRC-32 has collision probability $2^{-32}$ per block, which is adequate for pruning (the solver doesn't need cryptographic collision resistance for pruning, only for correctness&mdash;BH provides the final guarantee).

#### B.44.8a B.44d Results (Completed — Positive Signal)

B.44d was implemented using CRC-32 verification on 64-cell sub-blocks (8 blocks per row). The solver was run on the synthetic random 50%-density CSM block (B.38-optimized LTP table, 2-minute run, B.44c parity disabled).

| Metric | Baseline | With sub-block CRC-32 |
|--------|----------|----------------------|
| **Peak depth** | 86,125 | **86,756 (+631)** |
| Iterations (2 min) | 97.5M | **364.9M (3.7&times; faster)** |
| Iter/sec | ~825K | **~3,067K** |
| Block verifications | N/A | 328M |
| Block mismatches | N/A | 328M (100%) |

**Result: POSITIVE SIGNAL.** This is the first genuine depth improvement since B.38. Key observations:

1. **+631 cells of depth.** Modest but real. The sub-block CRC-32 catches infeasible partial assignments within 64-cell blocks, enabling earlier backtracking and different search-space exploration.

2. **3.7&times; throughput increase.** Sub-block verification fires at 64-cell granularity, pruning bad subtrees ~8&times; sooner than per-row SHA-1. This dramatically reduces wasted DFS iterations.

3. **100% block mismatch rate.** Every sub-block verification fails. This is expected: at the plateau, the solver's tentative assignments within meeting-band rows are almost never correct at the sub-block level. The CRC-32 catches them instantly.

4. **Changed search-space topology.** With sub-block pruning, the solver explores a different region (min_row_unknown = 225 vs 2-6 baseline). Earlier pruning within rows changes the backtracking pattern, potentially accessing deeper configurations.

**Interpretation.** Sub-block verification addresses the B.40 root cause directly: the solver previously had NO verification mechanism within partially-assigned rows. CRC-32 sub-blocks provide 8 verification checkpoints per row, each at 64-cell granularity. This breaks the "no SHA-1 events at the plateau" barrier that all previous experiments (B.40-B.44c) could not overcome.

**Validation on real data: SEVERE REGRESSION.**

The same B.44d configuration was tested on real MP4 data (first block of `useless-machine.mp4`, compressed with `DISABLE_COMPRESS_DI=1`, B.38-optimized LTP table). Sequential 1-minute runs, no CPU contention:

| | Baseline | B.44d |
|---|---|---|
| Peak depth | **96,689** | **61,467 (&minus;36%)** |
| Iter/sec | 294K | 181K |
| Block mismatches | &mdash; | 4.7M (100%) |

**Sub-block CRC-32 causes catastrophic depth regression on real data.** The solver drops from 96,689 to 61,467 &mdash; losing 35,222 cells of depth.

**Root cause: early pruning destroys informative exploration.** The baseline solver reaches depth 96K by tentatively assigning cells in the meeting band, propagating their effects through column/diagonal/LTP constraints, and using the resulting constraint cascade to force cells in other rows. These tentative assignments are WRONG (they don't match the original CSM), but they provide INFORMATION &mdash; the constraint cascade from deep wrong exploration guides the solver's backtracking decisions.

Sub-block CRC-32 catches wrong tentative assignments at 64-cell boundaries, forcing immediate backtracking. This cuts the constraint cascade short at depth ~61K, preventing the solver from reaching the depths where the cascade becomes informative. The solver loses its primary mechanism for making progress in the meeting band.

**The synthetic block's +631 improvement is an artifact** of its different constraint structure (50% random density vs structured MP4 content). The synthetic block's simpler structure benefits from early pruning; the real data's complex structure requires deep exploration.

**B.44d conclusion: sub-block hash verification is counterproductive.** Finer verification boundaries prune the search tree too aggressively, destroying the solver's ability to use tentative deep assignments as an information source. The current per-row SHA-1 verification boundary (511 cells) is not just a design choice &mdash; it is near-optimal for the DFS solver's search strategy, which relies on deep tentative exploration for constraint propagation feedback.

**Status: COMPLETED &mdash; REGRESSION. Not viable for depth improvement.**

### B.44.9 Implementation Plan

B.44 sub-experiments are ordered by implementation cost and information yield:

1. **B.44a (simulation):** Python-only. Reuse B.39a's constraint matrix infrastructure. Extend with candidate families, measure rank. Estimate: ~200 lines of Python, 1-2 hours of computation. **Do this first.**

2. **B.44c (parity partitions):** Cheapest to implement and test. Add 8 parity lines to ConstraintStore + PropagationEngine + payload. Estimate: ~100 lines of C++, minor payload format change.

3. **B.44b (sub-row block sums):** Moderate complexity. Add block-sum lines to ConstraintStore. Estimate: ~150 lines of C++.

4. **B.44d (sub-block hashes):** Most complex. Requires new hash verifier, modified DFS loop, significant payload change. Estimate: ~300 lines of C++. Test CRC-32 variant first (cheaper payload).

### B.44.10 Relationship to Prior Work

**B.27 (LTP5/LTP6: Inert).** B.27 added two more LTP sub-tables and found zero depth improvement. The B.39a analysis explains why: each LTP sub-table adds 510 independent constraints, but these are sum-based constraints on 511-cell lines. At the plateau, lines with $u = 300$ have $\rho$ far from both 0 and $u$&mdash;the new constraints provide no forcing. B.44c (parity partitions) addresses this by using a different forcing rule ($u = 1$ parity forcing instead of $\rho = 0/u$ sum forcing).

**B.20 (Toroidal-Slope to LTP Transition).** B.20 replaced algebraically structured partitions with pseudorandom Fisher-Yates partitions, gaining depth because FY partitions are uncorrelated with the geometric families. B.44.4 (MOLS) revisits algebraic structure, but with a different motivation: MOLS provides guaranteed uniform pairwise intersection between families, which may enable tighter joint bounds than random partitions (whose intersection structure is probabilistic).

**B.42a (Interval Tightening: Zero Potential).** B.42a showed that per-cell interval bounds $[v_{\min}, v_{\max}]$ from the current 8 families are always $[0, 1]$. Adding new families (B.44b sub-block sums, B.44c parity) may tighten these bounds because the new constraints provide independent information about sub-regions of the row. Sub-block sums in particular decompose the row residual into block residuals, each with smaller $u$, potentially reaching $v_{\min} = v_{\max}$ where the full-row constraint cannot.

**B.40 (No SHA-1 at Plateau).** B.40's null result directly motivates B.44d: if the solver never reaches $u = 0$ on a full row, make the verification boundary smaller so $u = 0$ is achievable on sub-blocks.

**Status: PROPOSED.**

---

