### B.23 Clipped-Triangular LTP Partitions

#### B.23.1 Motivation

B.25 (uniform-511 LTP) produces depth ~86,123. B.22 (full-coverage variable-length LTP,
triangular distribution with ltp_len(k) = 4·min(k+1, 511−k) − adj(k), range 2--1,021) achieved
depth ~80,300, a 7% *regression* from B.20's 88,503. The root cause: B.22's shortest lines (2-8
cells) are exhausted within the first few DFS rows and contribute no forcing in the plateau band
(rows 100-300). Their slots in the sub-tables are "wasted" relative to a longer line that would
still be active at the plateau.

B.21 (proposed joint-tiled variable-length, ltp_len(k) = min(k+1, 511−k), range 1-256) was
evaluated analytically but never implemented because B.22's experimental regression contradicted
the theoretical premise: short lines were expected to generate forcing cascades in early rows that
reduce the solver's branching factor, but in practice the extreme short lines (< 16 cells) provide
too little cross-row coverage to survive contact with the plateau band.

B.23 proposes *clipped-triangular* LTP line lengths: each line $k$ has length
$\text{ltp\_len}(k) = \max(L_{\min}, \min(k+1, 511-k))$, where $L_{\min}$ is a minimum clip
threshold. The triangular distribution is preserved at the top (maximum 256 cells for k=255),
but all lines shorter than $L_{\min}$ are extended to $L_{\min}$. This eliminates the
ineffective very-short lines while retaining the length variation that distinguishes B.23 from
uniform-511.

The working hypothesis is that B.22's regression is driven by the lines below some threshold
$L_{\min}^*$: lines shorter than $L_{\min}^*$ add noise (consuming cell slots for lines that
never generate plateau-band forcing) while lines at or above $L_{\min}^*$ provide the structural
variation that improves constraint topology. If this hypothesis holds, the optimal clip threshold
$L_{\min}^*$ lies where the marginal value of line-length variation (longer lines → more plateau
coverage) balances the marginal cost (shorter lines → fewer cells assigned to plateau-active
lines).

#### B.23.2 Objective

Find a clip threshold $L_{\min} \in \{16, 32, 64, 128\}$ such that the clipped-triangular LTP
partition achieves depth strictly greater than B.25's uniform-511 baseline of ~86,123 while
maintaining throughput $\geq 250K$ iter/sec. A secondary objective is to determine whether
the full uniform-511 ceiling (~88,503 from B.20) is reachable with clipped-triangular geometry,
or whether uniform-511 is structurally superior for any achievable $L_{\min}$.

#### B.23.3 Partition Structure

For a given clip threshold $L_{\min}$:

$$
\text{ltp\_len}(k) = \max\!\left(L_{\min},\; \min(k+1, \; 511-k)\right), \quad k = 0, 1, \ldots, 510
$$

The resulting length distribution:
- For $k < L_{\min} - 1$ or $k > 511 - L_{\min}$: $\text{ltp\_len}(k) = L_{\min}$ (clipped)
- For $L_{\min} - 1 \leq k \leq 511 - L_{\min}$: $\text{ltp\_len}(k) = \min(k+1, 511-k)$
  (unclipped triangular, range $L_{\min}$-256)

**Total cells per sub-table** (sum over k=0..510):

$$
N(L_{\min}) = 2\!\sum_{k=0}^{L_{\min}-2}\! L_{\min} \;+\; \sum_{k=L_{\min}-1}^{511-L_{\min}}\!\min(k+1, 511-k)
$$

For the clipped region, each of the $2(L_{\min}-1)$ short lines contributes $L_{\min}$ cells
instead of 1..$(L_{\min}-1)$ cells, so $N(L_{\min}) > N(0) = 65,536$ (B.21). Coverage exceeds
B.21 but may not reach 261,121 (full coverage). The four sub-tables together must cover all
261,121 cells; the construction protocol (B.23.4) is responsible for complete coverage.

**Payload impact:** Variable-width encoding uses $\lceil\log_2(\text{ltp\_len}(k)+1)\rceil$ bits
per element. For $L_{\min} = 64$: minimum width = $\lceil\log_2(65)\rceil = 7$ bits; for
$L_{\min} = 128$: minimum width = $\lceil\log_2(129)\rceil = 8$ bits. The mean bit-width
increases as $L_{\min}$ increases, converging to 9 bits (uniform-511) at $L_{\min} = 256$.
`kBlockPayloadBytes` changes accordingly.

