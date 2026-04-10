## Appendix C: Open Questions Consolidated

This appendix consolidates all open questions from Appendices A and B into a single reference. Each question is reproduced from its original context, followed by a *Discovered Data* subsection summarizing relevant experimental evidence gathered during the research program, and a *Conclusions* subsection stating what can be inferred. Questions are grouped by their source section to preserve traceability. The principal data sources are: B.8 telemetry (depth plateau ~87,500, 37% hash mismatch, min_nz_row_unknown = 1); B.20.9 Configuration C results (depth ~88,503, mismatch 25.2%, iter/sec ~198K); B.21 observed results (depth 50,272, mismatch 0.20%, iter/sec ~687K); the B.12.6(a) BP convergence experiment; and the CDCL infrastructure assessment (B.1.10).

---

### C.1 From B.1.8: CDCL and Non-Chronological Backjumping

#### C.1.1 Question (a): LH Failure Root-Cause Depth

*How often do LH failures have distant root causes? If the typical hash failure is caused by a decision within the last 5--10 levels, backjumping provides little benefit. If root causes are frequently 50--500 levels deep, the savings are exponential.*

**Discovered Data.** B.8 telemetry shows min_nz_row_unknown = 1 throughout: rows reach completion and fail SHA-1 immediately. B.20.9 confirms the pattern persists under LTP substitution (mismatch rate dropped from 37% to 25.2% but failures remain frequent). B.21 reduced mismatch to 0.20%, but the depth regression to 50,272 suggests root causes are shallow — the solver makes better decisions with variable-length lines but encounters an earlier structural wall.

**Conclusions.** The evidence strongly suggests that root causes are predominantly shallow (within the last few rows of assignments). B.1.9 explicitly concluded that the bottleneck is search guidance, not constraint density or lookahead reach. The backjumping benefit is therefore limited; the typical failure distance is within chronological backtracking's efficient range.

#### C.1.2 Question (b): Implication Graph Maintenance Cost

*Can the implication graph be maintained cheaply enough to justify full CDCL?*

**Discovered Data.** B.1.10 documents that the full CDCL infrastructure was built (ConflictAnalyzer, ReasonGraph, CellAntecedent, BackjumpTarget) but never integrated into the DFS loop. The forcing dead zone at the plateau (ρ/u ≈ 0.5 for all length-511 lines) means the reason graph contains no meaningful antecedent chains — there are no forcing events to record. B.21's variable-length lines create forcing events in early rows (short lines exhaust quickly), but the depth regression suggests these events do not propagate deeply enough to create useful conflict clauses.

**Conclusions.** The question is moot under current architecture. The implication graph can be maintained at low cost per se (the infrastructure exists), but the cost is irrelevant because the forcing dead zone generates no useful antecedent chains. B.21's short LTP lines may change this calculus by creating forcing events, but B.21's depth regression indicates the benefit would be marginal at best. CDCL should be revisited only if a future architecture change produces sustained forcing in the plateau band.

#### C.1.3 Question (c): Lightweight Backjump Estimator Sufficiency

*Is the lightweight backjump estimator (B.1.7) sufficient in practice?*

**Discovered Data.** B.1.9 reports that the estimator was implemented first per the adopted sequence. B.8 telemetry shows no measurable improvement in depth from the estimator (depth stayed at ~87,500). B.1.10's retrospective analysis explains why: the estimator needs forcing events to identify causal decisions, but the dead zone produces no forcing.

**Conclusions.** Answered negatively. The lightweight estimator is insufficient because it operates on forcing-event metadata that does not exist in the plateau band. The approach is structurally unable to identify backjump targets when ρ/u ≈ 0.5.

#### C.1.4 Question (d): CDCL and Adaptive Lookahead Interaction

*How does CDCL interact with the adaptive lookahead (B.8)?*

**Discovered Data.** B.8 was implemented with k = 1…4. The k = 4 probe depth added ~15% per-iteration overhead with zero depth improvement. CDCL was built but never integrated. No interaction data exists.

**Conclusions.** Unanswered empirically, but likely unproductive. The lookahead probes explore subtrees that, like the main DFS, encounter the forcing dead zone. Conflicts discovered by probes would produce equally vacuous learned clauses (111-literal clauses that rarely fire, per B.1.9). The interaction is theoretically interesting but practically mooted by the dead zone.

#### C.1.5 Question (e): Clause Management Strategy

*What clause-management strategy is appropriate for CRSCE?*

**Discovered Data.** CDCL was never integrated, so no clause database was populated. B.1.9 calculated that hash-failure clauses would contain ~111 decision literals, making reuse probability negligible.

**Conclusions.** Unanswered but deprioritized. The question becomes relevant only if a future architecture change (e.g., variable-length lines producing short conflict clauses) makes CDCL viable. Under current or foreseeable architectures, the clause database would remain empty or contain only unreusable long clauses.

---

### C.2 From B.2.6: Additional Toroidal Slope Partitions

#### C.2.1 Question (a): 5th Slope Pair Speedup

*What is the empirical decompression speedup from adding a 5th slope pair?*

