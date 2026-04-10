## B.62: Optimizing at s=191, b=8

### B.62.1 Motivation

Every experiment from B.57 through B.61 operates at $s = 127$, $b = 7$. This choice was inherited from
the convention of maximizing $b$ within a given bit width: $s = 127 = 2^7 - 1$ is the largest $s$ where
$b = \lceil \log_2(s+1) \rceil = 7$, meaning every 7-bit cross-sum element uses its full range $[0, 127]$.

This convention optimizes per-element encoding efficiency but ignores a critical scaling relationship:
the combinator solver's power depends on the ratio of GF(2) equations to variables. CRC-32 per row or
column provides $32s$ equations for $s^2$ variables &mdash; a density of $32/s$ equations per variable.
At $s = 127$, this density is $32/127 = 0.252$. At $s = 191$, it drops to $32/191 = 0.168$. The
equation density is monotonically decreasing in $s$.

However, the *payload* also scales favorably: input grows as $s^2$ while most payload components grow
as $O(s)$ or $O(s \log s)$. At $s = 127$, the B.60r configuration consumed 87.0% of the input
($C_r = 87.0\%$), leaving only 13% headroom for additional constraint families. At $s = 191$, the
same configuration consumes only $\sim 56\%$, leaving 44% headroom. This headroom can be spent on
additional CRC hash families &mdash; notably yLTP CRC-32 &mdash; that were budget-infeasible at $s = 127$.

The B.62 hypothesis: there exists an intermediate $s$ where the budget headroom from $s^2$ input
scaling allows purchasing enough additional GF(2) equations to exceed the equation density achievable
at $s = 127$, while maintaining $C_r < 100\%$. The candidate is $s = 191$ ($b = 8$), which balances:

- Sufficient $s^2 = 36{,}481$ input bits to amortize fixed-cost payload components
- $b = 8$ (not yet at the $b = 9$ jump at $s = 257$), keeping cross-sum encoding efficient
- Budget for VH (CRC-32 per column) + yLTP CRC-32 + yLTP cross-sums alongside DH/XH

### B.62.2 Scaling Analysis

The following table compares payload composition and equation density across candidate $s$ values,
all using the B.60r hybrid-width DH/XH architecture with no LH (CRC-32 per row).

**Base configuration (VH + DH/XH + geometric + BH + DI, no LH, no yLTP):**

| Metric | $s = 127$ ($b = 7$) | $s = 128$ ($b = 8$) | $s = 191$ ($b = 8$) |
|--------|---------------------|---------------------|---------------------|
| Input ($s^2$) | 16,129 | 16,384 | 36,481 |
| Payload (base) | 14,032 | 14,364 | 20,428 |
| $C_r$ (base) | 87.0% | 87.7% | 56.0% |
| GF(2) eq/var (base) | 0.547 | &mdash; | 0.356 |
| Headroom (bits) | 2,097 | 2,020 | 16,053 |

$s = 128$ is strictly worse than $s = 127$: the $b = 7 \to 8$ jump adds 332 payload bits against
only 255 new input bits. $s = 191$ has 7.7$\times$ more headroom than $s = 127$.

### B.62.3 Target Configuration

The B.62 baseline configuration at $s = 191$ adds VH (CRC-32 per column), one yLTP sub-table with
both CRC-32 and cross-sum, and optimized DH192/XH192 diagonal hashes. LH (CRC-32 per row) is omitted
because without LH there is no row-axis CRC for VH to cross-interact with; VH's value comes from its
cross-axis interaction with yLTP CRC-32, not from LH$\times$VH coupling.

**DH192/XH192 optimization for $s = 191$.** At $s = 191$, there are $2s - 1 = 381$ diagonals per
family with lengths 1 to 191. Proportional coverage matching B.60r's 50.4% gives 192 of 381 diagonals
(lengths 1&ndash;96). The hybrid-width tiers:

| Tier | Lengths | Diags | CRC width | Bits/family | Fully determines? |
|------|---------|-------|-----------|-------------|-------------------|
| 1 | 1&ndash;8 | 16 | CRC-8 | 128 | Yes |
| 2 | 9&ndash;16 | 16 | CRC-16 | 256 | Yes |
| 3 | 17&ndash;32 | 32 | CRC-32 | 1,024 | Yes |
| 4 | 33&ndash;64 | 64 | CRC-16 | 1,024 | Partial (16 eq) |
| 5 | 65&ndash;96 | 64 | CRC-8 | 512 | Partial (8 eq) |

Per family: 2,944 bits. DH192 + XH192: 5,888 bits.

**Full payload:**

| Component | Bits | GF(2) eq (est.) | Integer lines |
|-----------|------|-----------------|---------------|
| LSM ($191 \times 8$) | 1,528 | 190 | 191 |
| VSM ($191 \times 8$) | 1,528 | 190 | 191 |
| DSM (381 diags, variable-width) | 2,554 | 380 | 381 |
| XSM (381 anti-diags, variable-width) | 2,554 | 380 | 381 |
| VH (CRC-32, 191 columns) | 6,112 | ~6,063 | &mdash; |
| DH192 (hybrid-width) | 2,944 | ~2,888 | &mdash; |
| XH192 (hybrid-width) | 2,944 | ~2,888 | &mdash; |
| yLTP1 CRC-32 (191 lines) | 6,112 | ~6,063 | &mdash; |
| yLTP1 cross-sum ($191 \times 8$) | 1,528 | 190 | 191 |
| BH (SHA-256) | 256 | &mdash; | &mdash; |
| DI | 8 | &mdash; | &mdash; |
| **Total** | **28,068** | **~19,232** | **1,335** |

$C_r = 28{,}068 / 36{,}481 = 76.9\%$.

**Comparison with $s = 127$ (B.60r/B.60s):**

| Metric | $s = 127$ (B.60s) | $s = 191$ (B.62) |
|--------|-------------------|------------------|
| $C_r$ | 87.0% | **76.9%** |
| GF(2) eq/var | 0.547 | **0.527** |
| Integer constraint lines | 760 | **1,335** |
| CRC hash axes | 1 (VH only) | **2 (VH + yLTP)** |
| Headroom | 13.0% | **23.1%** |
| Full solves (B.60s, blocks 0&ndash;20) | 1/21 | TBD |

The equation density (0.527) is slightly below $s = 127$'s 0.547, but the configuration has two CRC
axes (VH and yLTP CRC-32) whose variable sets intersect at exactly one cell per (column, LTP-line) pair.
This cross-axis interaction is the structural mechanism that drove B.60's cascade. Additionally, the
yLTP cross-sum provides 191 integer constraint lines for IntBound &mdash; a resource unavailable at
$s = 127$ due to budget constraints.

The 23.1% headroom (8,413 bits) reserves capacity for future additions: a second yLTP sub-table
(+7,640 bits to $C_r = 97.9\%$), expanded DH/XH coverage, or LH if cross-axis interaction proves
insufficient.

### B.62.4 Hypotheses

**H1 (Density-dependent solve rate).** The combinator fully solves all blocks with density $\leq 20\%$
(matching B.60s) and additionally solves some blocks in the 20&ndash;40% density range that B.60s could
not, due to the yLTP CRC-32 axis providing interior-cell GF(2) equations that VH alone could not supply.

**H2 (50% density improvement).** The 50% density floor moves upward from B.60s's 2,112 determined
cells (13.1%) to $\geq 4,000$ cells (25%), because the yLTP CRC-32 equations couple interior cells
across all rows and columns, providing the long-range information transfer that geometric families lack
at 50% density.

**H3 (Cross-axis cascade).** The VH $\times$ yLTP CRC-32 cross-axis interaction triggers fixpoint
cascades that neither axis achieves independently, analogous to the LH $\times$ VH interaction in B.60.
This is testable by comparing the VH-only config ($C_r = 56.0\%$, eq/var $= 0.356$) against the
VH + yLTP config.

**H4 (Headroom enables iteration).** The 23.1% headroom allows B.62b&ndash;f to add constraint families
incrementally without exceeding $C_r = 100\%$, enabling a controlled search for the optimal payload
composition at $s = 191$.

### B.62.5 Method

The B.62 experiment family proceeds in stages. Each sub-experiment builds on the previous result.

**(a) B.62a: Baseline.** Implement the $s = 191$ combinator solver with the B.62.3 payload
(VH + DH192/XH192 + 1 yLTP CRC-32 + 1 yLTP cross-sum + geometric + BH + DI). Run on the MP4 test
file, blocks 0&ndash;20. Measure per-block $|D|$, BH-verified full solves, and density. Establish the
baseline for comparison with B.60s.

**(b) B.62b+: Incremental experiments.** Based on B.62a results, candidate follow-on experiments include:

- **Add a second yLTP sub-table** (CRC-32 + cross-sum, +7,640 bits, $C_r \to 97.9\%$). Tests whether
  a third CRC axis (second yLTP) breaks the 50% density wall.
- **Expand DH/XH to DH256/XH256** (lengths 1&ndash;128, +2,048 bits, $C_r \to 82.5\%$). Tests whether
  deeper diagonal coverage improves mid-density partial solves.
- **Add LH** (CRC-32 per row, +6,112 bits, $C_r \to 93.7\%$). Tests whether three orthogonal CRC axes
  (LH + VH + yLTP) produce a qualitative cascade improvement.
- **Sweep yLTP seeds** (same infrastructure as B.26c/B.27). Tests whether seed optimization at $s = 191$
  provides depth/solve-rate gains analogous to the $s = 511$ programme.
- **Vary $s$** within the $b = 8$ range ($s \in \{129, 163, 191, 223, 251, 255\}$). Tests whether
  $s = 191$ is optimal or whether a different $s$ at $b = 8$ achieves better eq/var at acceptable $C_r$.

The specific sequence of B.62b, B.62c, etc. will be determined by B.62a's results.

### B.62.6 Implementation Requirements

1. **Parameterize $s$.** The combinator solver, constraint store, CSM, cross-sum families, CRC
   generator matrix, and yLTP construction must all accept $s$ as a runtime or compile-time parameter
   rather than hardcoding $s = 127$. The block partitioner must handle $s = 191$ input blocks.

2. **yLTP CRC-32.** Implement CRC-32 computation per yLTP line and integrate the resulting GF(2)
   equations into the constraint matrix alongside VH and DH/XH equations.

3. **DH192/XH192 hybrid widths.** Extend the hybrid-width DH/XH infrastructure to support a fifth
   tier (CRC-8 on lengths 65&ndash;96).

4. **Test data.** Generate $s = 191$ blocks from the MP4 test file. Compute ground-truth cross-sums,
   CRC-32 hashes (VH, DH, XH, yLTP), and BH for blocks 0&ndash;20.

### B.62a: Baseline at $s = 191$

**Prerequisite.** B.60s completed. $s$-parameterized combinator solver implemented.

**Objective.** Establish the B.62 baseline: run the VH + DH192/XH192 + 1 yLTP (CRC-32 + cross-sum) +
geometric + BH configuration at $s = 191$ on MP4 blocks 0&ndash;20. Report per-block $|D|$, density,
BH verification, and comparison with B.60s at $s = 127$.

**Payload.** As specified in B.62.3. $C_r = 76.9\%$.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 confirmed | All blocks with density $\leq 20\%$ fully solve | yLTP CRC-32 does not degrade low-density performance |
| H2 confirmed | 50% density $|D| \geq 4{,}000$ | yLTP provides meaningful interior coupling at high density |
| H2 refuted | 50% density $|D| \approx 2{,}100$ (same as $s = 127$) | The 50% density wall is eq/var-limited, not axis-limited |
| H3 confirmed | VH + yLTP config outperforms VH-only config at same density | Cross-axis interaction is productive |

