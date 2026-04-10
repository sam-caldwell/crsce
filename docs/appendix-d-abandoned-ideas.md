## Appendix D: Abandoned Ideas

This appendix collects research directions that were explored in detail but ultimately abandoned on
cost-benefit grounds. Each section is preserved in full for archival purposes: the analysis may become
relevant if future architectural changes alter the assumptions that led to abandonment.

### D.1 Loopy Belief Propagation as Integrated Inference (formerly B.15)

B.12 proposes belief propagation (BP) as a periodic reordering oracle: the solver pauses DFS at checkpoint rows,
runs BP to convergence on the residual subproblem, and uses the resulting marginals to guide cell ordering or
batch decimation. This appendix examines a more aggressive alternative: *loopy belief propagation* (LBP)
embedded directly into the propagation loop, running continuously alongside constraint propagation rather than
at discrete checkpoints. The question is whether LBP can provide sufficiently accurate marginal estimates on the
CRSCE factor graph to break the 87K-depth stalling barrier, and whether the computational cost can be amortized
to a tolerable level.

#### D.1.1 Background and Theoretical Foundations

Belief propagation computes exact marginal distributions on tree-structured factor graphs by passing messages between
variable nodes and factor nodes until convergence (Pearl, 1988). Each variable-to-factor message summarizes the
variable's belief about its own state given all constraints *except* the recipient factor, and each factor-to-variable
message summarizes the factor's constraint on the variable given messages from all other variables in the factor's
scope. On trees, BP converges in a number of iterations equal to the graph diameter, and the resulting marginals are
exact (Kschischang, Frey, & Loeliger, 2001).

When the factor graph contains cycles&mdash;as the CRSCE graph invariably does, since every cell participates in 8
constraint lines and those lines share cells&mdash;BP is no longer guaranteed to converge, and the marginals it
produces are approximations. This regime is called *loopy belief propagation*. Despite the lack of formal
convergence guarantees on loopy graphs, LBP has achieved remarkable empirical success in several domains: turbo
decoding and LDPC codes in communications (McEliece, MacKay, & Cheng, 1998), stereo vision and image segmentation
in computer vision (Sun, Zheng, & Shum, 2003), and random satisfiability near the phase transition (Mézard, Parisi,
& Zecchina, 2002).

The theoretical justification for LBP's empirical success rests on two results. First, Yedidia, Freeman, and Weiss
(2001) showed that fixed points of LBP correspond to stationary points of the Bethe free energy, a variational
approximation to the true log-partition function. The Bethe approximation is exact on trees and typically accurate
when the factor graph has long cycles (low density of short loops). Second, Tatikonda and Jordan (2002) proved that
LBP converges to a unique fixed point when the spectral radius of the dependency matrix is less than one&mdash;a
condition related to the graph's coupling strength and cycle structure.

#### D.1.2 The CRSCE Factor Graph: Structural Analysis

The CRSCE factor graph has specific structural properties that bear directly on LBP's applicability. The graph
contains $s^2 = 261{,}121$ binary variable nodes and $10s - 2 = 5{,}108$ factor nodes (with an additional $s = 511$
LH verification factors). Each variable participates in exactly 8 factors (one per constraint-line family: row,
column, diagonal, anti-diagonal, and four LTP partitions), and each factor connects to exactly $s = 511$ variables
(all lines in the 8 standard families are length $s$; DSM/XSM lines near the matrix boundary are shorter, ranging
from 1 to 511).

The shortest cycles in this graph are length 4: any two cells that share both a row and a column form a 4-cycle
through the row factor and column factor. Since every pair of cells in the same row shares a row factor, and many
pairs share a column or diagonal factor, the CRSCE graph is densely loopy. The average cycle density can be
quantified: for each cell at position $(r, c)$, the 8 factors it participates in collectively touch approximately
$8 \times 511 - 7 = 4{,}081$ other cells (subtracting the 7 redundant counts of the cell itself). Pairs among those
4,081 cells that share a second factor with each other create 4-cycles through the original cell's factors. The
expected number of such 4-cycles per cell is $O(s)$, yielding a total 4-cycle count of $O(s^3) \approx 1.3 \times
10^8$.

This high density of short cycles is the central concern for LBP in CRSCE. The Bethe approximation assumes that the
factor graph's neighborhood is locally tree-like&mdash;that the subgraph reachable from any node within $k$ hops
contains few cycles. With $O(s)$ 4-cycles per cell, this assumption is violated at $k = 2$. The practical
consequence is that LBP's marginal estimates will overcount correlations: a cell's belief will incorporate the same
constraint information multiple times through different cycle paths, producing overconfident (polarized) marginals.

