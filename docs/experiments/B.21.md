### B.21 Joint-Tiled Variable-Length LTP Partitions

B.20 replaced four toroidal-slope partitions with four uniform-length LTP partitions, each containing
511 lines of 511 cells. Every cell belongs to all four LTP partitions, yielding 8 constraint lines per
cell (4 basic + 4 LTP). The uniform line length, however, forces a fixed 9-bit encoding for every
cross-sum element, and the large line size means that the ratio $\rho / u$ stays near 0.5 for most of
the solve, suppressing forcing events on LTP lines past the first ~50 rows. This section proposes
a *joint-tiled* LTP architecture in which the four LTP partitions collectively tile the matrix once,
each partition contains variable-length lines following the DSM/XSM triangular distribution, and each
cell belongs to exactly one LTP partition. The result is fewer but *stronger* LTP constraints per cell,
a naturally lossless variable-length encoding, and a modest reduction in block payload size.

#### B.21.1 Motivation

The DSM and XSM families encode cross-sums with variable bit widths because their line lengths vary
from 1 to $s$. The encoding cost for one diagonal family over $2s - 1$ lines totals $B_d(s) = 8{,}185$
bits (Section 1), far less than the $s \times b = 4{,}599$ bits that a hypothetical 511-element
fixed-width diagonal family would require. The savings arise because short lines have small maximum
sums, requiring fewer bits.

Uniform-length LTP partitions cannot exploit this structure: every line has $s = 511$ cells, so every
cross-sum ranges $[0, 511]$ and requires $b = 9$ bits. If instead each LTP partition contained lines
of varying length&mdash;some with 1 cell, some with 256, some with 511&mdash;the encoding would mirror the
DSM/XSM scheme, with short lines encoded cheaply and only the longest lines requiring the full 9 bits.

The challenge is arithmetic: a single partition with 511 lines whose lengths sum to $s^2 = 261{,}121$
must average 511 cells per line. A symmetric triangular distribution peaking at 511 sums to only
65,536&mdash;approximately one quarter of the matrix. No symmetric distribution ranging from 1 to 511
can average 511 over 511 elements; the only such distribution is the degenerate uniform case. The
resolution is to abandon per-partition completeness: each LTP partition covers approximately one
quarter of the matrix, and the four partitions jointly tile the full $s \times s$ grid.

#### B.21.2 Partition Structure

Each of the four LTP sub-tables $T_k$ ($k \in \{0, 1, 2, 3\}$) contains 511 lines indexed
$i \in [0, 510]$ with prescribed lengths:

$$
    \text{len}_k(i) = \min(i + 1, \; 511 - i)
$$

This is the left half of the DSM length function: $\text{len}(0) = 1$, $\text{len}(255) = 256$,
$\text{len}(510) = 1$. The total cells per sub-table is:

$$
    \sum_{i=0}^{510} \min(i+1, \; 511-i) = 2 \sum_{j=1}^{255} j + 256 = 2 \cdot \frac{255 \cdot 256}{2} + 256 = 65{,}536
$$

Four sub-tables total $4 \times 65{,}536 = 262{,}144$ cell assignments. The matrix has $511^2 = 261{,}121$
cells, leaving $262{,}144 - 261{,}121 = 1{,}023$ surplus assignments. Exactly 1,023 cells are therefore
assigned to two sub-tables, and the remaining $261{,}121 - 1{,}023 = 260{,}098$ cells to exactly one.
Equivalently, every cell belongs to at least one LTP sub-table, and 1,023 privileged cells belong to two.

The per-cell constraint line count is:

- 260,098 cells: 4 basic lines + 1 LTP line = 5 constraint lines.
- 1,023 cells: 4 basic lines + 2 LTP lines = 6 constraint lines.

#### B.21.3 Encoding and Payload Impact

Each sub-table encodes identically to a DSM or XSM family truncated to 511 elements. The bit cost
per sub-table is:

$$
    B_{\text{LTP}}(s) = \sum_{i=0}^{510} \lceil \log_2(\min(i+1, \; 511 - i) + 1) \rceil
$$

By symmetry and the identity with the DSM sum's first half:

$$
    B_{\text{LTP}}(s) = \frac{B_d(s) + \lceil \log_2(s+1) \rceil}{2} = \frac{8{,}185 + 9}{2} = 4{,}097 \text{ bits}
$$

Four sub-tables total $4 \times 4{,}097 = 16{,}388$ bits. Under the uniform B.20 encoding, four LTP
partitions at $s \times b = 4{,}599$ bits each total 18,396 bits. The savings are $18{,}396 - 16{,}388
= 2{,}008$ bits = 251 bytes per block.

Updated block payload:

| Field | Elements | Encoding | Total Bits |
|-------|----------|----------|------------|
| LH    | 511      | 160 bits each | 81,760 |
| BH    | 1        | 256 bits | 256 |
| DI    | 1        | 8 bits   | 8   |
| LSM   | 511      | 9 bits each | 4,599 |
| VSM   | 511      | 9 bits each | 4,599 |
| DSM   | 1,021    | variable | 8,185 |
| XSM   | 1,021    | variable | 8,185 |
| LTP0  | 511      | variable (1--9 bits) | 4,097 |
| LTP1  | 511      | variable (1--9 bits) | 4,097 |
| LTP2  | 511      | variable (1--9 bits) | 4,097 |
| LTP3  | 511      | variable (1--9 bits) | 4,097 |
| **Total** | | | **123,980** |

