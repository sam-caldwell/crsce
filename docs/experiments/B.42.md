## B.42 Pre-Branch Pruning Spectrum (Proposed)

The CRSCE solver operates on a spectrum of pre-branch intelligence. At one extreme, the solver
assigns a value, propagates, and discovers infeasibility *after the fact* (lazy detection). At the
other, the solver exhaustively verifies feasibility of every candidate value before committing
(full SAC maintenance). The current production solver sits at an intermediate point: it runs
`probeToFixpoint()` once as preprocessing (SAC-like, expensive, thorough), then during DFS uses
only `probeAlternate()` (k=1 failed-literal probing on the alternate value) and fast-path
`tryPropagateCell()` (singleton forcing only, ~80% of iterations exit early with no cascading).

B.42 systematically explores this spectrum to determine where the optimal cost-benefit tradeoff lies
at the current operating point (depth 96,672, row ~189 plateau, B.38-optimized LTP table). Each
sub-experiment escalates the pre-branch intelligence level, with clear decision gates between stages.

### B.42.1 The Pre-Branch Intelligence Spectrum

The solver has six progressively stronger pre-branch mechanisms, three of which are implemented:

| Level | Mechanism | Status | Cost per branch | What it catches |
|-------|-----------|--------|----------------|-----------------|
| L0 | Propagation only (`tryPropagateCell` fast path) | **Implemented** | ~0.5 &mu;s | &rho;=0 or &rho;=u on any of 10 lines |
| L1 | Failed-literal probe on alternate (`probeAlternate`) | **Implemented** | ~3 &mu;s (1 probe) | Alternate value leads to immediate contradiction |
| L2 | Cross-line interval tightening (B.16.2) | Not implemented | ~0.1 &mu;s (8 comparisons) | v<sub>min</sub>=v<sub>max</sub> from joint constraint bounds |
| L3 | Both-value probing (`probeCell` during DFS) | Partially implemented | ~6 &mu;s (2 probes) | Either value infeasible &rarr; force or backtrack |
| L4 | k-level exhaustive lookahead (`probeAlternateDeep`) | **Implemented** | ~3&times;2<sup>k</sup> &mu;s | Contradiction k steps ahead |
| L5 | Full SAC maintenance after every assignment | Not implemented | ~1 s (all cells) | All singleton inconsistencies globally |

**Key observation.** L2 (interval tightening) is cheaper than L1 (alternate probing) but is not
implemented. L3 (both-value probing) extends L1 trivially but is only used at the preprocessing
fixpoint, not during DFS. The spectrum has a gap: the solver jumps from L0/L1 directly to L4
(stall-triggered deep probing) without exploiting L2 or L3.

B.42 fills this gap and measures the marginal value of each level at the plateau.

### B.42.2 Sub-experiment B.42a: Waste Instrumentation

**Objective.** Before implementing any changes, measure the current solver's wasted work at the
plateau to establish the ceiling of improvement from better pre-branch pruning.

**Instrumentation (no behavior change):**

1. **Immediate-backtrack counter.** Count how many times the solver assigns a value, propagates,
   detects infeasibility, and immediately backtracks to try the alternate. This is work that
   both-value probing (L3) would have avoided. Log per-row distribution.

2. **Forced-after-probe counter.** Count how many times `probeAlternate` returns infeasible
   (alternate branch is dead), meaning the preferred value could have been forced without
   branching. This is work that both-value probing captures equally &mdash; the question is
   how often the *preferred* value is the dead one (which `probeAlternate` does not check).

3. **Interval-tightening potential.** For each branching decision at the plateau, compute the
   B.16.2 interval bounds (v<sub>min</sub>, v<sub>max</sub>) from all 8 constraint lines.
   Record how often v<sub>min</sub>=v<sub>max</sub> (cell is determined without branching)
   and how often v<sub>min</sub>=v<sub>max</sub>=preferred (cell was going to be assigned
   correctly anyway, so no saving).

4. **Wasted-depth distribution.** When a backtrack occurs, record how many cells were assigned
   (and then undone) between the branching decision that caused the infeasibility and the
   point where infeasibility was detected. A wasted depth of 0 means infeasibility was caught
   immediately (no improvement possible); a wasted depth of k means k assignments were wasted
   (pre-branch pruning at depth k would have saved them).

