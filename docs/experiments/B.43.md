## B.43 Bottom-Up Initialization via Row-Completion Look-Ahead (Completed &mdash; Infeasible)

### B.43.1 Motivation

Experiments B.40 through B.42 established that the plateau is caused by constraint exhaustion, not solver inefficiency. The solver backtracks because no row in the meeting band reaches full assignment ($u = 0$) during top-down DFS, and eliminating branching waste (B.42c) does not change this.

However, B.42a diagnostics revealed that some rows fluctuate to within 2-6 unknowns of completion at the plateau. This raises a question: if the solver could detect these near-complete rows and resolve them via exhaustive enumeration rather than DFS branching, would the resulting cascade of newly constrained cells extend the propagation depth?

B.43 proposes **Row-Completion Look-Ahead (RCLA)** combined with a bottom-up initialization sweep: before the main DFS begins, and at each plateau checkpoint, scan all rows for those with small unknown counts and enumerate their feasible completions against the SHA-1 lateral hash. A row with $u$ unknowns and residual sum $\rho$ has at most $\binom{u}{\rho}$ candidate completions, each verifiable via a single SHA-1 computation.

The hypothesis is that completing near-complete rows will tighten constraints on adjacent rows (via column, diagonal, and LTP lines), potentially triggering a cascade of further RCLA completions that "peels" the meeting band from both ends.

### B.43.2 Method

**RCLA eligibility.** A row $r$ is RCLA-eligible when $u(r) \leq T$ for threshold $T$ chosen such that $\binom{T}{\lfloor T/2 \rfloor}$ is computationally tractable. Candidate thresholds:

| Threshold $T$ | Max completions $\binom{T}{\lfloor T/2 \rfloor}$ | Enumeration cost |
|---|----|---|
| 5 | 10 | ~1 &mu;s |
| 10 | 252 | ~25 &mu;s |
| 15 | 6,435 | ~640 &mu;s |
| 20 | 184,756 | ~18 ms |
| 25 | 3,268,760 | ~330 ms |

**RCLA completion.** For each eligible row $r$:

1. Identify the $u(r)$ unassigned cell positions within the row.
2. Enumerate all $\binom{u(r)}{\rho(r)}$ binary assignments to those cells that satisfy the row sum.
3. For each candidate assignment, pack the full 511-bit row and compute SHA-1.
4. Compare to the stored lateral hash $\text{LH}[r]$.
5. If exactly one candidate matches: **force all $u(r)$ cells** to their resolved values and propagate.
6. If zero candidates match: the current partial assignment is infeasible (backtrack trigger).
7. If multiple candidates match: row is not uniquely resolvable (SHA-1 collision within the sub-enumeration; astronomically unlikely for $T \leq 25$).

**Bottom-up sweep.** After initial constraint propagation:

1. Scan rows from 510 downward to 0.
2. For each RCLA-eligible row, attempt completion.
3. If a row is completed, propagate forced cells.
4. Re-check adjacent rows (those sharing a column, diagonal, or LTP line with the completed row) for newly reduced $u$ values.
5. Repeat until no more rows are eligible (fixpoint).

**DFS integration.** During the main DFS, check RCLA eligibility at each row-completion-heap event (when $u(r)$ drops below the threshold). This catches rows that become near-complete due to DFS branching.

### B.43.3 Diagnostic Results (Completed)

Before implementing RCLA, diagnostic instrumentation was added to measure how many rows are RCLA-eligible at the plateau. The solver was run on the synthetic random 50%-density CSM block (B.38-optimized LTP table, 1-minute observation window).

**Row unknown distribution at peak depth 86,125:**

| Category | Rows |
|----------|------|
| $u = 0$ (complete) | 168 |
| $u = 1$-$5$ | 0-1 (fluctuates) |
| $u = 6$-$10$ | 0-1 (fluctuates) |
| $u = 11$-$20$ | 0 |
| $u > 20$ | ~342 |
| Total | 511 |

**Key finding: at most 1 row is RCLA-eligible at any time** (for any threshold $T \leq 20$). The near-complete row fluctuates between $u = 4$ and $u = 6$ as the DFS branches and backtracks. The remaining 342 meeting-band rows have $u \gg 20$ (typically $u \approx 300$-$400$), far beyond any tractable RCLA threshold.

### B.43.4 Analysis

**Why RCLA cannot cascade.** Completing a single row fixes $u$ cells and tightens constraints on 10 lines per cell (row, column, diagonal, anti-diagonal, 4 LTP). For the adjacent meeting-band rows, this reduces their unknown counts by at most 1 per shared constraint line. A row with $u = 300$ would need ~280 constraint-tightening events to reach $T = 20$&mdash;requiring ~280 other rows or columns to be completed first. This is circular: each row needs other rows to complete first.

The fundamental issue is the **constraint density** in the meeting band. The 8-family constraint system provides $\sim 10$ constraints per cell, but only $5{,}097 / 261{,}121 \approx 2\%$ of the cells are independently constrained ($\text{rank}(A) / N$). In the meeting band (~175K cells), the effective constraint density is even lower because the propagation zone (rows 0-167) has already consumed the "easy" forcings.

**Comparison with top-down DFS.** The DFS reaches the plateau precisely because it has exhausted all forceable cells. RCLA requires rows to be near-complete, which happens only transiently during DFS backtracking (when the solver has tentatively assigned most of a row's cells, only to discover infeasibility a few cells later). These transient near-completions are artifacts of the search path, not stable structural features of the constraint system.

**Bottom-up sweep: identical result.** After initial propagation, the row-unknown distribution is the same regardless of sweep direction. Rows 0-167 have $u = 0$ (fully propagated); rows 168-510 have $u \gg 20$. A bottom-up sweep from row 510 finds zero RCLA-eligible rows because meeting-band rows have ~300-400 unknowns. The bottom-up direction provides no advantage because the constraint system's information content is direction-invariant.

### B.43.5 Conclusion

**B.43 outcome: INFEASIBLE.** The RCLA approach requires near-complete rows, but the meeting band has at most 1 row with $u \leq 20$ at any time. The bottom-up cascade hypothesis is not viable: completing a single row cannot trigger a chain reaction of further completions because adjacent rows have $u \gg 20$.

This result confirms the B.42 conclusion from a different angle: the depth ceiling is a property of the constraint system's information content, not the solver's search strategy. The 8-family constraint system (5,097 independent constraints over 261,121 cells) leaves 256,024 degrees of freedom (98% of cells unconstrained). No solver technique&mdash;top-down, bottom-up, probing, interval analysis, RCLA, or direction switching&mdash;can compensate for this structural information deficit.

**Implication for future work.** Reaching the 100K depth target requires adding information to the constraint system itself. Candidate approaches include additional projection families (beyond the current 8), finer-grained hash constraints (sub-row or sub-block hashes), or a fundamentally different matrix encoding that increases the constraint-to-cell ratio above the current 2%.

**Status: COMPLETED &mdash; INFEASIBLE.**

---