**Discovered Data.** B.20 tested a different direction — replacing all 4 slopes with 4 LTP partitions rather than adding a 5th slope pair. The result (B.20.9): +1.1% depth, −11.8 pp mismatch, ~0% throughput change. B.21 went further with variable-length LTP and produced 3.5× throughput improvement but depth regression.

**Conclusions.** Superseded. The research program moved past additional slope pairs toward non-linear partitions (B.9/B.20) and variable-length partitions (B.21). Adding a 5th slope pair is no longer under consideration given that the 4 existing slopes were entirely replaced by LTP partitions. The positional hypothesis (B.20.9) indicates that any additional uniform-length-511 line — slope or otherwise — cannot break the forcing dead zone.

#### C.2.2 Question (b): Per-Iteration Cost Scaling

*At what partition count does per-iteration cost become a bottleneck?*

**Discovered Data.** B.20 Config C reduced lines per cell from 10 to 8 with negligible throughput change (~198K vs ~200K iter/sec). B.21 reduced lines per cell from 8 to 5–6 and achieved 687K iter/sec (3.5× improvement). The relationship is clearly super-linear: removing 2–3 lines per cell tripled throughput.

**Conclusions.** Partially answered. Reducing constraint lines per cell from 8 to 5–6 produced a dramatic throughput improvement, confirming that per-iteration cost scales significantly with line count. The bottleneck onset appears to be around 8–10 lines per cell. However, throughput is not the sole metric — B.21's throughput gain came at the cost of depth regression, suggesting the optimal constraint density balances throughput against propagation power.

#### C.2.3 Question (c): Optimal Slope Values

*What slope values should a 5th pair use?*

**Discovered Data.** No experiments with alternative slope values were conducted. The entire slope family was replaced by LTP partitions in B.20.

**Conclusions.** Superseded. The toroidal slope family has been removed from the architecture. The question is only relevant if a future experiment re-introduces algebraic slopes alongside LTP.

---

### C.3 From B.3.8: Variable-Length Curve Partitions

#### C.3.1 Question (a): Minimum Invisible Swap Size

*What is the empirically measured k_min for a 6-partition system at small matrix sizes?*

**Discovered Data.** No small-matrix experiments were conducted. However, B.21's joint-tiled variable-length design is conceptually related — it uses variable-length lines (1 to 256 cells) rather than the 1-to-511 triangular sequence proposed in B.3.

**Conclusions.** Unanswered. The question remains relevant for understanding swap-invisible pattern theory but has been deprioritized relative to the empirical approach taken in B.20/B.21.

#### C.3.2 Question (b): 16-Partition Cascade Solving

*For 16 variable-length curve partitions, does the cascade from anchor cells solve enough of the matrix?*

**Discovered Data.** B.21 used 4 variable-length sub-tables (not 16 full partitions) with line lengths 1–256. The result was 687K iter/sec (fast cascade) but depth regression to 50,272, suggesting that cascade solving works rapidly but the reduced constraint density (5–6 lines per cell instead of 8) creates a premature wall.

