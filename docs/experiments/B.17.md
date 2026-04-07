### B.17 Look-Ahead on Row Completion

The CRSCE solver verifies each completed row against its SHA-1 lateral hash (LH). A hash mismatch after all 511
cells in a row are assigned triggers a backtrack, unwinding the most recent branching decision and trying the
alternative value. The cost of this mismatch is proportional to the depth of the wasted subtree: the solver may have
assigned thousands of cells beyond the row boundary before discovering (via a later row's hash check) that an
assignment within the earlier row was wrong. This appendix proposes *row-completion look-ahead* (RCLA): when a row
drops to a small number of unknowns $u \leq u_{\text{max}}$, enumerate all $\binom{u}{\rho}$ valid completions
consistent with the row's cardinality constraint, check each against the SHA-1 hash, and prune immediately if none
pass.

#### B.17.1 Enumeration Cost

A row with $u$ unknowns and residual $\rho$ has exactly $\binom{u}{\rho}$ completions consistent with the row's
cardinality constraint (all other constraints aside). The SHA-1 cost per completion is approximately 200 ns
(the row message is 512 bits = one SHA-1 block). The total enumeration cost is:

$$C = \binom{u}{\rho} \times 200\;\text{ns}$$

For small $u$, this is tractable:

| $u$ | $\rho$ (worst case $= u/2$) | $\binom{u}{\rho}$  | Cost       |
|:---:|:---------------------------:|:------------------:|: ----     :|
| 3   | 1-2                         | 3                  | 0.6 $\mu$s |
| 5   | 2-3                         | 10                 | 2 $\mu$s   |
| 8   | 4                           | 70                 | 14 $\mu$s  |
| 10  | 5                           | 252                | 50 $\mu$s  |
| 15  | 7-8                         | 6,435              | 1.3 ms     |
| 20  | 10                          | 184,756            | 37 ms      |

At $u \leq 10$, the cost is under 50 $\mu$s&mdash;comparable to a single iteration of the DFS loop at $k = 2$
lookahead. At $u \leq 15$, the cost (1.3 ms) is tolerable if invoked once per row (511 rows per block, so
$\sim 0.7$s total). At $u = 20$, the 37 ms cost is marginal; beyond $u = 20$, the exponential growth of
$\binom{u}{\rho}$ makes exhaustive enumeration impractical.

#### B.17.2 Pruning Power

RCLA provides a fundamentally different pruning mechanism from constraint propagation or lookahead. Standard
propagation identifies infeasible cells by analyzing residuals; lookahead identifies infeasible cells by tentatively
assigning values and checking for cardinality violations. Neither mechanism can detect a row that is *cardinality-
feasible but hash-infeasible*: all $\binom{u}{\rho}$ completions satisfy the row's cardinality constraint (and
possibly all other cardinality constraints), but none produce a 512-bit message whose SHA-1 matches the stored
lateral hash.

When RCLA determines that no valid completion exists, it provides *proof of infeasibility* for the current partial
assignment to the row. The solver can immediately backtrack to the most recent branching decision within the row,
without assigning the remaining $u$ cells one by one, propagating after each, and eventually discovering the
mismatch at $u = 0$. The savings are proportional to $u$: instead of $u$ assign-propagate-check cycles (each
costing ~2-5 $\mu$s), the solver pays one $\binom{u}{\rho} \times 200$ ns enumeration.

More importantly, RCLA catches doomed rows *before* the solver descends into subsequent rows. Without RCLA, a row
that will fail its hash check at completion may first trigger thousands of cell assignments in rows below it (as
the solver fills the matrix top-down). When the hash finally fails at $u = 0$, all those subsequent assignments
are wasted. RCLA at $u = 10$ catches the doomed row 10 cells before completion, preventing the solver from
descending into the next row's subtree on a doomed path.

#### B.17.3 Interaction with Cross-Line Constraints

The enumeration in B.17.1 considers only the row's cardinality constraint. Some of the $\binom{u}{\rho}$
completions may violate other constraints (column, diagonal, slope residuals). Pre-filtering completions against
cross-line feasibility before checking SHA-1 can reduce the enumeration count.

For each candidate completion, the solver checks whether assigning the $u$ cells to their candidate values would
create a negative residual or an excessive residual on any of the cross-lines passing through those cells. This
check costs $O(8u)$ per candidate (8 lines per cell, each requiring a residual update). At $u = 10$, this is 80
operations per candidate, or $252 \times 80 = 20{,}160$ operations total&mdash;negligible compared to the SHA-1 cost.

If cross-line filtering eliminates most candidates, the effective SHA-1 count drops substantially. In the plateau
band, where cross-line residuals are moderately constrained, filtering might reduce the candidate count by 2-10×,
making RCLA feasible at higher $u$ thresholds.

#### B.17.4 Incremental SHA-1 Optimization

