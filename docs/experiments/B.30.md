### B.30 Pincer Decompression — Option A: Propagation Sweep on Plateau (ABANDONED)

> *Inspired by the fictional "middle-out compression" algorithm of Pied Piper, Inc., as depicted
> in* Silicon Valley *(Judge et al., 2014–2019). Unlike Richard Hendricks's middle-out approach,
> Pincer attacks from both ends simultaneously — but the conceptual debt to the framing is
> acknowledged.* How many times can I watch this show
> and not be inspired?

#### B.30.1 Motivation

Every experiment from B.8 through B.29 attacks the decompression problem in one direction:
top-down, row-major, DFS from row 0 to row 510. The propagation cascade terminates at plateau
row $K_p \approx 178$ (= $91{,}090 / 511$), leaving $333 \times 511 \approx 170{,}000$ cells
in an underdetermined band that requires exponential backtracking. Seed optimization (B.26c,
B.27) extends the cascade by a few hundred rows; constraint density improvements (B.22, B.27)
adjust the line geometry — but all are bounded by the same fundamental asymmetry:

**The top-down solver uses only half the available constraint information.** The stored
cross-sums (LSM, VSM, DSM, XSM, LTP1–6) are global: they constrain the *entire* matrix, not
just the top half. An LTP line has 511 cells scattered across all 511 rows; at plateau entry,
~333 of them are in unassigned rows. The solver knows their collective sum residual but has
nothing forcing them individually, because no line is near $u = 1$. The bottom rows contribute
exactly as much to these line budgets as the top rows — but the solver never uses that fact.

The **Pincer** hypothesis: run a second DFS wavefront upward from row 510. When both
wavefronts have completed their respective propagation cascades, the constraint lines carry
contributions from *both* ends. The underdetermined middle band sees dramatically higher
constraint density than either wavefront could produce alone, and the forcing cascade that
stalled at $K_p$ can restart from a much more favorable initial state.

#### B.30.2 The Pincer Structure

The Pincer strategy uses a **single shared ConstraintStore** and alternates the DFS traversal
direction upon detecting a plateau. No separate constraint stores, residual initializations, or
compatibility checks are required — the shared store enforces consistency between the two
directions automatically.

| Phase | Direction | Trigger | Effect |
|-------|-----------|---------|--------|
| **Top-down** | Row 0 → $K_p$ | Initial | Standard DFS; plateaus at $K_p \approx 178$ |
| **Bottom-up** | Row 510 → $K_r$ | Top-down plateau | Exploits residual budget from below; tightens constraints in meeting band |
| **Top-down resume** | Row $K_p$ → meeting band | Bottom-up plateau | Constraints shaken loose by reverse pass may force cells previously underdetermined |
| **Iterate** | Alternating | Each plateau | Repeat until no progress in either direction or meeting band fully resolved |

**Direction switching is triggered by plateau detection:** when the current DFS direction
produces no forced cells and N consecutive branching decisions have been made without constraint
propagation, the solver records the current frontier row and inverts direction. The plateau
threshold N is a tunable parameter (candidate: N = 511, one full row of unconstrained branching).

**SHA-1 is direction-agnostic.** Row $r$ verifies its lateral hash when all 511 cells in row $r$
are assigned, regardless of whether the assignment proceeded top-down or bottom-up. The
bottom-up pass verifies rows 510, 509, 508, \ldots in that order as each row completes.

**Residual budgets are implicit.** Because both passes share the same ConstraintStore, the
bottom-up DFS automatically consumes the constraint budget remaining after the forward pass.
There is no explicit $\rho_{\text{available}}(L) = \text{stored\_sum}(L) - \rho_{\text{forward}}(L)$
computation — the ConstraintStore already holds this residual as its current state.

#### B.30.3 Constraint Density at the Meeting Frontier

At forward plateau entry (row $K_p \approx 178$) with only the top-down pass:

- Each LTP line: ~178 of 511 cells assigned (~35%), $u \approx 333$, $\rho/u \approx 0.5$
- Each column: 178 of 511 assigned, $u = 333$
- Forcing requires $u = 1$: all LTP and column lines are far from forcing

After the bottom-up pass additionally completes its cascade to $K_r \approx 332$:

- Each LTP line: ~178 + ~171 = ~349 of 511 cells assigned (~68%), $u \approx 162$
- Each column: 178 top + 171 bottom = 349 assigned, $u = 162$
- **Variance in $\rho/u$ across lines increases sharply.** A line whose combined top and
  bottom contributions nearly exhaust its budget has $\rho_{\text{remaining}} \approx 0$ or
  $\rho_{\text{remaining}} \approx u_{\text{remaining}}$ — forcing all remaining middle cells to
  0 or 1 respectively. These forced cells cascade into neighboring lines.

