### B.11 Randomized Restarts with Heavy-Tail Mitigation

Gomes, Selman, and Kautz (2000) demonstrated that backtracking search times on hard CSP instances follow *heavy-
tailed distributions*: there is a non-negligible probability of catastrophically long runs caused by early bad
decisions that trap the solver deep in a barren subtree. The standard remedy is *randomized restarts*&mdash;the solver
periodically abandons its current search, randomizes its decision heuristic (e.g., by shuffling variable ordering
or perturbing value selection), and begins a fresh search from the root. Luby, Sinclair, and Zuckerman (1993) proved
that the optimal universal restart schedule is geometric: restart after $1, 1, 2, 1, 1, 2, 4, 1, 1, 2, \ldots$
units of work, guaranteeing at most a logarithmic-factor overhead relative to the optimal fixed cutoff.

The CRSCE solver's plateau at row ~170 has the hallmarks of a heavy-tailed stall. The solver commits to assignments
in rows 0-100 during a fast initial phase, but some of those early decisions propagate into the mid-rows via column,
diagonal, and slope constraints, creating an infeasible partial assignment that is only discovered much later when
hash checks begin failing. Chronological backtracking unwinds these bad decisions one level at a time, exploring an
exponential number of intermediate configurations before reaching the root cause. A restart strategy would abandon
the current trajectory, perturb the ordering heuristic, and re-enter the search with a different sequence of early
decisions&mdash;potentially avoiding the bad branch entirely.

#### B.11.1 The Determinism Constraint

**Restarts are fundamentally incompatible with CRSCE's disambiguation index (DI) semantics unless the restart
policy is fully deterministic.** The DI is defined as the ordinal position of the correct solution in the
lexicographic enumeration of all feasible solutions. Both compressor and decompressor must enumerate solutions in
the identical order to agree on which solution corresponds to DI = $k$. If the restart policy involves any
randomness (a random seed, a wall-clock timer, or a non-deterministic perturbation), the compressor and
decompressor will follow different search trajectories and assign different ordinal positions to the same solution.

This constraint rules out classical randomized restarts as described by Gomes et al. (2000), where the solver
uses a fresh random seed after each restart to diversify its variable ordering. It also rules out timer-based
restarts, where the solver restarts after $t$ seconds of wall-clock time (because wall-clock time varies between
machines and runs).

What remains permissible is *deterministic restarts*: restart policies where the decision to restart, and the
post-restart heuristic state, are pure functions of the solver's search history&mdash;the sequence of assignments,
backtracks, and constraint-store states encountered so far. Because the constraint system is identical in compressor
and decompressor (both derive it from the same payload), and the initial state is identical, a deterministic restart
policy produces the identical search trajectory in both. The DI is preserved.

#### B.11.2 Deterministic Restart Triggers

A deterministic restart policy requires a trigger condition and a post-restart heuristic modification, both defined
solely in terms of the solver's internal state. Three candidate triggers are:

*Backtrack-count threshold.* Restart when the solver has performed $B$ backtracks since the last restart (or since
the start of search). The threshold $B$ can follow the Luby sequence: $B_1, B_1, 2B_1, B_1, B_1, 2B_1, 4B_1,
\ldots$ for a base unit $B_1$. This is deterministic because the backtrack count is a function of the search
trajectory, which is identical in both compressor and decompressor.

*Depth stagnation.* Restart when the solver's maximum achieved depth has not increased for $W$ consecutive
backtracks. This detects the plateau directly: the solver is stuck at depth ~87K, repeatedly entering and exiting
subtrees without making forward progress. This trigger is a generalization of B.8's stall-detection metric
($\sigma$) applied at a coarser granularity&mdash;B.8 escalates lookahead depth, B.11 restarts the search.

*Row-failure rate.* Restart when the rate of LH failures on a specific row exceeds a threshold. If the solver
has attempted row $r$ more than $F$ times without finding a valid assignment, the current partial assignment to
rows 0 through $r - 1$ is likely the root cause. A restart abandons this partial assignment and re-enters with a
modified ordering that assigns rows 0 through $r - 1$ differently.

All three triggers are deterministic: they depend only on counters and states that are identical in compressor and
decompressor.

#### B.11.3 Post-Restart Heuristic Modification

After a restart, the solver must change its behavior to avoid re-entering the same barren subtree. In classical
randomized restarts, a new random seed provides diversification. In the deterministic setting, diversification must
come from the solver's *accumulated search history*.

*Constraint-store-guided reordering.* After a restart, recompute the cell ordering using the tightness score
$\theta$ from B.10, incorporating information from the failed search. Specifically, if row $r$ was the deepest row
reached before stagnation, increase the weight $w_{\text{row}}$ for rows near $r$ in the tightness score, biasing
the solver toward completing those rows earlier in the next attempt. This deterministic perturbation causes
different cells to be assigned first, producing a different trajectory through the search space.

*Phase saving.* Borrow the *phase saving* technique from CDCL SAT solvers (Pipatsrisawat & Darwiche, 2007): record
the most recent value (0 or 1) assigned to each cell before the restart. On the next attempt, when the solver
branches on a cell, try the saved phase first instead of the canonical 0-before-1 order. This biases the solver
toward the partial assignment from the previous attempt but allows constraint propagation to steer it away from the
infeasible region. Phase saving is deterministic because the saved phases are a function of the prior search
trajectory.