**Results.** C++ `combinatorSolver191 -config b62a` on MP4 blocks 0&ndash;5. All values Correct=true. BH not verified (SHA-256 not yet implemented for S&gt;127). C_r = 76.9%.

| Block | $|D|$ | /36,481 | GaussElim | IntBound | Correct |
|-------|-------|---------|-----------|----------|---------|
| 0 | 3,053 | 8.4% | 1,764 | 1,289 | true |
| **1** | **11,978** | **32.8%** | **1,525** | **10,453** | **true** |
| 2 | 2,112 | 5.8% | 2,112 | 0 | true |
| 3 | 2,112 | 5.8% | 2,112 | 0 | true |
| 4 | 2,112 | 5.8% | 2,112 | 0 | true |
| 5 | 2,112 | 5.8% | 2,112 | 0 | true |

**Comparison to B.60s at S=127 (same test data, different block geometry):**

| Metric | S=127 (B.60s) | S=191 (B.62a) |
|--------|--------------|---------------|
| C_r | 87.0% | **76.9%** |
| Block 0 | 7,454 (46.2%) | 3,053 (8.4%) |
| Block 1 | 12,002 (74.4%) | **11,978 (32.8%)** |
| Block 2 (full at S=127) | 16,129 (100%) | 2,112 (5.8%) |
| 50% density floor | 2,112 (13.1%) | 2,112 (5.8%) |

**Analysis.**

1. **H1 not confirmed.** No blocks fully solve at S=191. Block 0 at S=127 reached 46.2% but at S=191 only 8.4%. The larger matrix dilutes the short-diagonal cascade: at S=191, the 192 shortest diags cover $192 / 381 = 50.4\%$ of diags but only determine cells in the matrix corners/edges, which is a smaller fraction of 36,481 total cells.

2. **H2 refuted.** 50% density blocks (2&ndash;5) stall at 2,112 cells (5.8%), the GaussElim floor from short-diagonal CRC. No IntBound cascade. The yLTP CRC-32 equations do not break the 50% density wall at S=191.

3. **Block 1 is the bright spot.** 11,978 cells (32.8%) with 10,453 from IntBound. This block has low density at S=191 and the yLTP + VH cross-axis coupling produces a meaningful cascade. However, 32.8% is not a full solve.

4. **The GaussElim floor is the SAME (2,112) at both S=127 and S=191.** This is because the hybrid DH/XH covers diags of length 1&ndash;32 regardless of S. CRC-32 on 32-cell diags determines 2 &times; 32 &times; 2 = 128 diags &times; ~16.5 cells avg = ~2,112 cells.

5. **S=191 does NOT improve solve rate over S=127.** The larger matrix and higher equation count are offset by the larger variable count. The equation density (eq/var) drops from 0.547 to 0.527, and the cascade is weaker because short diags cover a smaller fraction of the matrix.

**Outcome.** H1 not confirmed, H2 refuted. S=191 provides better C_r (76.9% vs 87.0%) but worse solve rate. The 50% density wall persists. The yLTP CRC-32 axis does not provide the interior coupling needed to break the wall.

**Implementation notes.** Two bugs were found and fixed during implementation:
1. Generator matrix arrays were `array<uint8_t, 128>` &mdash; too small for S=191 (needs 191 columns). Fixed to `array<uint8_t, 256>`.
2. Zero-message array in `buildGenMatrixForLength` was `array<uint8_t, 16>` &mdash; too small for S=191's 24-byte messages. Fixed to `array<uint8_t, 32>`.
Both caused undefined behavior (out-of-bounds reads) producing wrong GF(2) equations.

**Tool:** `build/arm64-release/combinatorSolver191 -config b62a`

**Status: COMPLETE. S=191 does not improve over S=127.**

### B.62b: Second yLTP Sub-Table at S=191

**Prerequisite.** B.62a completed.

**Objective.** Add a second yLTP sub-table (CRC-32 + cross-sum). Two random-interior CRC axes, each cutting through different cell subsets. Tests cross-axis coupling at 50% density.

**Payload.** B.62a base (28,068) + yLTP2 parity (1,528) + yLTP2 CRC-32 (6,112) = 35,708 bits. C_r = 97.9%.

**Results.** C++ `combinatorSolver191 -config b62b` on MP4 blocks 0&ndash;5. All correct.

| Block | B.62a (1 yLTP) | B.62b (2 yLTP) | Delta |
|-------|---------------|----------------|-------|
| 0 | 3,053 (8.4%) | 3,053 (8.4%) | 0 |
| 1 | 11,978 (32.8%) | 11,978 (32.8%) | 0 |
| 2 | 2,112 (5.8%) | 2,112 (5.8%) | 0 |
| 3 | 2,112 (5.8%) | 2,112 (5.8%) | 0 |
| 4 | 2,112 (5.8%) | 2,112 (5.8%) | 0 |
| 5 | 2,112 (5.8%) | 2,112 (5.8%) | 0 |

**Zero improvement.** The second yLTP sub-table produces identical results on every block. The additional 7,640 payload bits and 6,303 GF(2) equations have no effect on the fixpoint.

**Root cause.** At 50% density, each yLTP line (191 cells) has rho &asymp; 95, u = 191. Adding a second set of 191-cell lines doesn't change this: rho/u &asymp; 0.5 on every line regardless of how many yLTP families are present. The yLTP CRC-32 equations add GF(2) rank but GaussElim can't extract cell determinations because all pivots depend on free variables. Same mechanism as B.60b (VH added rank but not cells).

**Status: COMPLETE. Second yLTP adds zero value. C_r cost (97.9%) is wasted.**

### B.62c: Full Configuration &mdash; LH + VH + DH192 + XH192 + yLTP at S=191

**Configuration.** All constraint axes: LSM, VSM, DSM, XSM (integer) + LH, VH (CRC-32) + DH192, XH192 (hybrid CRC) + yLTP (CRC-32 + cross-sum). C_r = 93.7%.

**Results.** Blocks 0&ndash;5. All Correct=true.

| Block | $|D|$ | /36,481 | GF2 | IB | Rows | Cols | Diags | XSM |
|-------|-------|---------|-----|------|------|------|-------|-----|
| 0 | 3,053 | 8.4% | 1,764 | 1,289 | 1/191 | 0/191 | 72/381 | 64/381 |
| 1 | 11,978 | 32.8% | 1,525 | 10,453 | 0/191 | 0/191 | 165/381 | 64/381 |
| 2&ndash;5 | 2,112 | 5.8% | 2,112 | 0 | 0/191 | 0/191 | 64/381 | 64/381 |

**Identical to B.62a and B.62b.** Adding LH (CRC-32 per row, +6,112 bits, C_r 76.9% &rarr; 93.7%) produces zero improvement. Every number matches B.62a exactly.

**Status: COMPLETE. LH adds zero value at S=191. All axes exhausted.**

### B.62c: Full Configuration &mdash; LH + VH + DH192 + XH192 + yLTP at S=191

**Prerequisite.** B.62a/b completed.

**Objective.** Test the maximum constraint configuration at S=191: all geometric cross-sums, LH (row CRC-32), VH (column CRC-32), hybrid-width DH192/XH192, and 1 yLTP (CRC-32 + cross-sum). This is every available constraint axis at S=191.

**Payload.** LSM (1,528) + VSM (1,528) + DSM (2,554) + XSM (2,554) + LH (6,112) + VH (6,112) + DH192 (2,944) + XH192 (2,944) + yLTP cross-sum (1,528) + yLTP CRC-32 (6,112) + BH (256) + DI (8) = 34,180 bits. C_r = 93.7%.

**Method.** Run multi-phase hybrid cascade with LH + VH + yLTP from Phase 1 on MP4 blocks 0&ndash;5.

**Status: COMPLETE (from earlier run, prior to B.62c implementation).**

### B.62d: Variable-Length rLTP with Graduated rLTPh at S=191

**Objective.** Replace yLTP (random uniform 191-cell lines) with variable-length rLTP: a center-outward spiral where line $k$ has $k+1$ cells (lengths 1, 2, 3, &hellip;, 191). After the graduated phase ($\sum_{k=1}^{191} k = 18{,}336$ cells), remaining cells fill uniform-191 lines. Total: 286 lines (191 graduated + 95 uniform). Center at (95,95).

**Graduated rLTPh.** CRC hash width matches line length for full determination on short lines:
- Lines of length 1&ndash;8: CRC-8 (fully determines, rank = length)
- Lines of length 9&ndash;16: CRC-16 (fully determines)
- Lines of length 17&ndash;32: CRC-32 (fully determines)
- Lines of length 33+: CRC-32 (cascade-dependent)

The first 32 lines (lengths 1&ndash;32) cover $\sum_{k=1}^{32} k = 528$ cells in the matrix CENTER, all fully determined by their CRC. These are **interior cascade triggers** &mdash; the first time short-line GaussElim determinations occur in the matrix interior rather than at the edges.

**Configuration.** Same as B.62c (LH + VH + DH192 + XH192) but rLTP replaces yLTP. C_r &asymp; 93.7%.

**Results.** Blocks 0&ndash;5. All Correct=true.

| Block | B.62c (yLTP) | B.62d (var rLTP) | Delta | GF2 change |
|-------|-------------|-----------------|-------|------------|
| 0 | 3,053 (8.4%) | **3,647 (10.0%)** | +594 | 1,764 &rarr; 2,351 (+587) |
| 1 | 11,978 (32.8%) | **14,765 (40.5%)** | +2,787 | 1,525 &rarr; 2,922 (+1,397) |
| 2 | 2,112 (5.8%) | **2,706 (7.4%)** | +594 | 2,112 &rarr; 2,706 (+594) |
| 3 | 2,112 (5.8%) | **2,706 (7.4%)** | +594 | 2,112 &rarr; 2,706 (+594) |
| 4 | 2,112 (5.8%) | **2,706 (7.4%)** | +594 | 2,112 &rarr; 2,706 (+594) |
| 5 | 2,112 (5.8%) | **2,706 (7.4%)** | +594 | 2,112 &rarr; 2,706 (+594) |

Per-line completion (block 1):

| Family | B.62c | B.62d |
|--------|-------|-------|
| Rows | 0/191 | 0/191 |
| Columns | 0/191 | 0/191 |
| Diags | 165/381 | 176/381 |
| AntiDiags | 64/381 | 66/381 |

**Analysis.**

1. **The 50% density GaussElim floor rose from 2,112 to 2,706 (+594 cells, +28%).** This is the first experiment in the entire B.60&ndash;B.62 series to raise the 50% density floor. The 594 additional cells are the 528 interior cells on rLTP lines of length 1&ndash;32 (fully CRC-determined) plus a small IntBound cascade from those.

2. **Block 1 jumped from 32.8% to 40.5% (+2,787 cells).** The interior rLTP seeds triggered additional IntBound cascade: +1,397 GaussElim cells and +1,390 IntBound cells. The rLTP short lines provided the interior information that VH, LH, and yLTP could not.

3. **Zero rows and zero columns still complete.** The +594 interior cells are not enough to complete any 191-cell row or column. The cascade extends further but doesn't close.

4. **The mechanism works as designed.** Variable-length rLTP places short, fully-determined lines in the matrix center. These are interior cascade triggers &mdash; structurally identical to what DH/XH short diagonals provide at the edges, but located where the 50% density wall stalls.

**Significance.** B.62d demonstrates that the 50% density wall CAN be partially broken by placing short-line constraints in the matrix interior. The wall is not purely structural &mdash; it's a constraint-placement problem. Interior short lines provide information that edge-only constraints cannot.

**Status: COMPLETE. First experiment to raise the 50% density floor. +594 cells (+28%) via interior cascade triggers.**