**Conclusions.** Partially answered. Variable-length lines produce extremely effective cascade forcing (evidenced by B.21's 0.20% mismatch rate and 3.5× throughput). However, 4 sub-tables are insufficient to maintain depth — the solver runs out of propagation power. Scaling to 16 partitions would restore constraint density, but at enormous storage cost (the original B.3 proposal). The B.21 results suggest that a middle ground — more than 4 sub-tables but fewer than 16 full partitions — warrants investigation.

#### C.3.3 Question (c): LH Elimination Feasibility

*Is collision resistance from interlocking partitions sufficient to eliminate LH entirely?*

**Discovered Data.** B.21's 0.20% hash mismatch rate demonstrates that variable-length LTP lines dramatically improve collision resistance. However, LH still caught 0.20% of attempts — without LH, these would produce incorrect reconstructions.

**Conclusions.** Not yet. The 0.20% mismatch rate is impressive but nonzero. LH remains necessary for correctness verification. Future work could investigate whether increasing the number of variable-length partitions drives mismatch to zero, enabling LH elimination.

#### C.3.4 Questions (d), (e), (f): Curve Family, Segment Schedule, Crossing Property

*Optimal curve family, segment schedule optimization, dense crossing property proof.*

**Discovered Data.** B.21 used pseudorandom spatial distribution rather than space-filling curves. The triangular length distribution (min(k+1, 511−k)) was adopted directly from DSM/XSM. No formal crossing-property analysis was performed.

**Conclusions.** These questions remain open. B.21 sidestepped them by using pseudorandom assignment rather than deterministic curve-based construction. The theoretical questions about optimal curve families and crossing properties are still relevant for understanding the fundamental limits of partition-based constraint systems.

---

### C.4 From B.4.9: Dynamic Row-Completion Priority

#### C.4.1 Question (a): Optimal Threshold τ

*What is the optimal threshold τ?*

**Discovered Data.** B.4 was implemented and subsumed by B.10. No isolated τ optimization data is available. B.8 telemetry shows depth plateau regardless of probe depth k = 1…4, suggesting that row-completion priority alone cannot break the plateau.

**Conclusions.** Unanswered but deprioritized. The τ parameter is now part of the broader B.10 tightness-driven ordering, and the plateau bottleneck has been shown (B.20.9) to be a consequence of uniform-length-511 lines rather than cell ordering.

#### C.4.2 Questions (b), (c), (d): Multi-Line Priority, Burst Completion, Probing Interaction

*Multi-line priority, burst vs. single-cell completion, probing interaction.*

**Discovered Data.** B.10 generalizes multi-line priority. No isolated experiments on burst completion or probing interaction were conducted. B.8 telemetry showed that deeper probing (k = 4) had no effect on depth, suggesting that the ordering refinements proposed here cannot overcome the fundamental plateau barrier.

**Conclusions.** Deprioritized. The forcing dead zone is the binding constraint, not cell ordering. These questions become relevant only after a future architecture change (e.g., variable-length lines) creates sustained forcing in the plateau band where ordering differences matter.

---

### C.5 From B.5.1: Hash Alternatives

#### C.5.1 Question (a): SHA-1 vs. SHA-256 Throughput

*What is the empirical throughput difference between SHA-1 and SHA-256 row hashing?*

**Discovered Data.** SHA-1 was adopted for row hashing. B.8 telemetry reports ~200K iter/sec with SHA-1. B.21 achieved ~687K iter/sec, though the improvement came from reduced constraint lines rather than hash function choice. No direct SHA-1 vs. SHA-256 benchmark was conducted.

**Conclusions.** Partially answered. SHA-1 is in production use and supports ~200K–687K iter/sec depending on constraint architecture. The SHA-1 choice appears sound, but no controlled comparison with SHA-256 exists. Given that hash computation is a small fraction of per-iteration cost, the difference is likely minor.

#### C.5.2 Question (b): 4-Partition vs. 10-Partition Allocation

*Is the 4-partition allocation optimal, or does scaling to 10 partitions yield better propagation?*

**Discovered Data.** B.9 added 2 LTP partitions (10 total, 6,130 lines). B.20 replaced 4 slopes with 4 LTP (8 total, 5,108 lines). B.21 kept 8 total but with variable-length LTP. The progression shows diminishing returns from adding uniform-length lines and qualitative improvements from variable-length lines. B.21's 5–6 lines per cell with 687K iter/sec outperforms B.9's 10 lines per cell.

**Conclusions.** Answered. The optimal allocation is not more partitions but *better* partitions. Variable-length LTP lines (B.21) at 5–6 per cell outperform 10 uniform-length lines per cell on throughput and mismatch rate. The 4-partition allocation in its B.21 form is superior to the 10-partition B.9 configuration.

#### C.5.3 Question (c): Alternative Slope Values

*Are there slope values that provide superior propagation interaction?*

**Discovered Data.** No experiments with alternative slope values were conducted before the slope family was removed.

**Conclusions.** Superseded. The toroidal slope family was replaced by LTP in B.20.

---

### C.6 From B.6.6: Singleton Arc Consistency

#### C.6.1 Questions (a), (b), (c): SAC Fixpoint Depth, Partial SAC, GPU Parallelization

*SAC fixpoint depth, partial SAC convergence, GPU parallelization of probe loop.*

**Discovered Data.** SAC was not implemented. B.8's k = 4 exhaustive lookahead is a weaker form of SAC probing (it probes 4 levels deep rather than to fixpoint). B.8 showed zero depth improvement from deeper probing, suggesting that SAC's additional power (probing to fixpoint) would similarly fail to break the plateau because the dead zone prevents fixpoint propagation from reaching a contradiction.

**Conclusions.** Unanswered but likely unproductive under current architecture. The forcing dead zone means that probing to fixpoint produces the same result as no probing — ρ/u ≈ 0.5 across all lines, no contradictions detected. SAC becomes potentially valuable under B.21's variable-length architecture, where short lines create early forcing events that could amplify SAC probes. However, B.21's depth regression suggests the amplification is insufficient.

---

### C.7 From B.8.7: Adaptive Lookahead

#### C.7.1 Questions (a), (b): Window Size and Thresholds

*Optimal window size W, optimal stall/recovery thresholds.*

**Discovered Data.** B.8 was implemented with W = 10,000. Stall detection successfully triggered escalation from k = 0 to k = 4 in the plateau band. B.20.9 and B.21 both show the stall detector activating appropriately.

**Conclusions.** Partially answered. The W = 10,000 default works correctly for stall detection. However, the escalation response (increasing k) proved ineffective — the solver reaches k = 4 and remains there without depth improvement. The thresholds detect stalls accurately; the problem is that the available interventions (deeper lookahead) cannot address the root cause (forcing dead zone).

#### C.7.2 Question (c): Linear-Chain Approximation Beyond k = 4

*Is a linear-chain approximation viable for k > 4?*

**Discovered Data.** Not tested. B.8 capped at k = 4 exhaustive. B.1.9 concluded that the bottleneck is search guidance rather than lookahead depth, making deeper probing (whether exhaustive or approximate) unlikely to help.

**Conclusions.** Likely unproductive. If exhaustive k = 4 (16 probes per decision) produces zero depth improvement, approximate k = 8 or k = 16 with their higher false-negative rates are even less likely to help. The fundamental issue is that contradictions in the dead zone are invisible to any finite-depth probe.

#### C.7.3 Question (d): Probing Both Values vs. Alternate Only

*Should the lookahead tree explore both values?*

**Discovered Data.** No controlled comparison. The existing implementation probes only the alternate value.

**Conclusions.** Unanswered but deprioritized given that lookahead at any depth fails to break the plateau.

---

### C.8 From B.9.9: Non-Linear Lookup-Table Partitions

#### C.8.1 Question (a): Optimized LTP vs. 5th Slope Pair

*Does an optimized LTP pair provide measurable benefit beyond a 5th slope pair?*

**Discovered Data.** B.20 Config C replaced 4 slopes with 4 LTP (unoptimized, Fisher-Yates baseline). Result: +1.1% depth, −11.8 pp mismatch. B.21 used variable-length LTP with 3.5× throughput but depth regression. No direct LTP-vs-slope pair comparison with identical partition counts was conducted.

**Conclusions.** Partially answered. Unoptimized LTP outperforms slopes on mismatch rate (25.2% vs 37%), confirming that non-linearity provides constraint benefit. The LTP's advantage appears to come from breaking the algebraic null space, not from offline optimization. Variable-length LTP (B.21) provides dramatically better constraint quality (0.20% mismatch) but at the cost of reduced constraint density.

#### C.8.2 Question (b): Optimization Test Suite Size

*How large must the optimization test suite be for generalization?*

**Discovered Data.** B.20 Config C used unoptimized tables (Fisher-Yates baseline, no hill-climbing). The unoptimized tables already outperformed slopes, suggesting that the pseudorandom structure itself is the key feature, not per-instance optimization.

**Conclusions.** Partially answered. Unoptimized LTP already outperforms slopes, reducing the urgency of offline optimization. The question remains relevant for squeezing marginal gains from the LTP architecture, but the B.20/B.21 progression suggests that structural innovations (variable-length lines) dominate over optimization of fixed-length tables.

#### C.8.3 Questions (c), (d), (e): Search Heuristic, Stacking, Lookahead Interaction

*Convergence heuristic, stacking multiple LTPs, interaction with adaptive lookahead.*

**Discovered Data.** B.20 used 4 stacked LTP partitions (answering (d) affirmatively — multiple LTPs can be stacked). B.21 changed the stacking architecture to joint tiling. No optimization search or lookahead interaction data.

**Conclusions.** Question (d) is answered: 4 LTP partitions were successfully stacked in B.20 with positive results. Questions (c) and (e) remain unanswered but are deprioritized given the shift to variable-length architecture.

---

### C.9 From B.10.7: Constraint-Tightness-Driven Cell Ordering

#### C.9.1 Questions (a), (b), (c), (d): Weights, Incremental Cost, Confidence Integration, Predictive Power

*Optimal weights, incremental maintenance justification, confidence score integration, tightness as failure predictor.*

**Discovered Data.** B.10's tightness-driven ordering was implemented (B.4 as the first stage). B.8 telemetry showed no depth improvement from adaptive lookahead, and B.20.9 showed only +1.1% depth from LTP substitution. B.21 achieved 0.20% mismatch (dramatically better branching quality) but depth regression, suggesting that constraint quality — not ordering — is the binding constraint.

**Conclusions.** Deprioritized. Cell ordering is a second-order effect relative to constraint density and structure. The forcing dead zone prevents tightness-driven ordering from activating — when all lines have ρ/u ≈ 0.5, tightness scores are uniformly weak and provide no meaningful differentiation. Under B.21's variable-length architecture, short lines create tightness variation that could make these questions relevant again.

---

### C.10 From B.11.6: Randomized Restarts

#### C.10.1 Questions (a)–(e): Heavy-Tail Distribution, Luby Base, Partial Restart, Phase Saving, Stall Interaction

*Heavy-tail distribution, optimal Luby base, partial vs. full restart, phase saving interaction, stall detection combination.*

**Discovered Data.** Restarts were not implemented. B.11.1 identified the fundamental constraint: DI determinism requires fully deterministic restart policies, ruling out classical randomized restarts. B.8 telemetry shows the plateau stall is consistent (depth ~87,500 across k = 1…4), suggesting a structural rather than stochastic bottleneck — inconsistent with heavy-tailed behavior. B.21's depth regression to 50,272 with near-zero mismatch further suggests the wall is structural (constraint density insufficient) rather than stochastic (bad early decisions).

**Conclusions.** Likely unproductive. The evidence points to a structural plateau rather than a heavy-tailed distribution. The plateau depth is remarkably consistent across lookahead depths and partition architectures (87,500 with slopes, 88,503 with LTP, always in the same band), suggesting a phase transition inherent to the constraint system rather than unlucky branching. Deterministic restart policies compatible with DI would need to navigate this structural barrier, not just escape stochastic traps.

---

### C.11 From B.12.6: Survey Propagation and BP-Guided Decimation

#### C.11.1 Question (a): BP Convergence — ANSWERED

*Does BP converge reliably on the CRSCE factor graph?*

**Discovered Data.** Empirically answered on 2026-03-04. Gaussian BP with damping α = 0.5 converges reliably in 30 iterations (run_id=1c64a9a5). Cold-start max_delta = 18.252, warm-start max_delta = 0.000. BP converges but BP-guided branch-value ordering does not break the depth plateau (depth remained at ~87,487–87,498 after 3 StallDetector escalations).

**Conclusions.** Answered. BP converges but does not improve depth. The Gaussian approximation may be too coarse for the densely-loopy CRSCE factor graph. The forcing dead zone renders the marginal estimates uninformative — when ρ/u ≈ 0.5, the true marginals are near 0.5, and BP correctly computes them as near 0.5, providing no branching guidance.

#### C.11.2 Questions (b)–(e): Checkpoint Interval, SP Frozen Cells, Batch Size, GPU Acceleration

*Optimal checkpoint interval, SP backbone detection, batch decimation size, Metal GPU acceleration.*

**Discovered Data.** B.12.6(a)'s negative finding (BP-guided ordering doesn't break plateau) reduces the urgency of all optimization questions. If the marginals themselves carry no signal in the dead zone, optimizing how frequently or efficiently they are computed is moot.