#### D.1.3 The Case for LBP in CRSCE

Despite the structural concerns, several arguments favor LBP for CRSCE plateau-breaking.

*Argument 1: Marginal accuracy need not be high.* LBP is proposed not as an exact inference engine but as a
heuristic guide for branching decisions. The solver needs only a *ranking* of cells by how constrained they are, not
precise probabilities. If LBP's marginals are monotonically correlated with true marginals&mdash;even with substantial
absolute error&mdash;the resulting ordering will be superior to the current static `ProbabilityEstimator`. Empirical
studies in SAT solving have shown that even crude BP approximations improve branching heuristics relative to purely
local measures (Hsu & McIlraith, 2006).

*Argument 2: LBP captures long-range correlations that local propagation misses.* The 87K-depth stalling barrier
arises because cardinality forcing becomes inactive in the plateau band (rows ~100--300): residuals are neither 0 nor
equal to the unknown count, so the propagation engine cannot force any cells. The information needed to break the
stalemate exists in the constraint system&mdash;distant cells' assignments constrain the residuals of lines passing
through the plateau&mdash;but the current propagator does not transmit this information because it operates only on
individual lines. LBP's message-passing iterates through the entire factor graph, propagating constraints across
multiple hops. After $t$ iterations, each cell's belief incorporates information from cells up to $t$ hops away in
the factor graph. At $t = 10$, a cell's belief reflects constraints from cells up to 80 constraint lines distant
($10 \times 8$ lines per hop), potentially spanning the entire matrix. This global view is precisely what the solver
lacks at depth 87K.

*Argument 3: Incremental LBP amortizes cost across assignments.* Rather than running BP from scratch at checkpoints
(as B.12 proposes), incremental LBP updates only the messages affected by each new assignment. When cell $(r, c)$ is
assigned, only the 8 factors containing $(r, c)$ receive new information. Those 8 factors send updated messages to
their $\sim 4{,}000$ neighboring variables, which update their beliefs and propagate outward. If the solver limits
propagation to $\delta$ hops from the assigned cell (a "message-passing wavefront"), the per-assignment cost is
$O(\delta \cdot s)$ multiply-add operations. At $\delta = 3$ and $s = 511$, this is $\sim 1{,}500$ operations per
assignment&mdash;approximately $10\times$ the cost of the current constraint-propagation step, but far cheaper than
a full BP computation ($\sim 50$ ms).

*Argument 4: Warm-starting preserves convergence quality.* Because LBP runs continuously, each new assignment
perturbs the message state only slightly. The messages are already close to the fixed point of the previous
subproblem, so convergence to the new fixed point (given the new assignment) requires only a few iterations of the
affected messages. This warm-start property is well-established in the graphical models literature (Murphy, Weiss,
& Jordan, 1999) and is the key to making continuous LBP computationally feasible.

#### D.1.4 The Case Against LBP in CRSCE

The arguments against LBP are substantial and grounded in both theory and the specific structure of the CRSCE
problem.

*Objection 1: Non-convergence on densely loopy graphs.* Tatikonda and Jordan's (2002) convergence condition requires
that the spectral radius of the graph's dependency matrix be less than one. For the CRSCE factor graph, each factor
connects to $s = 511$ variables, and each variable participates in 8 factors. The coupling strength is directly
related to the factor's constraint tightness: a cardinality constraint with residual $\rho$ and $u$ unknowns has
coupling strength proportional to $|\rho / u - 0.5|$ (how far the constraint is from maximum entropy). In the
plateau band, $\rho / u \approx 0.5$ (the constraint is maximally uninformative), so coupling is weak and LBP may
converge. But at the matrix edges (early and late rows), constraints are tight and coupling is strong. The spectral
radius condition may be satisfied only in the plateau band&mdash;exactly where LBP is needed but also where the
marginals carry the least information. This creates a paradox: LBP converges where it is least useful and diverges
where it could provide the strongest guidance.

*Objection 2: Overconfident marginals from short cycles.* The $O(s^3)$ 4-cycles in the CRSCE graph cause LBP to
double-count constraint evidence. A cell $(r, c)$ receives a message from its row factor and a message from its
column factor, but these messages are not independent: they share information about cells in the same row-column
intersection. The result is overconfident marginals&mdash;beliefs close to 0 or 1 even when the true marginal is near
0.5. Overconfident marginals produce aggressive branching decisions that frequently lead to conflicts, increasing
rather than decreasing the backtrack count. Wainwright and Jordan (2008, Section 4.2) document this failure mode
extensively and show that it is intrinsic to the Bethe approximation on graphs with dense short cycles.

