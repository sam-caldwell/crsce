### B.12 Survey Propagation and Belief-Propagation-Guided Decimation

The CRSCE solver's `ProbabilityEstimator` computes a static approximation of marginal beliefs: for each cell, it
multiplies residual ratios across 7 non-row constraint lines to estimate the probability that the cell should be 1
versus 0. This computation runs once before DFS begins and is never updated. The result is a crude proxy for the
true marginal probability, because it treats the 7 lines as independent (they are not) and ignores the global
constraint structure (how assignments to distant cells propagate through shared lines).

Survey propagation (SP) and belief propagation (BP) are message-passing algorithms that compute far more accurate
marginal estimates by iterating messages between variables (cells) and constraints (lines) on the factor graph until
convergence (Mézard, Parisi, & Zecchina, 2002; Braunstein, Mézard, & Zecchina, 2005). BP computes approximate
marginal probabilities for each variable given the constraint structure. SP goes further: it estimates the probability
that each variable is *forced* to a specific value in the space of satisfying assignments, capturing the "backbone"
structure that BP misses. Marino, Parisi, and Ricci-Tersenghi (2016) demonstrated that backtracking survey
propagation (BSP)&mdash;which uses SP-guided decimation with backtracking fallback&mdash;solves random K-SAT instances
in practically linear time up to the SAT-UNSAT threshold, a region unreachable by CDCL solvers.

#### B.12.1 Application to CRSCE

The CRSCE constraint system is a factor graph with $s^2 = 261{,}121$ binary variable nodes (CSM cells) and
$10s - 2 = 5{,}108$ factor nodes (constraint lines), plus $s = 511$ hash-verification factors (LH). Each variable
participates in 8 factors (or 10 with an LTP pair). The graph is sparse: each factor connects to $s$ or fewer
variables, and the total edge count is $\sum_L \text{len}(L) \approx 2{,}000{,}000$.

A single BP iteration passes messages along all edges: each factor sends a message to each of its variables
summarizing the constraint's belief about that variable given messages received from all other variables. One
iteration costs $O(\text{edges}) \approx 2 \times 10^6$ multiply-add operations. Convergence typically requires
10-50 iterations for sparse factor graphs, so a full BP computation costs $20$-$100 \times 10^6$ operations
($\approx 20$-100 ms on Apple Silicon at $\sim 10^9$ operations/second).

This is far too expensive to run at every DFS node (the solver processes $\sim 500{,}000$ nodes/second in the fast
regime). However, BP can be used as a *periodic reordering oracle*: at checkpoint rows (e.g., every 50 rows, or at
the plateau entry point), the solver pauses the DFS, runs BP to convergence on the residual subproblem (unassigned
cells with their current constraint bounds), and recomputes the cell ordering based on BP's marginal estimates.
The BP-informed ordering replaces the static `ProbabilityEstimator` scores for the next segment of the search.

#### B.12.2 BP-Guided Decimation

An alternative to using BP purely for ordering is *decimation*: use BP to identify the most-biased variable (the
cell with the strongest marginal preference for 0 or 1), fix that cell to its preferred value, propagate, and repeat.
This converts the DFS into a greedy assignment sequence guided by global marginal information. When a conflict is
detected (infeasibility or hash mismatch), the solver falls back to backtracking search from the last decimation
point.

The decimation approach has two advantages over pure ordering. First, it exploits BP's global view to make
assignments that are unlikely to cause conflicts, reducing the backtrack count. Second, it naturally concentrates
assignments on cells with strong beliefs (near-forced cells), which tends to trigger propagation cascades earlier
than a tightness-driven ordering would.

The disadvantage is computational cost. Each decimation step requires a full BP computation ($\sim 50$ ms), and the
solver makes ~261K assignments per block. Running BP at every step would take ~3.6 hours per block&mdash;far slower
than the current solver. The practical approach is *batch decimation*: run BP, decimate the top $D$ most-biased
cells (e.g., $D = 1{,}000$), propagate, and repeat. This amortizes the BP cost across $D$ assignments, reducing
the overhead to $\sim 50$ ms per 1,000 assignments ($\sim 50$ $\mu$s/assignment)&mdash;roughly 25$\times$ the current
per-assignment cost, but potentially offset by a dramatic reduction in backtracking.

#### B.12.3 Survey Propagation for Backbone Detection

SP extends BP by introducing a third message type: the probability that a variable is *frozen* (forced to a specific
value in all satisfying assignments). In the CRSCE context, a frozen variable is a cell whose value is determined
by the constraint system regardless of how other cells are assigned. SP identifies these frozen cells without
exhaustive enumeration.

Frozen cells discovered by SP can be assigned immediately without branching, effectively shrinking the search space.
If SP identifies 10,000 frozen cells in the plateau region, the DFS has 10,000 fewer branching decisions to make&mdash;
a potentially exponential reduction in tree size. The cost is one SP computation ($\sim 100$-500 ms, as SP
converges more slowly than BP), amortized across thousands of forced assignments.

