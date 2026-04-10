### B.14 Lightweight Nogood Recording

B.1 proposes full CDCL adaptation for CRSCE, including implication-graph maintenance, 1-UIP conflict analysis,
learned-clause databases, and non-chronological backjumping. The implementation complexity is substantial (B.1.6).
This appendix proposes a *lightweight alternative*: record hash-failure nogoods without maintaining an implication
graph or performing conflict analysis. The approach captures a significant fraction of CDCL's benefit at a fraction
of the implementation cost.

#### B.14.1 Row-Level Nogoods

When an SHA-1 lateral hash check fails on row $r$, the solver knows that the current 511-bit assignment to row $r$
is wrong. Record this assignment as a *row-level nogood*: a 511-bit vector $\bar{x}_r$ such that any future
partial assignment that matches $\bar{x}_r$ on all 511 cells of row $r$ can be pruned without recomputing the hash.

The nogood is stored as a pair $(\text{row}, \text{bitvector})$ in a per-row hash table. When the solver completes
row $r$ in a future DFS branch, it checks the hash table before computing SHA-1. If the assignment matches a
recorded nogood, the solver backtracks immediately&mdash;saving the ~200 ns SHA-1 computation and, more importantly,
any propagation work that would have followed a (doomed) hash match.

The per-nogood storage is 64 bytes (511 bits rounded to 512 bits = 64 bytes). The hash-table lookup is a single
64-byte comparison&mdash;cheaper than SHA-1. The hash table is keyed on a fast hash of the bitvector (e.g., the first
8 bytes XORed with the last 8 bytes) to avoid linear scanning.

#### B.14.2 Partial Row Nogoods

A full 511-bit row nogood is maximally specific: it matches only the exact same assignment to all 511 cells. This is
useful only if the solver re-enters the exact same row configuration, which is rare in a 261K-cell search space. A
more powerful variant records *partial row nogoods*: the subset of cells in row $r$ that were assigned by branching
decisions (not forced by propagation).

At the point of hash failure, some cells in row $r$ were determined by the solver's branching decisions (directly or
via propagation from decisions in other rows), while others were forced by cardinality constraints. The forced cells
are consequences of the current constraint state; only the decision cells represent genuine choices. A partial row
nogood records only the decision cells and their values: "if cells $(r, c_1), (r, c_2), \ldots, (r, c_k)$ are
assigned values $v_1, v_2, \ldots, v_k$, then row $r$ will fail its LH check regardless of how the forced cells
are assigned."

This partial nogood is shorter ($k$ cells instead of 511) and therefore matches more broadly: it prunes any future
branch that makes the same $k$ decisions, even if the forced cells take different values (due to different
constraint states from other rows). The pruning power is proportional to $2^{511 - k}$&mdash;the number of full-row
assignments that the partial nogood eliminates.

Identifying which cells are decisions versus forced requires inspecting the undo stack at the point of failure. Each
entry on the stack records whether the assignment was a branch or a propagation. Scanning the stack for row $r$'s
cells is $O(s)$&mdash;negligible compared to the cost of the DFS that produced the failure.

#### B.14.3 Nogood Checking Efficiency

Partial row nogoods must be checked during the search. The naive approach checks all recorded nogoods for row $r$
whenever a new cell in row $r$ is assigned. This is expensive if many nogoods accumulate.

A more efficient approach checks nogoods only at row completion ($u(\text{row}) = 0$), immediately before the SHA-1
computation. The check is: for each nogood recorded for row $r$, verify whether the current assignment matches the
nogood's decision cells. If any nogood matches, skip SHA-1 and backtrack. The cost is proportional to the number of
nogoods recorded for row $r$ times the number of decision cells per nogood. If the solver records at most $N$
nogoods per row and each nogood has $k \approx 100$ decision cells, the check costs $\sim 100N$ byte comparisons
-- negligible for $N < 1{,}000$.

