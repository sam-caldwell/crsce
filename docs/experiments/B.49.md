## B.49 Extended Hybrid: 4 Geometric + 2 yLTP + 4 rLTP (Proposed)

### B.49.1 Motivation

B.48b demonstrated that 2 yLTPs + 2 rLTPs produce 1.04M forcings at the plateau without depth regression. B.49 extends this by adding 2 more rLTPs (4 total), each centered on a different vertical quarter of the matrix. The hypothesis is that 4 spatially diverse rLTPs provide denser forcing coverage across the full width of the meeting band, potentially resolving cells that 2 rLTPs miss.

### B.49.2 Constraint System

The B.49 constraint system comprises 10 families (same as B.27's 10-family architecture, but with rLTPs replacing the inert yLTP3-6):

| Family | Type | Lines | Line lengths | Construction |
|--------|------|-------|-------------|--------------|
| LSM | Geometric | 511 | 511 (uniform) | Row sums |
| VSM | Geometric | 511 | 511 (uniform) | Column sums |
| DSM | Geometric | 1,021 | 1-511-1 (variable) | Diagonal sums |
| XSM | Geometric | 1,021 | 1-511-1 (variable) | Anti-diagonal sums |
| yLTP1 | Fisher-Yates | 511 | 511 (uniform) | B.38-optimized seed CRSCLTPV |
| yLTP2 | Fisher-Yates | 511 | 511 (uniform) | B.38-optimized seed CRSCLTPP |
| rLTP1 | Center-spiral | 1,021 | 1-511-1 (variable) | Center (255, 63), columns 0-126 |
| rLTP2 | Center-spiral | 1,021 | 1-511-1 (variable) | Center (255, 191), columns 127-254 |
| rLTP3 | Center-spiral | 1,021 | 1-511-1 (variable) | Center (255, 319), columns 255-382 |
| rLTP4 | Center-spiral | 1,021 | 1-511-1 (variable) | Center (255, 447), columns 383-510 |

**Total lines:** 3,064 (geometric) + 1,022 (yLTP) + 4,084 (rLTP) = 8,170.

### B.49.3 rLTP Construction

Each rLTP sub-table divides the CSM into concentric rings around its center point. The center points are placed at row 255 (center row) in each of the four vertical column quarters:

- **rLTP1:** Center (255, 63) &mdash; midpoint of columns 0-126 (127-column slice)
- **rLTP2:** Center (255, 191) &mdash; midpoint of columns 127-254 (128-column slice)
- **rLTP3:** Center (255, 319) &mdash; midpoint of columns 255-382 (128-column slice)
- **rLTP4:** Center (255, 447) &mdash; midpoint of columns 383-510 (128-column slice)

Each spiral starts from its center point (line 0, length 1) and expands outward. The shortest lines (length 1-10) cluster in rows 251-259, directly within the meeting band. The spirals terminate at the farthest matrix corners, with line 1020 (length 1) being the single cell most distant from the center.

### B.49.4 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Depth $>$ 96,690 | Denser rLTP coverage across 4 column quarters resolves cells that 2 rLTPs cannot |
| H2 (Neutral) | Depth $\approx$ 96,690 | Additional rLTPs provide more forcings but no new resolvable cells |
| H3 (Regression) | Depth $<$ 96,690 | 4 rLTPs create inter-partition correlations that degrade propagation |

Based on B.48b (2 rLTPs = 1.04M forcings, depth unchanged), H2 is the most likely outcome.

### B.49.5 Results (Completed &mdash; Regression)

Tested via B.45 sidecar (2 yLTP + 4 rLTP), augmenting the baseline 4-geometric + 4-yLTP constraint system. MP4 first block, B.38-optimized yLTP table, 90-second run.

| Configuration | Peak Depth | Iter/sec | Forcings |
|---------------|-----------|----------|----------|
| Baseline (4 geo + 4 yLTP) | 96,690 | 294K | N/A |
| B.48b (2 yLTP + 2 rLTP sidecar) | 96,510 | 159K | 1,044,577 |
| **B.49 (2 yLTP + 4 rLTP sidecar)** | **85,775** | **122K** | **3,037,004** |

**Depth regressed by 10,915 cells (&minus;11.3%)** despite 3 million forcings&mdash;triple the B.48b count.

### B.49.6 Analysis

The regression follows the same pattern as B.48a (8 rLTP, &minus;16.8%): more rLTPs degrade depth. The mechanism is the deep exploration paradox identified in B.44d: the solver relies on deep tentative wrong assignments for constraint cascade feedback. Aggressive forcing from 4 rLTPs (3M events) overrides the solver's tentative assignments, cutting the cascade short and reducing the depth the solver can reach before backtracking.

**The sweet spot is 2 rLTPs (B.48b).** This configuration produces 1.04M forcings without regression. Scaling to 4 rLTPs pushes past the threshold where forcing volume begins to interfere with the solver's exploration dynamics.

The 4 rLTP centers are all at row 255 (same row, different columns). When the solver assigns cells in the meeting band, all 4 rLTPs trigger forcing on nearby cells simultaneously, creating cascading forced assignments that constrain the solver's path too aggressively. The inter-partition correlations from same-row centers compound this effect.

### B.49.7 Conclusion

Adding more rLTPs beyond 2 degrades depth. The optimal augmented configuration tested is **4 geometric + 4 yLTP (base) + 2 yLTP + 2 rLTP (sidecar)** from B.48b, which achieves 1.04M forcings at baseline depth. The constraint system's information content remains the binding limit; additional spatially targeted forcings cannot push past the ceiling and, in excess, actively harm the solver's search dynamics.

**Tool:** `tools/b49_sidecar.py`

**Status: COMPLETED &mdash; REGRESSION (&minus;11.3%).**

---

