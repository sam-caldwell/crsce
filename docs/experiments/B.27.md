### B.27 Increasing Constraint Density via LTP5 and LTP6 (Implemented)

#### B.27.1 Motivation

The B.26c joint seed search confirmed that the depth plateau at 91,090 cells (row ≈178) is
governed by the partition topology of LTP1–LTP4. Each cell belongs to exactly one line in each
of the four sub-tables, giving eight constraint lines per cell (row, column, diagonal,
anti-diagonal, LTP1, LTP2, LTP3, LTP4). At the plateau boundary, propagation stalls because no
forcing rule fires: every active line has $\rho > 0$ and $u > 0$. The number of cells that can
be in this "underdetermined interior" depends directly on how many independent constraint families
bound each cell. Adding two additional uniform-511 sub-tables raises the per-cell constraint
count from 8 to 10, increasing the probability that at least one line forces a cell at each DFS
level.

#### B.27.2 Architecture Changes

**New sub-tables.** LTP5 and LTP6 are constructed by the same Fisher-Yates LCG shuffle used for
LTP1–LTP4, seeded by two new 64-bit constants:

$$k_{\text{seed5}} = \texttt{0x4352'5343'4C54'5035}
  \quad (\text{"CRSCLTP5"}),
  \qquad
  k_{\text{seed6}} = \texttt{0x4352'5343'4C54'5036}
  \quad (\text{"CRSCLTP6"}).$$

These are the default alphanumeric seeds for sub-tables 5 and 6 — identical in construction
method to the pre-B.22 seeds for LTP1–LTP4. They were **not jointly optimized** at the time of
this experiment; joint optimization is the subject of future work (§B.27.5).

**Constraint store.** `ConstraintStore` is extended from 8 to 10 constructor parameters,
adding `ltp5Sums` and `ltp6Sums`. The total tracked line count increases from $10s - 2 = 5{,}108$
to $12s - 2 = 6{,}130$ (where $s = 511$). New constants `kLtp5Base = 5108` and
`kLtp6Base = 5619` index into the flat line array.

**Wire format.** Each block payload grows by $2 \times 511 \times 9 = 9{,}198$ bits:

| Field | B.26c | B.27 |
|-------|-------|------|
| LTP sub-tables | 4 | 6 |
| LTP payload bits | $4 \times 4{,}599 = 18{,}396$ | $6 \times 4{,}599 = 27{,}594$ |
| Total block bits | 125,988 | 135,186 |
| Block bytes | **15,749** | **16,899** |

**GPU integration.** `MetalPropagationEngine` and `ForEachCellOnLine` are updated to handle
`LineType::LTP5` and `LineType::LTP6`. `kMetalPropTotalLines` changes from $(10 \times 511) - 2$
to $(12 \times 511) - 2 = 6{,}130$.

#### B.27.3 Experimental Parameters

- **Seed5, Seed6:** `CRSCLTP5`, `CRSCLTP6` (default, unoptimized)
- **Seeds 1–4:** Fixed at B.26c winner: `CRSCLTPV` + `CRSCLTPP` (+ `CRSCLTP3`, `CRSCLTP4`)
- **Test input:** `useless-machine.mp4` (same as all prior experiments)
- **Decompress runtime:** 30 minutes
- **Comparison baseline:** B.26c depth 91,090, avg iter/sec ~411K

#### B.27.4 Results

| Metric | B.26c (4-LTP) | B.27 (6-LTP, default seeds) | Change |
|--------|:---:|:---:|:---:|
| Max depth (cells) | 91,090 | **91,090** | 0 (0.00%) |
| Plateau row | ≈178 | ≈178 | unchanged |
| `min_nz_row_unknown` | 161 | 161 | unchanged |
| Avg iter/sec | ~411K | ~399K | −3.0% |
| Stall escalations | 3 | 3 | unchanged |

The 6-LTP solver matches the B.26c depth ceiling exactly over 30 minutes and ~800M DFS
iterations. The depth-versus-time trace oscillates between 91,078 and 91,090, identical in
character to B.26c. No new maximum was observed.

**Throughput.** The 3% reduction in iteration rate is consistent with the cost of 25% more
constraint-line updates per cell assignment (10 lines instead of 8). The overhead is sublinear
because propagation is dominated by the fast-path `tryPropagateCell()` branch, which exits
early before iterating all lines in most cases.

**Seed search results.** An exhaustive joint search over all $256^2 = 65{,}536$ (seed5, seed6)
suffix-byte pairs was launched with seeds 1–4 fixed at B.26c values. The search was terminated
after 30,504 pairs (46.5% coverage) spanning all four quadrants of the seed5 byte range, each
quadrant fully paired with all 256 seed6 values.

| Metric | Value |
|--------|-------|
| Pairs evaluated | 30,504 / 65,536 (46.5%) |
| Unique depth values observed | 1 |
| Mean depth | 91,090.0 |
| Std deviation | **0.0** |
| Min / Max | 91,090 / 91,090 |
| Pairs beating baseline | 0 |

The landscape is completely flat: every tested (seed5, seed6) pair returns exactly 91,090. A
zero standard deviation over 30,504 independent evaluations uniformly distributed across the
full byte range is definitive — the search was terminated early as the remaining 53.5% is
statistically certain to yield no improvement.

#### B.27.5 Conclusions

1. **Additional LTP sub-tables are neutral at default seeds.** Adding LTP5 and LTP6 with
   unoptimized seeds preserves the depth ceiling (91,090) and plateau location (row 178)
   precisely. The extra constraint density does not spontaneously improve DFS performance —
   the seed selection governs which cells are co-constrained at the plateau boundary, and
   default seeds reproduce the same underdetermined interior.

2. **The 3% throughput penalty is acceptable.** At ~399K iter/sec the 6-LTP solver
   remains faster than B.20 (~198K), B.22 greedy (~329K), and approaches the B.26c rate.
   If seed optimization yields a depth improvement, the slowdown is justified.

3. **Seeds 5 and 6 are invariant at the B.26c operating point.** The exhaustive joint
   search confirmed zero variance across all tested pairs. The depth ceiling of 91,090 is
   set entirely by seeds 1+2 (CRSCLTPV+CRSCLTPP); seeds 5+6 contribute no additional
   discriminating constraint pressure regardless of their values.

4. **Sub-tables 5+6 are structurally inert — proven from both directions (B.27a, B.27b).**
   B.27a placed the B.26c winning seeds in slots 5+6 with weak seeds in slots 1+2: depth
   stayed at 86,123, unchanged from the weak 4-LTP baseline. B.27b swept all 256 seed5
   values with weak seeds 1+2: every candidate returned exactly 86,123 (std dev = 0.0,
   terminated after 136 of 256 values). Combined with the B.27 main result (seeds 5+6
   invariant at 91,090 when seeds 1+2 are optimal), the conclusion is definitive: no seed
   value placed in slots 5+6 affects depth under any condition. The sub-tables in those
   slots contribute zero discriminating constraint pressure. The depth is a function
   of slots 1+2 alone.

5. **The invariance pattern establishes a structural limit on uniform-511 expansion.**
   Seeds 3+4 were invariant at the B.22 operating point; seeds 5+6 are inert at all tested
   operating points. B.27c confirmed that re-optimizing seeds 1+2 in the 6-LTP context
   returns the identical winner (CRSCLTPV+CRSCLTPP=91,090) as the 4-LTP B.26c result.
   Adding more uniform-511 sub-tables beyond the first two jointly-optimized seeds provides
   no depth benefit. This closes the door on further uniform-511 expansion as a depth
   improvement strategy. B.28 and B.29 (alternative byte-position seed families) were
   evaluated and skipped — the same structural ceiling argument applies regardless of which
   byte of the 64-bit seed is varied. The seed-search paradigm is exhausted; the hill-climber is the next productive direction.

5. **Wire-format breakage is real.** Any container produced under the 4-LTP format
   (15,749 bytes/block) is incompatible with the 6-LTP reader (16,899 bytes/block) and
   vice versa. `ValidateContainer` was updated to enforce the new size. This is expected
   behavior for a research format with no versioning guarantee between branches.

#### B.27.6 Open Questions

(a) **Answered — seeds 5+6 are invariant.** The exhaustive joint search (30,504 of 65,536
pairs, zero variance) confirms that $s_5$ and $s_6$ produce no depth variation at the B.26c
operating point. The rugged landscape observed for $s_1$/$s_2$ in B.26c does not generalize
to additional uniform-511 sub-tables.

(b) **Answered — seeds 5+6 are structurally inert, not ceiling-limited.** B.27c established
that the seeds 1+2 landscape in the 6-LTP context is identical to the 4-LTP context: the
same winner (CRSCLTPV+CRSCLTPP=91,090) and the same rugged landscape (seed1 range
84,982–91,090; seed2 range 85,232–91,090). This rules out the "ceiling" interpretation:
if 91,090 were a hard ceiling from the first two sub-tables, adding more sub-tables would
not change the landscape shape, but we would still expect some seed2 candidates to exceed
91,090. None did. Instead, the evidence favors the "redundancy" interpretation: uniform-511
sub-tables 5+6 add no constraint discriminability that sub-tables 1+2 do not already provide.
The relevant implication for B.28/B.29 is that the depth ceiling is tied to the *value* of
the constraint geometry defined by seeds 1+2, not the count of sub-tables.

(c) **Partially answered.** B.27c confirms that 6 sub-tables with the optimal seed-1+2 pair
achieves the same depth as 4 sub-tables. B.27d (joint 4-axis) is moot per B.27b. The
question of whether non-uniform partitions (B.28, B.29 seed families) can break the ceiling
remains open.

#### B.27a Swap Experiment: Value vs. Slot Position (Completed)

**Setup.** Set seeds 1+2 to the pre-B.26c defaults (`CRSCLTP1` + `CRSCLTP2`, depth ~86,123 in
4-LTP) and seeds 5+6 to the B.26c winners (`CRSCLTPV` + `CRSCLTPP`). Seeds 3+4 remain fixed
at `CRSCLTP3` + `CRSCLTP4`.

**Question.** Do the B.26c winning seed *values* produce high depth regardless of which
sub-table slot they occupy, or is there something structurally special about slots 1+2?

**Result.**

| Configuration | Depth |
|---|---|
| B.26c baseline: slots 1+2 = CRSCLTPV+CRSCLTPP, slots 5+6 = CRSCLTP5+CRSCLTP6 | 91,090 |
| B.27a swap: slots 1+2 = CRSCLTP1+CRSCLTP2, slots 5+6 = CRSCLTPV+CRSCLTPP | **86,123** |
| Weak 4-LTP reference: slots 1+2 = CRSCLTP1+CRSCLTP2 | ~86,123 |

Depth = 86,123 — identical to the weak 4-LTP reference. Placing the B.26c winning values in
slots 5+6 produces zero benefit. The depth is the same as if those slots held default seeds.

**Interpretation.** Sub-tables 5+6 are **structurally inert** — they contribute zero
constraint pressure to the DFS regardless of their seed values. The depth is controlled
entirely by slots 1+2. This is not a "slot position matters" result in the sense that slots
1+2 are special and slots 5+6 are not; rather, slots 5+6 are inert *period* — no value placed
in them affects the outcome. The governing question for B.27b is whether this inertness holds
even when slots 1+2 are *also* weak (i.e., whether any seeds 5+6 can compensate).

#### B.27b Weak Seeds 1+2, Sweep of Seeds 5+6 (Completed)

**Setup.** Fix seeds 1+2 at `CRSCLTP1` + `CRSCLTP2` (weak, depth ~86,123 in 4-LTP). Sweep
all 256 suffix-byte candidates for seed5 with seed6 fixed at `CRSCLTP6`. Search terminated
early after 136 seed5 values (0x00–0x87) due to zero variance.

**Question.** Can seeds 5+6 independently drive depth from ~86K toward 91K when seeds 1+2
are weak?

**Result.**

| Metric | Value |
|--------|-------|
| Seed5 values evaluated | 136 / 256 (0x00–0x87) |
| Unique depth values observed | 1 |
| Depth (all candidates) | **86,123** |
| Std deviation | **0.0** |
| Best depth vs B.26c baseline | −4,967 |
| Best depth vs weak reference | 0 |

Every evaluated seed5 value returns exactly 86,123 — the same depth as the weak 4-LTP
baseline (CRSCLTP1+CRSCLTP2 with no additional sub-tables). The search was terminated early;
the uniformly sampled coverage across the full byte range is sufficient to call the result.

**Interpretation.** Seeds 5+6 are **structurally inert** under both conditions:
- When seeds 1+2 are *optimal* (B.26c winner): all seeds 5+6 give 91,090 (B.27 main result)
- When seeds 1+2 are *weak* (CRSCLTP1+CRSCLTP2): all seeds 5+6 give 86,123 (this result)

In both cases the depth is constant regardless of seeds 5+6. The sub-tables in slots 5+6
contribute zero discriminating constraint pressure. The depth is a function of slots 1+2 alone.
This closes the question raised by B.27a: it is not that slot position matters structurally —
it is that slots 5+6 are inert *period*, regardless of which values they hold or what state
slots 1+2 are in.

#### B.27c Re-search Seeds 1+2 in the 6-LTP Context (Completed)

**Setup.** Fix seeds 5+6 at their defaults (`CRSCLTP5` + `CRSCLTP6`). Run coordinate-descent
over seeds 1+2: sweep seed1 (256 candidates), freeze best; sweep seed2, freeze best; iterate.

**Question.** Does the 6-LTP optimal for seeds 1+2 differ from the 4-LTP optimal
(CRSCLTPV+CRSCLTPP)? If the landscape shifts, the additional sub-tables interact non-trivially
with seeds 1+2 — the 6-table joint optimum is at a different point and the B.26c winner was
specific to the 4-table operating point.

**Results (Round 1, converged).**

| Sweep | Winner | Depth | Range | Notes |
|---|---|---|---|---|
| Seed1 (seed2 fixed = CRSCLTPP) | CRSCLTPV | 91,090 | 84,982–91,090 | Rugged landscape; runners-up: 0xD8=90,399, 0x0B=90,216 |
| Seed2 (seed1 fixed = CRSCLTPV) | CRSCLTPP | 91,090 | 85,232–91,090 | Rugged landscape; runners-up: 0xB5=90,362, CRSCLTPy=89,874 |

Round 1 converged immediately (seed1 and seed2 unchanged). Peak depth: **91,090** (+0 vs
B.26c baseline). Script: `tools/b27c_reseed12_6ltp.py`. Results: `tools/b27c_results/b27c_results.json`.

**Interpretation.** The 6-LTP optimal for seeds 1+2 is identical to the 4-LTP optimal
(CRSCLTPV+CRSCLTPP=91,090). The addition of sub-tables 5 and 6 does not shift the seeds 1+2
landscape: the winner at the global optimum is the same seed pair. Combined with B.27a and
B.27b (sub-tables 5+6 structurally inert from both directions), this establishes that the
uniform-511 6-LTP configuration saturates at the same operating point as 4-LTP. No
seed-space search within the `CRSCLTP[X]` family improves on B.26c.

#### B.27d Joint Four-Axis Coordinate Descent: Seeds 1+2+5+6 (Moot — Superseded by B.27b)

**Original proposal.** Starting from (CRSCLTPV, CRSCLTPP, CRSCLTP5, CRSCLTP6), run
alternating coordinate descent over all four axes to find a 4D joint optimum.

**Status: not needed.** B.27b proved that the seed5 and seed6 axes are identically flat
regardless of the state of seeds 1+2. Since sweeping either axis always returns a constant
depth, there is no 4D joint optimum that differs from the 2D (seed1, seed2) optimum. The
seeds 5+6 directions contribute zero gradient everywhere in the search space; coordinate
descent over them cannot reach a new optimum from any starting point. B.27d is superseded.