Mitigation strategies exist. Tree-reweighted BP (TRW-BP) (Wainwright, Jaakkola, & Willsky, 2005) assigns
edge-appearance probabilities that correct for double-counting, guaranteeing an upper bound on the log-partition
function. However, TRW-BP requires computing edge-appearance probabilities from a set of spanning trees, which is
$O(|E|^2)$ for the CRSCE graph ($|E| \approx 2 \times 10^6$, so $O(4 \times 10^{12})$)&mdash;prohibitively
expensive. Fractional BP (Wiegerinck & Heskes, 2003) and convex variants (Globerson & Jaakkola, 2007) similarly
incur overhead that exceeds the computational budget.

*Objection 3: Cardinality factors are expensive.* Standard BP message computation for a factor with $k$ variables
requires summing over $2^k$ joint configurations. For CRSCE's cardinality constraints (each connecting $s = 511$
binary variables), the naive cost is $2^{511}$&mdash;clearly intractable. Efficient message computation for cardinality
factors is possible using the convolution trick: the sum-product messages for a cardinality constraint on $k$ binary
variables with target sum $\sigma$ can be computed in $O(k^2)$ time using dynamic programming (Darwiche, 2009,
Section 12.4). At $k = 511$, this is $\sim 261{,}000$ operations per factor per iteration. With 5,108 factors and
10 iterations, the total cost is $\sim 1.3 \times 10^{10}$ operations ($\sim 13$ seconds on Apple Silicon) ---
orders of magnitude slower than the current solver's throughput of $\sim 500{,}000$ assignments/second.

The incremental approach (D.1.3, Argument 3) mitigates this by updating only 8 factors per assignment rather than
all 5,108, but each factor update still costs $O(s^2) \approx 261{,}000$ operations. At 8 factors per assignment,
the incremental cost is $\sim 2 \times 10^6$ operations per assignment&mdash;a $4{,}000\times$ overhead relative to
the current propagation step ($\sim 500$ operations per assignment). Even with Apple Silicon's throughput, this
reduces the solver from 500K assignments/second to $\sim 125$ assignments/second, extending block solve times from
minutes to weeks.

*Objection 4: LBP provides no pruning, only ordering.* Unlike constraint propagation (which prunes infeasible
assignments) or CDCL (which prunes infeasible subtrees), LBP provides only soft guidance: a ranking of cells and
value preferences. The solver still explores the same search tree; it merely traverses it in a different order. If
the search tree's branching factor is dominated by the 87K-depth plateau (where $\sim 111$ cells per row are
unconstrained), reordering the traversal cannot reduce the tree's exponential size&mdash;it can only improve the
constant factor in front of the exponential. The fundamental problem is that the plateau's combinatorial explosion
requires *pruning* (eliminating subtrees) rather than *reordering* (visiting subtrees in a better sequence). LBP
does not prune.

This objection does not apply if LBP's marginals enable other pruning mechanisms to activate. For example, if LBP
reveals that a cell is near-forced ($p \approx 0.01$), the solver can treat it as forced and propagate, triggering
constraint-propagation cascades that genuinely prune. But this amounts to using LBP as a soft forcing heuristic ---
converting approximate beliefs into hard assignments&mdash;which risks correctness violations if the marginal is wrong.
The solver would need a verification mechanism (e.g., backtracking on LBP-guided forced assignments that lead to
conflicts), eroding much of the computational savings.

*Objection 5: DI determinism is fragile under floating-point LBP.* LBP's messages are real-valued quantities
updated by iterative multiplication and normalization. Floating-point arithmetic is not associative, and message
update order can affect the converged values. If the compressor and decompressor execute LBP with different message
scheduling (e.g., due to parallelism or platform-specific optimizations), the resulting marginals may differ,
producing different cell orderings and different search trajectories, violating DI determinism. B.12.4 addresses
this for checkpoint-based BP by proposing fixed-point arithmetic or a fixed iteration count. For *continuous* LBP
integrated into the propagation loop, the determinism requirement is stricter: every incremental message update after
every assignment must produce bitwise-identical results on both compressor and decompressor. This requires either
(a) fixed-point arithmetic with specified precision and rounding mode, or (b) a deterministic message schedule
(e.g., always process the 8 affected factors in a fixed order, then propagate outward in breadth-first order from
the assigned cell). Both approaches constrain the implementation and prevent performance optimizations (e.g.,
parallel message updates on GPU) that would violate the deterministic schedule.

#### D.1.5 Quantitative Cost-Benefit Estimate