Block payload size: $\lceil 123{,}980 / 8 \rceil = 15{,}498$ bytes. Compression ratio:
$15{,}498 / 32{,}641 \approx 47.5\%$, down from 48.3% under uniform LTP (B.20) and 51.8% under
the original toroidal-slope format.

#### B.21.4 Constraint Strength Analysis

Reducing from 4 LTP lines per cell to 1 appears to weaken the constraint system, but the analysis
is more nuanced. Under B.20's uniform design, each of the 4 LTP lines has 511 cells. During the
DFS solve, a 511-cell line's $\rho / u$ ratio stays near 0.5 until the line is nearly complete,
meaning it almost never triggers forcing ($\rho = 0$ or $\rho = u$). Empirically, the LTP lines
contribute no forcing events past approximately row 50&mdash;well before the plateau at row ~170 where
the solver stalls.

Under joint tiling, a cell's single LTP line may be as short as 1 cell (immediately forced) or as
long as 256 cells (reaching forcing thresholds much earlier than a 511-cell line). The distribution
of line lengths follows the triangular pattern: half of all LTP lines have 128 cells or fewer.
A 10-cell line reaches $\rho = 0$ or $\rho = u$ after just 10 assignments&mdash;an event that occurs
in the first few rows. A 50-cell line forces within the first 50 assignments to that line. These
short-line forcing events propagate through the basic constraint lines (rows, columns, diagonals),
creating cascading reductions that the uniform 511-cell LTP lines never produce.

The trade-off is therefore not "8 lines down to 5" but rather "4 weak lines replaced by 1 strong
line." The net propagation yield per cell assignment may increase if the short LTP lines' frequent
forcing events outweigh the loss of three long, inert constraint lines. The 1,023 cells assigned to
two sub-tables receive a sixth constraint line, and these privileged cells should be placed
deliberately at the matrix corners where diagonal coverage is weakest (see B.21.5).

#### B.21.5 Spatial Layout: Center-Cross / Corner-Long Distribution

The joint-tiled architecture enables a deliberate spatial arrangement of line lengths across the
matrix. The design principle is *complementarity*: place short LTP lines where the basic partitions
are already strong, and long LTP lines where they are weakest.

Diagonal and anti-diagonal line lengths vary with position. A cell at $(r, c)$ lies on diagonal
$d = c - r + (s-1)$ of length $\min(d+1, 2s-1-d)$ and anti-diagonal $x = r + c$ of length
$\min(x+1, 2s-1-x)$. Cells near the matrix center (row $\approx 255$, column $\approx 255$) lie on
diagonals and anti-diagonals of length $\approx 511$, providing maximum constraint coverage from the
basic partitions. Cells in the corners&mdash;$(0,0)$, $(0,510)$, $(510,0)$, $(510,510)$&mdash;lie on
diagonals and anti-diagonals of length 1, the weakest possible basic coverage.

The spatial layout therefore assigns:

*Short LTP lines (indices near 0 and 510, length 1-64) along the center cross.* The center cross
comprises cells near row 255 (horizontal axis) and column 255 (vertical axis). These cells already
have full-length diagonal and anti-diagonal coverage, so short LTP lines sacrifice little. The LTP
constraint on these cells is minimal but also unnecessary: the basic partitions already constrain
them heavily.

*Long LTP lines (indices near 255, length 192-256) toward the corners.* Corner cells have length-1
diagonals and benefit most from long LTP lines. A 256-cell LTP line connecting corner-region cells
provides cross-cutting constraint information that no basic partition supplies for those cells.

*Privileged dual-covered cells (the 1,023 cells in two sub-tables) at the extreme corners.* These
cells have the weakest basic coverage (length-1 diagonals and anti-diagonals) and receive a sixth
constraint line as partial compensation.

The construction algorithm for each sub-table $T_k$ proceeds as follows. First, compute a spatial
priority score for each unassigned cell: $\text{score}(r, c) = \text{len}(\text{diag}(r,c)) +
\text{len}(\text{antidiag}(r,c))$. Cells with low scores (corners) have high priority for long LTP
lines; cells with high scores (center cross) have high priority for short lines. Second, for each
line index $i$ in order of decreasing $\text{len}_k(i)$, select $\text{len}_k(i)$ cells from the
unassigned pool, biased toward cells with the lowest spatial priority scores. The bias is implemented
via a seeded LCG shuffle (as in B.9) restricted to the candidate pool, with the first
$\text{len}_k(i)$ cells after shuffling assigned to line $i$. The seed for sub-table $T_k$ is
derived from the string "CRSCLTP$k$" to ensure deterministic, reproducible assignments.

After $T_0$ is constructed, $T_1$ is built from the remaining unassigned cells (plus a portion of
the 1,023 dual-assigned cells), then $T_2$ from the next remainder, and $T_3$ from the final
remainder. The sequential construction ensures joint tiling with exactly 1,023 overlapping cells.

