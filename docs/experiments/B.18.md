### B.18 Learning from Repeated Hash Failures (Superceded by B.40)

B.14 proposes lightweight nogood recording: when an SHA-1 hash check fails, the solver records the failed row
assignment (or the decision-cell subset thereof) as a nogood to prevent re-entering the same configuration. This
appendix extends that idea in a different direction: rather than recording exact failed configurations, the solver
*statistically tracks* which (row, partial assignment) patterns are associated with repeated failures and uses this
information to *bias branching decisions* away from failure-prone patterns. The distinction from B.14 is that B.18
operates at a coarser granularity&mdash;it does not require exact matches to trigger, and it influences branching
rather than pruning.

#### B.18.1 Failure Frequency Tracking

The solver maintains, for each row $r$, a *failure counter* $F(r)$ that increments each time the SHA-1 hash check
fails on row $r$. Additionally, for each cell $(r, c)$ that was a *decision cell* (not forced by propagation) at
the time of a hash failure, the solver maintains per-cell failure counters $F(r, c, 0)$ and $F(r, c, 1)$,
recording how many times cell $(r, c)$ was assigned value 0 or 1 in a failed configuration.

After $N$ hash failures on row $r$, the solver has a statistical profile of which cells and values are most
frequently associated with failure. A cell $(r, c)$ with $F(r, c, 1) \gg F(r, c, 0)$ was assigned 1 in most
failed configurations, suggesting that assigning it to 0 might be more productive. The solver can use this profile
to *reorder the branching preference*: instead of always trying 0 first (canonical order), try the value with the
lower failure count first.

#### B.18.2 Failure-Biased Value Ordering

The standard CRSCE solver branches with 0 first (canonical lexicographic order). Failure-biased value ordering
modifies the branch-value preference for cells that have accumulated failure statistics. For cell $(r, c)$, define
the *failure bias*:

$$\beta(r, c) = F(r, c, 0) - F(r, c, 1)$$

If $\beta > 0$, value 0 has been associated with more failures, so the solver tries 1 first. If $\beta < 0$,
value 1 has been associated with more failures, so the solver tries 0 first (the default). If $\beta = 0$, no
bias is available, and the default order applies.

**This immediately raises a DI determinism concern.** Canonical enumeration requires 0-before-1 at every branching
decision to ensure that the compressor and decompressor traverse the search tree in identical order. Changing the
value ordering changes the traversal order, which changes the solution index and breaks DI semantics.

#### B.18.3 Preserving DI Determinism

The DI determinism constraint prohibits modifying the branch-value order. However, failure statistics can still
inform the search through three alternative mechanisms that preserve canonical ordering:

*Mechanism 1: Cell reordering within a row.* The solver's cell selection order within a row is not specified by the
DI contract (which requires only that solutions are enumerated in lexicographic order over the full matrix, meaning
row-major). Within a row, the solver is free to select cells in any order, provided that the same ordering is used
in both compressor and decompressor. If the failure statistics are derived from the search trajectory (which is
identical in both), the reordering is deterministic and DI-preserving.

Specifically, within each row, the solver can prioritize cells whose failure statistics indicate they are most
likely to cause a hash mismatch. Assigning these "dangerous" cells first causes conflicts to surface earlier in the
row, before the solver invests effort in assigning "safe" cells. This is analogous to the fail-first heuristic in
classical CSP solving (Haralick & Elliott, 1980).

*Mechanism 2: Row-level restart priority.* When the solver backtracks out of a row with high $F(r)$, it can
prioritize retrying that row with a different assignment before exploring other branches. This is a form of the
restart mechanism proposed in B.11, localized to a specific row. DI is preserved because the row-level restart is
triggered by the deterministic search trajectory.

*Mechanism 3: Interaction with B.10 (tightness-driven ordering).* The failure statistics can be folded into B.10's
tightness score $\theta(r, c)$ as an additional term:

$$\theta'(r, c) = \theta(r, c) + \alpha \cdot \max(F(r, c, 0), F(r, c, 1))$$

where $\alpha$ is a tuning parameter that controls the weight of failure history relative to constraint tightness.
Cells with high failure counts are prioritized for early assignment, surfacing conflicts sooner. Because $\theta'$
is computed identically in both compressor and decompressor, DI is preserved.

#### B.18.4 Distinguishing Failure Patterns

Not all hash failures are equally informative. A row that fails its hash on every attempt carries little
information (the partial assignment above the row may be fundamentally wrong). A row that fails intermittently&mdash;
passing its hash on some attempts but not others&mdash;provides the most useful signal: the failures are localized
to specific cell configurations within the row.

The solver can distinguish these cases by tracking the *success rate* $P_s(r) = S(r) / (S(r) + F(r))$, where
$S(r)$ is the number of successful hash checks on row $r$. A row with $P_s \approx 0$ is chronically failing (the
problem lies above the row), while a row with $0 < P_s < 1$ has configuration-specific failures (the problem is
within the row's decision cells).

Failure-biased reordering is most effective for the $0 < P_s < 1$ regime: the failure statistics distinguish
good configurations from bad ones. For the $P_s \approx 0$ regime, reordering within the row is futile&mdash;
the solver should backtrack to a higher row instead, which is the domain of B.1 (CDCL) and B.11 (restarts).

#### B.18.5 Storage and Lifetime

The failure counters require $O(s^2)$ storage: one counter per cell per value, plus per-row counters. At 4 bytes
per counter and 2 counters per cell: $261{,}121 \times 2 \times 4 = 2{,}088{,}968$ bytes ($\approx 2$ MB). The
per-row counters add $511 \times 4 = 2{,}044$ bytes. Total: $\approx 2$ MB&mdash;negligible relative to the
constraint store.

Counters are scoped to the current block and reset between blocks. Within a block, counters accumulate across all
backtracking iterations. Unlike B.14's nogoods (which record exact configurations), failure counters are
statistical summaries that grow more accurate over time but never consume unbounded storage.

A potential concern is counter overflow: if the solver backtracks through a row millions of times, the counters
may saturate. Using 32-bit counters provides a range of $\sim 4 \times 10^9$, sufficient for any practical solve.
Alternatively, counters can be maintained as decaying averages (exponential moving averages with decay factor
$\gamma$), weighting recent failures more heavily than distant ones.

#### B.18.6 Open Questions

(a) How correlated are per-cell failure statistics with actual conflict causes? If hash failures are essentially
random (the SHA-1 hash is a pseudorandom function of the row's 511 bits), then cell-level failure statistics will
converge slowly and carry little signal. Empirical measurement of the correlation between failure bias and future
success is needed.

(b) Is the $O(s^2)$ overhead of maintaining per-cell counters justified by the branching improvement? A cheaper
alternative maintains only per-row counters $F(r)$ and uses them for row-level prioritization (Mechanism 2) without
cell-level granularity. This reduces the overhead to $O(s)$ at the cost of less precise branching guidance.

(c) Can failure statistics be shared across blocks? If the same input patterns recur across blocks (e.g., blocks
from the same file have similar statistical properties), failure statistics from block $n$ could warm-start the
solver for block $n + 1$. This violates the block-independence assumption but may be valid if the statistics are
used only for heuristic ordering, not for correctness-critical pruning.

(d) Does the combination of B.18 (failure-biased ordering) with B.14 (nogood recording) provide synergistic
benefit? B.14 prunes exact reoccurrences; B.18 biases away from approximate reoccurrences. If the solver rarely
re-enters the exact same configuration (making B.14's nogoods rarely activated), B.18's statistical approach may
provide the coarser-grained learning that B.14 misses.

