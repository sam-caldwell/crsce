## B.51 rLTP Spirals Above and Below the Plateau (Completed &mdash; Regression)

### B.51.1 Motivation

B.50b showed that rLTP forcings AT the plateau row (189) regress depth by &minus;4.1%. B.51 tests the opposite extreme: placing rLTP centers well ABOVE and BELOW the plateau to provide constraint information in the propagation zone and deep meeting band while leaving the plateau row entirely free for the solver's DFS exploration.

### B.51.2 Construction

- **rLTP1:** Center (94, 127) &mdash; row 94, midpoint of the propagation zone. Short lines at rows 90-98. Provides constraint information where the solver has already completed most cells via existing propagation.
- **rLTP2:** Center (350, 383) &mdash; row 350, deep in the meeting band. Short lines at rows 346-354. Provides constraint information where the solver rarely reaches.

Augmented with 2 B.38-optimized yLTPs via sidecar. Baseline: 4 geometric + 4 yLTP (from payload).

### B.51.3 Results

| Configuration | rLTP centers | Short-line rows | Peak Depth | Forcings | Iter/sec |
|---------------|-------------|----------------|-----------|----------|----------|
| B.50a (matrix center) | (255,127) (255,383) | 251-259 | 96,510 | 969,877 | 158K |
| B.50b (plateau center) | (189,127) (189,383) | 185-193 | 92,538 | 3,265,306 | 160K |
| **B.51 (above/below)** | **(94,127) (350,383)** | **90-98 / 346-354** | **87,971** | **1,340,915** | **282K** |

**Depth regressed by 8,719 cells (&minus;9.0%).**

### B.51.4 Analysis

The above-plateau rLTP (center row 94, short lines rows 90-98) places forcings in the **propagation zone**&mdash;rows 0-168 where the solver has already assigned most cells via existing geometric + yLTP constraint propagation. These forcings are entirely redundant with the existing constraint system. The solver gains no new information because the propagation zone is already solved.

The below-plateau rLTP (center row 350, short lines rows 346-354) places forcings in the **deep meeting band**&mdash;rows the solver never reaches during its DFS exploration (peak depth $\approx$ 96,690 $\approx$ row 189). These forcings resolve cells that are 160+ rows beyond the solver's frontier, providing no benefit.

The throughput improvement (282K vs 158K iter/sec) reflects the reduced forcing interference: with forcings in redundant/unreachable regions, the sidecar check finds fewer lines to force, reducing overhead. But the depth regression shows that even non-interfering forcings can harm the solver, likely through subtle changes in constraint-store state that alter propagation cascade behavior.

### B.51.5 Complete Spatial Targeting Summary

The B.47-B.51 experimental arc establishes a complete map of rLTP center placement vs depth:

| rLTP center row | Position relative to plateau (~189) | Peak Depth | $\Delta$ vs baseline | Forcings |
|----------------|-------------------------------------|-----------|---------------------|----------|
| 94 + 350 (B.51) | Far above + far below | 87,971 | &minus;9.0% | 1.34M |
| 189 (B.50b) | AT plateau | 92,538 | &minus;4.1% | 3.27M |
| 255 (B.50a) | 66 rows below plateau | **96,510** | **&minus;0.2%** | 970K |
| 255 (B.47) | 66 rows below (2 rLTP only) | 96,510 | &minus;0.2% | 425K |

**The optimal rLTP placement is at the matrix center (row 255)**, approximately 66 rows below the plateau. This placement minimizes interference with the solver's DFS exploration at the propagation frontier while occasionally resolving cells during deep tentative exploration in the mid-meeting-band. Every other placement tested&mdash;at the plateau, above it, below it, or straddling it&mdash;regresses depth.

The spatial targeting conclusion is definitive: **rLTP center placement cannot improve depth beyond baseline.** The best achievable result (B.50a: 96,510, &minus;0.2%) is within measurement noise of the baseline (96,690). The depth ceiling is invariant to the spatial distribution of constraint information.

**Tool:** `tools/b51_sidecar.py`

**Status: COMPLETED &mdash; REGRESSION (&minus;9.0%).**

---

