### B.16 Partial Row Constraint Tightening

The current propagation engine applies two forcing rules: when a line's residual $\rho(L) = 0$, all unknowns are
forced to 0; when $\rho(L) = u(L)$, all unknowns are forced to 1. These rules activate only at the extremes &mdash;
either the residual is fully consumed or it exactly matches the unknown count. Between these extremes, the
propagator does nothing: a line with $\rho = 47$ and $u = 103$ generates no forced assignments, even though the
constraint significantly restricts the feasible completions. This binary behavior is the root cause of the plateau
at depth ~87K: in the middle rows (100-300), residuals are far from both 0 and $u$, and the propagator becomes
inert. This appendix proposes *partial row constraint tightening* (PRCT): detecting and exploiting deterministic
cell values before the residual reaches a forcing extreme, by analyzing the interaction between a line's residual
and the positions of its remaining unknowns.

#### B.16.1 Positional Forcing from Cross-Line Interactions

Consider a row $r$ with $u$ unknown cells and residual $\rho$. The row constraint alone tells us that exactly
$\rho$ of the $u$ unknowns must be 1. Now consider a column $c$ that intersects row $r$ at an unknown cell
$(r, c)$. The column has its own residual $\rho_c$ and unknown count $u_c$. If every unknown on column $c$
*other than* $(r, c)$ were assigned to 1, the maximum contribution from column $c$'s other cells would be
$u_c - 1$. If $\rho_c > u_c - 1$, then cell $(r, c)$ *must* be 1 to satisfy column $c$'s constraint. Dually,
if $\rho_c = 0$, then cell $(r, c)$ must be 0 regardless of the row's state.

The existing propagator already captures the $\rho_c = 0$ and $\rho_c = u_c$ cases. PRCT extends this by
considering *joint* constraints. When the row has $u$ unknowns and residual $\rho$, and a column through one
of those unknowns has residual $\rho_c$ and unknown count $u_c$, the cell at their intersection is forced to 1
if assigning it to 0 would make the column infeasible given the row's constraint. Specifically, cell $(r, c)$ is
forced to 1 if:

$$\rho_c > u_c - 1$$

and forced to 0 if $\rho_c = 0$. But these are just the standard single-line rules. The novel contribution of
PRCT is reasoning about *groups* of cells. If a row has $u = 10$ unknowns and $\rho = 8$, then at most 2 of
those unknowns can be 0. If a column through one of those unknowns has $\rho_c = 1$ and $u_c = 5$, then that
column needs exactly 1 more one among its 5 unknowns. The row's constraint ($\rho = 8$ of $u = 10$) means that
at most 2 of the 10 row-unknowns are 0. If the column's 5 unknowns include 3 cells from this row, then at most
2 of those 3 can be 0 (from the row constraint), so at least 1 must be 1. If the column needs exactly 1 more one,
and at least 1 of its row-overlapping unknowns must be 1, and its non-row unknowns could supply at most
$u_c - 3$ ones, then we can reason about whether the column's non-row unknowns can satisfy the column without
any row-cell contribution.

#### B.16.2 Generalized Residual Interval Analysis

A more systematic formulation treats each unknown cell's value as a variable in $\{0, 1\}$ and computes, for each
cell, the *feasible interval* implied by all 8 constraint lines simultaneously. For cell $(r, c)$, define:

$$v_{\min}(r, c) = \max_{L \ni (r,c)} \left( \rho(L) - (u(L) - 1) \right)^+$$

$$v_{\max}(r, c) = \min_{L \ni (r,c)} \left( \rho(L),\; 1 \right)$$

where $(x)^+ = \max(x, 0)$ and the max/min range over all 8 lines containing $(r, c)$. If $v_{\min} = 1$, the cell
is forced to 1. If $v_{\max} = 0$, the cell is forced to 0. If $v_{\min} = v_{\max}$, the cell is determined.

This computation generalizes the current forcing rules: $\rho(L) = 0$ implies $v_{\max} = 0$ for all cells on $L$,
and $\rho(L) = u(L)$ implies $v_{\min} \geq 1$ for all cells on $L$ (since $\rho(L) - (u(L) - 1) = 1$). PRCT
activates strictly earlier than the current rules because it considers the *intersection* of constraints from all
8 lines, not each line independently.

#### B.16.3 Tightening via Residual Bounds Propagation

The interval analysis in B.16.2 can be strengthened by propagating bounds across lines. After computing
$v_{\min}(r, c)$ and $v_{\max}(r, c)$ for all cells, update each line's effective residual:

$$\rho'(L) = \rho(L) - \sum_{(r,c) \in L} v_{\min}(r, c)$$

$$u'(L) = \sum_{(r,c) \in L} (v_{\max}(r, c) - v_{\min}(r, c))$$