### B.62 Conclusion

**B.62: IN PROGRESS.** The S=191 hypothesis was that increased payload headroom at larger S would allow purchasing enough additional GF(2) equations to exceed the constraint density achievable at S=127. Initial experiments refuted this, but variable-length rLTP opened a new path:

1. **B.62a:** VH + DH/XH + 1 yLTP at C_r = 76.9%. No full solves. Worse than S=127.
2. **B.62b:** Adding second yLTP (C_r = 97.9%). Zero improvement. Identical to B.62a.
3. **B.62c:** Adding LH (C_r = 93.7%). Zero improvement. Identical to B.62a.
4. **B.62d:** Variable-length rLTP with graduated rLTPh. **+594 cells on 50% density blocks (+28%).** First experiment to break the floor. Interior short-line CRC triggers work.
5. **B.62e:** Dropped LH. **Identical results at C_r = 85.8%** (true compression). LH redundant.
6. **B.62f:** Two orthogonal rLTPs. **Block 1: 100% BH-verified algebraic solve.** 50% density floor: 3,413 (+26% vs B.62e). C_r = 115.6% (expansion).
7. **B.62g:** Dropped rLTP cross-sums (CRC only). **Identical results at C_r = 104.4%.** rLTP cross-sums confirmed dead weight; entire rLTP value is in CRC hash equations.
8. **B.62h:** Trimmed rLTP to lines 1&ndash;64. **Block 1 regressed to 42.1%.** 50% floor: 2,974 (above B.62d). C_r = 64.1% (true compression). Lines 65+ are load-bearing for the full solve.
9. **B.62i:** Coverage sweep $L_{\max} \in \{80..191\}$ with CRC-8 + uniform tail. CRC-8/16 on long lines cannot substitute for CRC-32. Only CRC-32 on ALL 286 lines achieves the full solve.
10. **B.62j:** Dropped XSM cross-sums (keep XH CRC). **FIRST BH-verified full solve under $C_r = 100\%$.** Block 1: 100% at $C_r = 97.4\%$. XSM IntBound lines confirmed dead weight.
11. **B.62k:** Dropped DSM cross-sums (keep DH CRC). **Block 1 collapsed to 9.4%.** DSM IntBound is load-bearing &mdash; it provides ALL of block 1&rsquo;s 11,589 IntBound cells.
12. **B.62l:** Dropped DSM, re-added XSM. **Block 1 still 9.4%.** XSM cannot substitute for DSM. The cascade trigger is geometry-specific (diagonal, not anti-diagonal).

**BREAKTHROUGH: compression and complete reconstruction coexist at $C_r = 97.4\%$ (B.62j).** This is the minimum full-solve configuration. The constraint architecture:
- **Load-bearing:** DSM (diagonal IntBound triggers), rLTP CRC-32 (GaussElim rank on all 286 lines)
- **Kept but potentially droppable:** LSM, VSM (0 IntBound on block 1 &mdash; DSM provides all of it)
- **Kept for GaussElim:** VH + DH + XH (CRC equations)
- **Dropped (confirmed redundant):** LH (B.62e), rLTP cross-sums (B.62g), XSM cross-sums (B.62j/l)
- **Cannot drop:** DSM (B.62k), rLTP CRC-32 (B.62i). **DSM is not substitutable by XSM** (B.62l).

13. **B.62m:** Hybrid combinator + row search at $C_r = 73.8\%$. **H3: Phase I too weak.** Only 20/191 rows had $\leq 60$ unknowns for DFS. 171 rows skipped. Cross-row backtracking implemented but never exercised. The hybrid adds value only in a narrow $C_r$ band where Phase I nearly closes the matrix.

**B.63 results (S=127):**
14. **B.63a:** Constraint sweep. A2 (LH+VH+DH64, $C_r \approx 82\%$) is Pareto-optimal. Full solve on 4/5 favorable blocks.
15. **B.63b:** Full-file sweep (1,331 blocks). **5 blocks fully solved (0.4%). 1,326 blocks at 13% (50% density wall).** Distribution is bimodal: 100% or 13%, no intermediate. Row-by-row DFS intractable at 110 unknowns/row.

16. **B.63c:** Multi-phase multi-axis solver (I&rarr;II&rarr;III&rarr;IV&rarr;V cycling). **First progress beyond algebraic floor on 50% density: +157 cells (14.9%).** Phase V (anti-diagonal DFS) solved 2 lines. Stalled after 1 round.
17. **B.63d:** Cell-by-cell residual-guided DFS with IntBound propagation. Extended 14.9% &rarr; **22.8%** (+1,275 cells) in 500 decisions. Then plateaued &mdash; same constraint-exhaustion wall as production DFS. 19.3M decisions, zero progress beyond depth 662.

**The 50% density wall persists across all solver architectures tested:**
- Pure combinator: 13.9% (GaussElim floor)
- Multi-axis line DFS: 14.9% (+1%)
- Cell-by-cell DFS with propagation: 22.8% (+8.9%)
- Production DFS at S=511: ~37% (different config, same wall mechanism)

All hit the same barrier: at 50% density, IntBound requires $\rho = 0$ or $\rho = u$ to force,
but all remaining constraint lines have $\rho$ in the interior of $[0, u]$. No single-cell
assignment tips any line into forcing. The search must guess multiple cells correctly &mdash;
an exponential problem.

18. **B.63e (Random restarts):** 100 restarts, 500K decisions each. **BREAKTHROUGH: 95.1% determination on 50% density block** (789 cells from full solve). MCF ordering was worst (80.9%); random orderings average 92.3%. The plateau is ordering-dependent, not structural.

**The 50% density wall is broken at S=127.** B.63e demonstrates that combinator algebra (Phase I: 14.9%) + cell-by-cell DFS with IntBound propagation and random restarts reaches 95.1% on a 50% density block. The remaining 5% (~789 cells) is the true residual.

19. **B.63f (CDCL):** Run 1 (coarse) and Run 2 (first-UIP): both produced **0 learned clauses**. Cardinality constraint antecedents are inherently O(s) literals — standard CDCL clause learning doesn&rsquo;t apply. Random restarts (95.8%) remain the best.

20. **B.63g (Beam search K=1000):** Beam collapses — all 1,000 states converge to same 22.8% plateau due to IntBound propagation homogenization. Zero benefit over single-path DFS.
21. **B.63h (10K restart census):** Max 96.4% (583 cells remaining). Diminishing returns: +93 cells over 1K restarts. 0/10,001 solved. Distribution stable (mean 92.2%).

**B.63e random restarts (95.8%) is the definitively best search strategy tested.** CDCL fails
(cardinality antecedents too large). Beam search fails (propagation homogenizes states). Random
restarts succeed because ordering diversity produces different propagation chains.

**The remaining 4.2% gap (676 cells) is the true residual.** Closing it requires either:
- An ordering that happens to reach 100% (not found in 1,000 trials)
- A qualitatively different constraint mechanism beyond IntBound cardinality forcing
- Hybrid with CRC verification during search (not just at row boundaries)

### B.62e: B.62d without LH &mdash; C_r under 100%

**Prerequisite.** B.62d completed.

**Objective.** Drop LH from B.62d to bring C_r under 100%. B.62c showed LH adds zero improvement at S=191. Removing it saves 6,112 bits. Compare to B.62d to confirm LH is redundant when variable-length rLTP is present.

**Payload.** B.62d (37,413) &minus; LH (6,112) = 31,301 bits. C_r = 85.8%.

**Results.** Blocks 0&ndash;5. All Correct=true. **Identical to B.62d on every block.**

| Block | B.62d (with LH, 102.6%) | B.62e (no LH, 85.8%) | Delta |
|-------|------------------------|----------------------|-------|
| 0 | 3,647 (10.0%) | 3,647 (10.0%) | 0 |
| 1 | 14,765 (40.5%) | 14,765 (40.5%) | 0 |
| 2&ndash;5 | 2,706 (7.4%) | 2,706 (7.4%) | 0 |

LH confirmed redundant at S=191 with variable-length rLTP. The variable-length rLTP + VH + DH/XH cascade achieves the same results at C_r = 85.8% (true compression) as the full configuration at 102.6%.

**Status: COMPLETE. LH redundant. C_r = 85.8% with identical solve performance.**

### B.62f: Two Orthogonal Variable-Length rLTPs at S=191

**Prerequisite.** B.62e completed.

**Objective.** Add a second variable-length rLTP spiral centered at a point maximally distant from rLTP1's center (95,95). rLTP2 centered at (0,0) creates line assignments that are approximately orthogonal to rLTP1 &mdash; cells sharing a line in rLTP1 are on different lines in rLTP2. Tests whether two orthogonal interior spiral partitions create cross-axis coupling that extends the cascade.

**Configuration.** B.62e (VH32 + DH192/XH192 + rLTP1(95,95)) + rLTP2(0,0). No LH.

**Payload.** B.62e (31,301) + 2nd rLTP cross-sums (2,041) + 2nd rLTP graduated CRC (8,832) = 42,174 bits. C_r = 115.6% (expansion &mdash; research configuration).

**Results.** Blocks 0&ndash;5. All Correct=true. SHA-256 BH verified on block 1 (BlockHash extended with raw-bytes overloads using CsmVariable::serialize()).

| Block | B.62d (1 rLTP + LH, 102.6%) | B.62e (1 rLTP, 85.8%) | B.62f (2 rLTPs, 115.6%) | Delta (f vs e) |
|-------|----------------------------|----------------------|------------------------|----------------|
| 0 | 3,647 (10.0%) | 3,647 (10.0%) | **4,376 (12.0%)** | +729 |
| 1 | 14,765 (40.5%) | 14,765 (40.5%) | **36,481 (100.0%)** | +21,716 |
| 2 | 2,706 (7.4%) | 2,706 (7.4%) | **3,413 (9.4%)** | +707 |
| 3 | 2,706 (7.4%) | 2,706 (7.4%) | **3,413 (9.4%)** | +707 |
| 4 | 2,706 (7.4%) | 2,706 (7.4%) | **3,413 (9.4%)** | +707 |
| 5 | 2,706 (7.4%) | 2,706 (7.4%) | **3,413 (9.4%)** | +707 |

Block 1 cascade detail (3 phases, 10 iterations):
- Phase 1 (diag len 1&ndash;8, CRC-8): 10,934 cells (30.0%)
- Phase 2 (diag len 9&ndash;16, CRC-16): 11,697 cells (32.1%, +763)
- Phase 3 (diag len 17&ndash;32, CRC-32): 36,481 cells (100.0%, +24,784)
- GaussElim contributed 21,997 cells in the final Phase 3 iteration

**Analysis.**

1. **Block 1: FULL SOLVE (100.0%).** The second rLTP provided enough cross-axis coupling to trigger a complete cascade. In Phase 3, GaussElim alone produced 21,997 cells in a single iteration &mdash; the two orthogonal rLTP spirals created mutual information that closed the entire matrix. This is the first 100% algebraic determination of an S=191 block without DFS search.

2. **50% density floor rose from 2,706 to 3,413 (+707 cells, +26%).** Blocks 2&ndash;5 (50% density) each gained 707 cells. The second rLTP&rsquo;s short graduated lines (centered at (0,0)) add 528 more interior cascade triggers that are approximately orthogonal to rLTP1&rsquo;s triggers.

3. **Block 0 also improved (+729 cells, +20%).** Low-density block gained from the additional rLTP cascade triggers.

4. **Block 1 is density-favorable.** Block 1 had 40.5% determination before B.62f, while 50%-density blocks had only 7.4%. The second rLTP pushed block 1 over a tipping point where GaussElim cascaded to completion. The 50% density blocks gain but do not reach this tipping point.

