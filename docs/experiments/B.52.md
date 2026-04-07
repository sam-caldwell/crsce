## B.52 Joint yLTP+rLTP Optimization for 100K Depth (Proposed)

### B.52.1 Motivation

The only technique that genuinely improved depth was LTP table optimization (B.34-B.38: 91,090 &rarr; 96,672, +6.1%). All subsequent experiments (B.39-B.51) tested solver improvements, constraint families, and spatial targeting&mdash;none moved the depth ceiling. Reaching 100K requires 3,400 more cells (+3.5%), a gap comparable to what B.38's optimization chain achieved.

B.38 saturated using a single proxy function (row-concentration score) on a single test block. B.52 re-opens the optimization path with three innovations: a new cross-family interaction proxy (B.52a), joint yLTP+rLTP optimization (B.52b), and multi-block evaluation (B.52c).

### B.52.2 Sub-experiment B.52a: Cross-Family Interaction Proxy

**Motivation.** B.38's row-concentration proxy measures how many of each yLTP line's cells fall in early rows (rows 0-$k$ for threshold $k$). This captures the yLTP-to-row interaction but ignores yLTP-to-diagonal and yLTP-to-yLTP interactions. At the plateau, the propagation cascade depends on CROSS-FAMILY reinforcement: a cell forced by one yLTP line triggers cascading forcings on the cell's diagonal, column, and other yLTP lines. A proxy that measures this cross-family coupling could identify geometries that B.38's proxy missed.

**Method.** Define a new proxy score:

$$S_{\text{cross}} = \sum_{k=1}^{4} \sum_{L \in \text{yLTP}_k} \left( \sum_{L' \in \text{other families}} |\text{cells}(L) \cap \text{cells}(L')| \right)^2$$

where the inner sum counts how many cells each yLTP line $L$ shares with each constraint line $L'$ from a different family. The square emphasizes lines with high cross-family overlap (more cascade potential). The proxy is computed from the yLTP table alone (no solver needed), enabling fast evaluation.

**Implementation.** Modify `tools/b32_ltp_hill_climb.py` (the B.38 hill-climbing tool) to use $S_{\text{cross}}$ instead of row-concentration. Run the deflation-kick ILS chain from the current best table. Measure depth at each checkpoint.