The $\binom{u}{\rho}$ completions differ only in the positions of $u$ bits within the 512-bit row message. If the
$u$ unknowns are clustered in the same 32-bit word of the SHA-1 input, the solver can precompute the SHA-1
intermediate state up to the word boundary, then finalize only the last few rounds for each candidate. SHA-1
processes its input in 32-bit words; if all unknowns fall within one or two words, the precomputation saves
approximately 80-90\% of the SHA-1 cost per candidate.

In practice, the $u$ unknowns are scattered across the 16-word (512-bit) input block, because the unknowns are the
cells not yet forced by propagation, and forcing patterns depend on cross-line interactions across the full row.
Therefore, the incremental optimization is unlikely to help in the general case. However, when RCLA is invoked at
very small $u$ (3-5 unknowns), the unknowns may happen to cluster, and the optimization is worth attempting.

#### B.17.5 Triggering Strategy

RCLA should not be invoked at a fixed $u$ threshold for all rows. The cost-benefit ratio depends on $\rho$ as well
as $u$: when $\rho$ is close to 0 or $u$, $\binom{u}{\rho}$ is small even for larger $u$ (e.g., $\binom{20}{1} =
20$, costing only 4 $\mu$s). The optimal trigger is therefore:

$$\text{Invoke RCLA when } \binom{u}{\rho} \leq C_{\max}$$

where $C_{\max}$ is a threshold on the enumeration count (e.g., $C_{\max} = 500$, corresponding to $\sim 100\,
\mu$s). This threshold-based trigger naturally adapts to the residual: rows with extreme residuals trigger RCLA
earlier (at higher $u$), while rows with $\rho \approx u/2$ trigger later (at lower $u$).

Computing $\binom{u}{\rho}$ is $O(1)$ with a precomputed lookup table for $u \leq 511$ and $\rho \leq u$. The
lookup table requires $\sim 1$ MB of storage (512 × 512 × 4 bytes) and can be generated once at solver
initialization.

#### B.17.6 DI Determinism

RCLA is purely deductive and deterministic. The enumeration order (e.g., lexicographic over the $u$ unknown
positions) is fixed, and the SHA-1 computation is deterministic. If *any* completion passes the hash check, the
row is feasible and the solver continues; if *none* pass, the row is provably infeasible and the solver backtracks.
In neither case does RCLA alter the set of feasible solutions or the order in which they are visited. It merely
detects infeasibility earlier than the standard approach (which waits until $u = 0$).

DI semantics are preserved because RCLA prunes only infeasible subtrees: it never eliminates a partial assignment
that could lead to a valid solution. The solver visits the same solutions in the same lexicographic order, but
backtracks sooner on doomed rows.

#### B.17.7 Expected Benefit

In the plateau band, the solver encounters approximately $200 \times 511 \approx 100{,}000$ row completions per
block (200 rows × 511 completions per row in the backtracking process&mdash;many rows are completed multiple times
due to backtracking). If RCLA detects infeasibility at $u = 10$ instead of $u = 0$, each detection saves the cost
of 10 assign-propagate cycles plus any wasted work in subsequent rows. At 2-5 $\mu$s per cycle, the direct saving
is 20--50 $\mu$s per detection. The indirect saving (preventing descent into subsequent rows on a doomed path) is
potentially much larger&mdash;up to hundreds of milliseconds per doomed subtree if the solver would have explored
several rows before discovering the hash mismatch.

Quantifying the indirect saving requires instrumentation: how deep does the solver typically descend beyond a
doomed row before backtracking? If the answer is rarely more than a few cells (because the doomed row's hash
is checked at $u = 0$ immediately), RCLA's benefit is limited to the 10-cell direct saving. If the answer is
hundreds or thousands of cells (because the solver completes the row, starts the next, and backtracks only when
a later hash fails), RCLA's benefit is substantial.

#### B.17.8 Open Questions

(a) What is the empirical distribution of $u$ and $\rho$ at the point where rows are completed in the plateau band?
If rows typically reach $u = 0$ through forcing cascades (most cells are propagated, not branched), the "natural"
$u$ at completion is already low, and RCLA has little room to trigger earlier.

(b) Can RCLA be combined with B.14 (nogood recording)? When RCLA proves a row infeasible, the current
partial assignment to the row's decision cells is a nogood. Recording this nogood (per B.14) prevents the solver
from re-entering the same configuration on future backtracking, compounding RCLA's benefit.

(c) For rows in the fast regime (rows 0-100 and 300-511), is RCLA ever triggered? In these regions, propagation
forces most cells, so $u$ drops to 0 rapidly. RCLA may be relevant only in the plateau band, where propagation
stalls and $u$ remains high.

(d) Should RCLA check cross-line constraints (B.17.3) before or after the SHA-1 check? Cross-line filtering is
cheaper per candidate ($O(8u)$ vs. SHA-1's $\sim 200$ ns), so checking cross-lines first and SHA-1 only on
survivors is more efficient when many candidates fail cross-line checks. But if most candidates pass cross-lines
(as expected when $u$ is small and residuals are loose), the filtering overhead is wasted.