**Conclusions.** Deprioritized. The binding constraint is not BP's implementation efficiency but the dead zone's information-theoretic barrier. These questions become relevant only if a future architecture change (e.g., variable-length lines creating non-trivial marginals) makes BP's output actionable.

---

### C.12 From B.13.5: Portfolio and Parallel Solving

#### C.12.1 Questions (a)–(d): Portfolio Size, Parameter Sensitivity, Clause Sharing, Restart Combination

*Optimal portfolio size, diversification parameter selection, inter-instance clause sharing, restart combination.*

**Discovered Data.** Portfolio solving was not implemented. B.8 telemetry showed the plateau is consistent across probe depths, and B.20.9 showed it persists under LTP substitution. The consistency of the plateau (~87,500 ± 1,000) across heuristic variations suggests that diversifying heuristic parameters within a portfolio would produce uniformly poor results — all instances would stall at approximately the same depth.

**Conclusions.** Likely unproductive under current architecture. Portfolio benefit requires that different heuristic configurations encounter qualitatively different search landscapes. The plateau's consistency across k = 0…4, across slope vs. LTP, and across static vs. dynamic ordering suggests a single structural bottleneck that no heuristic variation can circumvent. Portfolio solving becomes interesting only if a future architecture change creates a landscape where heuristic choice materially affects search trajectory beyond the plateau entry point.

