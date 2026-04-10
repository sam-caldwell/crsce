## B.50 rLTP Construction Alternatives (Proposed)

### B.50.1 Motivation

Experiments B.47-B.49 established that 2 rLTPs is the optimal count (B.48b: 1.04M forcings, depth unchanged; B.49: 4 rLTPs regresses). However, all rLTPs tested so far use spiral centers at **row 255** (matrix center), placing their short lines at rows 251-259. The solver's plateau is at **row ~189** (depth 96,672 / 511 $\approx$ 189). The short lines are 66 rows beyond where the solver is actually working&mdash;a spatial mismatch that may explain why 1M+ forcings fail to extend depth.

B.50 explores alternative rLTP constructions that maximize information delivery at the plateau row, not the matrix center. Each sub-experiment uses the proven 2-yLTP + 2-rLTP configuration (B.48b sweet spot) with different rLTP geometries.

### B.50.2 Sub-experiment B.50a: Matrix-Center Spirals (Baseline, Completed)

**Construction:** Euclidean-distance spirals centered at (255, 127) and (255, 383). Short lines at rows 251-259.

This is the configuration tested in B.48b and confirmed in the original B.50 run.

| Metric | Value |
|--------|-------|
| Peak depth | 96,510 |
| Forcings | 969,877 |
| Iter/sec | 158K |

**Tool:** `tools/b50_sidecar.py`

**Status: COMPLETED &mdash; DEPTH UNCHANGED.**

### B.50.3 Sub-experiment B.50b: Plateau-Centered Spirals (Completed &mdash; Regression)

**Construction:** Euclidean-distance spirals centered at **(189, 127)** and **(189, 383)** instead of (255, x). Short lines land at rows 185-193&mdash;directly at the propagation frontier where the solver stalls.

**Hypothesis:** Centering rLTPs at the plateau row places short-line forcing exactly where the propagation cascade exhausts. Every forced cell at row ~189 directly extends the propagation zone, unlike forcings at row ~255 which are too deep for the solver to reach.

**Results:**

| Configuration | rLTP centers | Short-line rows | Peak Depth | Forcings |
|---------------|-------------|----------------|-----------|----------|
| B.50a (matrix center) | (255, 127) (255, 383) | 251-259 | 96,510 | 969,877 |
| **B.50b (plateau center)** | **(189, 127) (189, 383)** | **185-193** | **92,538** | **3,265,306** |

**Depth regressed by 3,972 cells (&minus;4.1%)** despite 3.37$\times$ more forcings (3.27M vs 970K).

**Analysis.** Moving the spiral centers to the plateau row produces the opposite of the expected effect. The 3.27M forcings at the propagation frontier are TOO AGGRESSIVE&mdash;they force cells that the solver was using for tentative deep exploration. The DFS relies on branching at the plateau to explore wrong subtrees whose constraint cascades provide feedback for backtracking decisions (the deep exploration paradox, B.44d). Forcings at rows 185-193 override these branching decisions, cutting the cascade feedback loop and reducing depth.

The matrix-center spirals (B.50a, row 255) perform better precisely BECAUSE their short lines are far from the plateau. Their forcings affect cells in the deep meeting band (rows 251-259) that the solver rarely reaches, causing minimal interference with the solver's exploration strategy at the propagation frontier.

**Key finding: forcings are most useful when they are AWAY from the plateau and counterproductive when they are AT the plateau.** This is a spatial corollary of the deep exploration paradox: the solver needs freedom to make mistakes at the propagation frontier, and any mechanism that constrains this freedom&mdash;whether sub-block hashes (B.44d), excessive rLTPs (B.49), or plateau-targeted forcings (B.50b)&mdash;reduces rather than extends depth.

**Tool:** `tools/b50b_sidecar.py`

**Status: COMPLETED &mdash; REGRESSION (&minus;4.1%).**

### B.50.4 Sub-experiment B.50c: Row-Band rLTPs (Proposed)

