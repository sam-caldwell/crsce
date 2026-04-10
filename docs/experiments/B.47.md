## B.47 Center-Spiral Variable-Length LTP Partitions (Completed &mdash; Depth Unchanged)

### B.47.1 Motivation

All previous constraint families&mdash;geometric diagonals (DSM/XSM), Fisher-Yates LTP, and random variable-length rLTP&mdash;distribute their short lines (length 1-10) either at the matrix corners (diagonals), uniformly across the matrix (random LTP), or at random positions (rLTP with Fisher-Yates ordering). None specifically target the **plateau region** (~rows 168-340, center of the matrix) where the propagation cascade exhausts and forcing is most needed.

B.47 introduces **center-spiral partitions**: variable-length LTP sub-tables where cells are assigned to lines in order of their Euclidean distance from a center point, rather than in pseudorandom order. This places the shortest lines (length 1-10) directly at the center of the matrix&mdash;in the plateau region&mdash;where they can provide forcing information exactly where the solver stalls.

### B.47.2 Construction

Two spiral partitions are constructed:

- **rLTP-spiral-left:** Center point $(255, 127)$ (center-left of the matrix). Cells sorted by distance from this point. Assigned to 1,021 lines with lengths 1, 2, ..., 511, ..., 2, 1.
- **rLTP-spiral-right:** Center point $(255, 383)$ (center-right of the matrix). Cells sorted by distance from this point. Same line-length pattern.

For each partition, the construction is:

1. Compute Euclidean distance $d(r, c) = \sqrt{(r - r_0)^2 + (c - c_0)^2}$ for each cell.
2. Sort all 261,121 cells by ascending distance.
3. Assign the sorted cells to lines following the 1, 2, ..., 511, ..., 2, 1 pattern.

This ensures:
- **Line 0** (1 cell) = the center point itself.
- **Lines 1-9** (2-10 cells) = the innermost rings around the center, all in **rows 251-259**.
- **Line 510** (511 cells) = a ring of moderate radius, spanning a wide row range.
- **Line 1020** (1 cell) = the farthest cell from the center (a matrix corner).

### B.47.3 Short-Line Placement Comparison

| Partition type | Short lines (len $\leq$ 10) | Row range of short lines | Distance to plateau (~row 168) |
|----------------|----------------------------|-------------------------|-------------------------------|
| DSM/XSM diagonals | 40 | rows 0-9 and 501-510 | ~160 rows away |
| Fisher-Yates rLTP | 40 | random (uniform) | ~85 rows average |
| **Spiral from (255, 127)** | **55** | **rows 251-259** | **~85 rows (center of meeting band)** |
| **Spiral from (255, 383)** | **55** | **rows 251-259** | **~85 rows (center of meeting band)** |

The spiral partitions place 110 short lines in **rows 251-259**, directly within the meeting band.

### B.47.4 Results

The spiral partitions were tested as additional constraints via the B.45 sidecar mechanism (8 uniform LTP + 2 spiral rLTP, loaded via `CRSCE_B45_SIDECAR`). Tested on the MP4 first block with the B.38-optimized LTP table.

| Configuration | Peak Depth | Iter/sec | B.45 Forcings |
|---------------|-----------|----------|---------------|
| Baseline (4+4 only) | 96,690 | 294K | N/A |
| B.45 augmented (8u+3v random rLTP) | 96,673 | 111K | 100,019 |
| **B.47 spiral (8u + 2 spiral rLTP)** | **96,510** | **117K** | **425,211** |

**425,211 forcings**&mdash;4.25$\times$ more than the random rLTP configuration&mdash;confirm that center-focused short lines trigger forcing far more frequently in the meeting band. The spiral geometry successfully targets the plateau region.

However, **depth is 96,510**&mdash;180 cells below baseline (within noise, not an improvement). The 425K forcings convert branching waste into forced assignments at the plateau but do not extend the propagation cascade into new territory.

### B.47.5 Analysis

The spiral partitions answer a precise question: **does spatially targeting short lines at the plateau region improve depth?** The answer is no. The 4.25$\times$ increase in forcings (425K vs 100K) demonstrates that spatial targeting is mechanically effective&mdash;short spiral lines near the plateau DO trigger forcing. But the forcings do not resolve cells that constraint propagation cannot reach. They resolve cells that the DFS was already going to assign correctly.

This confirms the B.42/B.45 finding from a new angle: the depth ceiling is determined by the constraint system's **global information content** (rank 5,097, null-space dimension 256,024), not by the **spatial distribution** of constraint information. Whether short lines are at the corners (diagonals), distributed uniformly (random rLTP), or concentrated at the plateau (spiral rLTP), the same set of cells remains undetermined by the constraint system. Rearranging where the forcing events occur cannot change which cells are forceable.

### B.47.6 Conclusion

The center-spiral partitions are the most spatially targeted constraint family tested in the research program. Their 425K forcings at the plateau (4.25$\times$ the random rLTP rate) confirm that spatial targeting works mechanically but does not affect depth. The depth ceiling of ~96,672 is an information-theoretic property of the constraint system, invariant to the spatial distribution of constraint lines.

**Tool:** `tools/b46_spiral_sidecar.py`

**Status: COMPLETED &mdash; DEPTH UNCHANGED.**

---