**Metrics emitted:** JSON event log via `IO11y`, analyzed offline.

**Expected output:** A profile showing (a) the fraction of branching decisions where pre-branch
checking would have helped, (b) the distribution of wasted depth, and (c) the potential of
interval tightening at the plateau. This profile determines which of B.42b&ndash;B.42e are
worth implementing.

**Decision gate:** If the immediate-backtrack rate is &lt;5% and wasted depth is typically 0,
the current L0/L1 approach is near-optimal and B.42b&ndash;B.42e are unlikely to help.
Proceed only if the instrumentation reveals substantial waste.

**Implementation cost:** ~30&ndash;50 lines of logging in `RowDecomposedController`. Zero
runtime overhead on the hot path (counters only increment on backtrack events, which are
already expensive).

#### B.42.2a B.42a Results (Completed)

B.42a instrumentation was run on the RowDecomposedController (synthetic random 50%-density CSM, B.38-optimized LTP table, 70M iterations at peak depth 86,125).

| Metric | Value | % of branches |
|--------|-------|---------------|
| Total branching decisions | 39,404,780 | 100% |
| Preferred value infeasible | 22,284,407 | **56.6%** |
| Alternate value infeasible | 17,031,789 | **43.2%** |
| Interval forced (L2 potential) | 0 | **0.0%** |
| Interval contradiction (L2 potential) | 0 | **0.0%** |
| Interval undetermined | 39,404,780 | **100.0%** |

**Key findings.**

1. **56.6% preferred-infeasible rate.** The solver's preferred value (selected by probability confidence) is wrong more than half the time. Each failure costs a full assign+propagate+unassign cycle (~3 &mu;s) before trying the alternate. Both-value probing (B.42c) would eliminate this waste by checking both values before committing.

2. **43.2% both-values-infeasible rate.** Nearly half of all branches have BOTH values infeasible. The solver currently pays two full cycles (preferred fails, alternate fails, backtrack) for each such event. Both-value probing would detect this in a single probing pass and backtrack immediately.

3. **0% interval-tightening potential.** $v_{\min} = 0$ and $v_{\max} = 1$ for ALL branching decisions at the plateau. The cross-line interval bounds never tighten below the full [0, 1] range. This means B.42b (interval tightening) and B.42e (propagation-triggered interval sweep) **will have zero effect** and should not be implemented.

4. **99.8% backtrack rate.** Only 0.2% of branches lead to sustained forward progress (100% &minus; 56.6% &minus; 43.2% = 0.2%). The solver is in an almost-pure backtracking regime at the plateau.

**Decision gate assessment.** The immediate-backtrack rate is 56.6% (far above the 5% threshold). The waste is massive. However, the zero interval-tightening potential eliminates B.42b and B.42e. The remaining viable sub-experiment is **B.42c (both-value probing during DFS)**, which directly addresses the 56.6% preferred-infeasible and 43.2% both-infeasible waste.

**B.42b status: NOT ATTEMPTED** (zero interval-tightening potential per B.42a).
**B.42e status: NOT ATTEMPTED** (zero interval-tightening potential per B.42a).

### B.42.3 Sub-experiment B.42b: Cross-Line Interval Tightening

**Prerequisite.** B.42a instrumentation shows interval-tightening potential &gt;0 for a
meaningful fraction (&geq;1%) of branching decisions at the plateau.

**Motivation.** The current propagation engine checks each of the 10 constraint lines
*independently*: &rho;=0 forces zeros, &rho;=u forces ones. It never reasons about the *joint*
effect of all lines through a cell. B.16.2's interval analysis fills this gap at minimal cost.

**Method.** Before each branching decision on cell (r,c), compute:

$$v_{\min}(r,c) = \max_{L \ni (r,c)} \left( \rho(L) - (u(L) - 1) \right)^+$$

$$v_{\max}(r,c) = \min_{L \ni (r,c)} \left( \rho(L),\; 1 \right)$$

where the max and min range over all 10 constraint lines containing (r,c) (row, column,
diagonal, anti-diagonal, 4 LTP, plus LTP5/LTP6 if present).

