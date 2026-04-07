### B.20 LTP Substitution Experiment: Geometry versus Position (Implemented)

B.9 proposes adding one or more non-linear lookup-table partition (LTP) pairs *alongside* the existing 8 partitions,
increasing the per-cell constraint count from 8 to 10 at a storage cost of 9,198 bits per pair (reducing $C_r$ from
51.8% to 48.2%). This section proposes a different experiment: *substituting* the four toroidal-slope partitions
(HSM1/SFC1/HSM2/SFC2) with four independently optimized LTP pairs, holding the total partition count at 8 and the
storage budget at 125,988 bits. The experiment is designed to isolate a fundamental question about the plateau
bottleneck: is the solver's stalling at depth ~87K caused by the *geometry* of the constraint lines (linear
algebraic structure, uniform crossing density, shared null space), or by their *structural position* in the
propagation process (any length-511 uniform line, regardless of shape, cannot generate forcing events when
$\rho/u \approx 0.5$)?

#### B.20.1 Motivation: The Marginal Disappointment of Toroidal Slopes

The four toroidal-slope partitions (B.2) were added to increase constraint density from 4 lines per cell (geometric
partitions only) to 8. The expectation was that doubling the per-cell constraint count would substantially increase
the frequency of forcing events in the plateau band, where the geometric partitions alone are inert. In practice,
the slopes' contribution has been relatively poor compared to the original four cross-sum families. The geometric
partitions provide three qualitatively distinct pruning mechanisms: rows interact directly with SHA-1 hash
verification, columns provide orthogonal information transfer, and DSM/XSM generate short lines (length 1 to 511)
whose variable geometry triggers early forcing at the matrix periphery. The toroidal slopes provide none of these&mdash;
they are uniformly length-511 lines with no hash interaction and no variable-length early-forcing behavior.

This observation raises the possibility that the slopes' underperformance is not a property of their *algebraic
structure* (modular arithmetic, linear null space) but of their *position* in the solver's constraint hierarchy: any
uniform-length-511 line that lacks hash verification will underperform, because at the plateau entry point (row
~170), all such lines have $u \approx 341$ unknowns and residual $\rho \approx 170$, placing them deep in the
forcing dead zone ($\rho \gg 0$ and $\rho \ll u$). Adding more lines of the same length&mdash;whether algebraic or
pseudorandom&mdash;cannot change this arithmetic.

If this *positional hypothesis* is correct, replacing the slopes with optimized LTP pairs will produce similarly
poor marginal performance, despite the LTP's non-linear structure and plateau-targeted optimization. Conversely, if
the LTP pairs significantly outperform the slopes, the *geometric hypothesis* is supported: the non-linear
structure and early-tightening line composition (B.9.5) provide a qualitatively different kind of constraint that
uniform algebraic lines cannot replicate.

#### B.20.2 Experiment Design

The experiment consists of four solver configurations, benchmarked on a common test suite of $N \geq 50$ random
$511 \times 511$ binary matrices:

**Configuration A (baseline).** The pre-B.20 8-partition solver: 4 geometric (LSM, VSM, DSM, XSM) + 4 toroidal
slopes (HSM1, SFC1, HSM2, SFC2). This was the production configuration at the time of this experiment.

**Configuration B (geometric only).** The 4 geometric partitions alone, with toroidal slopes removed. This
establishes the baseline performance of the geometric partitions without any supplementary lines, quantifying the
slopes' actual marginal contribution.

**Configuration C (slopes replaced by LTP).** 4 geometric + 4 independently optimized LTP pairs, replacing the
toroidal slopes entirely. Each LTP pair is optimized per B.9.1-B.9.2 (Fisher-Yates baseline + hill-climbing
against simulated DFS trajectories). The four tables are optimized sequentially: the 1st LTP is optimized with
geometric partitions only, the 2nd with geometric + 1st LTP, and so on, so each table complements the existing
constraint set.

**Configuration D (slopes + LTP).** All 8 existing partitions + 4 LTP pairs (12 total). This is the additive
configuration, increasing per-cell constraints from 8 to 12 at a storage cost of $4 \times 9{,}198 = 36{,}792$
additional bits per block ($C_r$ drops from 51.8% to 40.5%). This configuration is included as an upper bound: if
12 partitions substantially outperform 8, the question becomes whether the improvement is worth the compression
penalty.

#### B.20.3 Metrics

For each configuration and each test matrix, record:

(a) **Total backtrack count** in the plateau band (rows 100-300). This is the primary metric: the plateau is where
the solver spends 90% of its time, and backtrack reduction is the clearest signal of constraint effectiveness.

(b) **Maximum sustained depth** before the first major regression ($d_{\text{bt}} > 1{,}000$ cells). This measures
how far the solver penetrates into the plateau before stalling. Deeper penetration indicates stronger constraint
propagation in the early plateau.

(c) **Forcing event rate** in the plateau band: the fraction of cell assignments that are forced by propagation
(rather than selected by branching). This directly measures constraint tightening. The geometric partitions produce
a high forcing rate in the easy regime (rows 0-100) and a near-zero rate in the plateau. The question is whether
slopes or LTP pairs increase the plateau forcing rate.

(d) **Wall-clock solve time** per block. The ultimate metric, but confounded by per-assignment overhead (LTP lookup
vs. slope formula) and machine-specific factors. Backtrack count is more portable.

(e) **Per-line residual distribution** at plateau entry (row 170). For each of the 8 (or 12) partition families,
record the distribution of $\rho/u$ across all lines at the moment the solver enters the plateau. If all families
cluster around $\rho/u \approx 0.5$, the positional hypothesis is supported: the dead zone is universal. If the LTP
early-tightening lines show a bimodal distribution (some lines with $\rho/u$ near 0 or 1, others near 0.5), the
geometric hypothesis gains support.

#### B.20.4 Predicted Outcomes and Interpretation

*Outcome 1: C $\approx$ A (LTP matches slopes).* Both achieve similar backtrack counts, forcing rates, and
residual distributions. **Interpretation:** the positional hypothesis is confirmed. Uniform-length-511 lines cannot
break the plateau regardless of their cell-to-line mapping. The productive research direction is proposals that
change *when* constraints activate (B.16 partial tightening, B.17 row-completion look-ahead) or that introduce
*variable-length* non-linear lines. The B.9 optimization infrastructure is not justified for uniform-length tables.

*Outcome 2: C $\gg$ A (LTP substantially outperforms slopes).* LTP pairs reduce plateau backtracks by $\geq 2
\times$ relative to slopes. **Interpretation:** the geometric hypothesis is confirmed. Non-linear structure and/or
early-tightening line composition provide qualitatively superior constraint information. The recommendation is to
replace all four toroidal slopes with optimized LTP pairs in production, at zero storage cost and minimal
implementation complexity ($4 \times 510$ KB static tables, each resolved by a single array lookup). Further
investment in the B.9 optimization infrastructure is justified.

*Outcome 3: B $\approx$ A (slopes provide no marginal benefit).* Removing the slopes entirely produces similar
performance to the 8-partition baseline. **Interpretation:** the four geometric partitions carry essentially all
the solver's constraint power. The slopes were never contributing meaningfully, and any supplementary partition&mdash;
linear or non-linear&mdash;faces the same positional barrier. This is the strongest evidence for the positional
hypothesis and redirects the research program entirely toward non-partition approaches (B.1 CDCL, B.11 restarts,
B.16-B.19).

*Outcome 4: C $\gg$ A and D $\gg$ C (LTP outperforms slopes, and more LTP is better).* Both the substitution and
the additive configuration show progressive improvement. **Interpretation:** non-linear constraint density scales
favorably. Each additional LTP pair contributes independent plateau-breaking power. The recommendation is to add
LTP pairs until the compression-ratio penalty becomes unacceptable, using $C_r$ as the binding constraint.

*Outcome 5: C $\approx$ A but D $\gg$ A (substitution equivalent, but addition helps).* Replacing slopes with LTP
doesn't help, but adding LTP on top of slopes does. **Interpretation:** the slopes and LTP pairs provide
complementary information (the slopes contribute algebraic regularity, the LTP contributes non-linear structure),
but neither alone is sufficient to break the plateau. Only the combination crosses a critical constraint-density
threshold. This favors the 12-partition configuration (D) despite its compression penalty.

#### B.20.5 LTP Table Construction for the Experiment

Each of the four LTP tables is constructed per B.9.1 (Fisher-Yates baseline from a fixed seed) and optimized per
B.9.2 (hill-climbing against simulated DFS). The four seeds are deterministic and distinct:

$$\text{seed}_i = \text{SHA-256}(\texttt{"CRSCE-LTP-v1-table-"} \| i) \quad \text{for } i \in \{0, 1, 2, 3\}$$