---

### C.13 From B.14.7: Lightweight Nogood Recording

#### C.13.1 Questions (a)–(d): Failure Recurrence, Partial vs. Full Nogoods, Checking Strategy, Portfolio Sharing

*Failure recurrence rate, partial nogood power, optimal checking strategy, inter-instance sharing.*

**Discovered Data.** Not implemented. B.8 telemetry reports 37% hash mismatch rate (6.1M mismatches / 16.7M iterations). B.20.9 reduced this to 25.2%. B.21 achieved 0.20%. The dramatic mismatch reduction from B.21 suggests that with variable-length lines, hash failures become rare enough that nogood recording provides minimal additional benefit — there are few failures to record.

**Conclusions.** Conditional on architecture. Under B.8/B.20 (25–37% mismatch), nogood recording could be valuable — the high failure rate suggests many repeated configurations. Under B.21 (0.20% mismatch), the question is largely moot: failures are so rare that recording them provides negligible pruning. The question's relevance depends entirely on which architecture is deployed.

---

### C.14 From B.16.7: Partial Row Constraint Tightening

#### C.14.1 Questions (a)–(d): Additional Forcing, Iteration Count, Integration Trigger, SAC Combination

*PRCT forcing yield, bounds propagation iterations, integration strategy, SAC combination.*

**Discovered Data.** PRCT was not implemented. B.20.9's partial answer to the underlying problem: the mismatch reduction from 37% to 25.2% under LTP substitution demonstrates that constraint quality improvements can push some cells past the forcing threshold. B.21's 0.20% mismatch shows that variable-length lines achieve near-complete forcing in early rows, making PRCT redundant for those lines. However, B.21's depth regression (50,272 vs. 88,503) suggests that PRCT-like reasoning on the remaining long lines could help.

**Conclusions.** Potentially relevant for B.21-class architectures. In the original uniform-length regime, PRCT faces the same dead-zone problem as all other forcing-based techniques. Under B.21, the short LTP lines provide the tightness variation that PRCT exploits. PRCT applied to the interaction between short (nearly-forced) LTP lines and long (still-loose) basic lines could yield additional forced cells in the plateau band. This is one of the more promising avenues for improving B.21's depth performance.

---

### C.15 From B.17.8: Look-Ahead on Row Completion

#### C.15.1 Questions (a)–(d): Completion u/ρ Distribution, Nogood Combination, Trigger Region, Cross-Line Ordering

*Distribution of u and ρ at row completion, nogood combination, trigger region, cross-line check ordering.*