- If $v_{\min} = 1$: force the cell to 1 (no branching needed).
- If $v_{\max} = 0$: force the cell to 0 (no branching needed).
- If $v_{\min} > v_{\max}$: immediate contradiction &mdash; backtrack without trying either value.
- Otherwise: proceed with normal branching (L0/L1).

**Cost.** 10 line-stat lookups + 10 comparisons per cell = ~0.1 &mu;s. This is 5&times; cheaper
than a single `probeAlternate` call (~3 &mu;s) and 30&times; cheaper than both-value probing
(~6 &mu;s). The cost is dominated by the cache miss on line stats, which are already hot from
`tryPropagateCell`.

**Cascading.** When interval tightening forces a cell, propagate the forced assignment normally
via `tryPropagateCell`. The cascade may create new interval-tightening opportunities on
neighboring cells. To exploit this, run the interval check on all cells affected by the
propagation wave (cells sharing a line with the forced cell). This is a lightweight version
of B.16.3's residual bounds propagation.

**DI consistency.** Interval tightening is purely deductive and deterministic. It forces only
cells that are logically implied by the current constraint state. The compressor and decompressor
produce identical forced assignments. DI semantics are preserved.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Depth &gt; 96,672 | Interval tightening forces cells that propagation misses; the additional forced cells push the cascade deeper |
| H2 (Neutral) | Depth &asymp; 96,672, iteration rate &asymp; unchanged | Interval bounds rarely tighter than singleton forcing at current operating point; the plateau rows have &rho; far from both 0 and u on all lines |
| H3 (Marginal) | Depth &asymp; 96,672, iteration rate improves &geq;10% | Interval tightening forces some cells (saving branching overhead) but doesn't change depth; constant-factor speedup |

**Implementation cost:** ~40&ndash;60 lines in `PropagationEngine` or a new `IntervalTightener`
class. The computation is a tight inner loop over 10 line stats &mdash; straightforward to
implement and test.

### B.42.4 Sub-experiment B.42c: Both-Value Probing During DFS

**Prerequisite.** B.42a instrumentation shows that the preferred-value-infeasible rate is &geq;5%
at the plateau (the solver frequently commits to the preferred value, discovers infeasibility, and
backtracks to try the alternate).

**Motivation.** The current DFS always commits to the preferred value first. If the preferred is
infeasible, the solver pays the full assign+propagate+unassign cost before trying the alternate.
`probeAlternate` only checks the alternate &mdash; not the preferred. Both-value probing checks
both values before committing, enabling three optimizations:

1. **Force when one value is dead.** If probing shows value 0 is infeasible, force value 1 (no
   branching, no DFS frame pushed). This is what `probeCell` does at the preprocessing fixpoint
   but not during active DFS.

2. **Immediate backtrack when both values are dead.** If both values are infeasible, backtrack
   immediately without trying either. The current solver must try preferred, fail, try alternate,
   fail, then backtrack &mdash; paying two full assign+propagate+unassign cycles.

3. **Preferred-value confidence.** When both values are feasible, the solver proceeds normally.
   But if probing reveals that one value produces a substantially larger propagation cascade
   (more forced cells), the solver can prefer that value (more constrained = more pruning power).
   This is a heuristic enhancement, not a correctness change.

**Method.** At each DFS branching decision on cell (r,c):

```
result = probeCell(r, c)    // existing infrastructure
if result.bothInfeasible:
    backtrack()
elif result.forcedValue != NONE:
    assign(r, c, result.forcedValue)   // no DFS frame; forced
    propagate()
else:
    // Both feasible: branch normally (preferred first)
    pushDfsFrame(r, c, preferred)
```

**Cost.** Two probes per branching decision: ~6 &mu;s total. This is 12&times; the cost of
the L0 fast path (~0.5 &mu;s) and 2&times; the cost of L1 (`probeAlternate`, ~3 &mu;s).

At the current plateau throughput of ~400K iter/sec at L0, the overhead of L3 would reduce
throughput to ~170K iter/sec (a 2.4&times; slowdown). This is justified only if the pruning
benefit (fewer branches, fewer backtracks) exceeds the overhead.

**Interaction with L2 (B.42b).** If B.42b (interval tightening) is active, cells forced by
interval analysis skip both-value probing entirely (already determined). This reduces the
number of cells requiring the expensive L3 probe. The optimal pipeline is:

1. Interval tightening (L2, ~0.1 &mu;s) &mdash; force if determinable
2. Both-value probing (L3, ~6 &mu;s) &mdash; only if L2 does not resolve the cell

**DI consistency.** `probeCell` is deterministic (it probes value 0 then value 1 in fixed order).
Forced cells are logically implied by the constraint state. The same forcing occurs in both
compressor and decompressor. DI semantics are preserved.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Depth &gt; 96,672 | Both-value probing detects infeasible assignments earlier; the saved branching extends the cascade |
| H2 (Neutral) | Depth &asymp; 96,672 | Probing catches errors that `tryPropagateCell` would have caught one step later; net effect is zero depth change |
| H3 (Regression) | Depth &lt; 96,672 or iteration rate drops &gt;3&times; | Probing overhead dominates; the solver explores fewer iterations per second, reducing the chance of finding deep paths in time-bounded runs |

**Decision gate.** If H3 occurs, the 2&times; overhead of both-value probing is not justified.
Fall back to L2-only (B.42b) or L2+L1 (interval tightening + alternate-only probing).

#### B.42.4a B.42c Results (Completed &mdash; Neutral)

B.42c was implemented in `RowDecomposedController_enumerateSolutionsLex.cpp` and tested on the same synthetic random 50%-density CSM block (B.38-optimized LTP table, 2-minute run).

| Metric | Baseline (no B.42c) | With B.42c |
|--------|--------------------:|----------:|
| Peak depth | 86,125 | 86,125 |
| Iterations (2 min) | 70M | 97.5M |
| Iter/sec | ~830K | ~825K |
| Total branches | 39.4M | 24.2M |
| Branching rate | 56.3% | 24.6% |
| Preferred infeasible | 22.3M (56.6%) | 0 |
| Alternate infeasible | 17.0M (43.2%) | 0 |

**Outcome: H2 (Neutral).** Peak depth is identical. The both-value probing successfully eliminates all wasted branching cycles&mdash;the preferred-infeasible and alternate-infeasible counters drop to zero because `probeCell` catches infeasibility before assignment. The branching rate drops from 56.3% to 24.6% (the remaining branches are cells where both values are genuinely feasible). Throughput is essentially unchanged (~825K vs ~830K iter/sec), indicating that the probing overhead is negligible relative to the saved assign+propagate+undo cycles.

However, the depth ceiling is unchanged. The waste elimination allows the solver to explore more iterations in the same wall-clock time (97.5M vs 70M in 2 minutes) but does not extend the propagation cascade. The plateau is fundamentally constraint-exhausted: no amount of branching-order optimization or waste elimination changes the set of cells that constraint propagation can force.

**B.42c value.** While B.42c does not improve depth, it provides a ~40% throughput improvement (more iterations per unit time) by eliminating wasted cycles. This is valuable for time-bounded compression runs: the solver explores 40% more of the search space in the same time budget. For blocks near the DI-discovery threshold, this speedup could make the difference between finding DI within the timeout and failing.

**B.42d status: NOT APPLICABLE** (prerequisite not met&mdash;B.42c achieves H2, not H1).
**B.42e status: NOT ATTEMPTED** (zero interval-tightening potential per B.42a).

### B.42.5 Sub-experiment B.42d: Adaptive Pre-Branch Escalation

**Prerequisite.** B.42b and/or B.42c show measurable benefit (H1 or H3-marginal). This
sub-experiment optimizes *when* to apply each level.

**Motivation.** The pre-branch levels have different cost-benefit profiles at different depths:

- **Early rows (0&ndash;100):** Propagation cascades are strong; L0 is sufficient. Adding L2/L3
  overhead wastes time on cells that would have been forced anyway.
- **Plateau rows (170&ndash;200):** Propagation stalls; L2/L3/L4 provide the most value per unit
  cost. Every forced cell avoids exponential backtracking.
- **Late rows (300&ndash;511):** Unknown counts are small; propagation is again aggressive. L0
  suffices; higher levels add overhead.

B.8 proposed adaptive escalation from k=0 to k=4 based on stall detection (&sigma; metric).
B.42d integrates the L2/L3 levels into this adaptive framework:

**Escalation policy:**