#### B.23.4 Construction Protocol

The clipped-triangular partition is built using the same sequential spatial-assignment algorithm
as B.22, with $\text{ltp\_len}(k)$ substituted for the B.22 length formula:

1. For each sub-table $j = 0, 1, 2, 3$:
   - Build candidate pool = cells with assignment count $< j+1$
   - Sort lines in decreasing order of $\text{ltp\_len}(k)$ (longest lines first)
   - For each line in sorted order:
     - Seed LCG: $\text{seed} = \text{baseSeed}[j] \oplus (k \times k_A + k_C)$
     - LCG-shuffle candidate pool
     - Sort by ascending spatial score (corners first)
     - Assign first $\text{ltp\_len}(k)$ candidates to line $k$ of sub-table $j$
     - Mark assigned cells

This is identical to B.22's construction except for the length function. The spatial-score sort
and LCG shuffle are unchanged. CSR storage (offsets + cells arrays) is used exactly as in B.22.

#### B.23.5 Impact on Solver Architecture

All solver components change only at the `ltpLineLen(k)` call site — the single function that
maps a line index to its length. Everything downstream (ConstraintStore stats initialization,
propagation loop, serialization bit-width) already reads from `ltpLineLen(k)`. The change is
therefore local to `LtpTable.h`:

```cpp
[[nodiscard]] inline std::uint32_t ltpLineLen(const std::uint16_t k) noexcept {
    constexpr std::uint32_t kClip = 64U;  // or 32, 128 — experiment parameter
    const auto tri = static_cast<std::uint32_t>(std::min(k + 1U, 511U - k));
    return std::max(kClip, tri);
}
```

No changes to ConstraintStore, PropagationEngine, BranchingController, or the DFS loop. The
`kBlockPayloadBytes` constant changes to reflect the new bit-width distribution.

#### B.23.6 Expected Outcomes

The expected depth ceiling as a function of $L_{\min}$:

| $L_{\min}$ | Description                     | Predicted Depth | Rationale |
|:-----------:|:--------------------------------|:---------------:|:----------|
| 1 (B.22)   | Full triangular, no clip         | ~80,300         | Short lines too weak; established experimentally |
| 16          | Clip at 16 cells                 | ~82--84K        | Some short-line noise eliminated; medium lines still active |
| 32          | Clip at 32 cells                 | ~84--86K        | Most short-line noise gone; expect near-parity with uniform-511 |
| 64          | Clip at 64 cells                 | ~85--88K        | Short lines gone; length variation in plateau range; target outcome |
| 128         | Clip at 128 cells                | ~86--88K        | Near-uniform; small variation at top; expected parity with B.25 |
| 256 (uniform-511) | Full uniform, no variation | ~86,123        | B.25 baseline |

The hypothesis predicts a performance peak at $L_{\min} = 64$ or $L_{\min} = 128$, where
line-length variation is concentrated in the plateau band (lines of length 64--256 correspond
to $k \in [63, 447]$, covering 384 of 511 lines and exactly the rows that stall).

**Best case:** $L_{\min} = 64$ achieves depth $\geq 89{,}000$, confirming that length variation
at the 64--256 scale provides structural benefits that uniform-511 cannot. B.23 becomes the new
production partition architecture.

**Likely case:** $L_{\min} = 64$ achieves depth ~87--88K, roughly matching or slightly exceeding
B.25, with lower `hash_mismatch_rate` (reflecting better plateau-band constraint quality). The
result is directionally positive but not a breakthrough; B.23 may be combined with B.22's seed
search to extract further gains.

**Pessimistic case:** All clip thresholds perform at or below B.25's 86,123 baseline, replicating
B.22's regression at milder scale. This would indicate that any length variation is detrimental
under the current construction protocol and that uniform-511 is the structural optimum for
Fisher-Yates partitions. Further architectural improvement would require either joint-tiling
(B.21) or a different construction strategy.

#### B.23.7 Relationship to Other Proposals

*B.22 (seed search).* B.22 and B.23 are complementary and independent. B.22 varies the seed
within a fixed (uniform-511) length distribution; B.23 varies the length distribution with a
fixed construction algorithm. The optimal combination is a clipped-triangular partition with
a seed-searched assignment. If B.23's $L_{\min} = 64$ peak outperforms B.22's best uniform-511
seed, the partition architecture (B.23) takes precedence and seed search within the B.23
architecture becomes the follow-on step.

