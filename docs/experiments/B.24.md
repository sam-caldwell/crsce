### B.24. LTP Hill-Climber: Direct Cell-Assignment Optimisation (ABANDONED)

#### B.24.1 Motivation

The B.9.2 optimization objective identified a specific failure mode: at plateau entry (~row 170),
every LTP line has roughly 170 of its 511 cells assigned, leaving $u \approx 341$ unknowns and a
residual far from the forcing conditions ($\rho = 0$ or $\rho = u$). Because Fisher--Yates shuffle
distributes cells uniformly across all rows, all LTP lines progress at the same rate and none
reaches its forcing threshold before the solver stalls. No line is tight enough to propagate
anything.

The proposed fix was a *direct hill-climbing optimizer* that would post-process the Fisher--Yates
baseline by swapping individual cell-to-line assignments, driving a subset of LTP lines to
concentrate their cells in the early rows (0--170) already solved before the plateau. At plateau
entry, these biased lines would have $u \lesssim 170$ (half their unknowns already resolved), with
residuals closer to the forcing threshold than any uniformly-distributed line at the same depth.
The remaining ~400 LTP lines absorb peripheral cells and remain loose, but those cells are
well-served by DSM/XSM (short diagonal lines) and row/column cardinality forcing.

**Degrees of freedom.** A Fisher--Yates seed provides one degree of freedom per sub-table over
$O(s^{2}!)$ possible assignments. The hill-climber relaxes this constraint and operates directly
in the assignment space: for an $s \times s$ matrix with $s$ lines of $s$ cells each, any two
cells on different lines can be exchanged while preserving the uniform-length invariant, giving
$O(s^{4})$ candidate swaps per round.

#### B.24.2 Hypothesis

> *Biasing 50--100 LTP lines so that the majority of their $s = 511$ cells lie in rows $0$--$170$
> will create forcing cascade opportunities at plateau entry that do not exist under the uniform
> Fisher--Yates construction.  A hill-climbing optimizer that iteratively accepts swaps reducing
> mean DFS backtrack count in the plateau band will converge to such a biased configuration and
> will increase depth materially above the Fisher--Yates baseline.*

Formally: let $\text{cov}(L) = |\{(r,c) \in L : r \leq 170\}|$ be the early-row coverage of
line $L$, and let $\Phi = \sum_L \max(0,\, \text{cov}(L) - \theta)^2$ for a threshold $\theta$
(e.g., $\theta = 340$, rewarding lines with $> 66.5\%$ early-row concentration). The hill-climber
maximises $\Phi$; increasing $\Phi$ is taken as a proxy for increasing DFS depth.

Secondary motivation from B.9.3: an optimized LTP has no algebraic relationship between the
cell-to-line mapping and the cell coordinates $(r, c)$.  Swap-invisible attack patterns must
simultaneously satisfy all linear partition constraints (row, column, diagonal, anti-diagonal)
*plus* the non-linear LTP constraint, which is harder when the LTP geometry is opaque to
linear-algebraic analysis.

#### B.24.3 Methodology

**Phase 1 — Baseline construction.** Generate a Fisher--Yates uniform partition seeded with a
fixed constant (B.9.1).  Each of the four sub-tables assigns all $s^2 = 261{,}121$ cells to one
of $s = 511$ lines with exactly 511 cells per line (uniform-511 invariant).

**Phase 2 — Hill-climbing loop.**  Repeat until convergence:

1. Sample two lines $L_a$, $L_b$ uniformly at random from the same sub-table ($L_a \neq L_b$).
2. Sample one cell $p_a \in L_a$ and one cell $p_b \in L_b$ uniformly at random.
3. Compute the change in $\Phi$ if $p_a$ and $p_b$ swap their line assignments:
   $$\Delta\Phi = [\max(0,\text{cov}^*(L_a) - \theta)^2 + \max(0,\text{cov}^*(L_b) - \theta)^2]
                 - [\max(0,\text{cov}(L_a) - \theta)^2   + \max(0,\text{cov}(L_b) - \theta)^2]$$
   where $\text{cov}^*$ is coverage after the proposed swap.
4. Accept the swap if $\Delta\Phi > 0$; otherwise reject.

**Filter:** A swap that moves cells within the same coverage zone ($r \leq 170$ for both, or $r > 170$
for both) does not change $\Phi$ and is skipped.

**Uniform-511 invariant:** After each accepted swap $L_a$ gains one cell and $L_b$ loses one cell
*from the same sub-table*, then the symmetric exchange restores both lines to $s = 511$ cells.
The invariant is preserved unconditionally.

**Implementation context.** The hill-climber was implemented in Python, using the same
compress/decompress pipeline used in B.26c seed-search experiments (20-second timed runs, peak
depth from `CRSCE_EVENTS_PATH` JSON-L log).  The proxy metric $\Phi$ was computed incrementally
in $O(1)$ per swap from pre-built coverage counters.

#### B.24.4 Experimental Results (ABANDONED)