Sequential optimization order matters. Table 0 is optimized with the geometric partitions only (Configuration B as
the base solver). Table 1 is optimized with geometric + Table 0. Table 2 with geometric + Tables 0-1. Table 3
with geometric + Tables 0-2. This greedy-sequential approach ensures each table complements the existing constraint
set rather than duplicating it. The alternative&mdash;joint optimization of all four tables simultaneously&mdash;is a
$4 \times 261{,}121$-dimensional search problem that is computationally infeasible.

The optimization test suite consists of $N = 100$ random matrices, split 50/50 for optimization and validation.
Tables are optimized against the first 50 matrices and evaluated on the second 50. If the validation backtrack
count is within 10% of the optimization backtrack count, the table generalizes; if it degrades by $> 25\%$, the
table is overfit and the optimization must be rerun with a larger or more diverse suite.

To manage the computational cost of the optimization loop, each candidate swap is evaluated using a *truncated DFS*:
the solver runs for the first $100{,}000$ nodes per matrix (covering the plateau entry), and the plateau-band
backtrack count in this window serves as the proxy objective. This reduces the per-evaluation cost from minutes to
seconds, enabling $\sim 100{,}000$ swap evaluations per table within a week of Apple Silicon compute time.

#### B.20.6 Implementation Path

The implementation is staged to minimize risk:

*Stage 1: Instrument the current solver.* Add per-line residual logging at row 170 (plateau entry) and per-family
forcing-event counters in the plateau band. Run Configuration A on the test suite to establish baseline metrics.
This requires no architectural changes&mdash;only observability instrumentation via the existing `IO11y` interface.

*Stage 2: Implement Configuration B.* Disable the four toroidal-slope partitions (set their line count to 0 in
`ConstraintStore`, skip their updates in `PropagationEngine`). Run the test suite. If Outcome 3 materializes
(slopes provide no marginal benefit), the experiment is already informative and Stage 3 can be prioritized or
deprioritized accordingly.

*Stage 3: Construct and integrate LTP tables.* Implement the `LookupTablePartition` class (a `CrossSumVector`
subtype backed by a `constexpr std::array<uint16_t, 261121>` literal). Run the B.9.1 optimization loop to produce
four tables. Integrate into `ConstraintStore` as drop-in replacements for the four `ToroidalSlopeSum` instances.
Run Configurations C and D.

*Stage 4: Analyze and decide.* Compare metrics across A/B/C/D per B.20.4. If Outcome 2 or 4, proceed to
production integration of LTP pairs. If Outcome 1 or 3, redirect research toward B.16-B.19.

#### B.20.7 Implications for the File Format

If LTP substitution proceeds to production (Outcome 2 or 4), the compressed payload format (Section 12) changes
in field semantics but not in field sizes. The HSM1/SFC1/HSM2/SFC2 fields in the block payload are reinterpreted
as LTP0/LTP1/LTP2/LTP3 cross-sums. Each field retains its encoding: $s = 511$ values at $b = 9$ bits each,
MSB-first packed bitstream, totaling 4,599 bits per field. The header `version` field should be incremented to
distinguish payloads that use LTP cross-sums from those that use toroidal-slope cross-sums, since the decompressor
must know which cell-to-line mapping to apply.

If additive LTP is adopted (Outcome 4, Configuration D), the block payload grows by $4 \times 4{,}599 = 18{,}396$
bits (4 additional LTP fields), and the header version must reflect the new field count. The format remains
backward-compatible in the sense that a version-2 decompressor can detect the version field and reject version-1
payloads (or vice versa) rather than silently misinterpreting the cross-sum fields.

#### B.20.8 Open Questions

(a) Is sequential table optimization (B.20.5) sufficient, or does the greedy ordering leave significant performance
on the table? If Table 3 is optimized against a solver that already has Tables 0-2, it may converge to a table
that merely patches the remaining gaps in the first three tables' coverage, rather than providing independent
constraint power. A partial remedy is to re-optimize Table 0 after Tables 1-3 exist (iterating the sequential
process), but convergence of this outer loop is not guaranteed.

(b) How sensitive is the experiment to the choice of test suite? Random matrices may not be representative of
real-world inputs (which may have structure&mdash;e.g., natural-language text produces non-uniform bit distributions).
The test suite should include structured inputs (all-zeros, all-ones, alternating patterns, natural data) alongside
random matrices.