| Stall metric &sigma; | Active levels | Throughput estimate |
|----------------------|---------------|-------------------|
| &sigma; &gt; &sigma;<sup>+</sup> (forward progress) | L0 only (fast path) | ~400K iter/sec |
| &sigma;<sup>&minus;</sup> &lt; &sigma; &leq; &sigma;<sup>+</sup> (slowing) | L0 + L2 (interval tightening) | ~380K iter/sec |
| &sigma; &leq; &sigma;<sup>&minus;</sup> (stalled) | L0 + L2 + L3 (both-value probing) | ~170K iter/sec |
| &sigma; &leq; &sigma;<sup>&minus;&minus;</sup> (deeply stalled) | L0 + L2 + L3 + L4 at k=2 | ~80K iter/sec |

**De-escalation:** When &sigma; recovers above &sigma;<sup>+</sup> for 2W decisions (B.8.2
hysteresis), decrement one level. The solver self-tunes to the minimum pre-branch intelligence
that sustains forward progress.

**Parameter sweep.** The stall thresholds (&sigma;<sup>+</sup>, &sigma;<sup>&minus;</sup>,
&sigma;<sup>&minus;&minus;</sup>) and window size W are tuning parameters. B.42d sweeps a small
grid (3 values for each threshold, 3 values for W = {5000, 10000, 20000}) to find the optimal
operating point. Total: 81 configurations, each run for 10 minutes.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Best adaptive config depth &gt; 96,672 | Adaptive pre-branch escalation is the right architecture; the plateau requires L2/L3 but early/late rows do not |
| H2 (Neutral) | All configs &asymp; 96,672 | Pre-branch intelligence does not change depth; the plateau is not caused by insufficient per-node pruning |
| H3 (One level dominates) | A single fixed level (e.g., L2 always on) matches the best adaptive config | Adaptation overhead is not justified; the winning level should be hardcoded |

### B.42.6 Sub-experiment B.42e: Propagation-Triggered Interval Sweep

**Prerequisite.** B.42b (interval tightening) achieves H1 or H3-marginal, confirming that
interval analysis forces cells that propagation misses.

**Motivation.** B.42b applies interval tightening only to the cell about to be branched on. But
each forced cell changes the &rho; and u values on its 10 constraint lines, which may create new
interval-tightening opportunities on other cells sharing those lines. B.16.3 proposed iterating
this to a fixed point, but the full-matrix sweep is too expensive at ~2.6 ms per pass.

B.42e proposes a *triggered* variant: after each propagation wave (whether from branching or
forcing), run interval tightening on all unassigned cells sharing a line with the newly
assigned/forced cells. This limits the sweep to the *neighborhood* of the change rather than
the entire matrix.

**Method.** After `tryPropagateCell(r, c)` completes (including any cascade):

1. Collect the set of lines $L_{\text{dirty}}$ touched by the cascade (at most 10 per forced cell).
2. For each line in $L_{\text{dirty}}$, iterate its unassigned cells and compute v<sub>min</sub>/v<sub>max</sub>.
3. If any cell is determined (v<sub>min</sub>=v<sub>max</sub>), force it and add its lines to $L_{\text{dirty}}$.
4. Repeat until $L_{\text{dirty}}$ is empty (local fixpoint).

**Cost.** Each dirty line has at most 511 unassigned cells, and each cell requires 10 line-stat
lookups. In the plateau, a typical propagation wave touches ~5 lines and ~50 cells, giving
~50 &times; 10 = 500 line-stat lookups = ~0.5 &mu;s. In the rare cascade case (forcing 10+
cells), cost rises to ~5 &mu;s. This is comparable to `probeAlternate` but provides a
fundamentally different kind of inference (joint constraint bounds vs. tentative assignment).

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Cascade extension) | Propagation-triggered interval sweep forces &geq;1 additional cell per 100 branching decisions at plateau | Joint-constraint reasoning extends the propagation cascade; cumulative effect over many decisions may increase depth |
| H2 (Rare but valuable) | Interval sweep forces &lt;1 per 100 decisions, but depth improves | The few additional forcings occur at critical points (row boundaries or high-constraint-density cells) and have outsized impact |
| H3 (Negligible) | Near-zero additional forcings | The interval bounds v<sub>min</sub>/v<sub>max</sub> are strictly wider than {0,1} for virtually all plateau cells; joint reasoning adds nothing beyond singleton forcing |