When the top-down pass resumes from $K_p$, the ConstraintStore already reflects the bottom-up
assignments. Propagation runs on the tightened constraint network: cells that were underdetermined
at the first forward plateau may now be forced outright, restarting the propagation cascade
without any branching. **This is the core mechanism by which the bottom-up pass "shakes loose"
new constraints for the resumed top-down pass.**

The improvement is not uniform — it concentrates in lines that happened to draw heavily from
the bottom rows. By the B.26c landscape result (wide variation in depth with seed choice),
partition geometry strongly affects which lines are heavily loaded in which row ranges. Seed
optimization for the combined forward+reverse objective is a new search problem (B.30.10(b)).

#### B.30.4 Meet-in-the-Middle Complexity

The classical meet-in-the-middle reduction (Diffie–Hellman, 1977) reduces an exhaustive search
over $2^N$ states to $2 \times 2^{N/2}$ by splitting the search space in half and matching
boundary states. The reduction applies when:

1. The search space decomposes cleanly at a boundary
2. The boundary state is compact enough to store and match
3. The two halves are roughly equal in size

For CRSCE decompression, the degrees of freedom concentrate in the plateau band. With only a
top-down pass, $D \approx 170{,}000$ underdetermined cells drive the backtracking cost. With
symmetric forward/reverse cascades:

$$D_{\text{total}} = D_{\text{forward}} + D_{\text{reverse}} + D_{\text{meeting band}}$$

If forward and reverse plateaus are symmetric ($K_p \approx K_r - K_p \approx 178$ rows from
each end), then $D_{\text{forward}} \approx D_{\text{reverse}}$ and $D_{\text{meeting band}}$
is the residual — ideally small due to heavy over-constraint from both sides. The exponential
backtracking cost, dominated by $2^D$, reduces to roughly $2^{D/2}$ if the meeting band mostly
propagates. This is not a polynomial improvement, but the exponent is halved.

#### B.30.5 Boundary State Compatibility

In the refined single-store architecture, compatibility between the forward and reverse passes is
**enforced implicitly** by the shared ConstraintStore. The bottom-up DFS operates on the same
residual budgets the forward pass left behind; it cannot over-commit a line budget because the
ConstraintStore's feasibility check ($0 \leq \rho(L) \leq u(L)$) is evaluated on every
assignment, exactly as in the forward DFS.

An incompatible bottom-up partial assignment (one that would require a meeting-band residual
infeasible given the remaining unknowns) is detected by the existing infeasibility check and
triggers backtracking within the bottom-up phase — no separate compatibility verification step
is needed.

This is a significant simplification over the original B.30 design, which required an explicit
compatibility check between separately computed forward and reverse residual vectors. The
single-store model inherits the ConstraintStore's existing invariant machinery for free.

#### B.30.6 The DI Ordering Challenge

DI indexes the $\text{DI}$-th valid solution in lexicographic (row-major) order. Lexicographic
order is strictly top-to-bottom: solution $A < B$ iff row 0 of $A$ is lexicographically smaller
than row 0 of $B$, breaking ties by row 1, and so on. This order is well-defined for the
forward wavefront but not for the reverse.

For a given forward partial $T_i$ (rows 0 to $K_p - 1$), the set of complete solutions
consistent with $T_i$ forms a contiguous lexicographic block. The DI-th complete solution
either falls in this block or doesn't, and DI is determined by:

$$\text{DI} = \sum_{T_j <_{\text{lex}} T_i} |\text{completions}(T_j)| + \text{rank within } T_i$$

Counting $|\text{completions}(T_j)|$ for each prior forward partial is the hard subproblem.
The current solver avoids this by never needing to count — it enumerates in lex order and stops
at the DI-th solution. A Pincer implementation must either:

- **Enumerate sequentially**: for each forward partial in lex order, enumerate compatible
  (reverse, meeting-band) completions in lex order, stop at DI. This is correct but forfeits
  the meet-in-the-middle speedup if DI is large.
- **Count approximately**: use sampling or bounding to estimate the number of completions per
  forward partial, seek directly to the block containing DI. Incorrect estimates require
  re-enumeration.
- **Restrict to DI = 0**: for the first valid solution only, the reverse search finds the
  lex-smallest compatible reverse partial for each forward partial, which is well-defined and
  avoids counting. This is a tractable special case.

In practice, DI is a single byte (0–255) and is usually small. The lex-order enumeration of
completions within a forward partial block is likely fast in practice, making the DI challenge
manageable even without an exact counter.

#### B.30.7 Proposed Implementation

The Pincer approach extends the existing DFS loop with a direction state and a plateau detector.
No additional ConstraintStores are required.