An even cheaper approach uses *watched literals*, borrowing from SAT solver clause management. Each partial nogood
"watches" two of its decision cells. The nogood is only examined when both watched cells are assigned to their
nogood values. This amortizes the checking cost across assignments, activating the full check only when the nogood
is close to being triggered.

#### B.14.4 Interaction with B.1 (CDCL)

Lightweight nogood recording is a strict subset of the CDCL machinery proposed in B.1. The key differences:

*No implication graph.* B.14 does not track which constraint triggered each propagation. It simply inspects the
undo stack at the point of failure to identify decision cells. This saves the per-propagation overhead of recording
antecedents.

*No conflict analysis.* B.14 does not perform 1-UIP resolution to derive a minimal conflict clause. The partial
row nogood is a conservative approximation: it includes all decision cells in the failed row, not just the
causally relevant ones. A full CDCL clause would typically be shorter (identifying only the decisions that caused
the conflict), providing stronger pruning per clause.

*No backjumping.* B.14 uses chronological backtracking. The recorded nogoods prevent re-entering known-bad
configurations but do not enable jumping past irrelevant decision levels. B.1's backjumping provides complementary
benefit by reducing the number of backtracks needed to reach the root cause.

*Incremental upgrade path.* B.14 can be implemented first as a low-risk, low-effort improvement. If empirical
results show that nogoods from hash failures are valuable, the solver can be incrementally upgraded to B.1 by
adding implication-graph tracking, conflict analysis, and backjumping.

#### B.14.5 DI Determinism

Nogood recording is deterministic: the nogoods recorded are a function of the search trajectory (which failures
occurred and which cells were decisions), and the search trajectory is identical in compressor and decompressor.
The nogood database evolves identically in both, and pruning decisions based on nogoods are therefore identical.
DI semantics are preserved.

Partial row nogoods prune infeasible subtrees (configurations that are known to fail the LH check) but never prune
feasible solutions (a correct row assignment will not match any nogood, because the nogood records a *failed*
assignment). The enumeration order is unchanged: the solver visits the same feasible solutions in the same
lexicographic order, but skips known-infeasible configurations faster.

#### B.14.6 Storage and Lifecycle

The nogood database grows as the solver encounters hash failures. Each partial row nogood occupies ~$k/8$ bytes
(the decision cells' bitvector, compressed). At $k \approx 100$ decision cells per nogood, each entry is ~13 bytes
plus overhead. If the solver records 10,000 nogoods across all rows during a block solve, the database occupies
~130 KB&mdash;negligible relative to the constraint store.

Nogoods are scoped to the current block and discarded between blocks. Within a block, nogoods from early failures
remain valid throughout the solve because the constraint system does not change (the same cross-sums and hashes
apply throughout). There is no need for clause deletion or garbage collection.

#### B.14.7 Open Questions

(a) How many hash failures does the solver encounter per block, and how many of those failures recur (same row with
the same decision-cell assignments)? If failures rarely recur, nogoods provide little pruning benefit. Instrumenting
the solver to log failure patterns would quantify the potential.

(b) Are partial row nogoods (B.14.2) significantly more powerful than full row nogoods (B.14.1)? The answer depends
on how much of the row is determined by decisions versus forcing. If ~400 of 511 cells are forced (leaving ~111
decision cells), partial nogoods are $2^{400}$ times more general than full nogoods&mdash;a dramatic increase in
pruning power. But the value depends on how often the same ~111 decisions recur, which is an empirical question.

(c) What is the optimal checking strategy? Row-completion checking (B.14.3) is simple but delays nogood pruning
until all cells are assigned. Watched-literal checking activates earlier but adds per-assignment overhead. The
tradeoff depends on the ratio of nogood-checking cost to SHA-1 cost and on how many assignments the solver makes
in a row before completing it.

(d) Should nogoods be shared across portfolio instances (B.13)? If instance $A$ discovers a nogood for row $r$,
instance $B$ could benefit from the same nogood if it encounters the same row configuration. Sharing nogoods
requires inter-instance communication but could provide the clause-sharing benefit of parallel CDCL without the
full CDCL machinery.