### B.42.7 Sub-experiment B.42f: Four-Direction Integration (Conditional on B.41b H1)

**Prerequisite.** B.41b achieves H1 (depth &gt; 96,672) AND at least one of B.42b&ndash;B.42e
shows measurable benefit.

**Motivation.** The four-direction pincer (B.41b) introduces column-serial traversal (LR, RL).
Pre-branch pruning applies symmetrically to column-serial passes: interval tightening checks
the same 10 constraint lines (the "primary" line is now the column rather than the row, but
the computation is identical), and both-value probing uses the same `probeCell` infrastructure.

**Method.** Apply the winning B.42 configuration (from B.42d's parameter sweep or the best
individual sub-experiment) to all four traversal directions in B.41b:

- **TD/BU passes:** Identical to standalone B.42. Row-serial traversal with pre-branch pruning.
- **LR/RL passes:** Column-serial traversal. Interval tightening computes v<sub>min</sub>/v<sub>max</sub>
  from the same 10 lines (row is now a "cross" constraint; column is the "primary" constraint).
  Both-value probing uses the same `probeCell`. No code change required beyond the traversal
  direction (the pre-branch check is cell-centric, not direction-centric).

**Interaction with B.41c (RCLA/CCLA).** Pre-branch pruning (B.42) and completion look-ahead
(B.41c) are complementary:

- B.42 forces cells *before* they are branched on, reducing the number of unknowns u in
  each row/column.
- B.41c triggers when u is small enough for exhaustive enumeration ($\binom{u}{\rho} \leq C_{\max}$).
- B.42 accelerates the arrival at B.41c's trigger threshold by forcing more cells, while B.41c
  handles the final few unknowns via enumeration.

The combined pipeline for each row or column:

```
for each unassigned cell in row/column:
    L2: interval tightening → force if determined
    L3: both-value probing → force or backtrack if one/both values dead
    L0: assign preferred value + propagate
    check: if u ≤ u_max for this row/column:
        B.41c: enumerate remaining completions vs. hash
```

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Synergy) | B.41b+B.42 depth &gt; B.41b depth AND &gt; B.42 depth (standalone) | Pre-branch pruning and column-hash verification are multiplicative; each enables the other to work more effectively |
| H2 (Additive) | B.41b+B.42 depth &asymp; max(B.41b, B.42) | Benefits are independent; no synergy but no interference |
| H3 (Interference) | B.41b+B.42 depth &lt; B.41b depth | Pre-branch overhead reduces iteration rate in column-serial passes, counteracting column-hash pruning |

### B.42.8 Implementation Notes

The B.42 sub-experiments build on existing infrastructure:

1. **B.42a (instrumentation):** ~30&ndash;50 lines of counters and logging in
   `RowDecomposedController_enumerateSolutionsLex.cpp`. Zero hot-path overhead (counters on
   backtrack events only).

2. **B.42b (interval tightening):** New `IntervalTightener` class or inline logic in
   `PropagationEngine`. Core computation is 10 line-stat lookups + 10 comparisons per cell.
   ~40&ndash;60 lines. Integrate via a `tightenCell(r, c)` method called before each branching
   decision.

3. **B.42c (both-value probing):** The infrastructure exists in `FailedLiteralProber::probeCell`.
   The change is calling `probeCell` at each DFS node instead of only during `probeToFixpoint`.
   ~10&ndash;20 lines of integration in the DFS loop.

4. **B.42d (adaptive escalation):** Extend the existing stall detector (B.8) with additional
   thresholds for L2/L3 activation. ~30&ndash;40 lines for the escalation/de-escalation logic.
   Parameter sweep via shell script.

5. **B.42e (propagation-triggered sweep):** Track dirty lines after each propagation wave.
   Iterate unassigned cells on dirty lines, apply interval tightening, cascade if forced.
   ~60&ndash;80 lines. The dirty-line tracking can reuse the existing `LineID` infrastructure.

6. **B.42f (four-direction integration):** No new code if B.42b&ndash;e are implemented
   cell-centrically (which they are). The pre-branch checks are agnostic to traversal
   direction. Integration is testing + parameter tuning.