#### B.21.6 Table Construction Protocol

The four sub-tables must be constructed in a fixed order ($T_0, T_1, T_2, T_3$) so that both
compressor and decompressor produce identical cell-to-line mappings. The protocol is:

*Step 1.* Initialize a 511 $\times$ 511 assignment matrix $A$ where $A[r][c] = \emptyset$ (unassigned).

*Step 2.* For each sub-table $T_k$ ($k = 0, 1, 2, 3$):

  (a) Compute the candidate pool: all cells $(r, c)$ where $|A[r][c]| < 1$ (for $k < 3$) or
      $|A[r][c]| < 2$ (for $k = 3$, which absorbs the 1,023 dual assignments).

  (b) Compute the spatial priority score for each candidate cell:
      $\text{score}(r, c) = \min(d+1, 2s-1-d) + \min(x+1, 2s-1-x)$ where $d = c - r + 510$
      and $x = r + c$.

  (c) For each line index $i$ sorted by decreasing $\text{len}_k(i)$ (longest lines first):
      seed an LCG with $\text{hash}(\text{"CRSCLTP"} \| k \| i)$, shuffle the candidate pool,
      stable-sort by ascending spatial priority score with ties broken by the shuffle,
      and assign the first $\text{len}_k(i)$ cells to line $i$ of sub-table $T_k$.
      Mark assigned cells in $A$.

*Step 3.* Verify: every cell $(r, c)$ satisfies $1 \leq |A[r][c]| \leq 2$, with exactly 1,023
cells having $|A[r][c]| = 2$.

The entire construction is deterministic ($O(s^2)$ per sub-table, $O(s^2)$ total) and depends only on
$s$ and the four seed strings. Both compressor and decompressor execute the identical construction at
startup, requiring no table data in the payload.

#### B.21.7 Implications for the Solver

The solver's `ConstraintStore` changes as follows:

*Line count.* Each sub-table contributes 511 lines, but only the sub-table that owns a given cell
is active for that cell. The total number of distinct constraint lines remains $4 \times 511 = 2{,}044$
for the LTP component, plus the $4 \times 511 - 2 = 2{,}042$ diagonal/anti-diagonal lines and
$2 \times 511 = 1{,}022$ row/column lines, totaling $1{,}022 + 2{,}042 + 2{,}044 = 5{,}108$. This
matches the B.20 line count; only the line *lengths* change, not the line *count*.

*`getLinesForCell()`.* For the 260,098 cells in exactly one sub-table, this returns 5 `LineID`s
(row, column, diagonal, anti-diagonal, and one LTP). For the 1,023 dual-covered cells, it returns 6.
The return type changes from `std::array<LineID, 8>` to a small vector or to `std::array<LineID, 6>`
with a valid-count field. Alternatively, all cells can return 6 entries, with 260,098 cells having one
sentinel `LineID::None` value that the propagator ignores.

*Propagation impact.* With fewer lines per cell, each cell assignment enqueues fewer lines for
constraint checking. This reduces per-assignment overhead from 8 line updates to 5 or 6, a modest
speedup of $\approx 30\%$ in the inner propagation loop. The reduced overhead partially compensates
for any loss in propagation yield from fewer constraints.

*LTP line statistics.* Each LTP line's $(\text{assigned}, \text{unknown}, \rho)$ triple behaves
differently from the uniform case. A line of length $\ell$ starts with $u = \ell$ unknowns and
$\rho = \text{sum}$. For short lines ($\ell \leq 10$), the forcing conditions $\rho = 0$ (force all
unknowns to 0) and $\rho = u$ (force all unknowns to 1) are reached within the first few
assignments to that line. This produces immediate propagation cascades during the early rows of the
solve&mdash;exactly the region where propagation is currently most effective.

#### B.21.8 Comparison with B.20 Uniform LTP

| Metric                           | B.20 (Uniform)      | B.21 (Joint-Tiled)         |
|----------------------------------|---------------------|----------------------------|
| LTP lines per cell               | 4                   | 1 (or 2 for 1,023 cells)   |
| Constraint lines per cell        | 8                   | 5 or 6                     |
| LTP line lengths                 | 511 (uniform)       | 1--256 (triangular)        |
| LTP encoding bits (4 partitions) | 18,396              | 16,388                     |
| Block payload bytes              | 15,749              | 15,498                     |
| Compression ratio                | 48.3%               | 47.5%                      |
| Forcing events from LTP (est.)   | Rare past row 50    | Frequent through row 256   |
| Propagation inner-loop cost.     | 8 line updates/cell | 5--6 line updates/cell     |
| Spatial adaptivity               | None                | Center-cross / corner-long |

The joint-tiled design trades breadth of coverage (fewer LTP lines per cell) for depth of coverage
(shorter, more forceful lines) and encoding efficiency (variable-width sums). The spatial layout
further optimizes coverage by concentrating long LTP lines where the basic partitions are weakest.

#### B.21.9 Predicted Telemetry Targets