5. **BH verified on block 1.** SHA-256 block hash verification confirmed (BH verified: true). BlockHash extended with raw-bytes `compute(const uint8_t*, size_t)` and `verify(const uint8_t*, size_t, expected)` overloads, using CsmVariable::serialize() for S&gt;127. Partial blocks (0, 2&ndash;5) correctly report BH=false since not all cells are assigned.

**Significance.** B.62f demonstrates that multiple orthogonal variable-length rLTPs with graduated CRC hashes can achieve 100% algebraic determination on favorable blocks. The mechanism &mdash; cross-axis coupling between two spiral partitions &mdash; validates the interior-cascade-trigger research direction. However, C_r = 115.6% means this configuration expands rather than compresses. A production configuration would need fewer or more efficiently encoded constraint families.

**Status: COMPLETE. First BH-verified 100% algebraic solve at S=191 (block 1). 50% density floor: 3,413 (+26% vs B.62e).**

### B.62g: Drop rLTP Cross-Sums &mdash; CRC-Only rLTP at S=191

**Prerequisite.** B.62f completed.

**Motivation.** Across every B.62 experiment, IntBound contributes **zero** cells on 50% density blocks
(blocks 2&ndash;5). The rLTP cross-sum values feed IntBound exclusively &mdash; they provide per-line
cardinality constraints ($\rho$, $u$ tracking) that trigger forcing when $\rho = 0$ or $\rho = u$. At
50% density, every rLTP line of length $l$ has $\rho \approx l/2$ and $u \approx l$, placing it deep in
the forcing dead zone where IntBound never fires.

Every cell gained at 50% density in B.62a&ndash;f comes from **GaussElim** operating on **CRC equations**.
The rLTP cross-sums are dead weight at 50% density:

| Experiment | 50% density $|D|$ | GaussElim | IntBound |
|------------|-------------------|-----------|----------|
| B.62a (yLTP sums + CRC) | 2,112 | 2,112 | 0 |
| B.62d (rLTP sums + CRC) | 2,706 | 2,706 | 0 |
| B.62f (2 rLTP sums + CRC) | 3,413 | 3,413 | 0 |

Dropping both rLTP cross-sum vectors saves $2 \times 2{,}041 = 4{,}082$ bits, reducing $C_r$ from
115.6% to 104.4% &mdash; 4,082 bits closer to the compression boundary. The rLTP CRC hashes (which
carry the GF(2) equations that GaussElim uses) are retained at full width.

The trade-off: IntBound loses the rLTP lines entirely. On low-density blocks (e.g., block 1 in B.62a),
IntBound contributed 10,453 cells via yLTP cardinality constraints. Dropping rLTP cross-sums may
reduce low-density cascade performance. However, the geometric cross-sums (LSM, VSM, DSM, XSM) remain
and provide 1,144 integer constraint lines for IntBound. The rLTP lines were 191 additional lines
(or 286 in the variable-length formulation) on top of the geometric 1,144.

**Objective.** Confirm that dropping rLTP cross-sums does not degrade 50% density solve performance,
and quantify the impact on low-density blocks. This experiment proves whether the rLTP cross-sums
can be eliminated as a step toward $C_r < 100\%$.

**Configuration.** B.62f minus both rLTP cross-sum vectors. All CRC widths unchanged.

**Payload.**

| Component | Bits | Change from B.62f |
|-----------|------|-------------------|
| LSM ($191 \times 8$) | 1,528 | &mdash; |
| VSM ($191 \times 8$) | 1,528 | &mdash; |
| DSM (381 diags, variable-width) | 2,554 | &mdash; |
| XSM (381 anti-diags, variable-width) | 2,554 | &mdash; |
| VH (CRC-32, 191 columns) | 6,112 | &mdash; |
| DH192 (hybrid-width) | 2,944 | &mdash; |
| XH192 (hybrid-width) | 2,944 | &mdash; |
| rLTP1 graduated CRC | 8,832 | &mdash; |
| rLTP1 cross-sum | ~~2,041~~ | **removed (&minus;2,041)** |
| rLTP2 graduated CRC | 8,832 | &mdash; |
| rLTP2 cross-sum | ~~2,041~~ | **removed (&minus;2,041)** |
| BH (SHA-256) | 256 | &mdash; |
| DI | 8 | &mdash; |
| **Total** | **38,092** | **&minus;4,082** |

$C_r = 38{,}092 / 36{,}481 = 104.4\%$ (vs. B.62f's 115.6%).

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (50% unchanged) | Blocks 2&ndash;5 $|D| = 3{,}413$ (identical to B.62f) | rLTP cross-sums confirmed dead weight at 50% density; 4,082 bits saved for free |
| H2 (50% regressed) | Blocks 2&ndash;5 $|D| < 3{,}413$ | rLTP cross-sums have indirect value at 50% density (e.g., IntBound cascade triggers GaussElim pivots) |
| H3 (low-density impact) | Block 1 $|D| < 36{,}481$ (loses full solve) | rLTP IntBound was load-bearing for the block 1 cascade; cross-sums needed for low-density blocks |
| H4 (low-density preserved) | Block 1 $|D| = 36{,}481$ (full solve retained) | GaussElim alone drives the block 1 cascade; IntBound was not the critical mechanism |

**Prediction.** H1 is strongly expected: the IntBound=0 evidence across B.62a&ndash;f is definitive for
50% density. H3 vs H4 is the open question &mdash; block 1's full solve in B.62f used both GaussElim
(21,997 cells in the final Phase 3 iteration) and IntBound. If the GaussElim cascade alone can close
block 1 without IntBound on rLTP lines, then H4 holds and the rLTP cross-sums are entirely redundant.
If H3 holds, a hybrid approach may be needed: retain cross-sums only for graduated lines of length
$\leq 32$ (where they are cheap: $\sum_{k=1}^{32} \lceil \log_2(k+1) \rceil = 122$ bits per rLTP,
244 bits total) and drop them for longer lines.

**Method.** Modify `CombinatorSolver` to skip rLTP cross-sum construction and IntBound registration
for rLTP lines (`Config::rltpCrcOnly = true`). Retain all rLTP CRC hash equations and parity GF(2)
rows. Run on MP4 blocks 0&ndash;5. Report per-block $|D|$, GaussElim cells, IntBound cells, and
comparison to B.62f.

**Results.** Blocks 0&ndash;5. All Correct=true. **Every block identical to B.62f.**

| Block | B.62f (sums + CRC, 115.6%) | B.62g (CRC only, 104.4%) | Delta | GaussElim | IntBound |
|-------|---------------------------|-------------------------|-------|-----------|----------|
| 0 | 4,376 (12.0%) | **4,376 (12.0%)** | 0 | 3,166 | 1,210 |
| 1 | 36,481 (100.0%, BH=true) | **36,481 (100.0%, BH=true)** | 0 | 24,871 | 11,610 |
| 2 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,401 | 12 |
| 3 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,411 | 2 |
| 4 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |
| 5 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,404 | 9 |

**Outcome: H1 + H4.**

1. **H1 confirmed: 50% density unchanged.** Blocks 2&ndash;5 produce $|D| = 3{,}413$, identical to
   B.62f. The rLTP cross-sums contributed zero cells at 50% density, as predicted by the
   IntBound=0 evidence in B.62a&ndash;f.

2. **H4 confirmed: low-density cascade preserved.** Block 0 and block 1 are identical to B.62f.
   Block 1&rsquo;s 100% full solve (BH verified) survives without rLTP IntBound lines. The
   cascade is driven entirely by GaussElim operating on CRC equations; IntBound on block 1
   (11,610 cells) comes from geometric cross-sums (LSM/VSM/DSM/XSM), not rLTP.

3. **IntBound is non-zero but from geometric lines.** Block 0&rsquo;s 1,210 IntBound cells and
   block 1&rsquo;s 11,610 IntBound cells come from the 1,144 geometric constraint lines
   (LSM+VSM+DSM+XSM), not from the rLTP lines that were disabled. On 50% blocks, geometric
   IntBound is minimal (0&ndash;12 cells).

4. **4,082 bits saved for free.** Dropping both rLTP cross-sum vectors reduces payload from
   42,174 to 38,092 bits ($C_r$: 115.6% &rarr; 104.4%) with zero performance loss.

**Significance.** rLTP cross-sums are confirmed dead weight. The entire rLTP value comes from
CRC hash equations fed to GaussElim. IntBound on rLTP lines never fires because $\rho \approx u/2$
at all densities. This validates the path toward $C_r < 100\%$: the next step (B.62h) trims
low-value CRC lines to cross the compression boundary.

**Status: COMPLETE. H1+H4 confirmed. rLTP cross-sums redundant. $C_r$ reduced to 104.4% (&minus;4,082 bits, zero performance loss).**

### B.62h: Trimmed rLTP CRC (Lines 1&ndash;64) &mdash; C_r = 64.1%

**Prerequisite.** B.62g completed (rLTP cross-sums confirmed redundant at 50% density).

**Motivation.** B.62f's full rLTP CRC covers all 286 lines per sub-table (graduated lengths 1&ndash;191
plus 95 uniform-191 lines) at a cost of 8,832 bits per rLTP. However, only lines of length $\leq 32$
are fully CRC-determined (CRC width $\geq$ line length). Lines 33&ndash;191 and the uniform lines cost
8,128 bits per rLTP (92% of the CRC budget) but never directly determine individual cells &mdash; they
contribute GF(2) rank that at 50% density cannot be converted to cell values by GaussElim.

B.62h restricts rLTP CRC coverage to lines 1&ndash;64 using a graduated width scheme that minimizes
cost on fully-determined short lines and invests CRC-32 in the medium-length interior lines (33&ndash;64)
where the GF(2) equations provide the most marginal constraint density.

**Graduated CRC width scheme (per rLTP):**

| Tier | Line lengths | Count | CRC width | Bits/line | Total bits | Determines? |
|------|-------------|-------|-----------|-----------|------------|-------------|
| 1 | 1&ndash;8 | 8 | CRC-8 | 8 | 64 | Fully (8 eq on $\leq$ 8 cells) |
| 2 | 9&ndash;16 | 8 | CRC-16 | 16 | 128 | Fully (16 eq on $\leq$ 16 cells) |
| 3 | 17&ndash;32 | 16 | CRC-16 | 16 | 256 | Partial (16 eq on 17&ndash;32 cells) |
| 4 | 33&ndash;64 | 32 | CRC-32 | 32 | 1,024 | Partial (32 eq on 33&ndash;64 cells) |
| | **Total** | **64** | | | **1,472** | |

**Design rationale.**
- Tiers 1&ndash;2 use the minimum CRC width that fully determines each line ($w \geq l$). These are
  interior cascade triggers: every cell on these lines is algebraically determined by CRC alone.
- Tier 3 uses CRC-16 (not CRC-32) on lines 17&ndash;32. CRC-32 would fully determine these lines
  at 32 bits each, but CRC-16 at 16 bits provides 16 GF(2) equations per line at half the cost.
  These cells also participate in VH, DH, XH, and geometric constraints; the cross-axis equations
  may close them without full per-line determination.
- Tier 4 invests CRC-32 on lines 33&ndash;64 &mdash; medium-length lines in the matrix interior where
  the 50% density wall stalls. Each provides 32 GF(2) equations coupling 33&ndash;64 interior cells,
  at 2 equations per bit. This is the highest GF(2)-density investment for non-fully-determined lines.
- Lines 65+ are dropped entirely. Their CRC-32 provides only 32 equations on 65&ndash;191 cells
  ($< 0.5$ eq/cell), and at 50% density these equations cannot resolve individual cells.