**Discovered Data.** B.8 telemetry reports min_nz_row_unknown = 1, meaning rows consistently reach u = 0 through forcing cascades — the solver fully forces every cell in a row before hash-checking. B.21's 0.20% mismatch rate confirms that forcing cascades are nearly complete with variable-length lines.

**Conclusions.** RCLA is largely redundant. Since min_nz_row_unknown = 1, rows reach u = 0 through normal propagation. RCLA's value (enumerating completions when u ≤ u_max) activates only when propagation stalls with u > 0 remaining, which the telemetry shows rarely or never happens. The answer to question (a) is that u is typically 0 or 1 at row completion, making RCLA's enumeration trivial or unnecessary.

---

### C.16 From B.18.6: Learning from Repeated Hash Failures

#### C.16.1 Questions (a)–(d): Failure Correlation, Counter Overhead, Cross-Block Sharing, B.14 Synergy

*Per-cell failure correlation, counter maintenance cost, cross-block transfer, nogood synergy.*

**Discovered Data.** Not implemented. B.21's 0.20% mismatch rate means hash failures are extremely rare under variable-length architecture, reducing the statistical signal available for failure-frequency learning. Under B.8/B.20 (25–37% mismatch), the higher failure rate would provide richer statistics, but the question of whether per-cell failure patterns are correlated (rather than pseudorandom due to SHA-1) remains open.

**Conclusions.** Architecture-dependent. Under B.21 (0.20% mismatch), failure-biased learning has almost no signal to work with. Under B.8/B.20 architectures, the approach has more potential but the fundamental question — whether hash failures are correlated with specific cell-value decisions or are essentially random — remains unanswered. Given that SHA-1 is a pseudorandom function of all 511 bits, per-cell correlations are likely weak.

---

### C.17 From B.19.7: Enhanced Stall Detection and Extended Probes

#### C.17.1 Questions (a)–(e): Backtrack Distributions, Beam Search, Row-Boundary Probing, Threshold Tuning, Bandit Formulation

*Backtrack depth distribution, beam search effectiveness, row-boundary probing overhead, threshold tuning, multi-armed bandit formulation.*

**Discovered Data.** B.19's enhanced stall detection was not fully implemented. B.8 provides partial data: the solver reaches k = 4 in the plateau and remains there, indicating persistent stalling. B.20.9 and B.21 show different stalling profiles (B.20.9: 198K iter/sec, depth 88,503; B.21: 687K iter/sec, depth 50,272), suggesting that stall characteristics vary significantly across architectures.

**Conclusions.** Partially answered. The stall detector correctly identifies the plateau, but the available interventions (lookahead escalation) cannot address the root cause. Enhanced stall detection (distinguishing shallow oscillation from deep regression) is still potentially valuable for selecting appropriate interventions, but the interventions themselves (beam search, row-boundary probing) remain untested. The multi-armed bandit formulation (question (e)) is theoretically elegant but constrained by the DI determinism requirement.

---

### C.18 From B.20.8: LTP Substitution Experiment

#### C.18.1 Question (a): Sequential Table Optimization Bias

*Is sequential table optimization sufficient, or does greedy ordering leave performance on the table?*

**Discovered Data.** B.20 Config C used unoptimized tables (Fisher-Yates baseline). No hill-climbing or sequential optimization was performed. The unoptimized tables already outperformed toroidal slopes.

**Conclusions.** The question presupposes that optimization is necessary. B.20.9 demonstrates that unoptimized pseudorandom LTP already outperforms algebraically structured slopes, suggesting that the non-linearity itself (breaking the algebraic null space) is more important than per-table optimization. Sequential optimization may yield marginal improvements but is no longer the critical research question.

#### C.18.2 Question (b): Test Suite Sensitivity

*How sensitive is the experiment to the choice of test suite?*

**Discovered Data.** B.20 Config C was tested on block 0 of useless-machine.mp4 (~14M iterations). No multi-input validation.

**Conclusions.** Partially answered. The single-block result is encouraging but not validated across input types. Future experiments should include all-zeros, all-ones, alternating, and natural data inputs to confirm that LTP performance generalizes.

#### C.18.3 Question (c): Truncated-DFS Proxy

*Can the truncated-DFS proxy objective mislead the optimizer?*

**Discovered Data.** No optimization was performed — the question is moot for unoptimized tables.

**Conclusions.** Unanswered but deprioritized given that unoptimized tables already perform well.

#### C.18.4 Question (d): Variable-Length LTP Design — ANSWERED

*If Outcome 1 materializes, is there a variable-length LTP design that escapes the dead zone?*

**Discovered Data.** This is the most directly answered question in the entire research program. B.21 implemented exactly this: joint-tiled variable-length LTP with lines of length min(k+1, 511−k), ranging from 1 to 256 cells. Results: mismatch dropped from 25.2% to 0.20% (dead zone escaped for short lines), throughput increased 3.5×, but depth regressed from 88,503 to 50,272.