To evaluate LBP's net value, consider a concrete scenario. Assume the solver spends $T_{\text{base}} = 600$ seconds
on a block without LBP, with 90\% of the time ($540$s) consumed in the plateau band (rows 100--300). The plateau
generates $\sim 10^{10}$ backtracks.

*Optimistic case.* LBP's improved ordering reduces the backtrack count by $100\times$ (from $10^{10}$ to $10^8$).
The per-assignment cost increases $4{,}000\times$ due to LBP overhead (Objection 3). The net effect: plateau time
changes from $10^{10} \times 2\,\mu\text{s} = 540$s to $10^8 \times 8{,}000\,\mu\text{s} = 800{,}000$s ($\approx
9.3$ days). LBP is a net loss despite a $100\times$ reduction in backtracks, because the per-node overhead
overwhelms the savings.

*Break-even analysis.* For LBP to be net-positive, the backtrack reduction must exceed the overhead ratio. At
$4{,}000\times$ per-node overhead, LBP must reduce backtracks by more than $4{,}000\times$ to improve on the
baseline. A $4{,}000\times$ backtrack reduction (from $10^{10}$ to $2.5 \times 10^6$) would reduce plateau time to
$2.5 \times 10^6 \times 8{,}000\,\mu\text{s} = 20{,}000$s ($\approx 5.6$ hours)&mdash;still slower than the 540s
baseline, because the LBP overhead applies to all $\sim 100{,}000$ assignments in the plateau, not just to the
$2.5 \times 10^6$ backtracks. The full accounting: $100{,}000$ assignments $\times$ $8{,}000\,\mu\text{s/assignment}
= 800$s for the forward pass, plus $2.5 \times 10^6$ backtracks $\times$ $8{,}000\,\mu\text{s} = 20{,}000$s for
backtracking. Total: $20{,}800$s. The LBP overhead on the forward pass alone ($800$s) exceeds the $540$s baseline.

*Reduced-overhead scenario.* If the incremental wavefront is limited to $\delta = 1$ hop (only the 8 directly
affected factors, with no outward propagation), the per-assignment cost drops to $\sim 8 \times 261{,}000 \approx
2 \times 10^6$ operations ($\sim 2$ms). At $100{,}000$ forward assignments: $200$s. At a $100\times$ backtrack
reduction ($10^8$ backtracks $\times$ $2$ms): $200{,}000$s. Still a net loss. Even at $\delta = 1$ and a
$10{,}000\times$ backtrack reduction: $100{,}000 \times 2\text{ms} + 10^6 \times 2\text{ms} = 200\text{s} +
2{,}000\text{s} = 2{,}200$s&mdash;roughly $4\times$ worse than baseline.

The arithmetic is unforgiving. LBP's per-node cost on length-511 cardinality factors is fundamentally too high for
integration into a DFS loop that processes hundreds of thousands of nodes.

#### D.1.6 Comparison with B.12's Checkpoint Approach

B.12 avoids LBP's per-node overhead by running BP only at checkpoints (every 50 rows, or at plateau entry). The
amortized cost is $\sim 50$ms per $25{,}000$ assignments ($\sim 2\,\mu\text{s/assignment}$)&mdash;nearly identical to
the baseline per-assignment cost. The tradeoff is that BP's marginals become stale between checkpoints: after
$25{,}000$ assignments, the factor graph has changed substantially, and the checkpoint marginals may no longer
reflect the current constraint state.

LBP's theoretical advantage over checkpoint BP is freshness: by updating messages after every assignment, LBP
always reflects the current constraint state. But as Section D.1.5 shows, the cost of freshness is prohibitive. The
checkpoint approach trades marginal accuracy for computational feasibility, and this trade is overwhelmingly
favorable. If the solver needs more accurate marginals between checkpoints, the natural remedy is to increase the
checkpoint frequency (e.g., every 10 rows) rather than to switch to continuous LBP. Even at 1 checkpoint per row,
the amortized cost is $\sim 50\text{ms} / 511 \approx 100\,\mu\text{s/assignment}$&mdash;a $50\times$ overhead rather
than $4{,}000\times$.

#### D.1.7 Mitigation Strategies and Their Limitations

Several techniques could reduce LBP's overhead, though none sufficiently to change the fundamental cost-benefit
calculus.

*Approximate message updates.* Rather than computing exact sum-product messages for cardinality factors ($O(s^2)$
per factor), the solver could use Gaussian approximation: approximate the binomial distribution of the cardinality
factor's output with a Gaussian and compute messages in $O(s)$ time. This reduces the per-factor cost from
$\sim 261{,}000$ to $\sim 511$ operations, yielding a per-assignment cost of $\sim 4{,}000$ operations ($8\times$
overhead). However, the Gaussian approximation is poor for binary cardinality factors when the residual is small
($\rho \ll s$) or large ($\rho \approx u$)&mdash;precisely the regime where the constraint is informative and LBP
would be most useful.