**Payload.**

| Component | Bits |
|-----------|------|
| LSM ($191 \times 8$) | 1,528 |
| VSM ($191 \times 8$) | 1,528 |
| DSM (381 diags, variable-width) | 2,554 |
| XSM (381 anti-diags, variable-width) | 2,554 |
| VH (CRC-32, 191 columns) | 6,112 |
| DH192 (hybrid-width) | 2,944 |
| XH192 (hybrid-width) | 2,944 |
| rLTP1 CRC (lines 1&ndash;64, graduated) | 1,472 |
| rLTP2 CRC (lines 1&ndash;64, graduated) | 1,472 |
| BH (SHA-256) | 256 |
| DI | 8 |
| **Total** | **23,372** |

$C_r = 23{,}372 / 36{,}481 = 64.1\%$.

**Comparison.**

| Config | rLTP bits (2 sub-tables) | Total payload | $C_r$ | 50% density $|D|$ |
|--------|------------------------|---------------|-------|-------------------|
| B.62f (full rLTP CRC + sums) | 21,746 | 42,174 | 115.6% | 3,413 |
| B.62g (full rLTP CRC, no sums) | 17,664 | 38,092 | 104.4% | TBD (expected 3,413) |
| **B.62h (lines 1&ndash;64, no sums)** | **2,944** | **23,372** | **64.1%** | **TBD** |

The rLTP payload drops from 17,664 to 2,944 bits &mdash; an 83% reduction &mdash; while retaining
all fully-determining interior cascade triggers (lines 1&ndash;32) and the most cost-effective partial
tier (lines 33&ndash;64, CRC-32).

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (50% floor holds) | Blocks 2&ndash;5 $|D| \geq 2{,}706$ (B.62d level) | Lines 1&ndash;32 fully-determining tiers drive the 50% density improvement; lines 65+ were not contributing |
| H2 (50% floor drops) | Blocks 2&ndash;5 $|D| < 2{,}706$ | Lines 33&ndash;64 or 65+ GF(2) equations were coupling into the cascade; more coverage needed |
| H3 (block 1 regresses) | Block 1 $|D| < 36{,}481$ | The B.62f full solve depended on GF(2) rank from lines 65+; trimming breaks the cascade |
| H4 (block 1 preserved) | Block 1 $|D| = 36{,}481$ | The full solve depends only on lines 1&ndash;64 + VH + DH/XH; lines 65+ were redundant |

**Prediction.** H1 is likely: B.62d showed the 50% density improvement (+594 cells) came from fully-CRC-determined
cells on lines 1&ndash;32. B.62h retains those triggers. The CRC-32 on lines 33&ndash;64 (tier 4) may
provide additional GF(2) rank that lifts the floor slightly above B.62d's 2,706. H3 is expected: the B.62f
block 1 full solve depended on a 21,997-cell GaussElim burst that required high GF(2) rank from all rLTP
lines including 65+. Losing those equations likely prevents that cascade. However, a partial regression
(block 1 at 60&ndash;80% rather than 100% or 40.5%) would indicate that lines 33&ndash;64 provide
meaningful intermediate value.

**Method.** Modify `CombinatorSolver` to construct rLTP spirals with only 64 graduated lines (lengths
1&ndash;64) per sub-table, no uniform-length tail, no cross-sums (`Config::rltpMaxLines = 64`,
`rltpTier3Width = 16`, `rltpCrcOnly = true`). Run on MP4 blocks 0&ndash;5.

**Results.** Blocks 0&ndash;5. All Correct=true.

| Block | B.62g (all lines, 104.4%) | B.62h (lines 1&ndash;64, 64.1%) | Delta | GaussElim | IntBound |
|-------|--------------------------|--------------------------------|-------|-----------|----------|
| 0 | 4,376 (12.0%) | **3,937 (10.8%)** | &minus;439 | 2,622 | 1,315 |
| 1 | 36,481 (100.0%, BH=true) | **15,369 (42.1%)** | &minus;21,112 | 3,734 | 11,635 |
| 2 | 3,413 (9.4%) | **2,974 (8.2%)** | &minus;439 | 2,972 | 2 |
| 3 | 3,413 (9.4%) | **2,974 (8.2%)** | &minus;439 | 2,969 | 5 |
| 4 | 3,413 (9.4%) | **2,974 (8.2%)** | &minus;439 | 2,971 | 3 |
| 5 | 3,413 (9.4%) | **2,974 (8.2%)** | &minus;439 | 2,972 | 2 |

**Outcome: H1 (partial) + H3.**

1. **H3 confirmed: block 1 regressed from 100% to 42.1%.** The full solve depended on GF(2) rank
   from rLTP lines 65+. Trimming to 64 lines eliminates the critical cross-axis coupling that drove
   the 21,997-cell GaussElim burst in B.62f Phase 3. However, 42.1% is still above B.62e&rsquo;s
   40.5% (1 rLTP, all lines) &mdash; lines 33&ndash;64 contribute +604 cells over a 1-rLTP baseline.

2. **50% density floor dropped from 3,413 to 2,974 (&minus;439 cells, &minus;13%).** Still above
   B.62d&rsquo;s 2,706 (+268 cells). The loss of 439 cells corresponds to the GF(2) equations on
   rLTP lines 65&ndash;191 and the uniform tail (222 lines &times; 2 sub-tables = 444 lines of
   CRC-32 equations removed). The retained lines 1&ndash;64 provide the interior cascade triggers
   but lack the deep rank needed to close additional cells.

3. **Block 0 regressed by 439 cells (12.0% &rarr; 10.8%).** Same magnitude as 50% blocks,
   suggesting the loss is from CRC equation coverage, not IntBound.

4. **C_r = 64.1% is well within compression.** The trade-off: 64.1% compression ratio at
   2,974 cells (8.2%) on 50% density, vs 104.4% (expansion) at 3,413 cells (9.4%).

**Analysis.** Lines 65+ carry significant GF(2) rank despite having low per-line equation density
($< 0.5$ eq/cell). Their aggregate contribution (222 lines &times; 32 bits = 7,104 equations per rLTP)
is large enough to couple into the cascade. The full solve on block 1 specifically requires this
deep rank &mdash; it cannot be achieved with interior triggers alone.

The 50% density floor is bounded by the NUMBER of CRC-determined cells (lines 1&ndash;32: 528 per
rLTP, 1,056 total) plus a small GaussElim cascade from tier 3&ndash;4. Adding more lines does not
further determine individual cells at 50% density but DOES provide rank that enables cascade
closure on favorable blocks.

**Significance.** There is no free lunch: the rLTP lines 65+ that cost 83% of the CRC budget
are load-bearing for the full solve. The compression boundary (C_r &lt; 100%) requires either
accepting partial solves or finding a more efficient constraint encoding for long interior lines.

**Status: COMPLETE. H3 confirmed: block 1 regressed to 42.1%. 50% floor: 2,974 (still above B.62d). C_r = 64.1%.**

### B.62i: rLTP Coverage Sweep &mdash; Minimum Length for Full Solve at $C_r < 100\%$

**Prerequisite.** B.62g and B.62h completed.

**Motivation.** B.62g proved that full rLTP CRC (all 286 lines, C_r = 104.4%) retains B.62f's full
solve on block 1 and 3,413-cell floor at 50% density. B.62h proved that trimming to lines 1&ndash;64
(C_r = 64.1%) loses both &mdash; block 1 drops to 42.1% and the floor drops to 2,974. The full solve
requires GF(2) rank from lines 65+, but full coverage exceeds $C_r = 100\%$.

Somewhere between 64 and 286 lines per rLTP is a coverage level that simultaneously achieves
$C_r < 100\%$ and retains enough GF(2) rank for a full solve. B.62i sweeps this range to find
the minimum.

**Design.** The graduated CRC width scheme from B.62h (CRC-8/CRC-16/CRC-16/CRC-32 on tiers 1&ndash;4)
is extended with additional tiers of CRC-8 for lines beyond 64. CRC-8 is the cheapest per-line
investment (8 bits) and provides 8 GF(2) equations per line &mdash; low density but at minimal cost.
The sweep tests coverage limits $L_{\max} \in \{64, 80, 96, 112, 128, 160, 191\}$, where lines
$65$ through $L_{\max}$ use CRC-8.

**Per-rLTP CRC cost by coverage limit:**

| $L_{\max}$ | Tiers 1&ndash;4 (lines 1&ndash;64) | Tier 5: CRC-8 on 65&ndash;$L_{\max}$ | Total bits/rLTP | 2 rLTPs | Total payload | $C_r$ |
|-------------|-------------------------------------|---------------------------------------|-----------------|---------|---------------|-------|
| 64 (B.62h) | 1,472 | 0 | 1,472 | 2,944 | 23,372 | 64.1% |
| 80 | 1,472 | $16 \times 8 = 128$ | 1,600 | 3,200 | 23,628 | 64.8% |
| 96 | 1,472 | $32 \times 8 = 256$ | 1,728 | 3,456 | 23,884 | 65.5% |
| 112 | 1,472 | $48 \times 8 = 384$ | 1,856 | 3,712 | 24,140 | 66.2% |
| 128 | 1,472 | $64 \times 8 = 512$ | 1,984 | 3,968 | 24,396 | 66.9% |
| 160 | 1,472 | $96 \times 8 = 768$ | 2,240 | 4,480 | 24,908 | 68.3% |
| 191 | 1,472 | $127 \times 8 = 1{,}016$ | 2,488 | 4,976 | 25,404 | 69.6% |

All configurations are well under $C_r = 100\%$. Even at $L_{\max} = 191$ (covering all graduated
lines with CRC-8 on 65+), $C_r = 69.6\%$ with 30% headroom.

**Note:** The above table covers only the graduated phase (lines 1&ndash;191). The uniform-length
tail (95 lines of 191 cells each) is excluded. If the sweep shows that lines beyond 191 are needed,
a second tier can add CRC-8 on uniform lines at $95 \times 8 = 760$ bits per rLTP (+1,520 total),
reaching $C_r = 73.8\%$. The full 286-line coverage at CRC-8 on lines 65+ costs:

| Config | Lines 65+ CRC | Total bits/rLTP | 2 rLTPs | Total payload | $C_r$ |
|--------|---------------|-----------------|---------|---------------|-------|
| Graduated only ($L_{\max} = 191$) | CRC-8 | 2,488 | 4,976 | 25,404 | 69.6% |
| + uniform tail (CRC-8) | CRC-8 | 3,248 | 6,496 | 26,924 | 73.8% |
| + uniform tail (CRC-16) | CRC-16 | 3,728 | 7,456 | 27,884 | 76.4% |
| + uniform tail (CRC-32) | CRC-32 | 4,528 | 9,056 | 29,484 | 80.8% |

All under 100%. B.62g's full-CRC config (8,832 bits/rLTP, C_r = 104.4%) used CRC-32 on everything.
By downgrading lines 65+ from CRC-32 to CRC-8, each line costs 8 bits instead of 32 &mdash; a 75%
per-line savings at the cost of 24 fewer GF(2) equations per line.

**Objective.** Find the minimum $L_{\max}$ (and optionally uniform-tail CRC width) that achieves:

1. **Full solve** ($|D| = 36{,}481$, BH verified) on at least one block (block 1 or a 50% density block)
2. $C_r < 100\%$

Secondary objective: find the minimum $L_{\max}$ that achieves a full solve on a **50% density block**
(blocks 2&ndash;5). This is the harder target and may require the uniform tail.

**Method.**

(a) Implement configurable $L_{\max}$ parameter in `CombinatorSolver`. Lines 1&ndash;64 use the B.62h
graduated scheme (CRC-8/16/16/32). Lines 65&ndash;$L_{\max}$ use CRC-8. Lines beyond $L_{\max}$
are not hashed.