*Nogood-guided avoidance.* If the solver records nogoods from hash failures (see B.1), the post-restart search
avoids assignments that violate recorded nogoods. Each restart builds on the information from all prior restarts,
progressively narrowing the search space. This combines the restart mechanism with lightweight clause learning
(without the full CDCL machinery) and is fully deterministic.

#### B.11.4 Interaction with Lexicographic Enumeration

Restarts change the order in which the solver explores the search tree, but they must not change which solutions are
*feasible*. The enumeration must still visit all feasible solutions in lexicographic order. This requires that the
restart mechanism satisfies two properties:

*Completeness.* After any finite number of restarts, the solver must eventually enumerate all feasible solutions.
If restarts permanently abandon regions of the search space, some solutions may be skipped, corrupting the DI.
Completeness is guaranteed if the restart policy is *fair*: every branch is eventually explored, even if the ordering
changes between restarts. The Luby restart schedule is fair by construction.

*Order preservation.* The solver must visit feasible solutions in the same lexicographic order regardless of the
restart history. This is the harder condition. If the post-restart heuristic changes the cell ordering, the DFS
may discover solution $S_j$ before $S_i$ even though $i < j$ lexicographically. To preserve order, the solver must
either (a) use restarts only to prune infeasible subtrees (never skipping feasible solutions), or (b) buffer
discovered solutions and emit them in sorted order.

Option (a) is achievable if the post-restart heuristic modifies only the *value* ordering (which value to try first
at each branch) and the *cell* ordering (which cell to branch on next), without changing the *feasibility* of any
partial assignment. Constraint propagation and hash verification are unchanged by the restart, so the set of feasible
solutions is identical across restarts. The solver explores the same tree, just in a different traversal order. For
DI enumeration, the solver must still count solutions in lexicographic order, which requires that the enumeration
logic track the canonical position of each discovered solution.

Option (b) is simpler but requires memory proportional to the number of solutions discovered between restarts. For
CRSCE, the DI is typically small (0-255), so the buffer is at most 256 solutions&mdash;negligible memory.

The safest approach is a *partial restart*: instead of restarting from the root, the solver backtracks to a
*checkpoint depth* (e.g., the beginning of the plateau band at row 100) and re-enters with a modified heuristic for
the subproblem below the checkpoint. The partial assignment above the checkpoint is preserved, so the lexicographic
prefix is unchanged and order preservation is automatic. The solver is effectively restarting only the hard subproblem
(the plateau), not the easy prefix (rows 0-100).

#### B.11.5 Expected Benefit

The heavy-tail argument predicts that a small fraction of search attempts will be catastrophically long, while most
will be fast. If the solver's plateau stalls follow a heavy-tailed distribution (which is plausible given the
combinatorial phase-transition structure of the mid-rows), then restarts with the Luby schedule truncate the long
tail at the cost of occasionally re-doing fast prefixes. The expected solve time is:

$$
    E[T_{\text{restart}}] = O(E[T_{\text{optimal}}] \cdot \log E[T_{\text{optimal}}])
$$

where $E[T_{\text{optimal}}]$ is the expected time with the optimal fixed cutoff (Luby et al., 1993). If the
current solver's mean solve time is dominated by the heavy tail (a few very long stalls accounting for most of the
total time), restarts could reduce the mean by an order of magnitude.

The benefit compounds with other techniques. Restarts provide diversification (trying different early decisions),
while B.8 (adaptive lookahead) provides intensification (probing more deeply at stall points). B.1 (CDCL/backjumping)
provides learning across the search (avoiding known-bad configurations). The three techniques are complementary:
restarts escape bad regions, lookahead navigates within a region, and learning prevents re-entry into known-bad
regions.

#### B.11.6 Open Questions

(a) Does the CRSCE solver's plateau stall time follow a heavy-tailed distribution? Instrumenting the solver to
log per-block solve times across many random inputs would answer this. If the distribution is heavy-tailed, restarts
provide significant benefit; if it is concentrated (low variance), restarts provide little.

(b) What is the optimal Luby base unit $B_1$? Too small and the solver restarts before making meaningful progress;
too large and it spends too long in barren subtrees before restarting. The optimal $B_1$ depends on the typical
depth of the plateau stall, which is an empirical quantity.

(c) Is partial restart (from a checkpoint at row ~100) sufficient, or does the solver sometimes need full restarts
from the root? If the root cause of most stalls is in rows 0-100, partial restarts cannot fix them. If the root
cause is in the plateau region itself (rows 100-300), partial restarts are sufficient and avoid re-doing the
fast prefix.

(d) How does phase saving interact with DI determinism? Phase saving changes the value ordering at each branch
point, which changes the traversal order within the search tree. For DI enumeration, the solver must still count
solutions in lexicographic order. If the solver uses option (b) from B.11.4 (buffering solutions and emitting in
sorted order), phase saving is compatible with DI semantics. The buffer size is bounded by the DI value (at most
255 solutions).

(e) Can the restart policy be combined with B.8's stall detection? B.8 escalates lookahead depth when stagnation
is detected; B.11 restarts when stagnation persists. A natural hierarchy is: first escalate lookahead ($k = 0
\to 1 \to 2 \to 4$), then restart if lookahead at $k = 4$ still stalls. This uses restarts as the last resort
after intensification has failed, minimizing unnecessary restarts.