*Selective LBP.* Run LBP only in the plateau band (rows 100--300) and use standard propagation elsewhere. This
limits the overhead to $\sim 100{,}000$ assignments (the plateau portion) rather than all $261{,}121$. At $8\times$
overhead with Gaussian approximation: $100{,}000 \times 16\,\mu\text{s} = 1.6$s for the forward pass. The
backtracking overhead depends on the backtrack reduction, but the forward pass alone is tolerable. The concern is
that selective LBP introduces a discontinuity at the plateau boundary: the solver switches from propagation-only
to propagation-plus-LBP at row 100, potentially causing the message state to be cold (uninitialized) at exactly
the point where it is needed. Warm-starting from a checkpoint BP computation (B.12) at row 100 mitigates this.

*GPU-accelerated message updates.* Apple Silicon's Metal GPU can parallelize the message computation across factors
and variables. The cardinality-factor DP is inherently sequential within a factor (each step depends on the
previous), but the 8 independent factor updates per assignment are independent and can run in parallel. At 8-way
parallelism, the per-assignment cost drops by $\sim 8\times$, but the GPU kernel launch overhead ($\sim
10\,\mu\text{s}$) and data transfer cost may exceed the computation savings for such small workloads.

#### D.1.8 Verdict

The weight of evidence is against continuous LBP as an integrated propagation mechanism for CRSCE. The core problem
is the mismatch between LBP's computational cost and the DFS solver's throughput requirements. The CRSCE solver
processes $\sim 500{,}000$ nodes/second in the fast regime and needs to maintain high throughput even in the plateau
band to solve blocks within practical time bounds. LBP's per-node overhead on length-511 cardinality factors ---
even with Gaussian approximation and selective application&mdash;reduces throughput by at least an order of magnitude.
The backtrack reduction LBP provides (improved ordering but no pruning) cannot compensate for this throughput loss
except under implausibly optimistic assumptions ($> 4{,}000\times$ backtrack reduction).

B.12's checkpoint approach captures most of LBP's benefit (global marginal information for branching guidance)
at a fraction of the cost (one BP computation every $N$ rows, amortized across thousands of assignments). The
marginal-staleness cost of checkpoint BP is small relative to the computational savings, and can be reduced by
increasing checkpoint frequency.

If future solver development reveals that checkpoint BP's marginal estimates are critically stale between
checkpoints&mdash;causing branching decisions that are actively worse than the current `ProbabilityEstimator` ---
then selective LBP with Gaussian approximation in the plateau band (D.1.7) merits empirical evaluation. But this
scenario is unlikely: checkpoint BP with fresh marginals at plateau entry should outperform the static estimator
over the entire plateau span, and the incremental degradation over 50 rows is unlikely to be catastrophic.

The recommendation is to pursue B.12's checkpoint BP approach and to treat continuous LBP as a theoretical
alternative that is dominated on cost-benefit grounds.

#### D.1.9 Open Questions

a. What is the empirical convergence behavior of LBP on the CRSCE factor graph at various depths? If LBP converges rapidly in the plateau band (where coupling is weak) but slowly at the edges, the selective-LBP strategy (D.1.7) may be more viable than the quantitative estimates suggest, because the plateau is where the solver spends most of its time.
b. Can the cardinality-factor message computation be restructured to exploit SIMD on Apple Silicon? The $O(s^2)$ dynamic programming has data dependencies that prevent naive vectorization, but the 8 independent factor updates per assignment could be pipelined across NEON lanes, potentially reducing the $8\times$ factor to $2$--$3\times$.
c. Does LBP's overconfidence (Objection 2) systematically harm branching in the CRSCE instance, or does the ranking correlation with true marginals (Argument 1) dominate? This is an empirical question that could be answered by running LBP on the CRSCE factor graph, comparing the resulting marginals with exact marginals (computed on small subproblems), and measuring the rank correlation.
d. Is there a hybrid approach where LBP runs asynchronously on a background thread, and the DFS solver queries the most recent marginal estimates without blocking? This decouples LBP's throughput from the DFS throughput, at the cost of using stale (but continuously improving) marginals. The determinism concern (Objection 5) is severe for this approach, as the asynchronous schedule is inherently non-deterministic.
e. Would region-based generalizations of BP&mdash;such as generalized belief propagation (GBP) on Kikuchi clusters (Yedidia, Freeman, & Weiss, 2005)&mdash;provide more accurate marginals on the densely-loopy CRSCE graph? GBP operates on clusters of variables rather than individual variables, explicitly accounting for short cycles within each cluster. The cost is $O(2^{|C|})$ per cluster of size $|C|$, which is tractable only for small clusters
($|C| \leq 20$). Whether meaningful clusters exist in the CRSCE graph's regular structure is an open question.