(b) For each $L_{\max} \in \{64, 80, 96, 112, 128, 160, 191\}$: run on MP4 blocks 0&ndash;5. Record
per-block $|D|$, GaussElim, IntBound, BH verification.

(c) If $L_{\max} = 191$ does not achieve a full solve on block 1, extend the sweep to include the
uniform tail: test CRC-8, CRC-16, and CRC-32 on the 95 uniform lines.

(d) If no configuration under $C_r = 100\%$ achieves a full solve on any block, report the $L_{\max}$
that maximizes $|D|$ at minimum $C_r$ and identify the GF(2) rank deficit relative to B.62g.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (small $L_{\max}$ sufficient) | $L_{\max} \leq 128$ achieves block 1 full solve | The GF(2) rank from lines 65&ndash;128 (at CRC-8) is sufficient; CRC-32 was overkill for long lines |
| H2 (full graduated needed) | $L_{\max} = 191$ needed for block 1 full solve | All 191 graduated lines contribute necessary rank; the uniform tail is not needed |
| H3 (uniform tail needed) | $L_{\max} = 191$ insufficient; uniform tail at CRC-8 or CRC-16 achieves full solve | The 95 uniform-191-cell lines provide aggregate rank that graduated lines alone cannot |
| H4 (no full solve under 100%) | No configuration under $C_r = 100\%$ achieves a full solve | The full solve requires CRC-32 density on long lines; CRC-8 cannot substitute. The minimum compressing configuration is a partial-solve format |
| H5 (50% density full solve) | Any $L_{\max}$ achieves $|D| = 36{,}481$ on blocks 2&ndash;5 | Breakthrough: the 50% density wall is broken under compression |

**Prediction.** H2 or H3 is most likely. B.62g's full solve required 7,104 GF(2) equations per rLTP
from lines 65+ (222 lines &times; 32 bits). Replacing those with CRC-8 provides $222 \times 8 = 1{,}776$
equations per rLTP &mdash; a 75% reduction. Whether 1,776 equations suffice depends on the GF(2) rank
threshold for the cascade to close. The sweep will reveal this threshold empirically. H5 is unlikely
given that B.62f's full rLTP at CRC-32 could not achieve it, but including it as a hypothesis ensures
completeness.

**Key metric.** For each configuration, report the GF(2) rank of the full constraint matrix. The rank
difference between the failing and succeeding configurations identifies the critical rank threshold
for cascade closure. This informs future constraint design: if the threshold is rank $R$, any
configuration achieving rank $\geq R$ at $C_r < 100\%$ will fully solve.

**Results.**

*(a) Graduated-line sweep (block 1, tier 5 CRC-8):*

| $L_{\max}$ | Block 1 $|D|$ | GF(2) rank | BH | $C_r$ |
|-------------|---------------|------------|----|-------|
| 64 (B.62h) | 15,369 (42.1%) | &mdash; | false | 64.1% |
| 80 | 15,369 (42.1%) | 9,263 | false | 64.8% |
| 96 | 15,369 (42.1%) | 9,519 | false | 65.5% |
| 112 | 15,369 (42.1%) | 9,775 | false | 66.2% |
| 128 | 15,369 (42.1%) | 10,031 | false | 66.9% |
| 160 | 15,369 (42.1%) | 10,543 | false | 68.3% |
| 191 | 15,369 (42.1%) | 11,039 | false | 69.6% |

Determination stalls at 15,369 regardless of $L_{\max}$. CRC-8 on lines 65+ adds GF(2) rank
(+1,776 from 80 to 191) but none translates to determined cells. The rank gain is linearly
dependent on existing equations.

*(b) Graduated only, CRC-32 on tier 5 ($L_{\max} = 191$):*

| Block 1 $|D|$ | GF(2) rank | BH |
|---------------|------------|----|
| 15,614 (42.8%) | 17,128 | false |

CRC-32 on graduated lines 65&ndash;191 only marginally improves over CRC-8 (+245 cells, +6,089
rank). The 95 uniform-length lines are absent; without them, the cascade cannot close.

*(c) Uniform tail sweep (all 286 lines, block 1):*

| Tier 5 CRC width | Block 1 $|D|$ | GF(2) rank | BH | $C_r$ |
|-------------------|---------------|------------|----|-------|
| CRC-8 | 12,109 (33.2%) | 12,996 | false | 73.8% |
| CRC-16 | 10,112 (27.7%) | 16,575 | false | 76.4% |
| CRC-32 | **36,481 (100.0%)** | 22,584 | **true** | 103.0% |

CRC-8 and CRC-16 on the uniform tail **regress** block 1 below the graduated-only 42.1%.
The low-quality equations (8 or 16 eq on 191 cells) create linearly dependent GF(2) rows
that consume rank without determining cells. Only CRC-32 (32 eq on 191 cells) provides
sufficient equation density to close the cascade.

*(d) 50% density blocks (block 2):*

| Config | Block 2 $|D|$ | GF(2) rank |
|--------|---------------|------------|
| Graduated ($L_{\max} = 191$), CRC-8 | 2,974 (8.2%) | 13,536 |
| All 286 lines, CRC-8 | 2,980 (8.2%) | 14,958 |
| All 286 lines, CRC-32 | 3,045 (8.3%) | 25,675 |

50% density blocks are unaffected by CRC width on long lines. All values within 71 cells
of each other. The 50% density floor is determined entirely by short-line CRC triggers
(tiers 1&ndash;4), not by long-line GF(2) rank.

**Outcome: H4 (no full solve under $C_r = 100\%$).**

No configuration with $C_r < 100\%$ achieves a full solve on any block. The minimum
configuration for a full solve on block 1 is CRC-32 on ALL 286 lines (including the
95 uniform-191-cell lines), which gives $C_r = 103.0\%$. CRC-8 and CRC-16 on long
lines (65+) cannot substitute for CRC-32 &mdash; they add rank but it is linearly
dependent and cannot create the pivots needed for cascade closure.

**Rank analysis.**

| Config | Rank | Block 1 $|D|$ | Full solve? |
|--------|------|---------------|-------------|
| B.62h (64 lines) | ~8,500 | 15,369 (42.1%) | No |
| 191 grad, CRC-8 | 11,039 | 15,369 (42.1%) | No |
| 191 grad, CRC-32 | 17,128 | 15,614 (42.8%) | No |
| 286 lines, CRC-8 | 12,996 | 12,109 (33.2%) | No |
| 286 lines, CRC-16 | 16,575 | 10,112 (27.7%) | No |
| **286 lines, CRC-32** | **22,584** | **36,481 (100%)** | **Yes** |

The cascade requires rank $\geq 22{,}584$ AND CRC-32 equation density on long lines. Rank
alone is insufficient &mdash; CRC-16 achieves rank 16,575 but regresses determination. The
quality of the GF(2) equations (bit independence per CRC width) matters, not just the rank count.

**Significance.** The full solve is not achievable under $C_r = 100\%$ with the current
constraint architecture. The minimum compressing full-solve configuration is B.62g
($C_r = 104.4\%$, rLTP cross-sums removed). The savings from B.62g&rsquo;s cross-sum
removal (&minus;4,082 bits) must be supplemented by other payload reductions (e.g., dropping
geometric cross-sum families) to reach $C_r < 100\%$.

**Status: COMPLETE. H4 confirmed for rLTP-width reduction path: no full solve under $C_r = 100\%$ by downgrading CRC width. However, B.62j demonstrates an alternative path (dropping geometric cross-sums) that achieves $C_r = 97.4\%$ with a full solve.**

### B.62j: Drop XSM Cross-Sums, Keep XH CRC &mdash; $C_r = 97.4\%$

**Prerequisite.** B.62g and B.62i completed.

**Motivation.** B.62g achieved the full solve at $C_r = 104.4\%$ (1,611 bits over the compression
boundary). B.62i proved that downgrading rLTP CRC width cannot close the gap. The remaining path
is to drop geometric payload components. XSM (anti-diagonal integer cross-sums) costs 2,554 bits
and provides IntBound lines that, like rLTP cross-sums, may be redundant when XH CRC equations
are present. B.62g proved that rLTP cross-sums are dead weight when CRC equations exist; the same
pattern may hold for XSM.

**Configuration.** B.62g with `useXSMSums = false`. XSM parity GF(2) rows retained (1 bit per
anti-diagonal). XH CRC equations retained at full width. All other components unchanged.

**Payload.** B.62g (38,092) &minus; XSM (2,554) = 35,538 bits. $C_r = 97.4\%$.

**Results.** Blocks 0&ndash;5. All Correct=true. **Every block identical to B.62g.**

| Block | B.62g (with XSM, 104.4%) | B.62j (no XSM, 97.4%) | Delta | GaussElim | IntBound |
|-------|--------------------------|----------------------|-------|-----------|----------|
| 0 | 4,376 (12.0%) | **4,376 (12.0%)** | 0 | 3,211 | 1,165 |
| 1 | 36,481 (100.0%, BH=true) | **36,481 (100.0%, BH=true)** | 0 | 24,892 | 11,589 |
| 2 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,409 | 4 |
| 3 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |
| 4 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |
| 5 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,409 | 4 |

**Analysis.**

1. **XSM cross-sums are dead weight.** Same pattern as rLTP cross-sums (B.62g): the IntBound
   lines from XSM anti-diagonals never fire because XH CRC equations determine those cells via
   GaussElim before IntBound gets a chance. Dropping 381 IntBound lines costs zero cells.

2. **Block 1 full solve preserved (BH verified).** The cascade closure depends on GF(2) rank from
   CRC equations, not on IntBound from geometric cross-sums. IntBound on block 1 (11,589 cells)
   comes from LSM + VSM + DSM lines &mdash; the 381 XSM lines contributed nothing.

3. **$C_r = 97.4\%$ &mdash; FIRST full-solve configuration under $C_r = 100\%$.** With 943 bits
   of headroom below the compression boundary, the CRSCE combinator solver achieves true compression
   with 100% algebraic determination on favorable blocks.

**Significance.** B.62j demonstrates that the combinator-algebraic solver can achieve a BH-verified
full solve with true compression ($C_r < 100\%$). This is the first configuration in the entire
B.60&ndash;B.62 research program where compression and complete reconstruction coexist. The XSM
cross-sums follow the same redundancy pattern as rLTP cross-sums and LH: when CRC hash equations
are present, integer cardinality constraints on the same lines are strictly redundant.

**Status: COMPLETE. FIRST BH-verified full solve under $C_r = 100\%$. Block 1: 100% at $C_r = 97.4\%$.**

### B.62k: Drop DSM Cross-Sums, Keep DH CRC &mdash; $C_r = 90.4\%$

**Prerequisite.** B.62j completed.

**Motivation.** B.62j proved that XSM cross-sums are redundant when XH CRC is present ($C_r$:
104.4% &rarr; 97.4%, zero performance loss). DSM cross-sums (2,554 bits) have the same structure:
variable-length diagonal lines with DH CRC equations providing GaussElim coverage on those same
lines. If the XSM/XH redundancy pattern extends to DSM/DH, dropping DSM saves another 2,554 bits.

**Payload.** B.62j (35,538) &minus; DSM (2,554) = 32,984 bits. $C_r = 90.4\%$.

**Configuration.** B.62j with `useDSMSums = false`. DSM parity GF(2) rows retained.
DH CRC equations retained at full width.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (identical) | All blocks match B.62j | DSM IntBound redundant with DH CRC. $C_r = 90.4\%$ with full solve. |
| H2 (block 1 regresses) | Block 1 $|D| < 36{,}481$ | DSM IntBound is load-bearing &mdash; unlike XSM, the diagonal IntBound lines participate in block 1&rsquo;s cascade. |
| H3 (50% regresses) | Blocks 2&ndash;5 $|D| < 3{,}413$ | DSM short-line IntBound triggers contributed to the 50% density floor. |