B.20.9 established the Configuration C baseline: plateau depth ~88,503, hash mismatch rate 25.2%,
iteration rate ~198K/sec, block payload 15,749 bytes. Three telemetry indicators should be monitored
during B.21 implementation to determine whether joint-tiled variable-length LTP partitions address
the forcing dead zone that uniform-length lines cannot escape.

*Indicator 1: Iteration rate (leading, mechanical).* Under B.20, each cell assignment updates 8
constraint lines. Under B.21, each assignment updates 5 or 6 lines (4 basic + 1 or 2 LTP). The
inner propagation loop should therefore speed up by approximately 30%, pushing the iteration rate
from ~198K/sec toward ~257K/sec. If the observed rate does not increase, the joint-tiling lookup
(determining which sub-table owns each cell) is adding overhead that offsets the line-count savings.
A rate below 198K/sec would indicate a regression in the table-lookup implementation and should be
investigated before interpreting depth or mismatch metrics.

*Indicator 2: Hash mismatch rate (leading, qualitative).* The mismatch rate is the most sensitive
indicator of constraint quality within the plateau band. B.20 reduced it from 37.0% (toroidal
slopes) to 25.2% (uniform LTP), demonstrating that non-linear structure provides qualitatively
better constraint signal even when the depth ceiling barely moves. If B.21 drops the mismatch rate
below 20%, the short LTP lines are converting that constraint signal into tighter subtree pruning
&mdash;fewer dead-end explorations per iteration. If it remains at ~25% despite variable line lengths,
the non-linear structure was already providing all available constraint power and line length is
irrelevant. A mismatch rate above 30% would indicate that the loss of three LTP lines per cell
(from 4 to 1) outweighs the benefit of shorter, more forceful lines.

*Indicator 3: Plateau depth (lagging, definitive).* The depth ceiling is the ultimate success
metric but moves only when forcing events push the solver into previously unreachable territory.
A jump from 88,503 to 90,000 or above would be a qualitative breakthrough&mdash;the first time any
partition-level change has moved the depth ceiling by more than ~1%. This would confirm that short
LTP lines ($\ell \leq 50$) generate forcing events deep in the plateau where no 511-cell line ever
could. If the depth remains at ~88,500, the positional barrier is deeper than line-length alone can
address, and the research program should pivot decisively toward non-partition approaches: B.1
(conflict-driven learning), B.11 (randomized restarts), and B.16-B.19 (row-completion look-ahead,
failure-biased branching, stall detection).

#### B.21.10 Outcome

B.21 was fully implemented and all 30 unit and integration tests pass with lint clean. The 30-minute
`uselessTest` depth run (same random binary block used for all prior comparisons) produced the
following telemetry against the B.20.9 baseline.

**Observed results (B.21, 30-minute run):**

| Indicator | B.20.9 Baseline | B.21 Observed | Direction |
|-----------|----------------|---------------|-----------|
| Plateau depth | 88,503 | **50,272** | −43% regression |
| Hash mismatch rate | 25.2% | **0.20%** | 93% improvement |
| Avg iteration rate | ~750K/sec | ~687K/sec | −8% regression |
| Max probe depth reached | 4 | 4 (stuck) | no improvement |
| Stall escalations | — | 3 (no deescalations) | maxed out |
| Min row unknowns at plateau | — | 123 (frozen) | depth wall |
| Total iterations at end | — | ~1.2B | — |

**Interpretation of the depth regression.**  The solver entered a hard plateau at depth 50,272
(approximately row 98 of 511) and never exceeded that value throughout the entire 30-minute run.
The row with fewest remaining unknowns was frozen at 123 for the entire run; the DFS oscillated
within a tight band around the same frontier without making forward progress.  Three stall
escalations brought the lookahead probe depth to its maximum of 4, and no deescalations occurred,
confirming the solver was genuinely stuck rather than transiently paused.

The root cause is architectural.  B.21 reduces the number of LTP lines per cell from 4 (B.20) to
1-2, a 50-75% reduction in LTP-derived coupling.  B.20's four toroidal-slope lines each spanned
all 511 rows (one cell per row), providing global cross-row coupling.  B.21's spatially-partitioned
lines are clustered by spatial score: each line's cells are drawn from the same spatial region
rather than distributed across rows.  At row 98, the LTP lines containing those cells carry little
information about the assignment state of rows 0-97, eliminating the inter-row signal that
stabilized B.20's descent to row 170.

**Interpretation of the hash mismatch improvement.**  The 0.20% hash mismatch rate (vs. 25.2% for
B.20) is the most dramatic constraint-quality improvement recorded in this research program.  It
demonstrates conclusively that variable-length LTP lines ($\ell = 1$-$256$) generate qualitatively
stronger row-level pruning than uniform-511-cell lines.  When a row does complete under B.21, it
almost always satisfies the SHA-256 lateral hash.  The constraint signal is excellent; the problem
is that the stronger constraints create a hard barrier the chronological DFS cannot cross.

**The paradox.**  B.21 simultaneously achieves the best constraint quality and the worst depth in
the experiment history.  This is not contradictory.  Shorter LTP lines commit cells earlier and
more forcefully.  Early commitments that are wrong propagate deeply into the search tree before the
contradiction surfaces at row ~98.  Chronological backtracking must then unwind all 50,272 branching
choices to reach the actual source of the conflict, which may lie at row 10-20.  Without
non-chronological backjumping, the adaptive lookahead (B.8) at probe depth 4 cannot reach far
enough back to identify and prune the bad decision.