---

### D.2 Conflict-Driven Clause Learning (CDCL) from Hash Failures (formerly B.1)

#### D.2.1 Proposal

When a SHA-1 lateral hash mismatch kills a subtree at depth ~87K, the solver backtracks one level ---
undoing the most recent branching assignment and trying the alternate value. This is chronological backtracking: the solver retries assignments in reverse order of when they were made, regardless of which assignments actually *caused* the conflict. It learns nothing from the failure. If the root cause was an assignment made 500 levels earlier, the solver exhausts an exponential number of intermediate configurations before reaching that level.

CDCL adaptation was proposed to exploit hash failures as conflict sources. An LH mismatch on row $r$ constitutes a proof that the current partial assignment to the $s$ cells in row $r$ is infeasible. The solver could analyze this proof to identify a small subset of earlier assignments jointly responsible for the conflict, record that combination as a *nogood clause*, and backjump directly to the deepest responsible assignment rather than unwinding the stack one frame at a time.

#### D.2.2 Background: CDCL in SAT Solvers

In a CDCL SAT solver, when unit propagation derives a conflict, the solver performs *conflict analysis* by
tracing the implication graph backward from the conflicting clause. The analysis identifies a *unique
implication point* (UIP)&mdash;the most recent decision variable that, together with propagated literals,
implies the conflict. The solver learns a new clause encoding the negation of the responsible assignments,
adds it to the clause database, and backjumps to the decision level of the second-most-recent literal in
the learned clause. The learned clause immediately forces the UIP variable to its opposite value at the
backjump level, avoiding the need to re-explore the intervening search space (Marques-Silva & Sakallah,
1999).

The power of CDCL comes from two properties: *non-chronological backtracking* (backjumping past irrelevant
decision levels) and *clause learning* (preventing the solver from ever entering the same conflicting
configuration again). Together, these properties transform an exponential-time DFS into a procedure that
can prune large regions of the search space based on information extracted from each conflict.

#### D.2.3 Hash Failures as Conflict Sources

In the CRSCE solver, conflicts arise from two sources: *cardinality infeasibility* (a constraint line's
residual $\rho$ falls outside $[0, u]$) and *hash mismatch* (an SHA-1 lateral hash check fails on a
completed row). The cardinality conflicts are detected immediately during propagation and trigger standard
backtracking. The hash conflicts are detected only when a row is fully assigned ($u(\text{row}) = 0$) and
the computed SHA-1 digest disagrees with the stored LH value.

A hash mismatch on row $r$ means that the 511-bit assignment to row $r$ does not match the unique preimage
expected by the stored hash. However, some of those 511 bits were determined by branching decisions (either
directly or via propagation from decisions in other rows), while others were forced by cardinality
constraints with no branching alternative. The forced bits are *consequences* of earlier decisions; the
branching decisions are the *causes*. Conflict analysis must distinguish between the two.

When row $r$ fails its LH check, the conflict clause (in the SAT sense) is the conjunction of all 511 cell
assignments in the row. A typical row in the plateau region has ~400 cells forced by propagation and ~111
cells assigned by branching, reducing the conflict clause to ~111 decision literals. Further reduction
requires tracing the implication graph backward to identify the first UIP&mdash;the most recent decision
variable whose reversal would have prevented the conflict.

#### D.2.4 Implementation

The following data structures were fully implemented and unit-tested:

- `CellAntecedent`&mdash;per-cell reason record (stack depth, antecedent line ID, isDecision flag)
- `ReasonGraph`&mdash;flat 511x511 heap-allocated antecedent table, populated during propagation
- `ConflictAnalyzer`&mdash;BFS 1-UIP backjump target finder; returns `BackjumpTarget{targetDepth, valid}`
- `BackjumpTarget`&mdash;result struct from conflict analysis

`PropagationEngine_propagate.cpp` was modified to record the triggering line (`antecedentLine`) for every
forced cell. The DFS loop recorded decisions via `recordDecision()` and propagated assignments via
`recordPropagated()`. On hash failure, `conflictAnalyzer.analyzeHashFailure(failedRow, ...)` computed the
backjump target and `brancher_->undoToSavePoint(savePoint)` executed the jump.

