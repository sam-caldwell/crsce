### B.8 Adaptive Lookahead

Sections B.6 and B.7 propose stronger consistency and deeper lookahead as independent techniques. This
appendix proposes a unified *adaptive lookahead* strategy that begins with no lookahead ($k = 0$) and
escalates the depth dynamically in response to search stalling, up to a maximum of $k = 4$ using full
exhaustive lookahead at every depth. The motivation is twofold. First, the early rows of the matrix are
heavily constrained by cross-sum residuals and require no lookahead; paying for it there wastes
throughput. Second, the middle rows (approximately rows 100-300) enter a combinatorial phase transition
where cardinality forcing alone produces negligible propagation, and the solver needs stronger pruning
to sustain forward progress. An adaptive strategy applies each level of cost precisely when the solver
needs it.

The cap at $k = 4$ is deliberate. Exhaustive lookahead at depth $k$ explores $2^k$ probe paths per
branching decision, giving it the strong pruning guarantee that an assignment is marked doomed only when
*all* continuations fail. At $k = 4$, this costs 16 probes per decision&mdash;roughly 32-80 $\mu$s at
2-5 $\mu$s per probe&mdash;keeping throughput in the 12K-30K iter/sec range. Beyond $k = 4$, exhaustive
lookahead becomes prohibitively expensive ($k = 5$ requires 32 probes, $k = 6$ requires 64), and
approximations such as linear-chain sampling sacrifice the completeness guarantee that makes lookahead
effective (see B.8.7). Each probe propagates through all 8 constraint lines, so $k = 4$ explores up to
$4 \times 8 = 32$ constraint-line interactions per path&mdash;a detection radius sufficient to catch
cardinality contradictions that span multiple lines without requiring a row-boundary SHA-1 check.

#### B.8.1 Stall Detection

The solver maintains a sliding window of the last $W$ branching decisions (e.g., $W = 10{,}000$). At
each decision, it records the *net depth advance*: +1 for a forward assignment, $-d$ for a backtrack
that unwinds $d$ levels. The *stall metric* $\sigma$ is the ratio of net depth advance to window size:

$$\sigma = \frac{\Delta_{\text{depth}}}{W}$$

When $\sigma > 0$, the solver is making forward progress. When $\sigma \leq 0$, the solver is stalled&mdash;
backtracking at least as often as it advances. The stall metric is cheap to maintain (one counter
and one circular buffer) and requires no tuning beyond the window size $W$.

#### B.8.2 Escalation and De-Escalation Policy

The adaptive strategy adjusts the lookahead depth $k \in \{0, 1, 2, 3, 4\}$ using two thresholds on
the stall metric: a stall threshold $\sigma^{-} \leq 0$ that triggers escalation and a recovery
threshold $\sigma^{+} > 0$ that permits de-escalation.

*Escalation.* When $\sigma \leq \sigma^{-}$ and $k < 4$, the solver increments $k$ by one. Each
escalation level corresponds to a well-defined capability:

- $k = 0$: cardinality forcing only (current `PropagationEngine` fast path). Maximum throughput
  (~510K iter/sec). No per-decision overhead beyond propagation.

- $k = 1$: 1-level failed-literal probing (existing `FailedLiteralProber::probeAlternate`). Two probes
  per decision (test both values for the alternate branch). Throughput ~250K iter/sec. This is the
  current production behavior of `RowDecomposedController`.

- $k = 2$: exhaustive 2-level lookahead. Four probes per decision ($2^2$). After tentatively assigning
  the alternate value and propagating, the solver probes both values for the next unassigned cell. An
  assignment is doomed if all four leaf states are infeasible. Throughput ~125K iter/sec.

- $k = 3$: exhaustive 3-level lookahead. Eight probes per decision ($2^3$). Extends the lookahead tree
  one level deeper. Throughput ~60K iter/sec.

- $k = 4$: exhaustive 4-level lookahead. Sixteen probes per decision ($2^4$). Each probe propagates
  through 8 constraint lines, so the full tree touches up to 128 line interactions. Throughput
  ~12K-30K iter/sec.

*De-escalation.* When $\sigma > \sigma^{+}$ for a sustained period of at least $2W$ decisions and
$k > 0$, the solver decrements $k$ by one. The sustained-period requirement provides hysteresis,
preventing oscillation between depths. De-escalation is essential because the CRSCE constraint landscape
is not monotonically harder with depth: the late rows (approximately 300-511) have small unknown counts
per line, causing cardinality forcing to become aggressive again. A solver locked at $k = 4$ in this
region pays a 17-42× throughput penalty for pruning it no longer needs.

The hysteresis mechanism works as follows. The solver maintains a *recovery counter* $R$ that
increments when $\sigma > \sigma^{+}$ and resets to zero otherwise. When $R \geq 2W$, the solver
decrements $k$ by one and resets $R$. This ensures de-escalation occurs only after sustained forward
progress, not on transient fluctuations.

#### B.8.3 Exhaustive Lookahead at Depth $k$

At each branching decision for cell $(r, c)$, the solver applies the B.7.3 algorithm at the current
depth $k$. For completeness, the procedure specialized to CRSCE:

