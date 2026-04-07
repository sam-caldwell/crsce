### B.9 Non-Linear Lookup-Table Partitions

All eight implemented constraint families are *linear*: each defines its cell-to-line mapping via an arithmetic
formula over $(r, c)$. Rows use $r$, columns use $c$, diagonals use $c - r + (s-1)$, anti-diagonals use $r + c$, and
toroidal slopes use $(c - pr) \bmod s$ for various $p$. This linearity is what makes the algebraic kernel analysis
in B.2.2 possible&mdash;swap-invisible patterns exist because the constraint system is a set of linear functionals over
$\mathbb{Z}$, and the null space of any finite set of linear functionals over a 261,121-dimensional binary vector
space is necessarily large.

This appendix proposes *non-linear partitions* defined by an optimized lookup table rather than an algebraic formula.
The partition uses $s$ uniform-length lines of $s$ cells each, encoded at $b$ bits per element&mdash;the same structure
and per-partition storage cost as LSM, VSM, or any toroidal-slope family. The table is constructed offline,
optimized against simulated DFS trajectories on random inputs to maximize constraint tightening in the plateau
region (rows ~100-300), then hardcoded into the source as an immutable literal. Because the mapping has no
exploitable algebraic structure and is tuned for the solver's empirical bottleneck, it offers a qualitatively
different kind of constraint compared to any additional slope pair.

#### B.9.1 Construction

Define a *lookup-table partition* (LTP) as a $511 \times 511$ matrix $T$ of `uint16` values, where $T[r][c] \in
\{0, 1, \ldots, s - 1\}$ assigns cell $(r, c)$ to one of $s$ lines. Each line contains exactly $s$ cells:
$|\{(r,c) : T[r][c] = k\}| = s$ for all $k \in \{0, \ldots, s-1\}$. The cross-sum for line $k$ is:

$$
    \Sigma_k = \sum_{\{(r,c) : T[r][c] = k\}} x_{r,c}
$$

where $x_{r,c} \in \{0, 1\}$ is the CSM cell value. Each $\Sigma_k \in \{0, 1, \ldots, s\}$, requiring $b =
\lceil \log_2(s + 1) \rceil = 9$ bits to encode&mdash;identical to the LSM, VSM, and toroidal-slope families.

Construction is a two-phase offline process. The first phase generates a *baseline table* from a deterministic PRNG
seeded with a fixed constant (e.g., the first 32 bytes of SHA-256("CRSCE-LTP-v1")). A Fisher-Yates shuffle of all
$s^2$ cell indices produces a random assignment of $s$ cells per line, satisfying the uniform-length constraint.
This baseline serves as the starting point for the second phase.

The second phase *optimizes* the baseline against simulated DFS trajectories. The procedure generates a test suite
of $N$ random $511 \times 511$ binary matrices, runs the full decompression solver on each (using the current 8
linear partitions plus the candidate LTP pair), and records the total backtrack count in the plateau band (rows
100-300). The optimizer applies a local-search heuristic&mdash;swapping the line assignments of two randomly chosen
cells, accepting the swap if it reduces the mean backtrack count across the test suite, rejecting it otherwise. This
hill-climbing loop iterates until convergence. The swap preserves the uniform-length invariant (both lines still
have $s$ cells after the exchange), so the storage cost remains $sb = 4{,}599$ bits throughout.

The resulting table is hardcoded into the source code as a `constexpr std::array<uint16_t, 261121>` literal. The
table is part of the committed source, not a build-time artifact&mdash;there is no code-generation step, no optimizer
invocation at compile time, and no possibility of variation between builds or platforms. This guarantees bit-identical
tables in compressor and decompressor unconditionally. At runtime, cell-to-line resolution is a single indexed
memory access. The table occupies $s^2 \times 2 = 522{,}242$ bytes ($\approx 510$ KB) of static read-only memory
per partition.

#### B.9.2 Optimization Objective

The solver's plateau at depth ~87K (row ~170) occurs because all 8 existing constraint lines through any given cell
have length $s = 511$. At the plateau entry point, each line has roughly 170 of its 511 cells assigned, leaving
$u \approx 341$ unknowns and a residual $\rho$ in the range 80-170. The forcing conditions ($\rho = 0$ or
$\rho = u$) are far from triggering. The solver branches on nearly every cell because no line is tight enough to
force anything.