(c) Can the truncated-DFS proxy objective (B.20.5) mislead the optimizer? A table that minimizes backtracks in the
first $100{,}000$ nodes may not minimize backtracks in the full solve if the plateau's difficulty profile changes
beyond the truncation window. The truncation point should be validated by comparing proxy rankings with full-solve
rankings on a small subset of matrices.

(d) If Outcome 1 materializes (positional hypothesis confirmed), is there a *variable-length* LTP design that
escapes the dead zone? B.9 specifies uniform-length lines ($s$ cells each), but a generalized LTP could assign
lines of varying length&mdash;some with 100 cells (reaching forcing thresholds early) and others with 900 cells
(providing long-range information). Variable-length LTP lines would encode with variable-width cross-sums
($\lceil \log_2(\text{len}(k) + 1) \rceil$ bits each, as DSM/XSM do), complicating the storage cost analysis but
potentially breaking the positional barrier that uniform lines cannot.

(e) Should the experiment include a Configuration E that replaces the toroidal slopes with a 5th-8th slope pair
(different slope parameters, still algebraic)? This would test whether the slopes' underperformance is specific to
the chosen parameters $\{2, 255, 256, 509\}$ or inherent to the algebraic family. If alternative slope parameters
perform equally poorly, the algebraic family is exhausted and only non-linear alternatives remain.

#### B.20.9 Observed Results (Implemented)

B.20 Configuration C was implemented: 4 geometric partitions + 4 uniform-length LTP partitions (seeds
`"CRSCLTP1"`-`"CRSCLTP4"`, Fisher-Yates baseline, no hill-climbing optimization). The toroidal slopes
(HSM1/SFC1/HSM2/SFC2) were removed. The total line count decreased from 6,130 (B.9) to 5,108, and the block
payload reverted to 15,749 bytes (identical to the pre-B.9 format). The implementation used `constexpr` lookup
tables built at compile time from the fixed seeds.

**Telemetry (useless-machine.mp4, block 0, ~14M iterations):**

| Metric                  | B.8/B.9 Baseline | B.20 (Config C) | Change              |
|-------------------------|------------------|-----------------|---------------------|
| Sustained plateau depth | ~87,500          | ~88,503         | +1,003 (+1.1%)      |
| Hash mismatch rate      | 37.0%            | 25.2%           | $-11.8$ pp          |
| Avg iterations/sec      | ~200K            | ~198K           | $\approx 0$%        |
| Block payload bytes     | 16,899           | 15,749          | $-1,150$ ($-6.8\%$) |

The plateau depth improved by $+1{,}003$ cells ($+1.1\%$), a modest but consistent gain. The hash mismatch rate
dropped from $37\%$ to $25.2\%$&mdash;a reduction of 11.8 percentage points&mdash;indicating that the LTP partitions
generate substantially more propagation in the plateau band than the toroidal slopes did. The iteration rate was
unchanged ($\approx 198{,}000$/sec), confirming that the LTP table lookup imposes no measurable per-iteration
overhead relative to the slope modular-arithmetic formula.

**Outcome classification:** The result falls between Outcome 1 and Outcome 2 from B.20.4. The depth improvement
($+1.1\%$) is far below the $2\times$ backtrack reduction threshold of Outcome 2, but the $-12$ pp reduction in
hash mismatch rate is material. The positional hypothesis is partially supported: uniform-length-511 lines (both
slopes and LTP) cannot break the plateau, but LTP's non-linear structure provides a qualitatively better
constraint signal within the plateau&mdash;evidenced by the mismatch rate falling&mdash;without changing the depth
ceiling.

**Interpretation:** The toroidal slopes were providing weak constraint power in the plateau band. LTP partitions
provide modestly stronger constraint power (fewer dead-end explorations per iteration), but not enough to push
through the fundamental depth barrier at $\approx 88{,}500$. This suggests both families share the same positional
weakness: at plateau entry (row $\approx 170$), every uniform-length-511 line has $u \approx 341$ and
$\rho \approx 170$, placing it in the forcing dead zone. The LTP lines' non-linear structure reduces the fraction
of explorations that terminate in hash failure&mdash;perhaps by providing slightly tighter cross-line bounds&mdash;but
cannot generate the forcing events necessary to push the solver past row $\approx 173$.

**Recommendation:** Proceed to B.21 (joint-tiled variable-length LTP partitions). The B.20 result rules out
uniform-length supplementary lines as a plateau-breaking mechanism; variable-length lines with the triangular
length distribution of DSM/XSM may escape the dead zone by allowing short lines ($\ll 511$ cells) to reach
forcing thresholds earlier in the plateau.

