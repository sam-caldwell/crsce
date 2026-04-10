### B.13 Portfolio and Parallel Solving with Diversification

Modern SAT and CSP solvers achieve their best performance through *portfolio* strategies: running multiple solver
instances in parallel, each using a different heuristic configuration, and accepting the result from whichever
instance finishes first (Xu, Hutter, Hoos, & Leyton-Brown, 2008). Hamadi, Jabbour, and Sais (2009) showed that
diversification&mdash;instantiating solvers with different decision strategies, learning schemes, and random seeds --
is the key to portfolio effectiveness, because different heuristics excel on different problem structures. A portfolio
hedges against the worst case of any single heuristic.

The CRSCE solver runs a single DFS with a fixed heuristic configuration (static ordering + row-completion queue +
optional lookahead). If the chosen heuristic happens to make a bad early decision, the solver pays the full
exponential cost of unwinding it. A portfolio approach would run $P$ solver instances concurrently, each with a
different cell-ordering heuristic, and accept the first to reach solution $S_{\text{DI}}$.

#### B.13.1 Diversification Strategies

The solver's behavior is determined by a small number of heuristic parameters: the cell-ordering weights $w_L$ (B.10),
the row-completion threshold $\tau$ (B.4), the lookahead escalation thresholds $\sigma^-$ and $\sigma^+$ (B.8), and
the branch-value heuristic (0-first vs. belief-guided). A portfolio instantiates $P$ copies of the solver with
different parameter vectors:

*Weight diversification.* Each instance uses a different weight vector $w_L$ for the tightness score $\theta$.
Instance 1 might use $w_{\text{row}} = 10, w_{\text{col}} = 3$; instance 2 might use $w_{\text{row}} = 5,
w_{\text{col}} = 5$; instance 3 might use $w_{\text{row}} = 1, w_{\text{col}} = 1$ (effectively uniform tightness).
Different weight vectors excel in different plateau regimes.

*Lookahead diversification.* Each instance uses a different lookahead policy. Instance 1 starts at $k = 0$ with
adaptive escalation (B.8); instance 2 starts at $k = 2$ throughout; instance 3 uses no lookahead but aggressive
row-completion priority. This hedges against the case where adaptive escalation wastes time on escalation/
de-escalation overhead.

*Value ordering diversification.* Each instance uses a different branch-value heuristic. Instance 1 always tries
0 first (canonical); instance 2 tries the value preferred by the `ProbabilityEstimator`; instance 3 alternates
based on parity. Different value orderings produce different DFS trajectories, and the optimal ordering is
input-dependent.

#### B.13.2 DI Determinism

**All portfolio instances must enumerate solutions in the same lexicographic order.** The DI is defined as the
ordinal position of the correct solution in this order. If different instances use different cell orderings, they
traverse the search tree in different orders and may discover solutions in different sequences. To preserve DI
semantics, the portfolio must satisfy one of two conditions:

*Condition 1: Identical enumeration order.* All instances use the same canonical enumeration order (row-major, 0
before 1) but differ in how they *prune* the search tree (via different lookahead policies, propagation strategies,
or constraint-tightening heuristics). The set of feasible solutions and their lexicographic ordering is identical
across instances; only the traversal speed differs. The first instance to reach $S_{\text{DI}}$ reports the correct
answer.

*Condition 2: Solution buffering.* Each instance emits solutions as discovered (potentially out of lexicographic
order) into a shared buffer. A coordinator thread merges the streams and identifies $S_{\text{DI}}$ by its
lexicographic position. This requires that at least one instance eventually enumerates all solutions up to
$S_{\text{DI}}$ in order&mdash;the other instances provide "hints" that accelerate the search but are not trusted
for ordering.

Condition 1 is simpler and sufficient for CRSCE. The cell ordering affects which subtrees are explored first, but
constraint propagation and hash verification are ordering-independent: the same partial assignments are feasible or
infeasible regardless of the order in which they are explored. Different orderings prune different infeasible
subtrees at different speeds, but all instances agree on which solutions are feasible and in what lexicographic
order.

More precisely: the *value* ordering (which value to try first at each branch) and the *lookahead* policy change
only the speed at which the solver traverses the canonical tree. They do not change the tree's structure. The *cell*
ordering, however, changes the tree structure&mdash;different cell orderings produce different DFS trees with different
branching sequences. For DI determinism under cell-ordering diversification, the solver must either (a) restrict
diversification to value ordering and pruning strategies only (preserving the canonical DFS tree), or (b) use
Condition 2 (solution buffering with a canonical-order verifier).

#### B.13.3 Hardware Mapping

Apple Silicon M-series chips provide a natural platform for portfolio solving. The M3 Pro has 6 performance cores
and 6 efficiency cores; the M3 Max has 12 performance cores and 4 efficiency cores. A portfolio of $P = 4$-6
solver instances, each pinned to a performance core, provides meaningful diversification without contending for
shared resources (each instance has its own constraint store and DFS stack in L2 cache).

The Metal GPU is less suitable for portfolio solving because the solver's DFS is inherently sequential (each
assignment depends on the propagation result of the previous one). The GPU is better used for bulk propagation
within a single instance (the existing `MetalPropagationEngine`) rather than for running independent solver
instances.

Shared-nothing parallelism (each instance is fully independent) is the simplest implementation. No clause sharing,
no work stealing, no inter-instance communication. The first instance to find $S_{\text{DI}}$ signals completion
and all others are terminated. This avoids the complexity of parallel CDCL clause sharing (which requires careful
synchronization and is a major source of bugs in parallel SAT solvers).

#### B.13.4 Expected Benefit

If the solver's per-block solve time has high variance (as predicted by the heavy-tail hypothesis in B.11), a
portfolio of $P$ independent instances reduces the expected solve time to:

$$
    E[T_{\text{portfolio}}] = E[\min(T_1, T_2, \ldots, T_P)]
$$

For heavy-tailed distributions, the minimum of $P$ independent samples is dramatically smaller than the mean of a
single sample. Even if one instance makes a catastrophically bad early decision, the other $P - 1$ instances
proceed on different trajectories and are likely to avoid the same trap. Empirical studies in SAT solving show
speedups of 3-10$\times$ for $P = 4$-8 on hard instances (Hamadi et al., 2009).

The portfolio approach is also *embarrassingly parallel*: no algorithmic changes to the solver are needed. Each
instance runs the existing solver code with different parameter settings. The only new infrastructure is a
launcher that spawns $P$ instances and a signal mechanism for early termination.

#### B.13.5 Open Questions

(a) What is the optimal portfolio size $P$ for CRSCE on Apple Silicon? Too few instances limit diversification;
too many contend for cache and memory bandwidth, slowing each instance. The sweet spot depends on the chip's core
count and the solver's memory footprint per instance ($\sim 500$ KB for the constraint store + DFS stack).

(b) Which heuristic parameters provide the most diversification benefit? If the solver's performance is most
sensitive to the row-completion threshold $\tau$, diversifying $\tau$ across instances is more valuable than
diversifying lookahead depth. A sensitivity analysis on the heuristic parameters would guide portfolio design.

(c) Should instances share learned information? Clause sharing (from B.1) between portfolio instances could
accelerate all instances by propagating nogoods discovered by one instance to the others. However, clause sharing
adds synchronization overhead and complexity. For CRSCE, where conflicts are rare (hash failures, not unit-
propagation conflicts), the sharing benefit may be small.

(d) Can the portfolio approach be combined with B.11 (restarts)? Each instance could use a different restart policy
(different Luby base units, different checkpoint depths), providing restart diversification in addition to heuristic
diversification. This combines two orthogonal diversification axes.