A uniform pseudorandom LTP has the same problem: its lines scatter cells uniformly across all rows, so at any given
DFS depth, all LTP lines progress at roughly the same rate and none reaches the forcing threshold before the others.
The optimization phase addresses this by biasing line composition so that *some* lines become tight early while
others stay loose.

The ideal optimized table has the following property: a substantial fraction of LTP lines (say 50-100) have a
majority of their $s$ cells concentrated in rows 0-170 (the region already solved when the DFS enters the plateau).
By row 170, these lines have $u \lesssim 170$&mdash;nearly half their cells are still unknown, but the residual is
correspondingly reduced. More importantly, the cells that remain unknown on these lines are scattered through the
plateau band (rows 100-300), so when the DFS assigns one of them, the line's residual drops toward a forcing
threshold faster than any length-511 linear line can achieve at the same depth. The remaining ~400 LTP lines absorb
the peripheral cells and stay loose, but those cells are already well-served by DSM/XSM (short lines at the corners)
and cardinality forcing on rows and columns.

The optimization objective is therefore: **minimize the mean backtrack count in the plateau band across a
representative suite of random inputs.** This objective is agnostic to the mechanism by which the table achieves its
benefit&mdash;it rewards any line-composition pattern that helps the solver, whether through early tightening, improved
crossing density with existing partitions, or some emergent structural property that the hill-climbing discovers.

#### B.9.3 Why Non-Linearity Matters

With linear partitions, the set of matrices indistinguishable from a given matrix $M$ under the constraint system
forms a coset of a lattice in $\mathbb{Z}^{s^2}$. The lattice kernel has dimension at least $s^2 - (ns - c)$ for $n$
partitions with $c$ dependent constraints. Swap-invisible patterns can be constructed algebraically by solving a
system of linear equations to find the null space, then enumerating binary-feasible vectors within it.

An optimized LTP breaks this structure. The cell-to-line mapping $T[r][c]$ has no algebraic relationship to
$(r, c)$, so the constraint $\Sigma_k = \text{target}$ is not a linear functional over the cell coordinates in any
useful sense. Formally, the constraint is still linear in the cell *values* (it is a sum), but the set of cells
participating in each constraint is defined by an optimized lookup rather than an arithmetic predicate. The "which
cells share a line" structure cannot be analyzed via rank-nullity over $\mathbb{Z}$&mdash;the partition's geometry is
opaque to linear-algebraic attack.

A 5th toroidal-slope pair, by contrast, is still a linear functional mod $s$. Its null-space contribution partially
overlaps with the existing 4 slope pairs, because all slope constraints live in the same algebraic family. An LTP
line's membership has zero algebraic correlation with any linear family, so the information it contributes is
maximally independent in the combinatorial sense. For swap-invisible patterns, a $k$-cell swap must simultaneously
balance across all linear partitions *and* the non-linear LTP&mdash;a strictly harder condition than balancing across
linear partitions alone.

#### B.9.4 Storage Cost

An LTP with $s$ uniform-length lines encodes identically to LSM, VSM, or any toroidal-slope family. Each line's
cross-sum requires $b = 9$ bits. The per-partition storage cost is:

$$
    B_u(s) = s \times b = 511 \times 9 = 4{,}599 \text{ bits}
$$

Adding one LTP pair costs $2 \times 4{,}599 = 9{,}198$ bits per block&mdash;identical to a toroidal-slope pair:

| Configuration                     | Block bits | $C_r$  |
|-----------------------------------|-----------|--------|
| Current (8 partitions)            | 125,988   | 0.5175 |
| + 1 LTP pair (10 partitions)     | 135,186   | 0.4823 |
| + 2 LTP pairs (12 partitions)    | 144,384   | 0.4471 |

The storage cost is identical to a slope pair. The decision between an LTP pair and a 5th slope pair therefore
reduces to propagation effectiveness per stored bit: does the optimized non-linear structure break the plateau more
effectively than a 5th algebraic slope?

#### B.9.5 Propagation Characteristics

The optimized LTP produces two classes of lines with distinct propagation behavior.

*Early-tightening lines* have a majority of their cells in rows 0-170. By the time the DFS reaches the plateau,
these lines have small $u$ and small $\rho$, approaching forcing thresholds. When the solver assigns a plateau-band
cell that belongs to one of these lines, the residual update may trigger $\rho = 0$ (forcing all remaining unknowns
to 0) or $\rho = u$ (forcing all remaining unknowns to 1). These forcing events propagate through the cell's 8
linear lines, potentially cascading into further assignments. The early-tightening lines thus inject forcing events
into the plateau band&mdash;exactly where the linear partitions provide none.