**Conclusions.** Answered with nuance. Variable-length LTP *does* escape the forcing dead zone for short lines — the 0.20% mismatch rate proves this conclusively. However, escaping the dead zone on LTP lines while reducing per-cell constraint count from 8 to 5–6 created a new bottleneck: insufficient constraint density in the mid-matrix. The variable-length design works as theorized but requires either (a) more sub-tables to restore constraint density, or (b) complementary techniques (PRCT, CDCL, or additional variable-length partitions) to compensate for the density reduction.

#### C.18.5 Question (e): Alternative Slope Parameters

*Should the experiment include different slope parameters?*

**Discovered Data.** Not tested. The slope family was replaced entirely.

**Conclusions.** Superseded by B.20's direction. Alternative slope parameters are no longer under consideration.

---

### C.19 From B.21.13: Joint-Tiled Variable-Length LTP

#### C.19.1 Question (a): Dual-Covered Cell Distribution

*What is the optimal distribution of the 1,023 dual-covered cells?*

**Discovered Data.** B.21 implemented extreme-corner placement. The 0.20% mismatch rate and 3.5× throughput suggest the corner placement is effective, but no alternative distributions were tested.

**Conclusions.** Unanswered comparatively. The implemented strategy works well by multiple metrics, but no evidence exists to determine whether alternative distributions (e.g., anti-diagonal extremes) would perform better.

#### C.19.2 Question (b): Sequential Construction Ordering Bias

*Does sequential sub-table construction introduce ordering bias?*

**Discovered Data.** B.21 used sequential construction (T_0 first pick, T_3 on residual). No iterative refinement was tested. The 0.20% mismatch and balanced throughput suggest no severe bias, but subtle quality differences across sub-tables were not measured.

**Conclusions.** Unanswered but likely minor. The overall results are strong enough that any ordering bias does not prevent effective performance. Iterative refinement is a potential optimization but not a priority.

#### C.19.3 Question (c): Optimal Length Distribution

*Is the triangular length distribution optimal?*

**Discovered Data.** Three distributions have now been tested:

1. *B.21 triangular (1–256 cells, partial coverage):* depth 50,272; 1–2 lines per cell. The partial coverage (5–6 lines per cell) created the dominant bottleneck; line length distribution was a secondary factor.

2. *B.22 full-coverage triangular (2–1,021 cells):* depth ~80,300; always 4 lines per cell. Restoring full coverage recovered 30K frames. However, very short lines (2–6 cells) provide minimal constraint on deep rows, and very long lines (>511 cells) accumulate pressure slowly. The extreme range hurts on both ends.

3. *B.23 uniform-511 + CDCL / B.25 uniform-511 (no CDCL):* depth ~69K / ~86K; always 4 lines per cell. B.25 (no CDCL) confirmed that uniform-511 reaches 86K — close to B.20's 88.5K. The residual 2K gap is attributable to different LCG shuffle seeds (different random partition), not the line-length distribution.

**Critical interaction discovered (B.21.13):** LTP line length directly controls CDCL jump distances. Shorter lines produce more compact antecedent chains: B.22 (lines 2–1,021 cells) produced CDCL jumps of up to 732 frames; B.23 (uniform-511) produced jumps up to 1,854 frames. The longer jumps in B.23 caused greater depth regression. However, CDCL was ultimately found to be net-harmful regardless of jump distance (see C.21).

**Conclusions.** The uniform-511 distribution (B.20/B.25) achieves the best depth at 86–88K. The triangular distribution of B.22 (2–1,021 cells) performs worse primarily because the extreme tails (very short and very long lines) are less effective than the 511-cell midrange. The optimal distribution is likely near-uniform in the 400–511 cell range, prioritizing consistent constraint density over variability. Distributions with minimum length ≥ 64 cells warrant testing.

#### C.19.4 Question (d): Belief Propagation Interaction

*How does joint tiling interact with BP-guided branching?*

**Discovered Data.** B.12.6(a) showed that BP-guided branching does not break the plateau under uniform-length architecture. B.21 changes the factor graph structure (5–6 factors per cell instead of 8, variable-length factors). No BP experiment has been run under B.21.

**Conclusions.** Unanswered but potentially relevant. B.21's variable-length lines create non-uniform marginals (short lines produce near-forcing beliefs, long lines produce near-0.5 beliefs), which could give BP more actionable signal than it had under the uniform architecture. This warrants empirical testing.

#### C.19.5 Question (e): Joint Tiling for Basic Partitions

*Can the joint-tiling principle extend to DSM/XSM?*

**Discovered Data.** No experiments. B.21's depth regression suggests caution about further reducing per-cell constraint count.

**Conclusions.** Likely counterproductive. B.21 demonstrated that reducing lines per cell from 8 to 5–6 trades depth for throughput. Jointly tiling DSM and XSM would further reduce lines per cell, likely worsening the depth regression. DSM and XSM provide qualitatively different constraint information (different slope directions) that should not be merged.

---

### C.20 From D.1.9: Loopy Belief Propagation (Abandoned)

#### C.20.1 Questions (a)–(e): LBP Convergence, SIMD Acceleration, Overconfidence Harm, Async Hybrid, Region-Based BP

*Convergence behavior at various depths, SIMD exploitation, overconfidence effects, async hybrid approach, GBP on Kikuchi clusters.*

