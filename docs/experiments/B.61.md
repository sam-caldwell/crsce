## B.61: Overlapping Blocks

### Prerequisites
1. B.60s completed: algebraic combinator solver with BH verification at $C_r = 87.0%$.
2. Sufficient $C_r$ headroom to absorb the compression cost of overlapping columns.

### Hypotheses
1. Some blocks are easier to solve than others. If adjacent blocks share overlapping columns, a solved block donates pre-determined cells to its neighbor, shifting the neighbor's rho/u ratios toward extremes and triggering the IntBound cascade.
2. The 50% density wall (blocks 5&ndash;20 stall at 2,112 cells) can be broken if sufficient pre-determined boundary cells are available from an adjacent solved block.
3. There exists an optimal overlap size $n$ that balances compression cost (each overlapping column is stored in two blocks) against solve-rate improvement.

### B.61a: Baseline Overlapping Compressor and Decompressor

**Objective.** Implement a configurable-overlap compressor and decompressor in C++. Establish the baseline: for each overlap size $n \in \{0, 8, 16, 32, 64\}$, measure how many blocks achieve BH-verified full solve across the MP4 test file.

**Architecture.**

**Compressor changes:**
- Input is partitioned into overlapping blocks. Block $k$ covers columns $[k \times (s - n), k \times (s - n) + s)$ of the input stream, where $s = 127$ and $n$ is the overlap width.
- Each block is a full $127 \times 127$ CSM. Adjacent blocks share $n$ rightmost/leftmost columns.
- Per-block payload is unchanged (VH32 + hybrid DH128/XH128 + geometric + BH + DI).
- The file contains more blocks than non-overlapping: $\lceil L / (s - n) \rceil$ vs $\lceil L / s \rceil$ where $L$ is the total column count.

**Decompressor changes:**
- Blocks are solved sequentially, left to right.
- When solving block $k$ ($k > 0$): the rightmost $n$ columns of block $k-1$'s solution are pre-assigned into block $k$'s leftmost $n$ columns before the combinator fixpoint runs.
- The pre-assigned cells reduce unknowns on boundary constraint lines, potentially triggering IntBound cascade.
- Block 0 is solved without overlap (same as B.60r/s).

**Effective C_r with overlap:**
- Non-overlapping: each input bit appears in exactly 1 block. Total blocks = $\lceil \text{input\_bits} / s^2 \rceil$.
- Overlapping by $n$ columns: each input bit appears in 1 or 2 blocks. Total blocks increases by factor $s / (s - n)$.
- Effective C_r = base C_r $\times$ $s / (s - n)$.

| Overlap $n$ | Blocks factor | Effective C_r (from 87.0% base) |
|-------------|--------------|-------------------------------|
| 0 | 1.00&times; | 87.0% |
| 8 | 1.067&times; | 92.8% |
| 16 | 1.144&times; | 99.5% |
| 32 | 1.337&times; | 116.3% |
| 64 | 2.016&times; | 175.4% |

**Method.**

(a) Implement the overlapping block partitioner in C++ (or as a Python orchestration wrapper around the existing `combinatorSolver`).

(b) Implement the sequential decompression with overlap donation: after solving block $k$, extract the rightmost $n$ columns and inject them as pre-assigned cells into block $k+1$'s solver state before running the fixpoint.

(c) Run on MP4 blocks 0&ndash;20 for each overlap $n \in \{0, 8, 16, 32\}$.

(d) Record per-block: $|D|$, BH verified, density. Record aggregate: total BH-verified full solves, effective C_r.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Cascade propagates) | $n = 16$ or $n = 32$ enables full solve on blocks that stalled at $n = 0$ | Overlap donation triggers the IntBound cascade; CRSCE decompression is viable with overlapping blocks |
| H2 (Insufficient) | No $n \leq 32$ enables additional full solves | Pre-determined boundary columns don't shift rho/u enough; the 50% density wall is not a boundary problem |
| H3 (Large $n$ needed) | $n = 64$ needed | Overlap works but the C_r cost exceeds 100%; not viable for compression |

**Results.** C++ `overlapSolver` on MP4 blocks 0&ndash;20.

| $n$ | C_r | BH-verified solves | Blocks solved |
|-----|------|-------------------|---------------|
| 0 | 87.0% | 1/21 | 2 |
| 8 | 92.8% | 1/21 | 2 |
| 16 | 99.5% | 2/21 | 1, 3 |
| 32 | 116.3% | 2/21 | 1, 3 |

**Key observations:**

1. **Overlap changes block boundaries.** At $n > 0$, blocks are loaded at shifted offsets: block $k$ starts at bit $k \times (127 - n) \times 127$. This means the data in "block 2" changes with $n$. At $n = 0$, block 2 solves; at $n = 16$, the same data appears across blocks 1&ndash;3, and blocks 1 and 3 solve instead.

2. **Cascade donation works but is limited.** At $n = 8$, block 3 receives 1,016 donated cells from solved block 2 but reaches only 74 total &mdash; the donated cells are in the leftmost 8 columns, which doesn't shift enough rho/u ratios on the long interior constraint lines. At $n = 16$, block 4 receives 2,032 donated cells but still only reaches 74.

3. **The 50% density wall persists.** Blocks 5&ndash;20 (~50% density) are unchanged at all overlap values. The donated columns don't help because the entire 127-column interior remains at rho &asymp; u/2.

4. **Net solve count doesn't increase.** $n = 0$ solves 1 block; $n = 16$ solves 2 but at 99.5% C_r. The additional solve comes from block boundary shift (different data falls into solvable density range), not from cascade donation.