The caveat is convergence. SP was designed for random CSP instances near the phase transition, where the factor graph
has tree-like local structure. The CRSCE factor graph is not random&mdash;it has regular structure (every row line has
exactly $s$ cells, every slope line visits cells at modular intervals). SP may fail to converge on structured graphs,
or may converge to inaccurate estimates. Empirical testing on the CRSCE factor graph is needed to determine whether
SP's backbone detection is reliable.

#### B.12.4 DI Determinism

BP and SP are deterministic algorithms: given the same factor graph and the same initial messages, they produce the
same marginal estimates. The factor graph is derived from the constraint system (which is identical in compressor and
decompressor), and the initial messages can be fixed (e.g., uniform beliefs). Therefore, BP/SP-guided decimation and
reordering produce identical search trajectories in both compressor and decompressor. DI semantics are preserved.

The only subtlety is convergence tolerance. BP iterates until messages stabilize within a threshold $\epsilon$. If
floating-point rounding differs between machines, the convergence point may differ by one iteration, producing
slightly different marginal estimates. This can be avoided by using fixed-point arithmetic or by rounding messages
to a fixed precision after each iteration. Alternatively, the solver can run a fixed number of BP iterations (e.g.,
50) rather than iterating to convergence, eliminating the convergence-tolerance issue entirely.

#### B.12.5 Interaction with Other Proposals

BP-guided reordering is complementary to B.10 (constraint-tightness ordering): B.10 provides a fast, local ordering
heuristic used at every DFS node, while BP provides a slow, global reordering used at periodic checkpoints. The
solver uses B.10 between checkpoints and BP at checkpoints, combining local responsiveness with global accuracy.

BP-guided decimation interacts with B.8 (adaptive lookahead): decimation reduces the number of branching decisions,
which reduces the number of stall points where lookahead is needed. The two techniques are complementary --
decimation handles the "easy" cells (strong beliefs), and lookahead handles the "hard" cells (ambiguous beliefs).

SP's backbone detection interacts with B.1 (CDCL): frozen cells identified by SP are logically implied by the
constraint system, so assigning them does not generate nogoods or conflict clauses. SP pre-solves the easy part
of the problem, leaving CDCL to handle the hard part.

#### B.12.6 Open Questions

(a) **[ANSWERED — 2026-03-04]** Does BP converge reliably on the CRSCE factor graph? The graph's regular structure
(uniform line lengths, modular slope patterns) may cause BP to oscillate rather than converge.

*Empirical result:* Gaussian BP with damping α = 0.5, run for 30 iterations over all 5,108 constraint lines,
converges reliably on the CRSCE factor graph. In a live solver run (`run_id=1c64a9a5`), the first BP checkpoint
(cold-start from zero messages) produced `max_delta = 18.252` LLR units, indicating significant message movement.
The second checkpoint (warm-start from checkpoint 1's fixed point) produced `max_delta = 0.000`, confirming that
the fixed point is stable and that 30 iterations suffice for cold-start convergence. `max_bias = 0.500` across all
checkpoints means the Gaussian approximation pushes some cells' beliefs to the sigmoid clamp boundary (near-certain
0 or 1). No oscillation was observed. Damped BP is sufficient; tree-reweighted BP is not needed.

*Negative finding:* BP-guided branch-value ordering (overriding the `preferred` field when `|p - 0.5| > 0.1`) does
not break the depth plateau. After 3 StallDetector escalation events (k escalated from 1 to 4), the solver depth
remained at approximately 87,487-87,498, indistinguishable from the B.8-only baseline. The Gaussian approximation
may be too coarse for CRSCE's extremely loopy factor graph (each cell participates in 8 constraint families
simultaneously). The BP fixed point may not accurately reflect the true marginals of the constraint satisfaction
subproblem at the DFS frontier, leading to branch-value hints that neither help nor reliably harm the search.

(b) What is the optimal checkpoint interval for BP-guided reordering? Too frequent and the BP overhead dominates;
too infrequent and the ordering becomes stale. The interval should be tuned to the plateau structure: more frequent
checkpoints in the plateau band (rows 100-300), less frequent in the easy prefix and suffix.

(c) Can SP identify a meaningful number of frozen cells in the CRSCE instance? If SP finds that most cells are
unfrozen (as expected for a problem with DI > 0, meaning multiple solutions exist), its backbone-detection benefit
is limited. SP may still provide useful ordering information even if few cells are frozen.

(d) Is batch decimation ($D = 1{,}000$) sufficient to amortize the BP cost, or does the solver need a larger batch?
The optimal $D$ depends on how quickly BP's marginal estimates become stale after $D$ assignments. If propagation
cascades from the $D$ assignments significantly change the constraint landscape, BP must be rerun sooner.

(e) Can BP be accelerated on Apple Silicon using the Metal GPU? The message-passing computation is highly parallel
(messages on different edges are independent within one iteration). A GPU implementation could reduce the per-BP
cost from $\sim 50$ ms to $\sim 5$ ms, making BP-guided decimation at every ~100 assignments feasible.