**State additions to the DFS loop:**
- `direction ∈ {TOP_DOWN, BOTTOM_UP}` — current traversal direction
- `top_frontier: int` — lowest row fully assigned by the top-down pass
- `bottom_frontier: int` — highest row fully assigned by the bottom-up pass
- `stall_count: int` — consecutive branching decisions without a propagation event

**Plateau detection:** after each branching decision (cell assigned by choice, not by forcing),
increment `stall_count`. If `stall_count` reaches the threshold N (candidate: 511), declare a
plateau and switch direction. Reset `stall_count` to 0. If both directions have stalled without
any new forced cells since the last direction switch, the meeting band is irreducibly hard and
the solver falls back to standard DFS within the meeting band.

**Top-down DFS** (existing behavior, direction = TOP_DOWN):
- Select next cell in row-major order from `top_frontier`
- Run `PropagationEngine`; if forced, continue; else branch (0 then 1)
- On row completion verify SHA-1; on failure backtrack
- On plateau: record `top_frontier = K_p`; switch direction to BOTTOM_UP

**Bottom-up DFS** (new, direction = BOTTOM_UP):
- Select next cell in reverse row-major order from `bottom_frontier`
- Same PropagationEngine, same ConstraintStore — no initialization needed
- On row completion verify SHA-1 in reverse order (row 510, 509, …); on failure backtrack
- On plateau: record `bottom_frontier = K_r`; switch direction to TOP_DOWN

**Top-down resume** (direction = TOP_DOWN, from `top_frontier`):
- Re-run propagation from `top_frontier` — cells in the meeting band (rows $K_p$ to $K_r$)
  now see tighter constraints from the bottom-up assignments
- Any newly forced meeting-band cells are assigned without branching
- Continue DFS on remaining underdetermined meeting-band cells; plateau again triggers another
  switch to BOTTOM_UP

**Termination:** when `top_frontier == bottom_frontier` (or they cross), all rows are assigned.
Verify SHA-256 block hash; if valid this is a solution. Count or advance DI as needed.

**Backtracking across direction switches:** the undo stack is unified. A backtrack in the
top-down resume phase may unwind past the direction-switch point, un-assigning cells from the
bottom-up phase. This is correct: the ConstraintStore invariant is maintained by the existing
assign/unassign machinery regardless of which direction made each assignment.

**DI enumeration:** the canonical cell order for DI purposes is row-major over all 261,121
cells. Within a given total assignment, the DI-th valid solution in this order is produced by
enumerating forward (0 before 1) within the top-down phase, forward within the meeting band,
and forward within the bottom-up phase. The DI value from compression is computed using the
same alternating-direction enumeration — compress and decompress use identical direction-switch
logic so the cell ordering is consistent.

#### B.30.8 Expected Outcomes

**Optimistic.** The bottom-up pass achieves a symmetric propagation cascade to $K_r \approx 332$
(~171 rows from the bottom). When the top-down pass resumes at $K_p$, the tightened constraints
force a large fraction of the meeting band (~154 rows, ~79K cells) without branching. The
iterative alternation converges in 2–3 direction switches. Effective backtracking space falls
from ~170K cells to ~5–20K. Wall-clock decompression time decreases by 1–2 orders of magnitude.

**Likely.** The bottom-up plateau is shallower than the forward plateau (SHA-1 row-major bit
order is not symmetric top-to-bottom; LTP Fisher-Yates partition is spatially symmetric but
row-hash verification is not). The top-down resume forces some meeting-band cells but not most.
Each successive direction switch provides diminishing returns; the solver terminates with a
modestly reduced meeting band and requires some residual backtracking. Net speedup is real but
less dramatic — perhaps 2–5× reduction in wall time rather than an order of magnitude.

**Pessimistic.** The residual constraint budgets after the forward pass are poorly distributed
for the bottom-up direction: many lines have near-zero residuals (heavily consumed by the top-down
pass) that over-constrain the bottom rows, causing frequent SHA-1 failures immediately. The
bottom-up plateau arrives within a few rows of row 510. The meeting band shrinks by a small
constant (10–20 rows) and the per-direction alternation overhead exceeds the constraint-tightening
benefit. The solver degrades to essentially the existing top-down DFS with added direction-switch
overhead.

#### B.30.9 Relationship to Prior Work

*B.26c / B.27 seed optimization.* Seed optimization extends the **forward** propagation cascade.
Pincer is orthogonal: it adds a **reverse** cascade using the same seed-optimized partition.
The two approaches are independently beneficial and fully composable. An optimized forward seed
may not be optimal for the reverse pass — a Pincer-specific seed search (optimizing both
forward and reverse plateau depth jointly) is a new research direction.

*B.22 variable-length LTP.* B.22 created short LTP lines to produce earlier forcing events.
Short lines benefit the meeting-band Phase 4 solve disproportionately: a line with only 20
cells in the meeting band reaches $u = 1$ (forcing) after just 19 of its meeting-band cells are
assigned by propagation from the two sides. This synergy makes variable-length LTP a natural
complement to Pincer even though B.22 regressed depth in isolation.