DI determinism holds: the backjump target is a deterministic function of the implication graph, which is
deterministic given identical initial constraint state. Both compressor and decompressor see the same
conflicts in the same order.

#### D.2.5 Experimental Results

Three configurations were tested to isolate whether CDCL interacts with partition quality:

**B.22 + CDCL (variable-length LTP, 2-1,021 cells/line, max jump 732):**
- Depth: ~80,300 (vs. B.20 baseline 88,503: **-9%**)
- CDCL max jump dist: 732 cells
- Conclusion: CDCL harmful even with shorter LTP lines

**B.23 + CDCL (uniform-511 LTP, same as B.20 partitions, max jump 1,854):**
- Depth: ~69,000 (vs. B.20 baseline: **-22%**)
- CDCL max jump dist: 1,854 cells
- iter/sec: ~306K
- Conclusion: uniform-511 lines produce longer antecedent chains; CDCL more harmful

**B.24 (CDCL disabled via cap=0, overhead intact):**
- Depth: ~86,123 (depth recovers without CDCL jumps)
- iter/sec: ~120K (40% throughput lost from record/unrecord overhead alone)
- Conclusion: CDCL overhead costs 40% throughput even when firing zero backjumps

**B.25 (CDCL fully removed):**
- Depth: ~86,123
- iter/sec: ~329K (**2.7x faster than B.24**)
- Conclusion: removing all record/unrecord call overhead is essential

#### D.2.6 Root Cause Analysis

CDCL was designed for unit-clause propagation failures in SAT solvers. In SAT, a conflict occurs when a
single variable's forced value contradicts an existing assignment&mdash;the implication chain is typically
short (5-20 steps). The 1-UIP algorithm finds a useful cut close to the conflict.

SHA-1 hash verification is fundamentally different: it is a global nonlinear constraint over all 511 cells
in a row. When a row hash fails, the conflict has no single proximate cause in the SAT sense. The 511-bit
row pattern is wrong as a whole, but the 1-UIP algorithm traces antecedent chains across 700-1,854 cells
before finding a cut. Each backjump discards hundreds of levels of correct work&mdash;progress that
chronological backtracking would have reused.

Additionally, at the 87K-88K plateau, all 511-cell constraint lines have $\rho / u \approx 0.5$, placing
them in the forcing dead zone. CDCL's non-chronological backjumping identifies *which* decision caused a
conflict, but when all constraint lines are equally inert&mdash;none generating forcing events&mdash;every
decision level is equally stuck. There is nowhere productive to jump to. CDCL degenerates to chronological
backtracking plus bookkeeping overhead, strictly worse than the baseline.

#### D.2.7 Abandonment Verdict

CDCL non-chronological backjumping is **incompatible with SHA-1 hash constraint topology** in CRSCE.
The mechanism assumes short implication chains with a meaningful UIP close to the conflict. Row hash
failures do not satisfy this assumption. All three partition configurations tested (B.22, B.23, B.24)
confirmed monotonically worsening depth under CDCL, with no configuration showing benefit.

The implementation was removed in B.25. All CDCL data structures (`ConflictAnalyzer`, `ReasonGraph`,
`CellAntecedent`, `BackjumpTarget`) remain in the codebase as unit-tested headers but are disconnected
from the DFS loop.

CDCL-style conflict learning with actual clause storage (not just backjumping) remains theoretically
interesting but requires a fundamentally different conflict analysis adapted to global row constraints.
This is a long-term research direction, not a near-term solver improvement.

### D.2 Toroidal-Slope Partitions (formerly HSM1/SFC1/HSM2/SFC2)

#### D.2.1 Original Design

The four toroidal-slope partitions were proposed in B.2 and implemented as the first auxiliary constraint
families added beyond the four geometric partitions (LSM, VSM, DSM, XSM). Each partition consisted of
$s = 511$ lines defined on the $s \times s$ torus by a fixed modular slope $p$:

$$
L_k^{(p)} = \{(t,\; (k + pt) \bmod s) : t = 0, \ldots, s-1\}, \quad k \in \{0, \ldots, s-1\}.
$$

Because $s = 511$ is prime and $\gcd(p, s) = 1$ for each implemented slope, every line contained exactly
$s$ cells and every (slope-line, row) pair intersected in exactly one cell. The four slopes and their
designations were:

| Designation | Slope $p$ | Line index formula |
|-------------|-----------|-------------------|
| HSM1        | 256       | $k = (c - 256r) \bmod 511$ |
| SFC1        | 255       | $k = (c - 255r) \bmod 511$ |
| HSM2        | 2         | $k = (c - 2r) \bmod 511$   |
| SFC2        | 509       | $k = (c - 509r) \bmod 511$ |