*Late-tightening lines* have most of their cells in the plateau band and beyond. These lines remain loose during the
plateau but gradually tighten as the DFS progresses past row 300. They provide little forcing during the critical
phase but contribute long-range information transfer: a cell assigned in row 150 updates a late-tightening line
whose other cells span rows 200-400, carrying residual information forward into rows the solver has not yet reached.

Linear partitions, by contrast, have uniform line composition: every row line has 511 cells spanning one row, every
column line has 511 cells spanning one column, and every slope line visits cells at regular modular intervals across
the entire matrix. No linear line becomes tighter than any other at a given DFS depth. The LTP's heterogeneous line
composition is a qualitative difference that uniform-length linear partitions cannot replicate.

#### B.9.6 Runtime Cost

Each LTP partition adds $s = 511$ lines to the `ConstraintStore`&mdash;the same count as a toroidal-slope partition.
At 8 bytes per line statistic (target, assigned, unknown), one LTP partition adds $511 \times 8 = 4{,}088$ bytes,
identical to a slope partition. The per-assignment cost increases by 2 line-stat updates per LTP pair (one lookup per
partition, one stat update each). The lookup is a single array access into the precomputed table&mdash;$O(1)$, typically
an L1 cache hit after the first few accesses. The `PropagationEngine` fast path (`tryPropagateCell`) checks 2
additional lines per LTP pair, extending the current 8-line check to 10&mdash;a 25% increase in per-iteration cost,
partially offset by the additional forcing events that reduce the total iteration count.

The runtime footprint is identical to adding a 5th toroidal-slope pair, with the sole difference being the 510 KB
lookup table in static read-only memory (versus a computed index formula for slopes).

#### B.9.7 Crossing Density and Orthogonality

Toroidal-slope partitions enjoy perfect 1-cell orthogonality with rows and columns: because $\gcd(p, s) = 1$ for
the implemented slopes ($s = 511$ is prime), every (slope-line, row) pair and every (slope-line, column) pair
intersects in exactly one cell. An optimized LTP does not guarantee this property. A random baseline produces
approximately uniform crossing density&mdash;each (LTP-line, row) pair contains roughly 1 cell in expectation&mdash;but
the optimization phase may skew this distribution to favor early-tightening lines at the expense of crossing
uniformity. The lack of a guaranteed 1-cell intersection means that the orthogonality argument from B.2.4 does not
extend to LTPs.

However, the optimization process can incorporate crossing density as a secondary constraint. The hill-climbing
loop could reject swaps that cause any (LTP-line, row) pair to exceed 2 cells, maintaining approximate orthogonality
while still optimizing for plateau performance. Whether this constraint materially limits the optimization's
ability to find effective tables is an empirical question.

More fundamentally, the LTP's value does not depend on orthogonality with the existing partitions. It depends on
injecting forcing events into the plateau band, which is a function of line composition (how many solved cells each
line contains at the plateau entry depth), not of crossing geometry. The LTP and the slope pair attack the plateau
through different mechanisms&mdash;the slope pair through uniform constraint density, the LTP through targeted early
tightening&mdash;and their benefits may be additive.

#### B.9.8 Interaction with Adaptive Lookahead (B.8)

The LTP's early-tightening lines amplify the effectiveness of adaptive lookahead. When the solver probes a cell
assignment at depth $k$ (B.8), the propagation cascade traverses 10 lines (8 linear + 2 LTP) per forced cell. If
the probed cell sits on an early-tightening LTP line with small $u$, the probe's propagation is more likely to
reach a forcing threshold on that line, triggering a cascade that the probe at $k - 1$ would have missed. This
broadens the "reach" of each probe step, potentially allowing $k = 2$ with an optimized LTP to achieve pruning
comparable to $k = 3$ without it.

The synergy is strongest in the plateau band, where lookahead probes are most frequent (because stall detection
escalates $k$) and LTP early-tightening lines are closest to their forcing thresholds. In the early rows (where
$k = 0$) and the late rows (where constraints tighten naturally), the LTP contributes less because the solver is
not probing deeply and the linear partitions are already effective.

#### B.9.9 Open Questions

Consolidated into Appendix C.8.