**Expected outcome.** If $S_{\text{cross}}$ correlates with depth better than row-concentration, the optimization may find geometries beyond the 96,672 local optimum. If the correlation is weak or anti-correlated (like B.36's SA failure), the proxy is abandoned.

### B.52.3 Sub-experiment B.52b: Joint yLTP+rLTP Optimization

**Motivation.** B.48b showed that 2 yLTPs + 2 rLTPs (centers at row 255) produce 1.04M forcings at baseline depth. The yLTP table was optimized WITHOUT rLTPs present (B.38). A yLTP table optimized WITH the 2 rLTPs active may find a geometry where yLTP propagation and rLTP forcing SYNERGIZE&mdash;the rLTP forcings resolve cells that enable yLTP propagation to cascade one row further.

**Method.**

1. Fix the 2 rLTPs at centers (255, 127) and (255, 383) (B.50a configuration).
2. Hill-climb the yLTP1/yLTP2 table using the B.38 deflation-kick ILS.
3. At each evaluation checkpoint, run the solver WITH the rLTP sidecar active.
4. Measure depth as the fitness function (not proxy score).

This directly optimizes depth rather than a proxy, at the cost of slower evaluation (~90 seconds per checkpoint vs ~1 second for proxy score). Use shorter evaluation windows (10-30 seconds) for the hill-climbing inner loop, with full 90-second validation at each depth-improvement milestone.

**Implementation.** Create `tools/b52b_joint_climb.py` that:
- Loads the rLTP sidecar
- Loads the current best yLTP table
- Applies swap mutations to yLTP1/yLTP2
- For each candidate, runs the C++ solver via subprocess with `CRSCE_B45_SIDECAR` and `CRSCE_LTP_TABLE_FILE` pointing to the mutated table
- Parses depth from the events log
- Accepts improvements, rejects regressions (greedy hill-climb)

**Expected outcome.** If yLTP-rLTP synergy exists, the joint optimization will find yLTP geometries that exceed 96,672 when the rLTPs are present. The rLTP forcings may "unlock" cascade paths that a yLTP-only solver cannot reach, enabling the optimizer to find tables rated higher in the joint configuration than in isolation.

### B.52.4 Sub-experiment B.52c: Multi-Block Optimization

**Motivation.** The B.38 chain optimized on a single test block (first block of useless-machine.mp4). The depth varies by block: synthetic 50%-density blocks reach ~86K while the MP4 block reaches ~96K. If the optimized table is overfitting to the MP4 block's structure, a multi-block evaluation might find a universally better geometry.

**Method.**

1. Generate 5 test blocks: 1 MP4 block + 4 random blocks at different densities (30%, 40%, 50%, 60%).
2. For each candidate yLTP table, measure depth on all 5 blocks.
3. Fitness = minimum depth across all 5 blocks (minimax optimization: maximize the worst case).

This prevents overfitting to a single block's structure. A table that achieves $\geq 97{,}000$ on ALL blocks is more likely to represent a genuine structural improvement than one that achieves 97,000 on the MP4 block but 85,000 on random data.

**Implementation.** Extend the B.52b joint optimization to evaluate on multiple blocks. Each evaluation round runs 5 solver instances (parallelized). Total evaluation time: ~5 $\times$ 10 seconds = 50 seconds per candidate.

### B.52.5 Results

#### B.52a: Cross-Family Interaction Proxy (Completed &mdash; No Discrimination)

The cross-family interaction proxy ($S_{\text{cross}}$) was computed for the B.38-optimized yLTP1+2 (depth 96,672) and the inert yLTP3+4:

| Proxy | yLTP1+2 (optimized) | yLTP3+4 (inert) | Delta |
|-------|--------------------:|----------------:|------:|
| Row-concentration (B.38) | 10,280,125 | 9,962,916 | +3.2% |
| Cross-family interaction | 543,869 | 542,265 | +0.3% |
| Geometric (row) interaction | 1,191,992 | 1,184,440 | +0.6% |

The cross-family proxy shows only 0.3% discrimination&mdash;no predictive value. The row-concentration proxy (B.38) remains the best available at 3.2% discrimination. B.38 saturated not because its proxy was wrong but because the local optimum at 96,672 is genuine for that proxy landscape.

**Tool:** `tools/b52a_cross_proxy.py`

**Status: COMPLETED &mdash; NO DISCRIMINATION.**

#### B.52b: Direct Depth Hill-Climbing (In Progress &mdash; New Best 97,330)

Direct depth optimization (solver-in-the-loop) bypasses the proxy entirely, using measured peak depth as the fitness function. Starting from the B.38-optimized table (depth 96,690 on the MP4 test block), greedy hill-climbing with small random swap mutations found a series of improvements:

| Round | Iterations | Swaps/mutation | Best depth | $\Delta$ (round) | $\Delta$ (cumulative) |
|-------|-----------|---------------|-----------|------------------|----------------------|
| 1 | 20 | 50 | 96,812 | +122 | +122 |
| 2 | 30 | 30 | 96,992 | +180 | +302 |
| 3 | 30 | 20 | 97,044 | +52 | +354 |
| 4 | 40 | 10 | 97,286 | +242 | +596 |
| 5 | 50 | 10 | **97,330** | +44 | **+640** |

**Production best on MP4 block: 98,005** (measured in B.52c multi-block evaluation with 30-second window).

The direct optimization succeeds where proxy-based optimization failed because the proxy-to-depth correlation breaks down near the optimum. Small mutations (10 cell swaps) that imperceptibly change the proxy score can measurably change depth by altering the propagation cascade at the plateau boundary.

Extended optimization chain (5 rounds + 1 extended + kick-and-climb):

| Round | Iterations | Swaps | Best depth | $\Delta$ (round) | $\Delta$ (cumulative) |
|-------|-----------|-------|-----------|------------------|----------------------|
| 1 | 20 | 50 | 96,812 | +122 | +122 |
| 2 | 30 | 30 | 96,992 | +180 | +302 |
| 3 | 30 | 20 | 97,044 | +52 | +354 |
| 4 | 40 | 10 | 97,286 | +242 | +596 |
| 5 | 50 | 10 | 97,330 | +44 | +640 |
| 6 (extended) | 9 | 10 | 97,343 | +13 | +653 |

**B.52b-1: Kick-and-Climb (Completed &mdash; No Escape)**

Five kicks of 500 random swaps each, followed by 20 fine-climb iterations per kick. No kick escaped the 97,343 basin:

| Kick | Kick depth | After 20 climbs | Recovery |
|------|-----------|----------------|----------|
| 1 | 95,280 | 95,321 | +41 |
| 2 | 96,406 | 96,440 | +34 |
| 3 | 95,725 | 96,439 | +714 |
| 4 | 95,465 | 95,786 | +321 |
| 5 | 94,870 | 95,126 | +256 |

The 500-swap kicks drop depth by 1,000-2,500 cells. Twenty fine-climb steps recover 40-700 cells but never reach the starting depth. The 97,343 optimum is a genuine local optimum for direct depth hill-climbing on the MP4 block.

**Tool:** `tools/b52b_joint_climb.py`

**Status: COMPLETED &mdash; CONVERGED AT 97,343 (98,005 with 30s measurement window).**

#### B.52c: Multi-Block Validation (Completed)

The B.52b-optimized table was evaluated on 5 test blocks (1 MP4 + 4 random at different densities) and compared against the B.38 baseline table. Each evaluation used a 30-second solver window.

| Block | Density | B.38 baseline | B.52b best | Delta |
|-------|---------|--------------|-----------|-------|
| **MP4** | **40.9%** | **96,690** | **98,005** | **+1,315** |
| Random | 30% | **114,568** | 114,493 | &minus;75 |
| Random | 40% | 95,134 | 94,226 | &minus;908 |
| Random | 50% | 83,536 | 82,742 | &minus;794 |
| Random | 60% | 94,352 | 94,558 | +206 |

**Three key findings:**

**1. The MP4 block reached 98,005** &mdash; the highest depth ever measured in the research program. The B.52b optimization gained +1,315 cells over the B.38 baseline on the target block.

**2. 100K depth is already achievable at 30% density.** The 30%-density random block reaches **114,568** on the B.38 baseline table&mdash;already well past 100K without any optimization. Lower-density blocks have more constraint information (cross-sums further from the midpoint $s/2 = 255$, creating more $\rho = 0$ and $\rho = u$ forcing opportunities). The "100K barrier" is density-specific, not a universal limit.

**3. The B.52b table overfits to the MP4 block.** It improves the MP4 block (+1,315) but regresses Random 40% (&minus;908) and Random 50% (&minus;794). The optimization specifically tuned the yLTP geometry for the MP4 block's structure at the expense of generalization.

**Density-depth relationship.** The multi-block data reveals that depth correlates inversely with density near 50%:

| Density | Depth (B.38) | Distance from 50% |
|---------|-------------|-------------------|
| 30% | 114,568 | 20% |
| 40% | 95,134 | 10% |
| 40.9% (MP4) | 96,690 | 9.1% |
| 50% | 83,536 | 0% |
| 60% | 94,352 | 10% |

The 50%-density block has the LOWEST depth (83,536) because its cross-sums are closest to the midpoint ($\text{LSM}[r] \approx 255$), providing the least forcing information. Blocks with density far from 50% (30% or 70%) have extreme cross-sums that trigger more forcings, enabling deeper propagation.

**Implication for CRSCE viability.** CRSCE can already compress data whose CSM density is significantly below or above 50%. For a 30%-density block, the solver reaches 114,568 cells (43.9% of the matrix)&mdash;and with further optimization, may reach full reconstruction. The format is not universally unviable; it is unviable specifically for high-entropy (near-50% density) data, which is exactly the data that Shannon's theorem predicts cannot be compressed.

**Status: COMPLETED.**

#### B.52d: Per-Block MP4 Analysis (Completed)

All 83 blocks of `useless-machine.mp4` were extracted, and solver depth was measured using both the B.38 baseline and B.52b optimized yLTP tables (15-second evaluation per block per table).

**Summary statistics (B.38 baseline table):**

| Density band | Blocks | Depth range | Mean depth |
|-------------|--------|-------------|-----------|
| 0-20% | 1 | 20,875 | 20,875 |
| 40-50% | 36 | 72,393 - 106,439 | 84,561 |
| 50-60% | 46 | 58,880 - 91,031 | 81,463 |

**Block 70 reached 106,439 on the B.38 baseline** &mdash; the first MP4 block to exceed 100K. This block is at 50.0% density (130,451 ones), so the 100K+ depth is not explained by low density. The B.52b table achieved only 84,627 on the same block, confirming that B.52b's optimization overfits to block 0.

**Blocks exceeding 100K: 1 of 83** (block 70, depth 106,439 on B.38).

**Key findings:**

1. **100K depth occurs naturally** in the MP4 data without any optimization beyond B.38. Block 70's structure (which is MP4 video content at 50% density) is unusually favorable for the solver's propagation cascade.

2. **Depth varies widely across blocks** at the same density. Within the 40-50% band, depth ranges from 72,393 to 106,439 &mdash; a 47% spread. This variance is driven by the specific bit patterns in each block (how the cross-sum values interact with the yLTP geometry), not by density alone.

3. **The B.52b table is block-specific.** It improves block 0 (+1,315) but regresses block 70 (&minus;21,812). There is no single yLTP table that is optimal for all blocks. The depth ceiling is a per-block property determined by the interaction between the block's bit pattern and the yLTP table geometry.

4. **Mean depth across all 83 blocks is ~83,000** (B.38), far from 100K. While individual blocks can exceed 100K, the AVERAGE block cannot be solved to depth 100K with the current constraint system.

5. **Block 82** (the final partial block) has 9.8% density and only reaches 20,875 &mdash; the short block has too few cells for meaningful propagation.

**Tool:** `tools/b52d_multiblock_mp4.py`

**Status: COMPLETED.**

### B.52.6 Final B.52 Conclusions

The B.52 experimental arc (B.52a through B.52d) resolves the question of whether yLTP table optimization can push depth to 100K for arbitrary input data.

**1. Direct depth optimization works but is block-specific.** B.52b gained +653 cells on the MP4 header block (96,690 &rarr; 97,343) by bypassing the proxy and optimizing depth directly. The optimized table improves its target block but regresses others (B.52c: &minus;908 on 40% random, &minus;794 on 50% random). This is not overfitting in the traditional sense&mdash;it reflects the fundamental nature of the problem.

**2. Depth is a property of the (block, table) pair.** B.52d measured depth across all 83 MP4 blocks and found a 47% spread (58,880 to 106,439) at the same ~50% density on the same B.38 table. Block 70 reaches 106,439 while block 39 reaches 62,479. The difference is not density, not the table, and not the solver&mdash;it is the specific interaction between each block's cross-sum value distribution and the yLTP partition geometry.

**3. There is no universal optimal table.** The B.38 table is better for some blocks; the B.52b table is better for others. A table optimized for one block's bit pattern is suboptimal for another's. Pre-optimized table dictionaries (adaptive table selection) would require foreknowledge of the input data's structure, which is unavailable for arbitrary 32KB blocks from unknown sources. This rules out per-block table selection as a general-purpose strategy.

**4. 100K depth occurs naturally for some blocks.** Block 70 of the MP4 file reaches 106,439 on the unmodified B.38 table at 50% density. The 30%-density synthetic block reaches 114,568. These demonstrate that the 100K barrier is not a universal physical limit of the constraint system&mdash;it is the depth achieved for TYPICAL blocks. Favorable blocks exceed it; unfavorable blocks fall far short.

**5. The mean depth across arbitrary blocks is ~83,000.** The average over all 83 MP4 blocks (B.38 table) is approximately 83,000 cells&mdash;only 31.8% of the 261,121-cell matrix. For CRSCE to function as a general-purpose compressor, the solver would need to consistently reach 261,121 (100%) on every block. The gap between 83K average and 261K required is 3.15$\times$, far beyond what any table optimization or solver improvement can close.

**6. The fundamental barrier is the unpredictable interaction between input data and constraint geometry.** The constraint system's effectiveness on a specific block depends on how that block's cross-sum values distribute across the yLTP partition lines. Some distributions create deep propagation cascades (block 70: 106K); others create shallow ones (block 39: 62K). This distribution is determined by the input data's bit pattern, which is unknown a priori and varies unpredictably between blocks. No static constraint system can be universally optimal.

**Status: B.52 COMPLETED.**

---