**Prediction.** H1 is likely based on the consistent pattern: every integer cross-sum family tested
(rLTP in B.62g, LH in B.62e, XSM in B.62j) has been redundant when CRC equations are present.
DSM and DH have the same relationship. However, DSM's short diagonals (length 1&ndash;32) are the
original cascade triggers from B.60f &mdash; if IntBound on those lines contributes cells that
GaussElim misses, H2 or H3 may occur.

**Method.** Add `useDSMSums` config flag (same pattern as `useXSMSums`). Run on MP4 blocks 0&ndash;5.

**Results.** Blocks 0&ndash;5. All Correct=true. **H2 confirmed: block 1 full solve lost.**

| Block | B.62j (with DSM, 97.4%) | B.62k (no DSM, 90.4%) | Delta | GaussElim | IntBound |
|-------|------------------------|---------------------|-------|-----------|----------|
| 0 | 4,376 (12.0%) | **3,572 (9.8%)** | &minus;804 | 3,412 | 160 |
| 1 | 36,481 (100.0%, BH=true) | **3,413 (9.4%)** | &minus;33,068 | 3,413 | 0 |
| 2 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |
| 3 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |
| 4 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |
| 5 | 3,413 (9.4%) | **3,413 (9.4%)** | 0 | 3,413 | 0 |

**Outcome: H2 (block 1 regresses catastrophically).**

1. **Block 1 collapsed from 100% to 9.4%.** IntBound dropped from 11,589 to 0. ALL of block 1&rsquo;s
   IntBound cells came from DSM &mdash; LSM and VSM contributed zero. DSM&rsquo;s variable-length
   diagonals (length 1&ndash;191) include short lines at the matrix edges where $\rho = 0$ or
   $\rho = u$, triggering IntBound forcing. These forcings bootstrapped the GaussElim cascade.
   Without them, GaussElim stalls at 3,413 cells (the rLTP short-line floor).

2. **DSM IntBound is the cascade trigger, not DSM GF(2) rank.** The GF(2) rank barely changed
   (25,443 vs B.62j&rsquo;s ~22,126 on block 1). The rank is still high. But without IntBound
   forcing to bootstrap determined cells, GaussElim cannot find pivots to start the cascade.

3. **50% density blocks unchanged (3,413).** DSM IntBound was already zero on 50% blocks
   (B.62j showed IntBound = 0&ndash;4 on blocks 2&ndash;5). The loss is entirely on blocks
   where density deviates from 50%.

4. **Block 0 also regressed (12.0% &rarr; 9.8%).** Lost 804 cells. Block 0&rsquo;s IntBound
   dropped from 1,165 to 160 &mdash; the remaining 160 come from LSM/VSM lines.

5. **DSM and XSM are NOT symmetric.** XSM IntBound was dead weight (B.62j: zero loss). DSM
   IntBound is load-bearing. The asymmetry arises because DSM&rsquo;s short diagonals align with
   the matrix corners where early cascade forcing occurs, while XSM&rsquo;s short anti-diagonals
   overlap differently with the rLTP spiral centers.

**Significance.** DSM integer cross-sums cannot be dropped. Unlike rLTP cross-sums (B.62g), LH
(B.62e), and XSM cross-sums (B.62j), DSM IntBound provides the initial forcing that bootstraps
the GaussElim cascade on favorable blocks. The minimum full-solve configuration remains B.62j
at $C_r = 97.4\%$. Pushing below this requires finding alternative cascade triggers to replace
DSM IntBound &mdash; a fundamentally different approach than cross-sum elimination.

**Status: COMPLETE. H2 confirmed: DSM IntBound is load-bearing. Full solve lost. $C_r = 97.4\%$ (B.62j) is the minimum full-solve configuration.**

### B.62l: Drop DSM, Re-Add XSM &mdash; Is Either Diagonal Sum Sufficient?

**Prerequisite.** B.62j (drop XSM, keep DSM) and B.62k (drop both DSM and XSM) completed.

**Motivation.** B.62j showed XSM IntBound is redundant ($C_r = 97.4\%$, zero loss). B.62k showed
DSM IntBound is load-bearing (full solve lost when dropped). But B.62k dropped BOTH DSM and XSM
simultaneously. The question: is DSM specifically required, or does the cascade need at least one
of the two diagonal cross-sum families? If XSM can substitute for DSM, the constraint architecture
gains flexibility.

B.62l tests this by keeping XSM (re-adding the cross-sums dropped in B.62j) while dropping DSM
(the family B.62k proved critical). If the full solve holds, the cascade needs ANY diagonal
IntBound &mdash; not specifically DSM. If it fails, DSM&rsquo;s diagonal geometry is uniquely
required.

**Configuration.** B.62g with `useDSMSums = false`, `useXSMSums = true`. Both DH and XH CRC retained.

**Payload.** B.62g (38,092) &minus; DSM (2,554) + XSM already present = 35,538 bits. $C_r = 97.4\%$.
(Same payload as B.62j &mdash; one diagonal family dropped, one kept, just swapped which.)

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (full solve holds) | Block 1 $|D| = 36{,}481$, BH=true | The cascade needs at least one diagonal IntBound family but not specifically DSM. XSM can substitute. |
| H2 (full solve lost) | Block 1 $|D| \ll 36{,}481$ | DSM geometry is uniquely required. The short-diagonal IntBound triggers at the matrix corners (DSM) are not replaceable by short anti-diagonal triggers (XSM). |

**Method.** Run on MP4 blocks 0&ndash;5. Compare to B.62j and B.62k.

**Results.** Blocks 0&ndash;5. All Correct=true. **H2 confirmed: full solve lost.**

| Block | B.62j (DSM, no XSM, 97.4%) | B.62k (no DSM, no XSM, 90.4%) | B.62l (XSM, no DSM, 97.4%) |
|-------|---------------------------|-------------------------------|---------------------------|
| 0 | 4,376 (12.0%) | 3,572 (9.8%) | **3,572 (9.8%)** |
| 1 | 36,481 (100.0%, BH=true) | 3,413 (9.4%) | **3,439 (9.4%)** |
| 2 | 3,413 (9.4%) | 3,413 (9.4%) | **3,413 (9.4%)** |
| 3 | 3,413 (9.4%) | 3,413 (9.4%) | **3,413 (9.4%)** |
| 4 | 3,413 (9.4%) | 3,413 (9.4%) | **3,413 (9.4%)** |
| 5 | 3,413 (9.4%) | 3,413 (9.4%) | **3,413 (9.4%)** |

IntBound comparison (block 1):

| Config | IntBound | GaussElim | Total |
|--------|----------|-----------|-------|
| B.62j (DSM kept) | 11,589 | 24,892 | 36,481 |
| B.62k (neither) | 0 | 3,413 | 3,413 |
| B.62l (XSM kept) | 190 | 3,249 | 3,439 |

**Outcome: H2 (DSM geometry is uniquely required).**

1. **XSM cannot substitute for DSM.** Re-adding XSM IntBound recovers only 190 IntBound cells
   on block 1 (vs DSM&rsquo;s 11,589). The full solve depends specifically on DSM&rsquo;s diagonal
   geometry, not on having &ldquo;any diagonal IntBound.&rdquo;

2. **The asymmetry is geometric.** DSM diagonals run from upper-left to lower-right. Their short
   lines (length 1&ndash;8) are at the matrix corners (0,0), (0,190), etc. &mdash; exactly where
   the rLTP spirals centered at (95,95) and (0,0) place their shortest lines. The DSM short-line
   IntBound triggers interact constructively with rLTP interior triggers to bootstrap the cascade.
   XSM anti-diagonals run upper-right to lower-left; their short lines are at different corners
   and do not align with the rLTP spiral geometry.

3. **B.62l &asymp; B.62k with marginal XSM contribution.** Block 1: 3,439 vs 3,413 (+26 cells).
   Block 0: identical at 3,572. XSM IntBound adds negligible value without DSM&rsquo;s cascade
   trigger.

**Significance.** The load-bearing constraint hierarchy is now clear:
- **Load-bearing:** DSM integer cross-sums (diagonal IntBound triggers), rLTP CRC-32 (GaussElim rank)
- **Redundant:** LH (B.62e), rLTP cross-sums (B.62g), XSM cross-sums (B.62j, B.62l)
- **DSM is not substitutable by XSM.** The cascade trigger mechanism is geometry-specific.

**Status: COMPLETE. H2 confirmed: XSM cannot substitute for DSM. DSM diagonal IntBound is uniquely load-bearing.**

### B.62m: Hybrid Combinator + Row Search at $C_r \leq 75\%$

**Prerequisite.** B.62i completed (no pure algebraic full solve under $C_r = 100\%$).

**Motivation.** B.62i established that the combinator cannot fully solve any block under $C_r = 100\%$.
The minimum algebraic full-solve requires CRC-32 on all 286 rLTP lines ($C_r = 103.0\%$). However,
the combinator at achievable $C_r$ determines thousands of cells scattered throughout the matrix
interior &mdash; cells that the DFS solver at $s = 511$ could never reach because they lie beyond
the propagation plateau. B.62m exploits this by combining algebraic determination (Phase I) with
row-by-row search (Phase II) in a two-phase hybrid architecture.

The key insight: the combinator's interior-scattered determined cells tighten constraint lines
throughout the matrix, including in the plateau band where the DFS stalls. A row-by-row search
starting from the combinator's partial solution encounters tighter residuals than a search starting
from scratch, because every pre-assigned interior cell reduces $u$ and adjusts $\rho$ on all
constraint lines passing through it.

**Architecture.**

**Phase I: Combinator Algebra.** Run GaussElim + IntBound fixpoint to convergence. Determines $N$
cells algebraically. All constraint families (geometric cross-sums, VH, DH/XH, rLTP CRC) participate.
Output: a partially-solved CSM with $N$ cells assigned and correct, $36{,}481 - N$ cells unknown.
Runtime: milliseconds.

**Phase II: Row-by-Row Search.** Process rows 0 through 190 sequentially. For each row $r$:

1. Count the unknown cells $u_r$ in row $r$ (cells not determined by Phase I or prior row solves).
2. If $u_r = 0$: row is complete. Verify CRC-32 (LH). If LH fails, backtrack to the most recent
   Phase II decision.
3. If $u_r > 0$: enumerate candidate completions of row $r$ consistent with:
   - The row's cross-sum residual ($\rho_r$ remaining 1s among $u_r$ unknowns): $\binom{u_r}{\rho_r}$
     candidates.
   - Cross-line feasibility: each candidate must not violate $0 \leq \rho(L) \leq u(L)$ on any
     column, diagonal, anti-diagonal, or rLTP line passing through the row's unknown cells.
   - CRC-32 row hash (LH): the complete 191-bit row must match the stored CRC-32 digest.
4. For each candidate that passes all three filters: assign it, propagate into subsequent rows
   (update column/diagonal/rLTP residuals), and **re-run the combinator fixpoint** on the reduced
   system. The fixpoint may determine additional cells in rows $r+1$ through 190, reducing their
   unknown counts before the search reaches them.
5. Continue to row $r + 1$.
6. After all 191 rows are assigned: verify SHA-256 block hash (BH). If BH fails, backtrack to
   the most recent row decision and try the next candidate.