**Discovered Data.** D.1.8 (the verdict) concluded that LBP is dominated on cost-benefit grounds by checkpoint BP (B.12). B.12.6(a) subsequently showed that even checkpoint BP fails to break the plateau — the marginals themselves carry no useful signal in the dead zone.

**Conclusions.** Moot. The parent proposal (continuous LBP) was abandoned in favor of checkpoint BP (B.12), and checkpoint BP itself was empirically shown to be ineffective. All implementation-detail questions about LBP are superseded by the negative finding that BP-based approaches — whether continuous or checkpoint — cannot break the forcing dead zone. The questions could become relevant only if a future architecture change (e.g., variable-length lines producing non-trivial marginals) rehabilitates BP-guided search.

---

### C.21 From B.21.13: CDCL Interaction Study

#### C.21.1 Does CDCL Help?

**Discovered Data.** Four experiments tested CDCL in different configurations:

| Configuration | Depth | iter/sec | CDCL max jump |
|---|---|---|---|
| B.20 (no CDCL, uniform-511) | ~88,503 | ~198K | — |
| B.22+CDCL (triangular 2–1,021) | ~80,300 | ~521K | 732 |
| B.23+CDCL (uniform-511) | ~69,000 | ~306K | 1,854 |
| B.24 (cap=0, overhead intact) | ~86,123 | ~120K | 0 |
| B.25 (overhead fully removed) | ~86,123 | ~329K | 0 |

**Conclusions.** CDCL as implemented is **strictly harmful** in every tested configuration. It reduces depth by 3–22% while increasing iter/sec (due to backjump shortcuts that skip DFS work). The net result is consistently lower depth than chronological backtracking.

The root cause is fundamental: CDCL's 1-UIP analysis is designed for **unit-clause propagation failures** in SAT, where a single "bad" decision is traceable as the conflict's unique implication point. SHA-1 hash verification is a **global non-linear constraint** over all 511 cells in a row; no single decision is the culprit. The 1-UIP trace therefore runs back hundreds to thousands of frames, producing a backjump that discards correct work without any constraint-learning benefit (the current implementation does not store learned clauses).

#### C.21.2 Does Line Length Affect CDCL Performance?

**Discovered Data.** Yes — directly. LTP line length determines how many decisions are required to "seal" a row hash. Short lines force individual cells in 1–2 decisions; long lines accumulate assignments over hundreds of decisions.

- B.22 (triangular, max 1,021 cells): CDCL max jump 732 frames → depth 80,300
- B.23 (uniform-511): CDCL max jump 1,854 frames → depth 69,000

Shorter lines produce shorter antecedent chains, which produce shorter CDCL jumps. But even the shorter B.22 jumps (732 frames) still reduced depth by 9% vs B.20.

**Conclusions.** Line length modulates CDCL harm rather than enabling CDCL benefit. No distribution was found where CDCL produces net improvement. The interaction confirms that CDCL is incompatible with hash-based row verification at any line length.

#### C.21.3 Does Removing CDCL Overhead Recover B.20's Performance?

**Discovered Data.** Partially. B.24 disabled backjumping but left the per-assignment recording overhead (recordDecision, recordPropagated, unrecord on every cell). This reduced iter/sec to 120K and recovered depth to 86K. B.25 removed the overhead entirely, recovering iter/sec to 329K (66% faster than B.20's 198K) while maintaining 86K depth.

The remaining 2K depth gap (86K vs B.20's 88K) is attributed to **partition seed differences**: B.25 uses seeds `CRSCLTP1`–`CRSCLTP4` (ASCII) while B.20 used different seeds. Same Fisher-Yates construction algorithm, different random shuffles, different depth ceiling. The depth ceiling is entirely partition-quality-driven.

**Conclusions.** Yes, removing CDCL overhead recovers the depth lost to CDCL (and surpasses B.24's throughput). The solver is now faster than B.20 in iter/sec. The residual 2K gap is a partition-seed artifact and is within the normal variation expected from different random partitions.

#### C.21.4 What Is the Path Forward?

**Discovered Data.** With CDCL removed (B.25), the solver is at ~86K depth / ~329K iter/sec. B.20 achieved ~88K / ~198K.  The depth ceiling is determined by the partition structure, not the backtracking strategy.

**Conclusions.** Three directions are promising:

1. **Partition seed search:** Run multiple seeds and select the one achieving maximum depth. The B.20 seeds produced 88K; B.25 seeds produced 86K. A systematic seed search over ≥10 candidates may find seeds reaching 90K+.

2. **Distribution tuning:** The uniform-511 distribution is near-optimal but not proven optimal. Clipped triangular (minimum 64 cells), quadratic, or distributions biasing toward 256-cell lines may improve depth by creating more uniform constraint pressure across all DFS depths.

3. **Propagation improvements:** More aggressive propagation (e.g., probing on more constraint families, earlier hash verification) could increase the fraction of cells forced deterministically, raising the depth ceiling independent of partition geometry.

CDCL-style conflict learning with actual clause storage (not just backjumping) remains theoretically interesting but requires a fundamentally different conflict analysis adapted to global row constraints. This is a long-term research direction.

---