**Outcome: H2 (Insufficient at $n \leq 32$).** Overlap donation does not trigger cascade on 50% density blocks. The donated boundary columns are too few to shift the interior's rho/u ratios.

**Tool:** `build/arm64-release/overlapSolver`

**Status: COMPLETE (H2).**

### B.61b: Minimum Overlap for 50% Density Cascade

**Prerequisite.** B.61a baseline established.

**Objective.** Determine the minimum overlap $n$ that triggers full solve on a 50% density block. Take the first 50% density block that is adjacent to a solved block, and sweep $n$ from 1 to 64 in steps of 1. Find the threshold.

**Results.** Swept pre-assigned leftmost columns (n=0 to 64) on blocks 0, 4, and 5 using B.60r hybrid cascade.

**Block 5 (density 0.496):**

| $n$ | $|D|$ | BH |
|-----|-------|----|
| 0 | 2,112 | false |
| 8 | 1,656 | false |
| 16 | 1,376 | false |
| 32 | 1,376 | false |
| 64 | 1,063 | false |

**Block 0 (density 0.149):**

| $n$ | $|D|$ | BH |
|-----|-------|----|
| 0 | 7,454 | false |
| 16 | 5,422 | false |
| 32 | 7,153 | false |
| 64 | 4,021 | false |

**Corrected results** (initial analysis had a bug: `determined` count excluded pre-assigned cells).

**Block 5 (density 0.496):**

| $n$ | Solver $|D|$ | Pre-assigned | Total known | BH |
|-----|------------|-------------|-------------|-----|
| 0 | 2,112 | 0 | 2,112 (13.1%) | false |
| 8 | 1,656 | 1,016 | 2,672 (16.6%) | false |
| 16 | 1,376 | 2,032 | 3,408 (21.1%) | false |
| 32 | 1,376 | 4,064 | 5,440 (33.7%) | false |
| 64 | 1,063 | 8,128 | 9,191 (57.0%) | false |

**Block 0 (density 0.149):**

| $n$ | Solver $|D|$ | Pre-assigned | Total known | BH |
|-----|------------|-------------|-------------|-----|
| 0 | 7,454 | 0 | 7,454 (46.2%) | false |
| 8 | 6,438 | 1,016 | 7,454 (46.2%) | false |
| 16 | 5,422 | 2,032 | 7,454 (46.2%) | false |
| 32 | 7,153 | 4,064 | 11,217 (69.5%) | false |
| 64 | 4,021 | 8,128 | 12,149 (75.3%) | false |

**Analysis.**

1. **Total known cells DO increase with overlap.** Block 5 goes from 2,112 (13%) at $n = 0$ to 9,191 (57%) at $n = 64$. The pre-assigned cells contribute directly. The solver's own contribution decreases (from 2,112 to 1,063) because pre-assigned boundary cells remove short-diagonal GaussElim triggers. But the net total rises.

2. **Block 0 shows a ceiling.** At $n = 8$ and $n = 16$, total stays at 7,454 &mdash; the pre-assigned cells exactly replace cells the solver would have found. At $n = 32$, the total jumps to 11,217 as pre-assignment extends beyond the solver's natural reach.

3. **No block fully solves.** Even at $n = 64$ (half the matrix pre-assigned), block 5 reaches only 57%. The interior remains unsolved because rho &asymp; u/2 persists on all non-boundary constraint lines. Pre-assigning boundary columns shifts boundary-line rho/u but the interior lines are unaffected.

4. **The minimum $n$ for full solve on 50% density may not exist** with this architecture. The IntBound cascade requires rho = 0 or rho = u, which requires the LINE (not just the boundary cells) to have an extreme sum ratio. Pre-assigning boundary cells doesn't change the interior lines' rho/u.

**Outcome: overlap helps but cannot fully solve 50% density blocks.** Total known cells increase linearly with $n$, but the cascade never completes the interior. No $n \leq 64$ achieves BH-verified full solve on a 50% density block.

**Observation.** The boundary overlap only shifts rho/u on constraint lines that cross the boundary &mdash; short diagonals at the edges, rows near the top/bottom. The interior lines (which are the ones stalled at rho &asymp; u/2) don't cross the boundary, so they're unaffected regardless of how many boundary columns we donate.

To break the 50% density wall, we'd need known cells scattered throughout the INTERIOR of the matrix, not concentrated at the edges. That's a fundamentally different architecture than column overlap.

**Status: COMPLETE. No minimum $n$ found; overlap alone insufficient for 50% density.**

### B.61c&ndash;f: ABANDONED

B.61c (asymmetric overlap), B.61d (full-file decompression), B.61e (block ordering), and B.61f (overlap optimization) are all refinements of the boundary overlap mechanism. B.61b demonstrated that boundary donation cannot break the 50% density wall because it only affects edge constraint lines. The interior lines that stall at rho &asymp; u/2 are geometrically isolated from the boundary. No refinement of the overlap architecture changes this fundamental limitation.

**Status: ABANDONED. Boundary overlap does not reach interior constraint lines.**

### B.61 Conclusion

**B.61: FAILED.** The overlapping blocks hypothesis was that solved blocks could donate boundary cells to trigger the IntBound cascade in unsolved neighbors. B.61a confirmed the mechanism works (donation flows, cells are pre-assigned) but B.61b proved it cannot break the 50% density wall. The reason is geometric: the IntBound cascade stalls on interior constraint lines (rows, columns, long diagonals) where rho &asymp; u/2, and boundary-column donation only affects constraint lines that cross the boundary.

The 50% density problem requires known cells in the matrix INTERIOR, not at the edges. This is a different research direction than block overlap.