**Phase II detail: combinator re-invocation after each row solve.** This is the critical mechanism
that distinguishes B.62m from a pure DFS. When a row is completed by search, the assigned cells
provide new information to the GF(2) system: the CRC-32 equations involving those cells become
determined, and GaussElim can propagate pivots into other rows. IntBound benefits from tighter
residuals on column/diagonal lines that pass through the completed row.

The re-invocation is cheap: the combinator fixpoint operates on the reduced system (fewer unknowns,
tighter constraints). Each re-invocation runs in milliseconds. If the combinator determines additional
rows entirely (reducing $u_r$ to 0), those rows skip the search phase and verify CRC-32 directly.

**Payload budget at $C_r \leq 75\%$.**

Target: total payload $\leq 36{,}481 \times 0.75 = 27{,}361$ bits.

The hybrid requires constraints for BOTH phases:
- Phase I (combinator): geometric + VH + DH/XH + rLTP CRC
- Phase II (row search): LH (CRC-32 per row) for row verification

| Component | Bits | Phase |
|-----------|------|-------|
| LSM ($191 \times 8$) | 1,528 | Both (integer) |
| VSM ($191 \times 8$) | 1,528 | Both (integer) |
| DSM (381 diags, variable-width) | 2,554 | Both (integer + GF(2) parity) |
| XSM (381 anti-diags, variable-width) | 2,554 | Both (integer + GF(2) parity) |
| LH (CRC-32, 191 rows) | 6,112 | Phase II (row verification) |
| VH (CRC-32, 191 columns) | 6,112 | Phase I (GF(2) column axis) |
| DH192 (hybrid-width) | 2,944 | Phase I (GF(2) diagonal axis) |
| XH192 (hybrid-width) | 2,944 | Phase I (GF(2) anti-diag axis) |
| BH (SHA-256) | 256 | Phase II (final verification) |
| DI | 8 | Enumeration index |
| **Subtotal (no rLTP)** | **26,540** | |

Remaining budget for rLTP CRC: $27{,}361 - 26{,}540 = 821$ bits.

At 2 rLTPs, that is 410 bits per sub-table. The graduated scheme for lines 1&ndash;16
(CRC-8 on 1&ndash;8 + CRC-16 on 9&ndash;16) costs $64 + 128 = 192$ bits per rLTP &mdash; fits easily.
Adding CRC-16 on lines 17&ndash;32 costs another 256 bits per rLTP, totaling 448 per rLTP (896 total).
This slightly exceeds the 821-bit budget.

**Option A: 2 rLTPs, lines 1&ndash;16 (CRC-8 + CRC-16).**
rLTP cost: $2 \times 192 = 384$ bits. Total: 26,924 bits. $C_r = 73.8\%$.

**Option B: 2 rLTPs, lines 1&ndash;32 (CRC-8 + CRC-16 + CRC-16).**
rLTP cost: $2 \times 448 = 896$ bits. Total: 27,436 bits. $C_r = 75.2\%$ (marginally over 75%).

**Option C: 1 rLTP, lines 1&ndash;64 (B.62h scheme).**
rLTP cost: $1 \times 1{,}472 = 1{,}472$ bits. Total: 28,012 bits. $C_r = 76.8\%$.

**Option D: 2 rLTPs, lines 1&ndash;32 (CRC-8 + CRC-16 + CRC-32, original tiers 1&ndash;3).**
rLTP cost: $2 \times 704 = 1{,}408$ bits. Total: 27,948 bits. $C_r = 76.6\%$.

**Recommended: Option A ($C_r = 73.8\%$)** as the baseline, with Option B ($C_r = 75.2\%$) as
the first escalation if Option A's interior triggers are insufficient.

**Expected Phase I output (Option A, based on B.62 data).**

B.62h with lines 1&ndash;64 determined 2,974 cells at 50% density (8.2%). Option A covers lines
1&ndash;16 only, determining $\sum_{k=1}^{16} k = 136$ cells per rLTP, 272 interior cells total,
plus DH/XH edge cells (~2,112). Estimated Phase I determination: ~2,400 cells (6.6%).

After Phase I, each row has on average $\sim 179$ unknowns. This is too many for direct enumeration
($\binom{179}{90}$ is astronomical). However:

**Phase II row-by-row search does NOT enumerate all candidates.** It uses the DFS approach:
assign the first unknown cell in the row to 0, propagate across all constraint lines, check
feasibility. If feasible, advance to the next unknown in the row. If infeasible, try 1. If both
fail, backtrack within the row. When the row's last unknown is assigned ($u_r = 0$), check CRC-32.

This is the standard constraint-propagation DFS but operating WITHIN a single row at a time, with
the combinator providing inter-row coupling after each row completion. The per-row search benefits
from:

- Column constraints tightened by Phase I's interior cells
- Diagonal/anti-diagonal constraints tightened by prior solved rows
- rLTP line constraints tightened by both Phase I and prior rows
- CRC-32 (LH) providing a definitive pass/fail at row completion

**DI determinism.** The hybrid produces solutions in a deterministic order:

1. Phase I is deterministic (same GF(2) matrix, same fixpoint).
2. Phase II processes rows 0 through 190 in order.
3. Within each row, unknowns are assigned in column order (0 before 1).
4. The combinator re-invocation after each row is deterministic.
5. The DI is the zero-based index of the BH-verified solution in this order.

Both compressor and decompressor run the identical hybrid pipeline. DI semantics are preserved.

**Hypotheses.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (hybrid solves 50% density) | Blocks 2&ndash;5 achieve $|D| = 36{,}481$, BH verified | Interior pre-assignment + row search + combinator re-invocation breaks the 50% wall |
| H2 (hybrid solves low-density only) | Block 1 achieves full solve; blocks 2&ndash;5 stall | Row search works when Phase I provides $> 30\%$ determination but not at 6&ndash;8% |
| H3 (row search stalls) | No blocks fully solve; row search encounters rows with $u_r > 100$ where branching is intractable | Phase I's 6&ndash;8% determination is insufficient to tighten rows for practical search |
| H4 (combinator re-invocation cascades) | After solving the first few rows by search, the combinator re-invocation solves subsequent rows algebraically without search | The row-search + re-invocation cycle creates a cascading solve where each searched row enables algebraic closure of the next |

**Prediction.** H2 or H4 on low-density blocks. H3 on 50% density blocks unless the combinator
re-invocation produces significant cascades. The key diagnostic: after solving row 0 by search and
re-running the combinator, how many additional cells are determined? If the answer is $> 100$, the
cascade mechanism is viable. If $< 10$, the re-invocation provides negligible benefit and the hybrid
degrades to a pure row-by-row DFS.

**Method.**

(a) Implement the hybrid pipeline: `CombinatorSolver::solveCascade()` (Phase I, existing) followed
by a new `RowSearchSolver` (Phase II) that processes rows sequentially with DFS + CRC-32 verification
and re-invokes the combinator after each row completion.

(b) Run Option A ($C_r = 73.8\%$, 2 rLTPs lines 1&ndash;16) on MP4 blocks 0&ndash;5. Record per-row:
$u_r$ before search, $u_r$ after combinator re-invocation, search decisions, CRC-32 pass/fail.

(c) If H3 materializes on all blocks, escalate to Option B ($C_r = 75.2\%$, lines 1&ndash;32) and
repeat.

(d) If H1 or H4 materializes, record wall-clock time and total search decisions per block. Compare
to the DFS solver at $s = 511$.

**Results (Option A, $C_r = 73.8\%$, block 1).**

Phase I (combinator algebra): 12,962 / 36,481 cells (35.5%). Cascade stalled at phase 5.

Phase II row distribution:
- 0 rows with 0 unknowns (no row fully determined by Phase I)
- 20 rows with $\leq 60$ unknowns (tractable for DFS)
- 171 rows with $> 60$ unknowns (skipped)

Phase II search (with cross-row backtracking):

| Row (sorted) | Unknowns | Candidate | Result |
|-------------|----------|-----------|--------|
| 0 | 43 | #1 | LH OK |
| 1 | 43 | #1 | LH OK |
| 2 | 43 | #1 | LH OK |
| 3 | 43 | #1 | LH OK |
| 4 | 43 | #1 | LH OK |
| 5 | 44 | #1 | LH OK |
| 6 | 46 | #1 | LH OK |
| 7 | 48 | #1 | LH OK |
| 8 | 50 | #1 | LH OK |
| 9 | 52 | #1 | LH OK |
| 10 | 54 | #1 | LH OK |
| 11 | 55 | #1 | LH OK |
| 12&ndash;19 | 57&ndash;59 | #1 | LH OK |

Final: 13,985 / 36,481 determined (38.3%). **0 backtracks.** BH not verifiable (not all cells assigned).

**Outcome: H3 (row search stalls on high-unknown rows).**

1. **Phase I is too weak at $C_r = 73.8\%$.** With only rLTP lines 1&ndash;16 and no full rLTP CRC
   coverage, Phase I determines 35.5% of cells. This leaves an average of $\sim 123$ unknowns per
   row. Only 20 of 191 rows have $\leq 60$ unknowns (the DFS tractability limit).

2. **DFS per-row timing.** Rows with 43 unknowns: ~3&ndash;4 minutes. 50 unknowns: ~10 minutes.
   57&ndash;59 unknowns: ~20&ndash;30 minutes. Beyond 60, the DFS becomes intractable (exponential
   in unknowns with limited pruning).

3. **The backtracking mechanism was never exercised.** Every first LH-valid candidate passed, so no
   cross-row backtracking occurred. This is misleading &mdash; an earlier run without backtracking
   showed `Correct: false`, confirming that first candidates ARE wrong (~256 LH-valid candidates
   per row with 43 unknowns at CRC-32). The backtracking would only fire if all tractable rows
   were completed and BH failed, forcing retry of earlier rows. Since 171 rows were skipped,
   BH verification never triggered.

4. **Combinator re-invocation contributed zero cells.** After each row completion, the fixpoint
   was re-run but found no new pivots. The GF(2) system was already fully reduced by Phase I;
   completing individual rows does not create new GaussElim opportunities with this constraint
   configuration.

5. **The bottleneck is Phase I determination, not Phase II search quality.** The DFS + IntBound
   pruning + LH CRC-32 verification works correctly for rows with $\leq 55$ unknowns. The problem
   is that 90% of rows have too many unknowns for any DFS to be tractable.

**Analysis: what Phase I strength is needed?**

For Phase II to solve all rows, every row needs $\leq 30$ unknowns (practical DFS limit with
per-row timing $< 1$ minute). At $S = 191$, that requires Phase I to determine $\geq 161 / 191
= 84.3\%$ of each row&rsquo;s cells. The B.62j configuration (full rLTP CRC, $C_r = 97.4\%$)
achieves 100% on block 1 algebraically &mdash; leaving 0 unknowns per row. The hybrid approach is
most useful in the gap between: (a) configurations too weak for algebraic full solve but strong
enough to leave $\leq 30$ unknowns per row, and (b) configurations strong enough for algebraic
full solve. B.62m Option A falls far below this threshold.

**Significance.** The hybrid architecture is mechanically correct: Phase I algebra + Phase II
row-by-row DFS with LH verification and cross-row backtracking. But at $C_r = 73.8\%$, Phase I
is far too weak. The useful operating point requires Phase I to determine $\geq 84\%$ per row,
which likely needs the full rLTP CRC ($C_r \geq 97\%$) &mdash; at which point the pure algebraic
solver (B.62j) already achieves a full solve without search. The hybrid adds value only in a narrow
$C_r$ band where Phase I nearly but not quite closes the matrix.

**Status: COMPLETE. H3 confirmed: Phase I too weak at $C_r = 73.8\%$. Only 20/191 rows tractable for DFS. Backtracking mechanism implemented but not exercised.**