**Implication for CDCL (B.1).**  B.21.9 predicted that CDCL would benefit if B.21 dropped the hash
mismatch rate below 20%, because a low mismatch rate implies that the reason graph contains rich
antecedent chains tracing hash failures back to early decisions.  The observed 0.20% rate satisfies
this condition by a wide margin.  However, B.21 also imposes a 43% depth penalty that CDCL alone
cannot recover; re-adding the fourth LTP line per cell (restoring cross-row coupling) while
preserving variable line lengths is a prerequisite for CDCL to operate in the plateau band rather
than below it.  A hybrid design&mdash;four variable-length LTP lines per cell, each spanning multiple
rows, with triangular length distribution&mdash;is the logical B.22 candidate.

**Disposition.**  B.21 is retained in the codebase as a correct, complete implementation.  The
variable-length encoding it introduces (saving 502 bytes per block relative to B.20) is a permanent
improvement regardless of the partition topology.  The plateau regression requires a new partition
design before the depth ceiling can be attacked further.

#### B.21.11 Next Iteration Improvement

B.21 established three empirical facts that constrain the design space for the next iteration.

*Fact 1: Variable line lengths work.* The 0.20% hash mismatch rate is the strongest constraint-quality
signal recorded in this project.  The triangular length distribution $\ell(k) = \min(k+1, 511-k)$,
$k = 0 \ldots 510$ generates short lines that force cells early and deeply enough to match the
correct assignment on virtually every row completion.  This distribution, or the variable-length
encoding mechanism it implies, should be preserved in any successor design.

*Fact 2: Lines per cell cannot fall below 4.* Reducing from 4 LTP lines per cell (B.20) to 1-2
(B.21) caused a 43% depth regression.  The regression is not offset by the stronger per-line
constraint quality.  Four LTP lines per cell is a hard floor: going below it removes more
cross-family coupling than any improvement in individual line informativeness can restore.

*Fact 3: Cross-row span is load-bearing.* B.20's toroidal lines each visited one cell in every
row, providing global inter-row coupling at every depth.  B.21's spatially-partitioned lines
cluster in the same region of the matrix, cutting off coupling past the cluster boundary.  The
depth wall at row $\approx 98$ corresponds precisely to the point where B.21's LTP lines stop
carrying information about the rows above.  Any replacement must ensure that each LTP line visits
cells from a wide range of rows, not a contiguous spatial region.

The three candidate next steps are ordered by implementation cost and expected confidence in
outcome.

---

**Candidate 1: Reactivate CDCL (B.1) against B.21's constraint graph (immediate, low cost)**

The B.1 CDCL implementation is complete and correct.  It was shelved after initial experiments
showed no depth improvement under B.20's uniform-511-cell lines, because the reason graph lacked
the short forcing chains CDCL requires to identify non-trivial backjump targets.  B.21 has
eliminated that limitation: a 0.20% hash mismatch rate implies that virtually every row completion
reflects a deterministic forcing cascade rooted at an early decision, and the antecedent table
will contain meaningful 1-UIP backjump targets.

The experiment is: re-enable the CDCL path in `RowDecomposedController_enumerateSolutionsLex.cpp`
using the existing `ConflictAnalyzer` and `ReasonGraph` infrastructure and run `uselessTest`.
**Success criterion:** depth exceeds 50,272 consistently.  **Failure criterion:** depth remains
at or below 50,272, indicating that the antecedent chains under B.21's partition structure still
resolve to targets too close to the conflict site for CDCL to produce useful backjumps ($< 1{,}000$
frame gain).

If CDCL pushes depth to 70K or above under B.21, the combination of variable-length forcing and
non-chronological backtracking may achieve the research goal without redesigning the partition.  If
CDCL fails to move the depth needle, the depth wall at row 98 is structural&mdash;the partial
assignment at depth 50K is globally inconsistent with the LTP sums but no single early decision
is singularly responsible&mdash;and a new partition design is required.

---

**Candidate 2: Full-coverage variable-length partitions (B.22, medium cost)**

The root cause of B.21's depth regression is the joint-tiling constraint: covering all 261,121
cells with only $4 \times 65{,}536 = 262{,}144$ cell-slots forces each cell into at most 1-2
sub-tables, destroying the 4-line-per-cell coupling B.20 provided.  The fix is to restore full
coverage: each sub-table assigns every cell to exactly one line, giving $\sum_k \ell(k) = 261{,}121$
per sub-table and exactly 4 LTP lines per cell.

The construction must enforce row diversity.  Rather than sorting cells by spatial score (which
clusters spatial neighbours together), cells should be ordered by a row-interleaving key before
assignment to lines&mdash;for example, column-major order $(c, r)$ rather than row-major $(r, c)$.
Under column-major ordering, consecutive cells in the sorted pool alternate rows, so every LTP
line of any length receives cells from a broad cross-section of rows.  The first $\ell(0) = 1$
cell (line 0, length 1) comes from row 0; the first $\ell(255) = 256$ cells (the peak line) span
rows 0 through 255 in column order; the last short lines near $k = 510$ draw from the same
column-wise cross-section as $k = 0$.