The slopes were chosen so that each pair of partitions was mutually 1-cell orthogonal with respect to
rows and columns: any two distinct slope lines from different slope families intersected in at most one
cell. This property ensured that all four families contributed maximally independent constraints.

Each partition's cross-sum vector contained $s = 511$ elements, each encoded in $b = 9$ bits (MSB-first
packed bitstream), for a per-partition storage cost of $sb = 4{,}599$ bits. Adding all four toroidal-slope
partitions increased the per-block payload from 25,568 bits (geometric only) to 43,964 bits, raising the
total to 125,988 bits after LH and BH.

The implementation used a `ToroidalSlopeSum` class (deriving from `CrossSumVector`) with $O(1)$ line-index
computation via a single multiply-and-modulus operation. The class remains in the source tree
(`include/common/CrossSum/ToroidalSlopeSum.h`) for mathematical validation and unit testing.

#### D.2.2 Observed Performance

Doubling the per-cell constraint count from 4 to 8 was expected to substantially increase forcing-event
frequency in the plateau band (rows 100--300). In practice, the toroidal slopes provided marginal
propagation benefit. The core limitation was algebraic: all eight partitions (four geometric + four
toroidal-slope) are linear functionals of the cell coordinates. The set of matrices indistinguishable from
a given matrix under any collection of linear partitions forms a coset of a lattice in $\mathbb{Z}^{s^2}$,
and swap-invisible patterns can be constructed by solving for the null space. Adding more linear partitions
reduces the null-space dimension but cannot eliminate it entirely; the marginal information contributed by
each additional slope pair diminishes because new slopes partially overlap with existing ones in the same
algebraic family.

Concretely, at the solver's plateau entry depth (~row 170), each toroidal-slope line had approximately 170
of its 511 cells assigned, leaving $u \approx 341$ unknowns and a residual $\rho$ in the range 80--170.
The forcing conditions ($\rho = 0$ or $\rho = u$) were far from triggering. The slopes progressed at the
same rate as the geometric lines because their uniform, regular spacing distributed cells evenly across all
rows. No slope line became tighter than any other at a given DFS depth.

#### D.2.3 Replacement by LTP Partitions

B.20 tested a direct substitution: replace the four toroidal-slope partitions with four pseudorandom
lookup-table partitions (LTPs) while holding the total partition count at 8 and the storage budget at
125,988 bits. Each LTP sub-table assigns cells to lines via a Fisher--Yates shuffle seeded by a
deterministic 64-bit LCG constant. The resulting partition has no algebraic relationship to the cell
coordinates, breaking the lattice structure that limited the toroidal slopes.

B.20 Configuration C (4 geometric + 4 LTP) achieved depth ~88,503, representing the best performance
observed at that point. The LTP partitions' non-linear cell-to-line assignment produced heterogeneous
line composition: some LTP lines concentrated cells in early rows, reaching forcing thresholds by the
plateau entry depth, while others scattered cells across the full matrix. This heterogeneity injected
forcing events into the plateau band where the uniform toroidal slopes could not.

#### D.2.4 Reason for Abandonment

The toroidal-slope partitions were abandoned on the following grounds:

1. **Algebraic redundancy.** All toroidal slopes belong to the same family of linear modular-arithmetic
   partitions. Their null-space contribution overlaps with the existing geometric partitions, providing
   diminishing marginal constraint information per additional slope pair.

2. **Uniform progression rate.** Because every slope line visits cells at regular modular intervals
   across all rows, all slope lines progress at the same rate during DFS. No slope line becomes tight
   before any other at a given search depth, preventing early forcing events in the plateau band.

3. **Strict dominance by LTP.** The B.20 experiment demonstrated that pseudorandom LTPs using identical
   storage budget (4,599 bits per partition, 8 lines per cell) achieved strictly higher depth than the
   toroidal slopes. The LTP architecture was subsequently validated across multiple research campaigns
   (B.21--B.25), culminating in the B.22 seed optimization that established the current production
   configuration.

4. **No loss of generality.** The LTP partitions satisfy the same three cross-sum partition axioms
   (Conservation, Uniqueness, Non-repetition) as the toroidal slopes. The wire-format field sizes are
   identical (511 elements at 9 bits each). Switching from toroidal slopes to LTPs required only
   semantic reinterpretation of the four payload fields, not a format change.

The `ToroidalSlopeSum` class and its unit tests are retained in the codebase as mathematical reference
implementations. Should future architectural changes reintroduce algebraic partitions (e.g., as part
of a hybrid LTP + slope configuration), the implementation is available without modification.