The procedure was conducted immediately following B.26c (which established the 91,090 baseline
with CRSCLTPV+CRSCLTPP seeds).  The hill-climbing optimizer was run overnight on the B.26c
winning partition.

**Outcome:** Peak depth collapsed to **$< 500$** — a catastrophic regression of more than 90,000
cells below the 91,090 baseline.  The optimizer was immediately halted and the approach abandoned.

| Metric | B.26c baseline | B.24 hill-climber |
|:-------|:--------------:|:-----------------:|
| Peak depth | 91,090 | $< 500$ |
| Regression | — | $> 99\%$ |
| $\Phi$ score | N/A (Fisher--Yates) | Increasing |
| Depth trajectory | Stable | Collapsed |

The proxy metric $\Phi$ continued to increase while depth simultaneously collapsed, demonstrating
that $\Phi$ and DFS depth are **anti-correlated** in this regime.

#### B.24.5 Root Cause Analysis

The Fisher--Yates shuffle produces partitions with three global statistical properties that are
critical for effective constraint propagation and that the hill-climber destroyed:

1. **Near-uniform pairwise co-occurrence.** For any two cells $(r_1, c_1)$ and $(r_2, c_2)$,
   the probability they belong to the same LTP line is close to $1/(s-1)$ under the uniform
   distribution.  When the hill-climber concentrates cells from rows 0--170 onto a subset of
   lines, those lines gain high co-occurrence among early-row cells and near-zero co-occurrence
   among plateau-band cells.  Constraint propagation depends on cross-row co-occurrence to
   propagate information between already-solved rows and the branching frontier; concentrating
   co-occurrence inside the solved region removes the cross-boundary linkage that drives forcing.

2. **Low maximum cross-line overlap.** Under Fisher--Yates, any two LTP lines share at most
   $O(\sqrt{s})$ cells in expectation.  After hill-climbing concentrates ~300 cells from each
   biased line into rows 0--170, the biased lines share a large fraction of their early-row
   cells with each other (because early rows provide only $170 \times 511 = 86{,}870$ total
   cell-slots for $511$ lines competing for early-row concentration).  High cross-line overlap
   makes multiple LTP constraints redundant: forcing on one biased line conveys little new
   information about cells already covered by the other biased lines.

3. **Well-distributed spatial coverage.** Under Fisher--Yates, each plateau-band row (100--300)
   contains approximately $511 \times 511 / 511 = 511$ cells per line, giving uniform density.
   After hill-climbing, the ~400 un-biased lines must absorb the late-row cells rejected by
   biased lines.  Each un-biased line becomes heavily loaded with rows 170--510, reducing their
   constraint value in the plateau band to near zero.  The plateau band (the critical region for
   depth) loses LTP constraint coverage precisely where it is most needed.

**Summary:** Maximising $\Phi$ explicitly concentrates the useful cells of biased lines *outside*
the plateau band and floods the un-biased lines with plateau-band cells but no corresponding sum
constraints that are tight.  The solver loses the LTP forcing information it had under the
uniform partition and regresses catastrophically.

#### B.24.6 Theoretical Interpretation

The Fisher--Yates LCG construction is not merely a convenient baseline; it is a *strong global
prior* that encodes the spatial regularity required for effective constraint propagation.  The
prior cannot be captured by any local metric:

- $\Phi$ measures concentration in a fixed row band; it is blind to co-occurrence structure.
- Any single scalar proxy can be improved by local swaps while global properties degrade.
- The space of valid uniform-511 partitions is $O(s^{s^2})$; the hill-climber gradient points
  away from the Fisher--Yates manifold of "useful" partitions toward a degenerate region that
  the optimizer has no mechanism to detect until the full decompressor is queried.

**Key conclusion:** The optimization variable (assignment) and the optimization objective (DFS
depth) are coupled through global properties of the partition geometry.  No local proxy captures
this coupling.  Direct hill-climbing of LTP cell assignments is not a viable optimization strategy.

#### B.24.7 Implications for Subsequent Experiments

B.24's failure definitively closed the direct-partition-manipulation branch of the search tree:

- **Seed search is the correct optimization strategy.** Varying the LCG seed varies the entire
  partition globally and simultaneously, preserving all Fisher--Yates invariants while exploring
  a different region of the assignment space.  This is exactly what B.26c exploited.
- **Proxy-based hill-climbing cannot substitute for end-to-end depth measurement.** Any
  experiment that proposes to optimize a surrogate metric without periodic DFS validation should
  be treated with extreme scepticism.
- **The B.9.2 objective is sound; the B.9.1 optimization mechanism is not.** The row-concentration
  idea (B.9.2) correctly identifies what a good partition should look like.  The error was
  assuming local swaps could achieve it without destroying the global invariants on which the
  solver relies.  A constructive approach (e.g., seeded shuffle with a modified initial ordering
  that biases early-row placement) might achieve the B.9.2 objective without destroying global
  co-occurrence structure, but this remains unverified and was not pursued.

This result was originally recorded informally in B.26.7 ("Lessons from Hill-Climbing
(Abandoned Side-Experiment)") immediately after B.26c.  B.24 is the primary documentation.