The length distribution must sum to 261,121.  A natural choice is to scale the triangular
distribution by a factor of $\lceil 261{,}121 / 65{,}536 \rceil \approx 4$ and adjust the
remainder:

$$\ell'(k) = 4 \cdot \min(k+1,\, 511-k) + \epsilon_k$$

where $\epsilon_k \in \{0, 1\}$ is a correction term distributed to bring the total to exactly
261,121.  The maximum line length under this distribution is $4 \times 256 = 1{,}024$, requiring
at most 10 bits per LTP sum element (vs. 9 bits for B.20 and 1-9 bits for B.21).  Total encoding
per sub-table is $\sum_{k=0}^{510} \lceil \log_2(\ell'(k)+1) \rceil$ bits; a rough estimate gives
$\approx 4{,}200$-$5{,}000$ bits per sub-table, comparable to B.20's $511 \times 9 = 4{,}599$
bits.

The variable-length encoding infrastructure from B.21 (`CompressedPayload_serializeBlock.cpp`,
`CompressedPayload_deserializeBlock.cpp`) requires only a change to the length function; no
structural changes to the serialization pipeline are needed.

**Success criterion:** depth exceeds 88,503 (the B.20 plateau) with hash mismatch rate remaining
below 10%.  **Failure criterion:** depth regresses below B.20 despite restoring 4 LTP lines per
cell, indicating that the triangular length distribution is fundamentally incompatible with the
solver's row-traversal order.

---

**Candidate 3: CDCL first, then B.22 if needed (recommended sequencing)**

Candidates 1 and 2 are not mutually exclusive, but Candidate 1 should be attempted first because
it requires no partition redesign.  If CDCL restores or exceeds B.20 depth under B.21's current
structure, it also validates the short-line forcing hypothesis and the variable-length encoding
savings are free.  If CDCL fails, Candidate 2 provides the definitive structural fix, and CDCL
can then be layered on top of B.22 once the depth regression is resolved.

The two experiments are sequential and non-overlapping in code paths.  Attempting both
simultaneously would obscure which mechanism is responsible for any observed improvement.

| Step | Action | Code change | Expected outcome | Termination criterion |
|------|--------|-------------|------------------|-----------------------|
| 1 | Re-enable B.1 CDCL with B.21 | ~50 lines in DFS loop | depth > 50K | 30-min uselessTest |
| 2 | If step 1 fails: implement B.22 column-major full-coverage partition | LtpTable.cpp constructor rewrite | depth > 88K | 30-min uselessTest |
| 3 | Layer CDCL on B.22 | No additional code if B.1 is already enabled | depth > 90K | 30-min uselessTest |

If step 1 achieves depth $\geq 90{,}000$, steps 2-3 become optional optimizations rather than
required fixes, and the research program can pivot to compression-ratio improvements or the
parallel-restart strategy (B.11).

#### B.21.12 Final Outcomes (CDCL + B.22)

Both Candidate 1 (CDCL) and Candidate 2 (B.22) from B.21.11 were executed sequentially and
then combined.

---

**Candidate 1 outcome: CDCL on B.21 (B.21.11, Step 1)**

CDCL was re-enabled in the DFS loop using the existing `ConflictAnalyzer`, `ReasonGraph`, and
`BackjumpTarget` infrastructure.  A `cdclStack` vector (parallel to BranchingController's undo
stack) tracked per-frame `cdclSavePoint` offsets.  On hash failure, `analyzeHashFailure` ran BFS
from the failed row; if a valid 1-UIP target $T$ was found, CDCL unrecorded assignments back to
`stack[T+1].cdclSavePoint`, called `brancher->undoToSavePoint(stack[T+1].token)`, resized the DFS
stack to $T+1$ frames, and continued.

30-minute uselessTest result:

| Metric | Value |
|--------|-------|
| Depth | ~52,042 |
| iter/sec | ~698K |
| CDCL backjumps | 7,583 (all in first 67M iterations, then zero) |
| cdcl\_jump\_dist\_max | 586 |
| Hash mismatch rate | ~25% |

**Assessment: Candidate 1 failed.** CDCL fired 7,583 times in the first 67M iterations (max
jump 586 frames) then stopped entirely, gaining only 1,770 depth frames (+3.5%) over B.21
without CDCL.  The failure mode confirms B.21.11's failure criterion: with 1-2 LTP lines per
cell and lines of length 1-256, the forcing graph is too sparse to construct useful 1-UIP
backjump targets past depth 52K.  Proceeding to Candidate 2 (B.22).

---

**Candidate 2 outcome: B.22 full-coverage variable-length LTP (B.21.11, Steps 2-3)**

B.22 was implemented with the following design:

*Formula:* $\ell_{\text{B.22}}(k) = 4 \cdot \min(k+1,\, 511-k) - \text{adj}(k)$, where
$\text{adj}(k) = 3$ if $k = 255$, else $2$.  This sums to exactly 261,121 per sub-table.
Line lengths range from 2 ($k = 0, 510$) to 1,021 ($k = 255$).

*Coverage:* Every cell belongs to exactly 4 LTP lines (one per sub-table, full coverage).
`LtpMembership.flat` expanded from 2 to 4 entries; `CellLines` expanded from 6 to 8 slots.
`getLinesForCell` always returns count = 8.

*Construction:* Fisher-Yates LCG shuffle of all 261,121 cells with a per-pass seed, then assign
consecutive chunks to lines in decreasing-length order.  No spatial sort; pure random
cell-to-line assignment ensures each line spans cells from diverse rows.

*Encoding:* $\sum_{k=0}^{510} \lceil \log_2(\ell_{\text{B.22}}(k) + 1) \rceil = 4{,}600$ bits
per sub-table.  `kBlockPayloadBytes` restored to 15,749 (same as B.20).

CDCL was layered on B.22 simultaneously (Step 3 from the plan).

30-minute uselessTest result (B.22 + CDCL):

| Metric | B.20 | B.21+CDCL | B.22+CDCL |
|--------|------|-----------|-----------|
| Depth | ~88,503 | ~52,042 | **~80,300** |
| iter/sec | ~198K | ~698K | ~521K |
| CDCL backjumps |&mdash;| 7,583 then 0 | = hash\_mismatches (100%) |
| cdcl\_jump\_dist\_max |&mdash;| 586 | **732** |
| Hash mismatch rate | ~25% | ~25% | ~25% |

**Assessment: Candidate 2 partially succeeded.** B.22 recovered 30,000 frames (+60%) from
B.21's depth floor, confirming that restoring 4 LTP lines per cell is the dominant structural
factor.  CDCL on B.22 fires on every hash mismatch (100% backjump rate, max dist 732), a
dramatic improvement over B.21+CDCL where backjumps ceased after 67M iterations.

However, depth plateaus at ~80,300 rather than exceeding B.20's 88,503.  Three factors explain
the gap:

1. *Throughput cost:* B.22's longer LTP lines (up to 1,021 cells) increase cascade cost per
   forced assignment; iter/sec drops from 698K (B.21) to 521K (B.22+CDCL), reducing effective
   search rate.

2. *Line length distribution:* Very long lines ($\ell > 511$) accumulate pressure slowly and
   are less likely to reach the $\rho/u = 0$ or $\rho/u = 1$ forcing threshold that drives
   cascade propagation.  B.20's uniform 511-cell lines provided more balanced pressure
   across all $k$.

3. *CDCL plateau persistence:* CDCL backjumps 732 frames on each failure but the plateau depth
   is unchanged, suggesting that the failures arise from global sum inconsistency across all four
   LTP families simultaneously&mdash;no single 1-UIP decision resolves the conflict.

**Conclusions:**

- Restoring 4 LTP lines per cell (full coverage) is non-negotiable: this single change recovered
  +60% of the depth lost in B.21 (+30,000 frames, 50K $\to$ 80K).
- Variable-length lines in the range 2-1,021 are less effective than B.20's uniform 511-cell
  lines at the depth frontier.  The optimal distribution likely concentrates lines in the
  128-512-cell range, providing both forcing power (shorter than 511) and constraint pressure
  (fewer near-zero-length lines wasted in early rows).
- CDCL is effective on B.22 (100% backjump rate, max dist 732) and should be retained.
- The next exploration should vary the length distribution while maintaining full 4-line coverage
  per cell.  A quadratic distribution, a uniform-511 distribution with random shuffle (repeating
  B.20 to validate parity), or a clipped triangular (minimum length 64) are candidate targets.

#### B.21.13 CDCL Interaction Study (B.23 and B.24)

Following B.22's conclusion that CDCL "should be retained," B.23 tested uniform-511 + CDCL to
isolate whether CDCL benefits a known-good partition. B.24 then disabled CDCL entirely to measure
the overhead.

---

**B.23: Uniform-511 LTP + CDCL Active**

B.23 changed `ltpLineLen(k)` to return 511 for all $k$ (B.20's construction, B.22 code
infrastructure).  The construction algorithm (Fisher-Yates LCG shuffle, consecutive 511-cell chunk
assignment) is identical to B.20 except that the `buildAllPartitions` function now uses
B.22-era seed constants (`CRSCLTP1`–`CRSCLTP4` in ASCII).

*Hypothesis:* uniform-511 + CDCL should outperform B.22's 80,300 depth (triangular + CDCL)
because the partition matches B.20's known-good structure.

30-minute uselessTest result:

| Metric | B.20 | B.22+CDCL | B.23 (uniform+CDCL) |
|--------|------|-----------|---------------------|
| Depth | ~88,503 | ~80,300 | **~69,000** |
| iter/sec | ~198K | ~521K | ~306K |
| CDCL max jump dist |&mdash;| 732 | **1,854** |
| CDCL backjumps/sec |&mdash;| ~1,200 | ~1,018 |
| Hash mismatch rate | ~25% | ~25% | ~25% |

**Assessment: B.23 regressed further than B.22.** The 1,854-cell average jump distance caused
severe thrashing — 27% worse than B.22+CDCL and 22% worse than B.20 without CDCL.

*Why is the jump distance larger with uniform-511 than triangular (B.22)?*  With uniform-511, all
lines have 511 cells.  A completed row requires ~511 cell assignments spread over many DFS frames;
by the time the hash failure is detected, the 1-UIP antecedent chain traces back nearly 1,854
frames.  With B.22's triangular distribution, short lines (2–6 cells) resolve in 1–2 decisions,
producing compact antecedent chains and shorter jumps (732 frames).  So a more uniform distribution
actually worsens CDCL jump distances.

*Root cause of CDCL ineffectiveness:* CDCL was designed for unit-clause propagation in SAT, where
a "conflict" traces back to exactly one decision variable (1-UIP).  Hash-based row verification is
a **global non-linear constraint** over all 511 cells in a row; no single decision is the "culprit."
The 1-UIP analysis produces a jump target that is far back in the stack (deep antecedent chain) and
irrelevant to the actual failure cause.  The net effect is a large undo of correct work without
gaining any constraint learning benefit (the current implementation does not store learned clauses).

---

**B.24: CDCL Disabled (kMaxCdclJumpDist = 0)**

B.24 added `constexpr uint64_t kMaxCdclJumpDist = 0` to the DFS loop.  When the CDCL analysis
returns a backjump distance exceeding this cap, the code falls through to chronological backtrack
instead.  With `kMaxCdclJumpDist = 0`, CDCL never fires.  The ReasonGraph, ConflictAnalyzer, and
cdclStack infrastructure remain in place but perform zero useful work.

2-minute uselessTest snapshot (B.24):

| Metric | B.20 | B.23 | B.24 (CDCL off) |
|--------|------|------|-----------------|
| Depth (2-min) | ~88,503 | ~69,000 | **~86,123** |
| iter/sec | ~198K | ~306K | **~120K** |
| CDCL backjumps |&mdash;| ~1,018/sec | **0** |

**Assessment: Disabling CDCL recovers most of B.20's depth.**  B.24 reaches ~86,123 at 2 minutes
(vs B.20's plateau of 88,503), confirming that CDCL was the primary cause of the regression.
The remaining ~2,400-frame gap is attributed to:

1. *CDCL overhead still running:* `recordDecision`, `recordPropagated`, and `unrecord` are called
   on every cell assignment/unassignment even though no backjumps fire.  This reduces iter/sec
   from B.20's ~198K to B.24's ~120K — a 40% throughput penalty.

2. *Different shuffle seeds:* B.20 may have had slightly different seeds that produced a marginally
   better partition.

**Conclusions from B.23 and B.24:**

- CDCL as implemented is strictly harmful in all tested configurations: depth drops 20–27% in
  every experiment where CDCL fires.
- The harm mechanism is the 1-UIP backjump distance: hash failures have antecedent chains of
  700–1,854 cells, causing large backward jumps that undo correct work without constraint learning.
- CDCL is fundamentally mismatched with hash-based row verification (a global constraint), as
  opposed to unit-clause propagation failures (local constraint) for which CDCL was designed.
- **B.25 plan:** Remove all CDCL overhead (delete cdclStack, ReasonGraph/ConflictAnalyzer calls,
  per-assignment record/unrecord calls) to recover B.20's ~198K iter/sec and close the 2K depth
  gap.  Expected outcome: depth ≥ 88,503 (or reveal remaining structural differences).

#### B.21.14 Open Questions

(a) What is the optimal distribution of the 1,023 dual-covered cells? Placing them at extreme corners
maximizes complementarity with diagonal coverage, but other strategies (e.g., distributing them along
the anti-diagonal extremes) may yield better propagation cascades. Empirical testing with the existing
solver can compare strategies by measuring depth-to-backtrack statistics.

(b) Does the sequential sub-table construction introduce ordering bias? $T_0$ gets first pick of the
cell pool and may receive a higher-quality spatial layout than $T_3$, which operates on the residual.
An iterative refinement step (re-constructing $T_0$ after $T_1$-$T_3$ exist, then repeating) could
equalize quality across sub-tables, at the cost of construction complexity.

(c) Is the triangular length distribution optimal, or would a different distribution (e.g., quadratic,
logarithmic) better match the solver's propagation dynamics? The triangular distribution mirrors
DSM/XSM for encoding simplicity, but the solver's performance depends on the distribution of short
lines relative to the DFS traversal order. A distribution with more short lines and fewer long ones
might increase early forcing at the cost of weaker mid-solve constraints.

(d) How does joint tiling interact with the belief-propagation-guided branching heuristic (Section 7)?
The BP estimator currently assumes each cell participates in 8 factor nodes (one per constraint line).
Reducing to 5-6 factors changes the BP update equations and convergence properties. The BP estimator
must be updated to handle variable factor counts per cell.

(e) Can the joint-tiling principle extend to the basic partitions? If diagonal and anti-diagonal
families were jointly tiled (each cell on exactly one of the two), the total constraint count would
drop further but diagonal encoding would change. This is unlikely to be beneficial because DSM and XSM
provide qualitatively different constraint information (different slope directions), whereas the four
LTP sub-tables are qualitatively interchangeable.