**Construction:** Instead of Euclidean-distance rings, use **row-distance** ordering. All cells in row 189 are assigned to line 0 (length 1&mdash;but 511 cells share row 189, so line 0 gets cell (189, 0), line 1 gets cells (189, 1) and (188, 0), etc.). More precisely: sort cells by $|r - 189|$ first, then by column within each row-distance band. This creates horizontal strips centered on row 189.

**Hypothesis:** Horizontal bands spanning the full matrix width within 1-2 rows of the plateau maximize constraint interactions at the propagation frontier. Each short line contains cells from the SAME row neighborhood, providing row-aligned constraint information that complements the geometric row sums (LSM).

### B.50.5 Sub-experiment B.50d: Diagonal-Through-Plateau rLTPs (Proposed)

**Construction:** Lines that pass diagonally through the plateau point (189, 255) at various angles. For angle $\theta$, cells are sorted by their perpendicular distance from the line $r - 189 = \tan(\theta) \cdot (c - 255)$. Short lines contain cells closest to this diagonal. Two rLTPs use angles $\theta = \pi/4$ and $\theta = -\pi/4$ (45-degree diagonals through the plateau point).

**Hypothesis:** Diagonal lines through the plateau provide cross-row constraint coupling at the propagation frontier, similar to DSM/XSM but offset to intersect at row 189 instead of the matrix corners.

### B.50.6 Sub-experiment B.50e: Adaptive Plateau-Centered rLTPs (Proposed)

**Construction:** Two-pass approach. First, run the solver without rLTPs to measure the exact plateau row $k^*$ for this specific block. Then construct rLTPs centered at $(k^*, 127)$ and $(k^*, 383)$.

**Hypothesis:** The plateau row varies by block and LTP table. Adaptive centering ensures the rLTP short lines are at the ACTUAL plateau row, not an estimated one. For the B.38-optimized table on MP4 data, $k^* \approx 189$.

**Note:** This requires the compressor to run the solver twice (once to find $k^*$, once with rLTPs to find DI). The first pass is cheap (runs to plateau, aborts, records $k^*$). The rLTP cross-sums are then computed and stored in the payload or sidecar for the second pass and for decompression.

### B.50.7 Sub-experiment B.50f: Elliptical rLTPs (Proposed)

**Construction:** Use an elliptical distance metric:

$$d(r, c) = \sqrt{\alpha^2 (r - r_0)^2 + (c - c_0)^2}$$

with $\alpha = 4$ (row direction stretched 4$\times$). This produces short lines that are narrow horizontal strips (spanning many columns but few rows) rather than circular rings. Centers at $(189, 127)$ and $(189, 383)$.

**Hypothesis:** Elliptical rLTPs create short lines that are compact horizontal bands at the plateau depth, each covering $\sim$64 columns $\times$ 1-2 rows. These bands are similar to sub-row blocks (B.44b) but with pseudorandom cell ordering within each band, avoiding the premature-pruning problem of B.44d's sub-block hashes.

### B.50.8 Implementation Priority

All B.50 sub-experiments use the same 2-yLTP + 2-rLTP sidecar framework. The only change per sub-experiment is the cell-to-line assignment function in the Python sidecar generator. Priority order:

1. **B.50b** (plateau-centered): directly tests row-255 vs row-189 mismatch. Simplest change.
2. **B.50f** (elliptical): tests whether horizontal-band geometry is better than circular rings.
3. **B.50c** (row-band): tests pure row-distance ordering.
4. **B.50d** (diagonal-through-plateau): tests cross-row coupling at the plateau.
5. **B.50e** (adaptive): requires two-pass solver; test only if B.50b shows improvement.

**B.50b result invalidates the plateau-targeting hypothesis.** B.50c-f are deprioritized: if centering rLTPs AT the plateau regresses depth, row-band and elliptical constructions (which also target the plateau) are expected to regress similarly. The optimal rLTP placement (B.50a/B.48b) is AWAY from the plateau, not at it.

**Status: B.50a COMPLETED (baseline). B.50b COMPLETED (regression). B.50c-f DEPRIORITIZED.**

---