*B.25 (current production).* B.23 is implemented as a drop-in replacement for B.25's
`ltpLineLen(k)`. No other files change. The experiment can be run by modifying a single
constant in `LtpTable.h` and rebuilding, making iteration fast.

*B.21 (proposed joint tiling).* B.21's joint-tiling construction protocol is significantly
more complex (CSR storage, `LtpMembership` forward table, variable cell coverage per sub-table).
B.23 retains B.25's simpler "every cell in all 4 sub-tables" ownership model. If B.23 achieves
the depth improvement, B.21's complexity is unnecessary. B.23 should be fully evaluated before
B.21 implementation is considered.

#### B.23.8 Open Questions

(a) What is the optimal clip threshold $L_{\min}^*$? The prediction in B.23.6 suggests 64--128,
but the true optimum may differ. Does depth ceiling vary monotonically with $L_{\min}$ (allowing
binary search), or does it exhibit a non-monotone peak?

(b) Does the optimal $L_{\min}^*$ interact with the seed constants? A clipped-triangular
partition at $L_{\min} = 64$ with default seeds may be suboptimal; combining $L_{\min}$ search
with seed search (B.22) may be required to find the global optimum. Does the $\Phi$ metric from
B.22.4 extend naturally to variable-length lines (weighting short lines less in the $\Phi$ sum)?

(c) Does the construction order (longest lines first) still produce optimal spatial layout for
clipped lines? For B.22's full-triangular construction, sorting by decreasing length ensures
that long lines (which cover the most cells) are assigned first and therefore receive higher
spatial priority. For clipped distributions, the many equal-length clipped lines at both ends of
the $k$ range are assigned in an arbitrary order. Does sub-sorting within the clipped band by
spatial score produce better partitions?

(d) How does `kBlockPayloadBytes` change across the tested $L_{\min}$ values, and does the
encoding cost offset any solver depth benefit in terms of compression ratio? Specifically: does
$L_{\min} = 64$ produce a smaller block payload than uniform-511, and if so, does that improve
the overall compression ratio $C_r$ meaningfully?

(e) Is the clipped-triangular architecture compatible with the LTP membership model (B.21's
joint-tiling)? If the flat "all 4 sub-tables" model is retained (as in B.23), can the
construction protocol be extended to allow joint-tiling within the clipped-triangular length
distribution for a future experiment?

#### B.23.9 Experimental Results (ABANDONED)

B.23 was implemented and tested with $L_{\min} = 64$ on 2026-03-05.

**Implementation:** Changed `ltpLineLen(k)` to `max(64, min(k+1, 511-k))` in `LtpTable.h`.
The construction assigns cells in decreasing-length order.  **Critical finding discovered during
implementation:** sum of `ltp_len(k)` over $k=0..510$ equals **69,568** (not 261,121).  With
261,121 cells in the pool and only 69,568 assigned per sub-table, coverage is 26.7% per sub-table.
Expected LTP memberships per cell: $4 \times 0.267 \approx 1.07$ (mostly 0 or 1).

**Result:** Depth peaked at **~46,022** (vs 86,123 for B.25 uniform-511). This is a **46%
regression**, far worse than B.22's 7% regression.

**Root cause:** The spec's claim that B.23 "retains B.25's 'every cell in all 4 sub-tables'
ownership model" is mathematically impossible for the given `ltp_len` formula — the total
cell budget is 4 × 69,568 = 278,272, covering 261,121 cells an average of **1.07 times** (not 4).
73% fewer LTP constraints than B.25 cripples propagation quality.

**Conclusion:** The simple "change `ltpLineLen(k)`" implementation is fundamentally broken for
any clip value where `sum(ltp_len(k)) < 261,121`.  B.23 as specified requires the full B.21-style
joint-tiling construction to achieve meaningful cell coverage.  Since B.21's joint-tiling was
already considered too complex and produced a regression itself (50,272 depth), B.23 is abandoned.

| Metric | B.25 (baseline) | B.23 kLtpClip=64 |
|:-------|:---------------:|:----------------:|
| Peak depth | ~86,123 | ~46,022 |
| Cells/sub-table | 261,121 | 69,568 |
| Avg LTP memberships/cell | 4.0 | ~1.07 |
| Depth regression | — | −46% |

**Abandonment verdict:** B.23 clipped-triangular (as a drop-in `ltpLineLen` substitution) is
abandoned.  The correct follow-on is B.22 seed search within the uniform-511 architecture (B.25),
which preserves full coverage while varying partition topology.

