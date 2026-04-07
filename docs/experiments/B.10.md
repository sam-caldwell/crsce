### B.10 Constraint-Tightness-Driven Cell Ordering

The probability-guided cell ordering computed by `ProbabilityEstimator::computeGlobalCellScores` is static: it is
calculated once before DFS begins and never updated. B.4 partially addressed this by introducing a dynamic
row-completion priority queue that overrides the static order for cells in nearly-complete rows
($u(\text{row}) \leq \tau$). The priority queue is effective at steering the solver toward LH checkpoints, but it
considers only the row dimension&mdash;it ignores constraint tightness on columns, diagonals, slopes, and LTP lines.

This appendix generalizes B.4 into a fully dynamic, multi-line tightness-driven cell ordering. The row-completion
priority queue (B.4) is a special case of this proposal with all non-row weights set to zero. B.4 should be
considered subsumed by this appendix; the implementation described in B.4 is the first stage of the ordering
strategy proposed here.

#### B.10.1 Limitations of Row-Only Priority

The row-completion priority queue activates when $u(\text{row}) \leq \tau$ and selects the cell in the row with the
smallest $u$. Among cells that qualify, it makes no distinction based on non-row constraint tightness.

Consider two cells, both in rows with $u = 3$. Cell $A$ sits on a column with $u = 200$ and a diagonal with
$u = 150$. Cell $B$ sits on a column with $u = 2$ and a diagonal with $u = 1$. The row-completion queue treats
them as equivalent (same row $u$), but assigning cell $B$ is far more valuable: it may trigger forcing on the column
(if $\rho = 0$ or $\rho = u - 1$) and will almost certainly force the diagonal's last unknown cell, producing a
cascade that propagates information into other rows. Cell $A$ provides no such secondary benefit.

#### B.10.2 Multi-Line Tightness Score

Define the *tightness score* $\theta(r, c)$ for an unassigned cell $(r, c)$ as a weighted sum of the proximity of
each constraint line through $(r, c)$ to its forcing threshold:

$$
    \theta(r, c) = \sum_{L \in \text{lines}(r,c)} w_L \cdot g(u(L))
$$

where $\text{lines}(r, c)$ is the set of all constraint lines containing cell $(r, c)$ (currently 8 linear lines,
potentially 10 with an LTP pair), $w_L$ is a per-line-type weight, and $g(u)$ is a monotonically decreasing function
of the line's unknown count that rewards lines close to forcing. A natural choice is $g(u) = 1 / u$, which gives
infinite weight to lines with $u = 1$ (one assignment away from completion) and diminishing weight to lines with
large $u$.

The weights $w_L$ encode the relative value of forcing on different line types. Row lines receive the highest weight
because completing a row enables an LH check&mdash;the strongest pruning event in the solver. Column and slope lines
receive lower but non-trivial weight because forcing on these lines triggers propagation cascades into other rows.
Diagonal and anti-diagonal lines in the mid-rows have large $u$ and contribute little during the plateau, so their
weight can be reduced. A reasonable starting point is $w_{\text{row}} = 10$, $w_{\text{col}} = 3$,
$w_{\text{slope}} = 2$, $w_{\text{diag}} = 1$.

The row-completion priority queue (B.4) is the degenerate case $w_{\text{row}} = 1$,
$w_{\text{col}} = w_{\text{slope}} = w_{\text{diag}} = 0$, $g(u) = -u$ (min-heap on $u(\text{row})$). The
multi-line score generalizes this by adding sensitivity to non-row constraint tightness.

#### B.10.3 Incremental Maintenance

A full recomputation of $\theta$ for all unassigned cells after every assignment is prohibitively expensive
($O(s^2)$ per step). Instead, the solver maintains $\theta$ incrementally: when cell $(r, c)$ is assigned, update
$\theta$ for each unassigned cell on each of the 8-10 affected lines. This is $O(\sum_L u(L))$ per assignment&mdash;
roughly $8 \times 341 \approx 2{,}700$ updates at the plateau midpoint.

At each DFS node, the solver selects the unassigned cell with the highest $\theta$. A max-heap keyed on $\theta$
supports this in $O(\log n)$ per extraction. The heap replaces B.4's min-heap on $u(\text{row})$, generalizing the
same priority-queue infrastructure to the multi-line score. When cells' $\theta$ values change due to neighboring
assignments, the heap must be updated&mdash;either via decrease/increase-key operations ($O(\log n)$ each) or via lazy
deletion (mark stale entries and re-insert, with periodic rebuilds).

A cheaper approximation is to recompute $\theta$ only for cells in the priority queue (those with
$u(\text{row}) \leq \tau$). This limits the update set to cells in nearly-complete rows, which is a small fraction
of the total. Outside the queue, the static ordering remains in effect.

#### B.10.4 Cost Analysis

The incremental $\theta$ maintenance adds ~2,700 score updates per assignment in the plateau. Each update is a
subtract-and-add on a floating-point accumulator (remove the old $g(u)$ contribution, add the new one). At ~2 ns per
update, this is ~5.4 $\mu$s per assignment&mdash;roughly 2.5$\times$ the current per-iteration cost of ~2 $\mu$s. The
total cost is significant but may be justified if the improved ordering reduces the backtrack count by more than
2.5$\times$.

The queue-only approximation reduces the cost to $O(\tau \times s)$ per assignment in the worst case, but typically
much less because most assignments do not change any row's $u$ below $\tau$. This approximation preserves B.4's
cost profile while adding non-row tightness sensitivity for the cells that matter most.

#### B.10.5 DI Determinism

The tightness score $\theta$ is a deterministic function of the constraint store state: it depends only on $u(L)$
and $\rho(L)$ for each line, which are identical in compressor and decompressor at every DFS node. The cell-selection
rule "choose the cell with the highest $\theta$, breaking ties by row-major order" is therefore deterministic and
produces the same DFS trajectory in both. DI semantics are preserved, exactly as they are for B.4's row-completion
queue.

#### B.10.6 Relationship to B.8 (Adaptive Lookahead)

Better cell ordering reduces the number of DFS nodes that require lookahead probing (because the solver makes better
branching decisions upfront), while lookahead handles the residual cases where no ordering heuristic can predict the
correct branch. The two techniques operate on different timescales&mdash;ordering is per-node, lookahead is per-stall
&mdash;and their benefits should be additive.

#### B.10.7 Open Questions

Consolidated into Appendix C.9.