1. **Base case ($k = 0$).** No lookahead. Assign and propagate; accept the result.

2. **$k \geq 1$.** Before committing to the branching value (0, per canonical order), probe the
   alternate value (1) at depth $k$:

   (a) Tentatively assign $x_{r,c} = 1$. Propagate to fixpoint. If immediately infeasible, the
       alternate branch is pruned (equivalent to $k = 1$). Undo and proceed with 0.

   (b) If feasible at depth 1, select the next unassigned cell $(r', c')$ in row-major order and
       recursively probe both values at depth $k - 1$.

   (c) If all $2^{k-1}$ leaf states under $x_{r,c} = 1$ are infeasible, the alternate branch is
       pruned at depth $k$. Undo and proceed with 0.

   (d) Otherwise, the alternate branch is viable. Undo, assign $x_{r,c} = 0$ (canonical order), and
       continue the DFS.

The recursive structure naturally extends the existing `FailedLiteralProber`&mdash;the $k = 1$ case is
exactly `probeAlternate`, and each additional level wraps another layer of tentative-assign-propagate-
undo around it.

#### B.8.4 Expected Behavior Profile

The adaptive strategy partitions the solve into distinct phases:

*Rows 0-100 ($k = 0$).* Cross-sum residuals are large and SHA-1 row-hash checks at each row boundary
provide strong pruning. The solver operates at maximum throughput (~510K iter/sec) with no lookahead
overhead. Depth advances rapidly through the first ~51,000 cells.

*Rows 100-170 ($k = 0 \to 1$).* As residuals shrink and the constraint landscape tightens, the solver
enters the current plateau zone. The stall detector triggers the first escalation to $k = 1$, enabling
failed-literal probing on alternate branches. Throughput drops to ~250K iter/sec but the additional
pruning may sustain forward progress through this region.

*Rows 170-300 ($k = 1 \to k_{\max} \leq 4$).* If $k = 1$ is insufficient (as current performance data
suggests), further stalls trigger incremental escalation to $k = 2$, then $k = 3$, then $k = 4$. Each
increment doubles the per-decision probe count, and the solver self-tunes to the minimum $k$ that
sustains forward progress. At $k = 4$, the 16-probe exhaustive tree explores up to 128 constraint-line
interactions per decision, substantially expanding the detection radius for cardinality contradictions.

*Rows 300-511 (de-escalation).* In the final rows, unknown counts per line are small and cardinality
forcing becomes aggressive again. The de-escalation mechanism detects sustained forward progress
($\sigma > \sigma^{+}$ for $2W$ decisions) and decrements $k$, recovering throughput. In the best case,
the solver returns to $k = 0$ for the final ~100 rows, running at full speed.

#### B.8.5 Implementation Considerations

The adaptive strategy composes cleanly with existing components. The stall detector is a lightweight
addition to the `EnumerationController` main loop (one counter update and one circular-buffer write per
decision). The exhaustive lookahead extends `FailedLiteralProber` with a depth parameter, recursively
calling `tryProbeValue` at each level. No changes to `ConstraintStore`, `PropagationEngine`, or
`IHashVerifier` are required.

The primary implementation concern is undo correctness. At lookahead depth $k$, the solver has up to
$k$ nested tentative assignments on the undo stack, each with its own propagation-forced cells. The
existing `BranchingController` undo stack supports this naturally&mdash;each tentative assignment pushes a
frame, and unwinding pops frames in reverse order. Care is needed to ensure that SHA-1 hash
verification is not triggered at intermediate lookahead levels: only the outermost probe should check
SHA-1 on completed rows, since intermediate levels will be undone regardless. A simple depth counter
passed to the hash verifier suffices.

The memory cost is negligible. The lookahead tree is explored depth-first, so at most $k = 4$ undo
frames are live simultaneously, each storing $O(s)$ forced-cell records in the worst case. The total
additional memory is $O(ks) = O(2{,}044)$ entries&mdash;under 20 KB.

#### B.8.6 Cost-Benefit Summary

| Depth $k$ | Probes/Decision | Approx. Throughput | Constraint-Line Reach | Pruning Guarantee |
|:----------:|:---------------:|:------------------:|:---------------------:|:-----------------:|
| 0          | 0               | 510K iter/sec      | 8 (propagation only)  | None              |
| 1          | 2               | 250K iter/sec      | 16                    | Exhaustive        |
| 2          | 4               | 125K iter/sec      | 32                    | Exhaustive        |
| 3          | 8               | 60K iter/sec       | 64                    | Exhaustive        |
| 4          | 16              | 12-30K iter/sec   | 128                   | Exhaustive        |

All depths maintain the full exhaustive pruning guarantee: an assignment is marked doomed only when
every continuation in the $2^k$ lookahead tree leads to a cardinality violation or hash mismatch. This
guarantee is what makes failed-literal probing ($k = 1$) effective in the existing solver, and the
adaptive strategy preserves it at every escalation level.

#### B.8.7 Open Questions

Consolidated into Appendix C.7.

