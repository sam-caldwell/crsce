### B.22 Partition Seed Search for Uniform-511 LTP

#### B.22.1 Motivation

B.25 (uniform-511 LTP, CDCL removed) achieves depth ~86,123 and 329K iter/sec. B.20 achieved
depth ~88,503 at ~198K iter/sec using the same Fisher-Yates construction with different LCG seed
strings. The gap of ~2,380 depth cells (2.7%) has no algorithmic explanation: both configurations
use identical constraint topology (511 cells/line, 4 LTP sub-tables, same line-count), the same
DFS loop, and the same propagation engine. The only difference is the random shuffle of cells into
lines produced by the four LCG seeds.

This 2.7% gap is the direct cost of re-seeding when moving from B.20 to B.25. The Fisher-Yates
shuffle maps each of the $511^2 = 261{,}121$ cells into exactly one of the four sub-tables' lines.
Different seed strings produce different shuffles, and different shuffles produce different constraint
topologies at the cell level: which cells share LTP lines, which LTP lines have high overlap with
diagonal/column lines at the plateau band (rows 100--300), and therefore which cells cascade most
effectively when forced.

The depth ceiling for a given seed is not random noise. B.20's depth of 88,503 was reproducibly
measured over multiple runs; B.25's depth of 86,123 is equally stable. The depth is a structural
property of the partition, not a stochastic accident. Different seeds produce different structural
depth ceilings, and a systematic search over seeds can identify partitions whose structural
properties (high LTP-line overlap with other constraint families in the plateau band) yield higher
depth ceilings.

The target is to recover the ~2,380-cell gap relative to B.25, matching or exceeding B.20's 88,503
depth while retaining B.25's 329K iter/sec throughput.

#### B.22.2 Objective

Find four LCG seed strings $(s_1, s_2, s_3, s_4)$ for the Fisher-Yates construction of LTP
sub-tables $T_0$-$T_3$ such that the resulting uniform-511 partition achieves depth
$\geq 88{,}503$ (the B.20 baseline) at B.25's throughput (~329K iter/sec), without any
algorithmic change to the DFS loop or constraint architecture.

Secondary objective: understand *why* some seed configurations produce higher depth ceilings,
to guide future partition design.

#### B.22.3 Methodology

**Phase 1 — Baseline Characterization (1 run per seed)**

Each candidate seed configuration $(s_1, s_2, s_3, s_4)$ is evaluated by running the useless-machine
depth test for a fixed 2-minute window and recording:

- `depth_max`: maximum DFS depth reached
- `iter_per_sec`: mean iteration rate
- `hash_mismatch_rate`: fraction of iterations ending in SHA-1 failure

The 2-minute window provides sufficient signal to distinguish partitions with meaningfully
different depth ceilings (difference $\geq 1{,}000$ cells is reliable; $\geq 500$ cells is
directional).

**Phase 2 — Seed Space**

The Fisher-Yates LCG uses seeds derived from a string constant via a hash or direct XOR:
$\text{seed} = \text{hash}(\text{string}) \oplus \text{row\_index}$. The current seeds are the
ASCII byte XOR of the strings `CRSCLTP1`, `CRSCLTP2`, `CRSCLTP3`, `CRSCLTP4` (B.25). B.20
used a different set of string constants.

The search space is structured as follows:

- *String prefix search*: vary a 4-8 character ASCII suffix while holding the prefix constant.
  This provides a dense neighborhood in seed space while keeping the LCG properties stable.
- *Independent sub-table search*: optimize each sub-table's seed independently. If sub-table
  quality is approximately independent (a cell's constraint value comes from its full 8-line
  membership, not just the LTP line), then $4 \times K$ evaluations suffice to find a near-optimal
  4-tuple rather than $K^4$.
- *Geometric proxy metric*: before running the full depth test, evaluate each partition by a
  cheap structural metric (see B.22.4). Use this metric to prune candidate seeds by $\geq 90\%$,
  spending full 2-minute runs only on the top candidates.

**Phase 3 — Validation**

Top-5 candidate seed configurations from Phase 2 are validated with 10-minute depth runs
($3\times$ longer for statistical confidence). The configuration achieving the highest
`depth_max` over the 10-minute window is adopted as the B.22 production seed set.

#### B.22.4 Geometric Proxy Metric

Rather than running a 2-minute depth test for every candidate seed, evaluate each partition by
a cheap structural metric that correlates with depth ceiling. The metric targets the plateau
band (rows 100-300) where the forcing dead zone activates.

**Cross-family line overlap score** $\Phi$: for each LTP line $\ell$ with index $k$ (belonging
to sub-table $T_j$), count the number of cells on $\ell$ that also lie on a nearly-tight
column or diagonal line in the plateau band. "Nearly tight" at the plateau means that the line
has small unknown count ($u \leq \tau_\Phi$, suggested $\tau_\Phi = 50$). A cell $(r, c)$ on
LTP line $\ell$ contributes to $\Phi(\ell)$ if any of its 6 non-LTP non-row constraint lines
(column, diagonal, anti-diagonal, and the other 3 LTP sub-tables) is nearly tight in the
plateau band.