If $\rho'(L) = 0$, all cells with $v_{\max} = 1$ (i.e., not yet forced to 1) are forced to their minimum value.
If $\rho'(L) = u'(L)$, all cells with $v_{\min} = 0$ are forced to their maximum value. This is a second-order
tightening: the initial interval computation identifies some forced cells, which tighten the residuals, which may
force additional cells.

The process can be iterated to a fixed point, analogous to arc consistency in classical CSP. Each iteration costs
$O(s)$ per line (scanning unknowns to compute bounds), and the total cost per fixpoint computation is $O(s \times
|\text{lines}|) = O(s \times 5{,}108) \approx 2.6 \times 10^6$ operations. At the solver's current throughput,
this is approximately 2.6 ms per full pass&mdash;too expensive to run at every DFS node (which processes at ~500K
nodes/sec), but feasible as a periodic tightening step (e.g., at row boundaries or when the stall detector fires).

#### B.16.4 Row-Specific Early Detection

A targeted variant focuses exclusively on rows approaching completion. When a row has $u \leq u_{\text{threshold}}$
unknowns remaining (e.g., $u_{\text{threshold}} = 20$), the solver performs the interval analysis of B.16.2
restricted to the $u$ unknown cells in that row, considering all 8 constraint lines through each cell. The cost
is $O(8u)$ per row&mdash;$O(160)$ operations at $u = 20$&mdash;negligible even at full DFS throughput.

This targeted variant captures the most valuable tightening: as a row nears completion, the row residual
constrains the remaining unknowns more tightly (fewer cells share the same residual budget), and cross-line
constraints interact more strongly (fewer unknowns means each cell's value has more impact on other lines). The
combination is most likely to produce forced cells in precisely the regime where the current propagator falls silent.

#### B.16.5 DI Determinism

PRCT is purely deductive: it infers forced cell values from the current constraint state using deterministic
arithmetic. No random or heuristic elements are involved. The forced cells identified by PRCT are logically
implied by the constraint system&mdash;they would be assigned the same values by any correct solver. Therefore, PRCT
preserves DI semantics: the compressor and decompressor produce identical forced assignments and identical search
trajectories.

The iteration order for bounds propagation (B.16.3) must be deterministic (e.g., process lines in index order,
iterate until no cell bounds change). Floating-point arithmetic is not involved&mdash;all quantities are integers
(residuals, unknown counts, binary bounds)&mdash;so bitwise reproducibility is automatic.

#### B.16.6 Interaction with Other Proposals

PRCT is complementary to B.8 (adaptive lookahead). Lookahead detects infeasibility by tentatively assigning values
and propagating; PRCT detects forced values by analyzing constraint bounds without tentative assignments. PRCT can
be seen as a *zero-cost lookahead*: it identifies some of the same forced cells that lookahead would discover, but
without the overhead of assigning, propagating, and undoing. When PRCT forces cells, lookahead has fewer branching
decisions to make, reducing the $2^k$ probe tree.

PRCT also strengthens B.10 (constraint-tightness ordering). The tightness score $\theta(r, c)$ currently measures
how close each line through $(r, c)$ is to a forcing state. PRCT's interval analysis provides a more direct signal:
cells with $v_{\min} = v_{\max}$ are already determined and need not be branched on. Cells with $v_{\max} -
v_{\min} = 1$ but $v_{\min} > 0$ on some lines are near-forced and should be prioritized for branching.

#### B.16.7 Open Questions

(a) How many additional cells does PRCT force in the plateau band compared to standard propagation? The answer
depends on the residual distribution at depth ~87K. If residuals in the plateau are far from both 0 and $u$ on all
8 lines simultaneously, even joint-constraint reasoning may fail to tighten bounds. Instrumentation of the solver
to log per-cell $v_{\min}$ and $v_{\max}$ at plateau entry would quantify the potential.

(b) How many iterations of bounds propagation (B.16.3) are needed to reach a fixed point? If the answer is
typically 1-2 iterations, the amortized cost is low. If it is $O(s)$ iterations, the cost is prohibitive except
at row boundaries.

(c) Should PRCT be integrated into the propagation engine (running after every assignment) or invoked only at
specific triggers (row boundaries, stall detection, or when $u$ drops below a threshold)? The answer depends on
the cost-benefit ratio, which is an empirical question.

(d) Can PRCT be combined with B.6 (singleton arc consistency) for stronger inference? SAC tentatively assigns each
value to each cell and checks for a propagation failure; PRCT computes bounds without tentative assignments. Running
PRCT inside SAC's propagation step would produce tighter bounds during SAC's feasibility check, potentially
identifying more singleton-inconsistent values.