*DAG reduction.* As discussed in the B.30 motivation, a full DAG (zero backtracking) would
require the combined constraints to uniquely determine every cell. Pincer is a step toward this:
it increases the constraint density at the meeting frontier, potentially pushing more cells into
the forced regime. With sufficiently many sub-tables and well-optimized seeds, the meeting band
may approach zero width — effectively a full DAG solution reachable through two convergent
propagation cascades.

#### B.30.10 Open Questions

(a) **How deep does the bottom-up cascade reach?** The forward plateau is well-characterized at
$K_p \approx 178$ rows. The bottom-up plateau $K_r$ is unverified; the symmetry hypothesis
(~332 rows from the bottom, matching the forward depth) is plausible because the LTP partition
is row-symmetric under Fisher-Yates, but SHA-1 is row-major so the verification environment is
not perfectly symmetric. Empirical measurement of $K_r$ is the first and cheapest validation
step — implement bottom-up DFS only, measure how far it reaches before stalling.

(b) **How many direction switches are needed before convergence?** If the first top-down resume
forces nearly all meeting-band cells, one switch is sufficient. If the meeting band remains
stubbornly underdetermined after several switches, the alternation may not terminate efficiently.
The stall threshold N and maximum switch count are implementation parameters that require tuning.

(c) **Does the bottom-up pass degrade the forward solution quality?** If the bottom-up DFS makes
incorrect branching decisions (that will eventually require backtracking), the ConstraintStore
state when the top-down pass resumes may be inconsistent with the correct solution, leading to
SHA-1 failures in the meeting band. The backtracking machinery handles this correctly, but the
effective "shaking loose" benefit depends on the bottom-up decisions being mostly right. A high
hash-mismatch rate in the bottom-up pass would indicate the residual budgets are already tight
enough that backtracking dominates.

(d) **Can seeds be jointly optimized for forward and reverse plateau depth simultaneously?**
A seed that maximizes the forward cascade may harm the reverse cascade if its partition
concentrates early-row cells on lines that then have over-tight residuals for the bottom-up
pass. The combined objective $\text{depth}_{\text{fwd}}(s) + \text{depth}_{\text{rev}}(s)$
is a new search problem that the B.26c seed-search infrastructure can address once B.30 is
implemented.

(e) **Is the stall threshold N the right plateau trigger?** An alternative trigger is
*propagation rate*: if fewer than M cells per K branch decisions are being forced (rather than
branched), switch direction. This is more sensitive to the onset of underdetermination than a
fixed count.

#### B.30.11 Experimental Results (ABANDONED)

B.30 Option A was implemented in `RowDecomposedController_enumerateSolutionsLex.cpp` and
tested on the reference input (`useless-machine.mp4`, B.26c seeds CRSCLTPV+CRSCLTPP).

**Implementation:** On each `StallDetector` escalation (after the existing B.12 BP checkpoint),
call `propagator_->propagate(allLines)` — where `allLines` is the full set of 5,108 constraint
lines built at solver initialization — and record any newly forced cells on the undo stack.
Telemetry counters `b30_sweep_count` and `b30_sweep_forced` track execution.

**Outcome:**

| Sweep | b30\_sweep\_count | b30\_sweep\_forced | bp\_depth |
|:-----:|:-----------------:|:------------------:|:---------:|
| 1     | 1                 | **0**              | 91,090    |
| 2     | 2                 | **0**              | 91,086    |
| 3     | 3                 | **0**              | 91,090    |

`b30_sweep_forced = 0` in every sweep. Peak depth is unchanged at 91,090 — identical to the
B.26c baseline. No cells were forced by the bottom-up propagation sweep that had not already
been forced by the existing per-assignment propagation.

**Root cause confirmed.** The `PropagationEngine` is a complete fixed-point computation: it
forces every cell derivable from the current partial assignment after every branching decision.
A full re-propagation from all 5,108 lines finds no additional forced cells because the
constraint network is already at its propagation closure. This is the theoretically expected
result: a complete fixed-point propagation engine cannot be improved by re-running itself from
the same state.

**Conclusion:** Option A (propagation sweep) provides zero benefit. The constraint network is
fully exploited by the existing per-assignment propagation. The only mechanism that can extract
new information from the bottom of the matrix is **actual branching** in the reverse direction:
committing to specific cell values in bottom rows allows SHA-1 hash verification to eliminate
infeasible subtrees — a non-linear global constraint that cardinality propagation cannot express.
This is Option B (B.31).

**Abandonment verdict:** B.30 is abandoned. B.31 is the correct implementation of the Pincer
hypothesis.

---