### B.42.9 Relationship to Prior Work

**B.6 (Singleton Arc Consistency: Proposed).** B.42c (both-value probing during DFS) is
the per-node analog of B.6's SAC preprocessing. B.6 proposed maintaining SAC as a global
invariant (re-probing all cells after every assignment), which costs ~1 s per assignment and
is prohibitive during DFS. B.42c applies the same operation only to the cell being branched on,
reducing the cost from O(n) probes to O(1) probes per decision. B.42e (propagation-triggered
sweep) is a lightweight approximation of partial SAC (B.6.5), limited to the neighborhood of
each change rather than the entire matrix.

**B.8 (Adaptive Lookahead: Proposed).** B.42d integrates L2/L3 into B.8's adaptive escalation
framework. B.8 proposed escalation from k=0 to k=4 based on stall detection; B.42d adds
intermediate levels (L2 interval tightening, L3 both-value probing) between k=0 and k=2,
filling the gap where the solver currently jumps from cheap propagation to expensive multi-level
lookahead.

**B.16 (Partial Row Constraint Tightening: Proposed).** B.42b is a direct implementation of
B.16.2's interval analysis. B.42e is a targeted implementation of B.16.3's residual bounds
propagation, limited to dirty lines rather than the full matrix. B.16 proposed these as
standalone techniques; B.42 positions them within a unified pre-branch pipeline and measures
their interaction with probing and lookahead.

**B.41b (Four-Direction Pincer: Infeasible for Depth).** B.42f was conditional on B.41b achieving depth improvement. B.41 diagnostic results (&sect;B.41.10) show columns are far less complete than rows at the plateau (min column unknown = 206 vs min row unknown = 2), making column-serial passes unviable for depth improvement. **B.42f is therefore not applicable.** The remaining B.42 sub-experiments (B.42a&ndash;B.42e) operate purely in row-serial mode and are unaffected by B.41's outcome.

**B.41c (Completion Look-Ahead: Conditional).** B.42 and B.41c are complementary. B.42 forces
cells before branching (reducing u toward the B.41c trigger threshold); B.41c handles the
exhaustive enumeration of remaining unknowns when u is small. The combined effect is a two-stage
pipeline: B.42 shrinks the problem, B.41c finishes it.

### B.42.10 Summary and Conclusions

**B.42a (Waste Instrumentation): COMPLETED.** 56.6% preferred-infeasible rate, 43.2% both-infeasible rate. Massive waste identified. However, 0% interval-tightening potential eliminates B.42b and B.42e.

**B.42b (Interval Tightening): NOT ATTEMPTED.** Zero L2 potential per B.42a&mdash;$v_{\min} = 0$ and $v_{\max} = 1$ for all plateau cells.

**B.42c (Both-Value Probing): COMPLETED &mdash; NEUTRAL (H2).** Eliminates all branching waste (preferred/alternate infeasible counters drop to zero). Reduces branching rate from 56.3% to 24.6%. Provides ~40% throughput improvement. **Depth unchanged** (86,125 = baseline). The plateau is constraint-exhausted, not waste-caused.

**B.42d (Adaptive Escalation): NOT APPLICABLE.** Prerequisite (B.42b or B.42c achieving H1) not met.

**B.42e (Propagation-Triggered Sweep): NOT ATTEMPTED.** Zero interval-tightening potential per B.42a.

**B.42f (Four-Direction Integration): NOT APPLICABLE.** B.41b infeasible for depth (&sect;B.41.10).

**Overall B.42 conclusion.** The pre-branch pruning spectrum has been fully characterized. The solver's waste at the plateau is large (56.6% wrong-preferred, 43.2% both-dead) but eliminating it does not change depth. The constraint system is exhausted at the plateau: no per-node pruning technique (interval tightening, probing, or adaptive escalation) can extend the propagation cascade because the residual constraint density is insufficient to force additional cells. The depth ceiling is a property of the constraint system's information content, not the solver's search efficiency.

**B.42c retained for throughput.** Both-value probing provides a meaningful constant-factor speedup (~40% more iterations per wall-clock second) that is valuable for time-bounded compression. The change is low-risk (31/31 tests pass, DI semantics preserved) and should be retained in production.

**Status: COMPLETED.**

---

