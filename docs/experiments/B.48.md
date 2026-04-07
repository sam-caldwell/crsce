## B.48 All-Spiral rLTP Constraint System (Completed &mdash; Regression)

### B.48.1 Motivation

B.47 demonstrated that center-spiral partitions generate 4.25$\times$ more forcings than random rLTPs by targeting the plateau region. B.48 tests the extreme: replace ALL 8 sidecar LTP sub-tables with spiral rLTPs, using 8 center points distributed across 4 vertical column slices of the matrix. This maximizes spatial targeting at the cost of pseudorandom independence.

### B.48.2 Construction

Eight spiral center points divide the matrix into four vertical quarters, with two spirals per quarter (one at the center row, one offset by 5 rows for diversity):

| Spiral | Center | Column slice | Short-line rows |
|--------|--------|-------------|----------------|
| 0 | (255, 63) | 0-126 | 251-259 |
| 1 | (255, 191) | 127-254 | 251-259 |
| 2 | (255, 319) | 255-382 | 251-259 |
| 3 | (255, 447) | 383-510 | 251-259 |
| 4 | (250, 63) | 0-126 | 246-254 |
| 5 | (250, 191) | 127-254 | 246-254 |
| 6 | (260, 319) | 255-382 | 256-264 |
| 7 | (260, 447) | 383-510 | 256-264 |

Each spiral covers all 261,121 cells with 1,021 variable-length lines (1, 2, ..., 511, ..., 2, 1). Total: 440 short lines (length $\leq$ 10) across rows 246-264.

### B.48.3 Results

Tested via B.45 sidecar (0 uniform LTP + 8 spiral rLTP), augmenting the baseline 4-geometric + 4-LTP constraint system. MP4 first block, B.38-optimized LTP table, 90-second run.

| Configuration | Peak Depth | Iter/sec | Forcings |
|---------------|-----------|----------|----------|
| Baseline (4+4) | 96,690 | 294K | N/A |
| B.47 (8u + 2 spiral) | 96,510 | 117K | 425K |
| **B.48 (0u + 8 spiral)** | **80,446** | **134K** | **612K** |

### B.48.4 Analysis

**Depth regressed by 16,244 cells (&minus;16.8%)** despite producing the highest forcing count (612K) of any experiment. The regression has two causes:

1. **Loss of pseudorandom independence.** The 8 spiral partitions are geometrically structured (distance-based from nearby center points). Cells near any center point have similar distance rankings across multiple spirals, creating correlated line assignments. This correlation reduces the effective independent constraint count far below the theoretical $8 \times 510 = 4{,}080$.

2. **Overhead without benefit.** The B.45 sidecar scans all 8,168 spiral lines after every propagation cascade. At 134K iter/sec (vs 294K baseline), the 54% throughput penalty reduces total iterations explored, and the correlated forcings don't compensate with deeper propagation.

**Key insight:** Spatial structure in constraint lines is counterproductive. The Fisher-Yates pseudorandom construction's value lies precisely in its LACK of spatial structure&mdash;each partition's line assignments are uncorrelated with every other partition and with the geometric families. Spiral partitions sacrifice this independence for spatial targeting, and the independence loss dominates.

### B.48.5 Terminology

To distinguish the two LTP construction methods used in experiments B.45-B.49 and beyond:

- **yLTP** (plural: yLTPs): A Fisher-Yates LTP sub-table with uniform 511-cell lines. Constructed via pool-chained Fisher-Yates shuffle seeded by a 64-bit LCG constant. All 511 lines have exactly 511 cells. The "y" denotes the Fisher-Yates construction. The B.38-optimized yLTP1/yLTP2 (seeds CRSCLTPV/CRSCLTPP) are the production sub-tables achieving depth 96,672.

- **rLTP** (plural: rLTPs): A variable-length spiral LTP sub-table with 1,021 lines following the triangular pattern (lengths 1, 2, ..., 511, ..., 2, 1). Cells are assigned in order of Euclidean distance from a center point, placing the shortest lines near that center. The "r" denotes the radial/spiral construction.

### B.48.6 Sub-experiment B.48b: 2 yLTP + 2 rLTP Hybrid

B.48a's regression (0 yLTP + 8 rLTP) showed that pseudorandom independence is essential. B.48b tests a hybrid: the 2 B.38-optimized yLTPs (yLTP1/yLTP2) for pseudorandom independence plus 2 center-spiral rLTPs for spatial targeting at the plateau.

**Configuration:** 4 geometric + 4 existing LTP (from payload) + sidecar with 2 yLTP (B.38-optimized) + 2 rLTP (centers (255, 127) and (255, 383)).

| Configuration | Peak Depth | Iter/sec | Forcings |
|---------------|-----------|----------|----------|
| Baseline (4 geo + 4 yLTP) | 96,690 | 294K | N/A |
| B.48a (0 yLTP + 8 rLTP) | 80,446 | 134K | 612K |
| **B.48b (2 yLTP + 2 rLTP)** | **96,510** | **159K** | **1,044,577** |

The hybrid produces **1.04 million forcings**&mdash;the highest of any experiment&mdash;with depth at 96,510 (within noise of baseline). The 2 optimized yLTPs preserve propagation cascade quality while the 2 rLTPs provide over a million center-targeted forcings. Depth is unchanged: the forcings resolve cells the solver was already going to assign correctly.

### B.48.7 Conclusion

The yLTP/rLTP hybrid (B.48b) is the optimal configuration tested: it maximizes forcings (1.04M) while maintaining baseline depth. However, more forcings do not translate to more depth. The constraint system's information content (rank 5,097, null-space dimension 256,024) is the binding limit regardless of forcing count or spatial targeting.

**Tools:** `tools/b48_quad_spiral_sidecar.py` (B.48a), `tools/b48b_hybrid_sidecar.py` (B.48b)

**Status: COMPLETED &mdash; B.48a REGRESSION (&minus;16.8%), B.48b DEPTH UNCHANGED.**

---

