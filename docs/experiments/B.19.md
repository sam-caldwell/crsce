### B.19 Enhanced Stall Detection and Extended Probe Strategies

B.8 introduces adaptive lookahead with stall-triggered escalation from $k = 0$ to $k = 4$, where $k = 4$ is the
current maximum. The stall detector monitors a sliding window of net depth advance and escalates when forward
progress stalls. In practice, the solver reaches $k = 4$ in the deep plateau (rows ~170-300) and remains there,
because the stall metric $\sigma$ never recovers sufficiently to trigger de-escalation. At $k = 4$, the solver
processes 12-30K iterations/second with 16 exhaustive probes per decision. If this throughput is still insufficient
to solve the block within the time bound, the solver has exhausted B.8's escalation ladder and has no further
recourse.

This appendix proposes two extensions: (1) enhanced stall detection that distinguishes *qualitatively different*
stalling regimes and responds with targeted interventions, and (2) extended probe strategies that go beyond $k = 4$
exhaustive lookahead by trading completeness for depth.

#### B.19.1 Limitations of the Current Stall Metric

The stall metric $\sigma = \Delta_{\text{depth}} / W$ measures net depth advance over a sliding window. This
single scalar conflates two distinct stalling modes:

*Shallow oscillation.* The solver repeatedly advances a few cells and backtracks the same distance. Net depth
advance is near zero, but the solver is actively exploring alternatives at a fixed depth. This mode is productive:
the solver is systematically eliminating infeasible branches at the current row, and each oscillation brings it
closer to a feasible assignment.

*Deep regression.* The solver backtracks through many rows, losing hundreds or thousands of depth levels before
resuming forward progress. Net depth advance is deeply negative. This mode is unproductive: the solver has
discovered that a high-level assignment (many rows above) is wrong, but it is unwinding cell by cell through
intervening rows, revisiting decisions that are irrelevant to the conflict.

Both modes produce $\sigma \leq 0$, triggering the same escalation response ($k \to k + 1$). But they call for
different interventions. Shallow oscillation benefits from deeper lookahead (helping the solver find the feasible
branch faster). Deep regression benefits from *backjumping* or *restarts* (jumping directly to the root cause
instead of unwinding incrementally).

#### B.19.2 Multi-Metric Stall Detection

To distinguish stalling modes, the solver can maintain multiple metrics:

*Backtrack depth distribution.* For each backtrack event, record the number of depth levels unwound, $d_{\text{bt}}$.
Maintain a sliding window of the last $W_{\text{bt}}$ backtrack depths. If the median backtrack depth exceeds a
threshold $d^{*}$ (e.g., $d^{*} = 100$ cells, corresponding to roughly $100 / 511 \approx 0.2$ rows), the solver
is in deep-regression mode.

*Row re-entry count.* Track how many times each row is re-entered (its unknowns drop to 0, a hash check occurs,
and the solver backtracks into the row to try a different assignment). A row with a high re-entry count is a
*conflict hotspot*: the partial assignment above the row is likely wrong, and no assignment within the row will
satisfy the hash.

*Forward-progress rate.* Instead of net depth advance, measure the *maximum depth reached* in the sliding window.
If the maximum depth has not increased in $W$ decisions, the solver is making no progress regardless of local
oscillations.

These metrics can be combined into a *stall classification*:

| Metric | Shallow Oscillation | Deep Regression | Mixed |
|:------:|:-------------------:|:---------------:|:-----:|
| $\sigma$ | $\approx 0$ | $\ll 0$ | $< 0$ |
| Median $d_{\text{bt}}$ | Small ($< d^{*}$) | Large ($> d^{*}$) | Bimodal |
| Max depth advance | Stationary | Declining | Stationary |

#### B.19.3 Targeted Interventions by Stall Mode

*For shallow oscillation.* The solver is stuck at a specific depth (typically a row boundary in the plateau).
Appropriate interventions:

(a) Escalate lookahead depth (B.8's existing response). Effective if the oscillation is caused by near-miss
    branches that fail within a few cells.

(b) Invoke RCLA (B.17) at a higher $u$ threshold for the stuck row. If the row has $u = 15$ unknowns, enumerate
    all $\binom{15}{\rho}$ completions and check hashes. This either finds a feasible completion (resolving the
    oscillation) or proves the row infeasible (allowing the solver to backtrack to the prior row).

(c) Invoke PRCT (B.16) on the stuck row. Tighter bounds on the remaining unknowns may force additional cells,
    reducing $u$ and enabling RCLA to trigger or enabling propagation cascades.

*For deep regression.* The solver is backtracking through many rows. Appropriate interventions:

(a) Trigger a partial restart (B.11). Rather than unwinding one level at a time, jump back to a checkpoint depth
    (e.g., the beginning of the plateau band) and resume the DFS from there. This skips the unproductive
    cell-by-cell unwinding.

(b) Escalate to CDCL-style backjumping (B.1) if implemented. Analyze the conflict to identify the root-cause
    decision and jump directly to it, skipping irrelevant intermediate rows.

(c) Update failure statistics (B.18) for all rows traversed during the regression. The deep backtrack provides
    evidence that the high-level assignment was wrong; recording this evidence biases future branching away from
    the failed pattern.

*For mixed stalling.* The solver alternates between oscillation and regression. A combined intervention uses
multi-metric detection to select the appropriate response at each decision point.

#### B.19.4 Extended Probe Strategies Beyond $k = 4$

B.8 caps exhaustive lookahead at $k = 4$ (16 probes per decision) due to the exponential cost of the full $2^k$
tree. When the solver reaches $k = 4$ and remains stalled, the following extensions offer deeper lookahead at
sub-exponential cost:

*Strategy A: Beam search.* Instead of exploring the full $2^k$ tree, maintain a beam of the $B$ most promising
partial assignments at each depth level. At depth $d$, each of the $B$ candidates is extended by both values (0
and 1), producing $2B$ candidates. These are evaluated by a heuristic (e.g., the number of constraint violations,
or the constraint-tightness score from B.10) and the top $B$ are retained. The cost is $O(B \cdot k)$ per decision
rather than $O(2^k)$.

At $B = 8$ and $k = 16$, the cost is $8 \times 16 = 128$ probes per decision&mdash;comparable to $k = 7$ exhaustive
($2^7 = 128$) but reaching 16 levels deep. The tradeoff is that beam search is incomplete: it may miss
contradictions that lie outside the beam. However, for detecting cardinality violations that manifest at depth
8-16 (spanning 1-3 cells into the next row), beam search's heuristic filtering is likely to retain the
relevant branches.

*Strategy B: Row-boundary probing.* Rather than probing to a fixed depth $k$, probe forward until the next row
boundary and check the SHA-1 hash. If the current cell is at position $(r, c)$ with $511 - c$ cells remaining
in the row, the probe completes the row (assigning $511 - c$ cells greedily, using constraint-tightness ordering),
checks the SHA-1 hash, and backtracks if the hash fails.

The cost is $O(511 - c)$ propagation steps per probe, which varies from $O(s)$ (if $(r, c)$ is at the beginning
of the row) to $O(1)$ (if $(r, c)$ is near the row end). On average, the probe cost is $O(s/2) \approx 256$
propagation steps ($\sim 500$ $\mu$s). The solver runs two probes per decision (one for each value), costing
$\sim 1$ ms per decision. At $\sim 1{,}000$ decisions per row ($\sim 200{,}000$ plateau-band decisions total), the
overhead is $\sim 200$s per block&mdash;comparable to the current solve time and thus marginally acceptable.

The advantage of row-boundary probing over fixed-depth lookahead is that it exploits the SHA-1 hash as a
verification oracle, which is far more powerful than cardinality-based pruning. A fixed-depth probe at $k = 4$ can
detect only cardinality violations within 4 cells; a row-boundary probe can detect hash mismatches for the entire
row, which catches failures that no finite $k$ would detect.

*Strategy C: Stochastic lookahead.* Rather than exhaustively exploring all $2^k$ paths, sample $M$ random paths
of depth $k$ and check each for feasibility. This is a Monte Carlo estimation of the feasibility rate at depth $k$:
if all $M$ samples are infeasible, the current assignment is likely doomed. The false-negative probability (failing
to detect a feasible path) is $(1 - p)^M$, where $p$ is the fraction of feasible paths. At $p = 0.01$ (1\% of
paths are feasible) and $M = 100$, the false-negative rate is $(0.99)^{100} \approx 0.37$&mdash;too high for
reliable pruning.

Stochastic lookahead is therefore unsuitable as a primary pruning mechanism but could serve as a *stall-breaking
heuristic*: when the solver is stuck at $k = 4$, sample 100 random paths of depth 32 (spanning $\sim 6$ cells
per line across 4 rows). If all samples fail, switch to a different branching decision rather than
exploring the full subtree deterministically. This trades completeness for speed in the deep stall regime.

**DI determinism caveat.** Stochastic lookahead requires a deterministic PRNG seeded from the search state (e.g.,
the current undo-stack hash) to ensure identical sampling in compressor and decompressor. Beam search and row-
boundary probing are fully deterministic given deterministic tie-breaking in the heuristic evaluation.

#### B.19.5 Stall Detector as Orchestrator

The enhanced stall detector serves as an *orchestration layer* that coordinates B.8, B.11, B.16, B.17, and B.18.
Rather than each proposal operating independently, the stall detector classifies the current stalling mode and
dispatches the appropriate intervention:

1. $\sigma > 0$: forward progress. No intervention. Use current $k$.

2. $\sigma \leq 0$, median $d_{\text{bt}} < d^{*}$: shallow oscillation.
   Escalate $k$ (B.8). If $k = 4$, invoke PRCT (B.16) + RCLA (B.17) on the stuck row. If still stalled,
   try row-boundary probing (Strategy B).

3. $\sigma \leq 0$, median $d_{\text{bt}} > d^{*}$: deep regression.
   Trigger partial restart (B.11) or backjumping (B.1). Update failure statistics (B.18) for traversed rows.

4. $\sigma \leq 0$, max depth not advancing for $> 3W$ decisions: persistent stall.
   Invoke beam-search lookahead (Strategy A) at $k = 16, B = 8$. If still stalled, escalate to stochastic
   lookahead (Strategy C).

This orchestration layer is the key architectural contribution of B.19: it unifies the various plateau-breaking
proposals into a coherent escalation hierarchy, applying each technique where it is most effective.

#### B.19.6 DI Determinism

All stall metrics (net depth advance, backtrack depth distribution, row re-entry counts, maximum depth) are
deterministic functions of the search trajectory. The stall classification and intervention dispatch are therefore
identical in compressor and decompressor. DI semantics are preserved.

The extended probe strategies (beam search, row-boundary probing) are deterministic given deterministic heuristic
evaluation. Stochastic lookahead requires a deterministic PRNG as noted in B.19.4.

#### B.19.7 Open Questions

(a) What are the empirical distributions of backtrack depth and row re-entry count in the plateau band? If the
solver predominantly exhibits shallow oscillation (as the current $k = 4$ data suggests), the deep-regression
interventions (B.11, B.1) are less urgent. If deep regressions are common, they are critical.

(b) Is beam search (Strategy A) effective on the CRSCE factor graph? The heuristic evaluation function (constraint
tightness, B.10) must correlate with actual feasibility for the beam to retain relevant branches. If the heuristic
is poorly calibrated in the plateau (where tightness is uniformly weak), beam search degenerates to random sampling.

(c) What is the overhead of row-boundary probing (Strategy B) in the non-plateau regime? If the stall detector
correctly limits row-boundary probing to the plateau band, the overhead applies only to the $\sim 200$ hard rows.
But if the stall detector misclassifies easy rows as stalled, probing wastes throughput on rows that would resolve
quickly with standard propagation.

(d) How should the stall detector's thresholds ($\sigma^{-}$, $d^{*}$, $W$, $W_{\text{bt}}$) be tuned? These
parameters interact: a low $d^{*}$ classifies more episodes as deep regression, triggering more restarts; a high
$d^{*}$ classifies more episodes as shallow oscillation, triggering more lookahead escalation. The optimal
settings depend on the relative cost and benefit of each intervention, which varies across blocks.

(e) Can the orchestration hierarchy (B.19.5) be formalized as a multi-armed bandit? Each intervention is an
"arm" with an unknown reward (solve-time reduction). The stall detector is a contextual bandit that observes the
stall classification and selects the arm with the highest expected reward. Thompson sampling or UCB could be used
to balance exploration (trying new interventions) with exploitation (repeating interventions that have worked). The
determinism requirement constrains the bandit: the selection policy must be a deterministic function of the search
history to preserve DI.