$$
\Phi = \frac{1}{4 \times 511} \sum_{j=0}^{3} \sum_{k=0}^{510} \Phi(T_j, k)
$$

Higher $\Phi$ indicates that LTP lines in the plateau band co-reside with tight non-LTP lines,
increasing the probability that LTP-line forcing events cascade into row completions and SHA-1
checks. This is the structural property that determines depth ceiling.

Computing $\Phi$ requires only the static partition layout and a precomputed estimate of which
lines are tight in the plateau band (derived from the cross-sum residuals of the test block).
Wall time: $O(4 \times 511 \times \text{line\_len}) = O(10^6)$ operations, approximately 1 ms&mdash;
three orders of magnitude cheaper than a 2-minute depth run.

#### B.22.5 Expected Outcomes

**Best case:** A seed configuration is found with depth $\geq 90{,}000$ ($\Phi$-selected, 2K+
improvement over B.20). This would confirm that the $\Phi$ metric is a reliable proxy and that
meaningful depth gains are available within the uniform-511 architecture without algorithmic
changes.

**Likely case:** A seed configuration is found in the range 87{,}500-89{,}500, recovering most
or all of the 2,380-cell B.20-to-B.25 regression and possibly exceeding B.20. This would
establish the B.22 seed set as the new baseline for all subsequent experiments.

**Pessimistic case:** No seed configuration meaningfully exceeds ~88{,}500 despite $K \geq 200$
candidate evaluations. This would indicate that the 88K-89K range is a structural ceiling for
uniform-511 partitions regardless of cell assignment, pointing toward architectural changes
(variable-length lines, increased constraint density) as the only path to deeper search.

In all cases, the secondary objective is satisfied: the distribution of depth ceilings over the
seed search gives a quantitative picture of how much variance the partition geometry contributes
to solver performance, complementing the partition-architecture data from B.20-B.21.

#### B.22.6 Relationship to Other Proposals

*B.20 (uniform-511 LTP baseline).* B.22 is a direct continuation of B.20's partition architecture,
reusing the Fisher-Yates construction and 511-cell-per-line constraint. The only change is
systematic exploration of the seed space rather than accepting the default constants.

*B.21/B.22 (variable-length LTP).* Variable-length partitions (B.21) achieved 0.20% mismatch
rate and dramatically better constraint quality, but depth regression to ~50K due to constraint
density loss. Seed search operates within the proven uniform-511 architecture; findings inform
whether architectural complexity is necessary or whether the uniform-511 ceiling can be raised
sufficiently to meet the project's depth target.

*B.8 (adaptive lookahead).* Deeper lookahead (k=4) produced zero depth improvement in the
forcing dead zone. Partition seed search addresses the dead zone directly by modifying which
lines are co-resident in the plateau band, rather than applying deeper probing to the existing
topology.

*B.10 (constraint-tightness ordering).* In the forcing dead zone, all lines have
$\rho/u \approx 0.5$ and tightness scores are uniformly weak. A partition with higher $\Phi$
creates more variation in line tightness at the plateau, making B.10 relevant again. Seed search
and tightness ordering are therefore synergistic: seed search creates the tightness variation
that B.10 can exploit.

#### B.22.7 Open Questions

(a) Does the $\Phi$ metric correlate reliably with depth ceiling? If $\Phi$ and depth ceiling
have Spearman rank correlation $\geq 0.7$ over 50+ candidate seeds, the metric is useful as a
pre-filter. If correlation is low, structural metrics alone cannot predict depth; full 2-minute
runs are required for all candidates.

(b) Are the four sub-table seeds independent? If optimizing $s_1$ while holding $s_2, s_3, s_4$
fixed produces the same depth ceiling as a joint optimization, the search reduces from $O(K^4)$
to $O(4K)$. Empirical testing: optimize each sub-table seed independently, then run the joint
configuration and compare.

(c) Is there a seed configuration that exceeds B.21's $\Phi$ score despite uniform-511 line
lengths? The triangular variable-length architecture achieves low $\Phi$ (short lines are sparse
in the plateau band) but high constraint quality (near-zero mismatch). A uniform-511 partition
with high $\Phi$ achieves high constraint density (all 511 cells/line) with good cross-family
overlap. If such a partition can be found, it may combine the best properties of B.20 and B.21
without the depth regression.

(d) Does the optimal seed set generalize across input blocks? The depth test uses a single
representative block. A seed configuration optimized on one block may overfit to its specific
cross-sum residual structure. Validation on 3-5 diverse blocks (all-zeros, all-ones, random,
alternating) is required before adopting a new seed set for production.

(e) What is the variance of depth measurements for a fixed seed at 2 minutes vs. 10 minutes?
If depth variance over 2-minute windows is large (standard deviation $\geq 500$ cells), the
phase 1 pre-filter will produce false rankings and Phase 2 validation will require longer runs.
The variance characterization should be performed on B.25's current seed set before beginning
the search.

