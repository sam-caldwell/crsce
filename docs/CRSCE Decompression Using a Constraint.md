---
title: "CRSCE Decompression Using a Constraint-Based Graph-Theoretical Approach"
author: "Samuel Caldwell"
affiliation: "Champlain College"
course: "CMIT-450 : Senior Seminar"
advisor: "Dr. Kenneth Revett"
date: "28 February 2026"
---

## Abstract

Cross-Sums Compression and Expansion (CRSCE) compresses a fixed-size two-dimensional binary matrix---the Cross-Sum
Matrix (CSM)---by replacing it with four families of exact-sum projections (row, column, diagonal, and anti-diagonal
sums) together with independent per-row SHA-256 digests. Decompression is a constrained reconstruction problem: recover
a matrix consistent with all projections and row hashes. Although per-row SHA-256 digests make non-uniqueness
astronomically unlikely under standard cryptographic assumptions, non-uniqueness is not logically impossible. This paper
formalizes a *disambiguated* decompression process in which the compressor discovers a *canonical, zero-based*
disambiguation index (DI) identifying the original matrix among all feasible reconstructions and appends that DI to the
compressed payload. The compressor is permitted to fail if DI discovery exceeds a single tolerance parameter,
`max_compression_time`, forcing a fallback to alternative compression. We develop the theory as a constraint
satisfaction problem (CSP) over a factor graph, analyze the computational implications of solution enumeration, and
present a deterministic solver design that combines incremental constraint propagation with belief-propagation-guided
branching heuristics.

## 1. Introduction

In the CRSCE scheme, an input bitstream is subdivided into blocks of approximately 32 KB. Each block is placed into $CSM
\in \{0,1\}^{s \times s}$ with fixed size $s = 511$. Four cross-sum vectors are computed from $CSM$: row sums (LSM),
column sums (VSM), diagonal sums (DSM), and anti-diagonal sums (XSM). An independent SHA-256 digest is also computed for
each row, producing the lateral hash vector (LH). A disambiguation index (DI) is appended to handle the rare case of
non-unique reconstruction. The compressed representation of each block is therefore the tuple $(\text{LH}, \text{DI},
\text{LSM}, \text{VSM}, \text{DSM}, \text{XSM})$.

Compression is realized because the cross-sum vectors and hash vector are smaller than the original matrix. LSM and VSM
each contain $s$ elements and each element requires $b = \lceil \log_2(s+1) \rceil$ bits. DSM and XSM each contain $2s -
1$ elements, but because straight diagonals have variable lengths ranging from 1 to $s$, each diagonal sum is encoded in
$\lceil \log_2(\text{len}(k)+1) \rceil$ bits, where $\text{len}(k) = \min(k+1,\; s,\; 2s-1-k)$ for diagonal index $k$.
The result defines the total bits for one diagonal family as:

$$
    B_d(s) = \lceil \log_2(s+1) \rceil + 2 \sum_{l=1}^{s-1} \lceil \log_2(l+1) \rceil
$$

LH consists of $s$ rows of 256 bits each. The disambiguation index (DI) is encoded as a fixed 8-bit unsigned integer per
block, permitting up to 256 feasible reconstructions before the compressor must fail. The per-block compression ratio is
therefore:

$$
    C_r = 1 - \frac{2sb + 2 B_d(s) + 256s + 8}{s^2}
$$

where $s = 511$,

$$
    b = \lceil \log_2(s+1) \rceil = 9, \quad B_d(511) = 8{,}185
$$

Thus,

$$
    C_r = 1 - \frac{9{,}198 + 16{,}370 + 130{,}816 + 8}{261{,}121} = 1 - \frac{156{,}392}{261{,}121} \approx 0.4011
$$

This means for any given suitable input, CRSCE is *guaranteed* to realize approximately 40.1% compression per block.
This excludes any additional compressed file format header information.

Decompression inverts this mapping: reconstruct $CSM$ given the stored constraints. The central difficulty is
computational: the feasible set of $CSM$ consistent with all sum constraints may be large, and finding *the* correct
$CSM$ is a constrained search problem. The SHA-256 row digests transform the problem from "find any feasible matrix"
into "find a feasible matrix whose rows match $s$ given digests," which, under standard cryptographic assumptions,
should make accidental ambiguity negligible (NIST, 2015; Dang, 2012).

However, negligible is not zero. Moreover, determinism requirements ("exactly one output for a given compressed
payload") require a defined behavior even in the pathological case of multiple feasible reconstructions. This paper
proposes a process-level remedy: define a canonical total order over feasible solutions and encode a zero-based DI
selecting the intended solution. Importantly, the DI is discovered at compression time by running the decompressor as an
enumerator with the original uncompressed matrix as the target.

Because solution enumeration can be expensive, the process is explicitly *time-bounded* by a single compressor
parameter, `max_compression_time`; if exceeded, compression fails and the caller must use an alternative algorithm. The
remainder of the paper builds a defensible theoretical foundation, then presents a deterministic solver design suitable
for modern hardware with both CPU and GPU resources.

## 2. Formal Problem Statement and Notation

### 2.1 Matrix Model

Let $s = 511$. For decompression, let the unknown matrix be

$$
    CSM = (CSM_{r,c}) \in \{0,1\}^{s \times s}, \quad r,c \in \{0, \ldots, s-1\}.
$$

The compressor serializes input bits into the $CSM$ in a deterministic row-major mapping (the precise mapping is assumed
fixed by the format specification).

### 2.2 Cross-Sum Constraints

Define the four projection families as exact cardinality constraints:

**Row sums (LSM):**

$$
    \forall r: \quad \sum_{c=0}^{s-1} CSM_{r,c} = \text{LSM}[r].
$$

**Column sums (VSM):**

$$
    \forall c: \quad \sum_{r=0}^{s-1} CSM_{r,c} = \text{VSM}[c].
$$

**Straight diagonal sums (DSM):**

For diagonal index $d \in \{0, \ldots, 2s-2\}$, define $\text{len}(d) = \min(d+1,\; s,\; 2s-1-d)$.
The diagonal $d$ consists of all cells $(r, c)$ satisfying $c - r = d - (s-1)$ with $0 \leq r,c \leq s-1$:

$$
    \forall d: \quad \sum_{\substack{(r,c) \,:\, c - r = d-(s-1) \\ 0 \leq r,c \leq s-1}} CSM_{r,c} = \text{DSM}[d].
$$

Each diagonal sum is encoded in $\lceil \log_2(\text{len}(d)+1) \rceil$ bits.

**Straight anti-diagonal sums (XSM):**

For anti-diagonal index $x \in \{0, \ldots, 2s-2\}$, define $\text{len}(x) = \min(x+1,\; s,\; 2s-1-x)$. The
anti-diagonal $x$ consists of all cells $(r, c)$ satisfying $r + c = x$ with $0 \leq r,c \leq s-1$:

$$
    \forall x: \quad \sum_{\substack{(r,c) \,:\, r + c = x \\ 0 \leq r,c \leq s-1}} CSM_{r,c} = \text{XSM}[x].
$$

Each anti-diagonal sum is encoded in $\lceil \log_2(\text{len}(x)+1) \rceil$ bits.

Each cell participates in exactly four sum constraints: one row, one column, one straight diagonal, and one straight
anti-diagonal.

### 2.3 Row Hash Constraints (LH)

For each row $r$, define the row bitstring

$$
    \text{row}_r = CSM_{r,0} \| CSM_{r,1} \| \cdots \| CSM_{r,s-1} \| 0
$$

as a message of length $s + 1 = 512$ bits. The trailing zero bit is a fixed padding constant that ensures byte alignment
(512 bits = 64 bytes), simplifying implementation without affecting the hash's discriminative power between distinct
rows. Let

$$
    H(\text{row}_r) = \text{SHA-256}(\text{row}_r),
$$

and require

$$
    \forall r: \quad H(\text{row}_r) = \text{LH}[r].
$$

SHA-256 is standardized in FIPS 180-4, including message padding and parsing into 512-bit blocks (NIST, 2015).

### 2.4 Decompression Objective

Given the constraint set $\mathcal{C} = (s, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{LH})$, decompression
seeks a $CSM$ satisfying all constraints. The premise that $\mathcal{C}$ admits exactly one feasible matrix is a design
intent, not a mathematical guarantee; the cross-sum and hash constraints make non-uniqueness astronomically unlikely but
cannot logically exclude it. The full compressed payload $\mathcal{C}' = (\text{LH}, \text{DI}, \text{LSM}, \text{VSM},
\text{DSM}, \text{XSM})$ therefore includes an 8-bit disambiguation index (DI) that selects the intended reconstruction
from the canonical enumeration of feasible solutions (Section 4). It is the DI that provides the "exactly one solution"
guarantee: even if $\mathcal{C}$ admits multiple feasible matrices, $\mathcal{C}'$ identifies precisely one.

## 3. Theoretical Foundations

### 3.1 Constraint Satisfaction and Factor-Graph View

The decompression problem is naturally a CSP over $s^2$ Boolean variables with $6s - 2$ cardinality constraints plus $s$
hash-equality constraints. For $s = 511$ this expands to:

$$
    s^2 = 261{,}121 \text{ Boolean variables}
$$

$$
    \underbrace{511}_{\text{row}} +
    \underbrace{511}_{\text{column}} +
    \underbrace{1{,}021}_{\text{diagonal}} +
    \underbrace{1{,}021}_{\text{anti-diagonal}} = 3{,}064 \text{ cardinality constraints}
$$

$$
    511 \text{ SHA-256 row-hash constraints}
$$

$$
    3{,}064 + 511 = 3{,}575 \text{ total constraints}
$$

This can also be represented as a factor graph: variable nodes for each $CSM_{r,c}$, factor nodes for each
row/column/diagonal/anti-diagonal sum constraint, and factor nodes for each SHA-256 row constraint. Factor graphs
support message-passing interpretations (Kschischang et al., 2001).

In principle, belief propagation (BP) or its generalizations can provide probabilistic guidance in large constraint
systems, especially when exact inference is intractable (Yedidia et al., 2002; Wainwright & Jordan, 2008). In practice,
the exact-sum constraints are hard factors (zero probability when violated), while the hash factors are extremely sharp:
for a fully specified row, either the digest matches or it does not.

### 3.2 Cryptographic Uniqueness as a Heuristic, Not a Proof

SHA-256 is designed so that, absent structural weaknesses, finding distinct messages with the same digest is
computationally infeasible; NIST guidance frames security strength for applications that rely on hash properties (Dang,
2012; NIST, 2015). From a modeling perspective, treating $H$ as a random mapping from 511-bit strings to 256-bit
digests, the expected number of 511-bit preimages for any fixed digest value is $2^{511}/2^{256} = 2^{255}$, so hashes
alone do not uniquely identify a row among all possible rows. However, the decompressor is not choosing among all rows;
it is choosing among rows that participate in a globally consistent $CSM$ satisfying four families of sum constraints.
The heuristic claim is that the intersection "sum-feasible" $\cap$ "row-hash-matching" is typically singleton.

This is a plausible engineering assumption but remains a heuristic: it does not exclude adversarially constructed
instances or rare coincidences.

### 3.3 Estimating the Probability of Non-Unique Reconstruction

Section 3.2 argued qualitatively that hash-backed uniqueness is a heuristic. This section estimates the probability of
non-uniqueness quantitatively under a random-oracle model for SHA-256. The analysis proceeds in two stages: first
estimating the probability that an alternative matrix satisfies the cross-sum constraints alone, then combining this
with the per-row hash collision barrier.

**Cross-sum collision probability.** For a second feasible matrix $CSM'$ to match the original $CSM$ on all four
cross-sum families, each row must independently have identical Hamming weight to the corresponding original row.
Treating both matrices as independent draws from the uniform distribution over $\{0,1\}^{s \times s}$, the probability
that two independent $s$-bit rows share the same Hamming weight is given by the central binomial coefficient:

$$
    P(\text{row-sum match}) = \frac{\binom{2s}{s}}{4^s}.
$$

For $s = 511$, Stirling's approximation yields $\log_{10} P \approx -1.603$. Since the $s$ row sums are independent
under the uniform model, the probability that all $s$ rows match simultaneously is:

$$
    P(\text{LSM collision}) = \left[\frac{\binom{2s}{s}}{4^s}\right]^s \approx 10^{-819.08}.
$$

This bound is conservative: it considers only the row-sum (LSM) family. A complete alternative matrix must also match
all $s$ column sums, $2s - 1$ diagonal sums, and $2s - 1$ anti-diagonal sums, each of which imposes additional
independent constraints that further reduce the probability.

**Lateral hash collision probability.** Even if an alternative matrix satisfies all cross-sum constraints, it must also
pass independent per-row SHA-256 verification. Under the random-oracle assumption, each of the $s$ row hashes must
independently collide with the original digest. The probability that all $s$ rows collide simultaneously is:

$$
    P(\text{LH collision}) = \left(\frac{1}{2^{256}}\right)^s = 2^{-256s} = 2^{-130{,}816}.
$$

Converting to base 10: $\log_{10} P(\text{LH collision}) = -130{,}816 \times \log_{10} 2 \approx -39{,}379.54$.

**Combined collision probability.** Treating the cross-sum and hash collision events as independent (a conservative
assumption, since in practice the constraints are correlated and further reduce the joint probability):

$$
    P_{\text{CRSCE}} \leq P(\text{LSM collision}) \times P(\text{LH collision}) \approx 10^{-819}
    \times 10^{-39{,}380} \approx 10^{-40{,}199}.
$$

A probability of $\sim 10^{-40{,}199}$ is not merely negligible in a practical sense---it exceeds the security margin of
any deployed cryptographic system by many orders of magnitude. For context, the estimated number of atoms in the
observable universe is approximately $10^{80}$, and the probability of a spontaneous SHA-256 collision under
birthday-bound assumptions is approximately $2^{-128} \approx 10^{-38.5}$. The CRSCE collision probability is smaller
than either by a factor exceeding $10^{40{,}000}$.

This analysis assumes SHA-256 behaves as a random oracle, which is standard for non-adversarial settings. Under
adversarial conditions (e.g., a crafted input designed to induce collisions), the analysis does not apply, but the DI
mechanism (Section 2.4) provides deterministic correctness regardless of collision probability.

### 3.4 Enumeration and Indexing: Why Worst Cases Can Be Hard

The proposed disambiguation mechanism depends on enumerating feasible solutions. Even when *finding one* solution is
tractable in some structured settings, *counting* or *enumerating* solutions is often much harder in general;
\#P-completeness results (e.g., via Valiant's theorem for counting perfect matchings / permanents) illustrate that
counting can be intractable even when related decision problems are easier (Valiant, 1979).

SAT/SMT research treats "AllSAT" as a distinct, often expensive task: enumerating all satisfying assignments can
dominate runtime and memory, and specialized methods exist to mitigate the cost (e.g., blocking and non-blocking
strategies) (Kobler et al., 2025). The relevance here is not that CRSCE decompression is literally SAT, but that
solution enumeration in combinatorial constraint systems can be intrinsically expensive. Therefore, any indexing-based
disambiguation must explicitly define a failure mode---hence the single time tolerance parameter.

### 3.5 Ranking-Based DI Discovery as an Alternative to Enumeration

The compressor's DI discovery algorithm (Section 4.3) enumerates feasible solutions in canonical order until it finds
the original $CSM$. If $\text{DI} = 0$, the first candidate matches and the cost is minimal. When $\text{DI} > 0$,
however, the compressor must produce and discard $\text{DI}$ complete solutions before reaching the target. This section
considers an alternative strategy that exploits the compressor's knowledge of the original $CSM$.

The key observation is that the compressor is solving a *ranking* problem: given a specific element (the original $CSM$)
and a total order (lexicographic), determine its zero-based position among all feasible solutions. In combinatorics,
ranking is often more efficient than full enumeration. For example, ranking a permutation in lexicographic order
requires $O(n)$ operations rather than enumerating all prior permutations (Knuth, 2005).

For CRSCE, a ranking approach would proceed as follows. At each branching point $(r, c)$ in the DFS, the compressor
knows $CSM_{r,c}$. If the enumerator's lex-first branch (value 0) disagrees with the original (value 1), then every
feasible solution in the 0-subtree lexicographically precedes the original. Rather than enumerating these solutions
individually, the compressor could *count* them and add the count to a running DI accumulator, then follow the 1-branch
directly.

This transforms DI discovery from solution enumeration into a sequence of subtree-counting steps. The DI is the sum of
subtree counts for every branching point where the canonical first branch disagrees with the original:

$$
    \text{DI} = \sum_{\substack{(r,c) \text{ branching} \\
    \text{points where} \\
    CSM_{r,c} = 1}} \left| \{ \text{feasible solutions in the 0-subtree at } (r,c) \} \right|.
$$

Subtree counting in a general CSP is \#P-hard (Valiant, 1979), so this does not eliminate worst-case intractability.
However, the approach admits practical optimizations. When constraint propagation forces most cells in a subtree, the
effective degrees of freedom are small and exact counting may be tractable via dynamic programming over the remaining
line residuals. A hybrid strategy can attempt counting at each branch point and fall back to enumeration if counting
exceeds a local time budget, yielding a best-effort ranking that degrades gracefully to the baseline enumerator.

This ranking perspective also connects to the broader literature on counting under constraints, including approximate
counting via Markov chain Monte Carlo methods and fully polynomial randomized approximation schemes (FPRAS) for related
problems (Jerrum et al., 2004). While approximate counting would not yield an exact DI, it could provide upper bounds
useful for early termination decisions.

## 4. Canonical Solution Indexing as Format-Level Disambiguation

### 4.1 Canonical Ordering

Let $\text{vec}(CSM) \in \{0,1\}^{s^2}$ be the row-major bitstring:

$$
    \text{vec}(CSM) = CSM_{0,0} \| CSM_{0,1} \| \cdots \| CSM_{0,s-1} \| CSM_{1,0} \| \cdots \| CSM_{s-1,s-1}.
$$

Define a strict total order $<_{\text{lex}}$ on matrices by lexicographic order on $\text{vec}(CSM)$, with $0 < 1$.

The *canonical enumeration* of solutions is the sequence:

$$
    S_0 <_{\text{lex}} S_1 <_{\text{lex}} \cdots
$$

where each $S_k$ is a feasible $CSM$ satisfying all constraints in $\mathcal{C}$.

### 4.2 Disambiguation Index (DI)

The compressed representation is extended to:

$$\mathcal{C}' = (\text{LH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}),$$

where DI is an unsigned 8-bit integer, selecting the intended reconstruction:

$$
    D(\mathcal{C}') = S_{\text{DI}}.
$$

This creates a fully deterministic decompression mapping *even if* $\mathcal{C}$ has multiple feasible reconstructions.
If the original matrix's position in the canonical enumeration exceeds 255, compression fails; this is subsumed by the
`max_compression_time` failure mode, since enumerating that many solutions would almost certainly exceed any practical
time bound.

### 4.3 Compressor-Side DI Discovery

Compression proceeds as usual to compute $\mathcal{C}$, then discovers the DI by running the decompressor to compare
candidates to the known original $CSM$. If the decompressor yields solutions in canonical order, the compressor can stop
as soon as the candidate equals $CSM$, and the zero-based index of that candidate is the correct DI. Later, during
decompression, the decompressor proceeds to $S_{\text{DI}}$ to successfully reconstruct the original payload.

Notably, the compressor does not need to enumerate solutions *after* the match. This bounds work to "solutions up to the
target" rather than all solutions, but the target could still be late in the order.

### 4.4 Single Tolerance Parameter: Time-Bounded Compression

To avoid unbounded compression runs where the compressor fails to discover the correct DI, define one and only one
tolerance parameter: `max_compression_time`, a wall-clock limit on the entire compression operation including DI
discovery.

If the limit is exceeded before the compressor identifies the target $CSM$, compression fails. The output is not
produced (or is flagged invalid by container rules). This makes the algorithm explicitly "best-effort" over inputs: it
is permitted to reject inputs that induce excessive ambiguity or excessive enumeration cost.

## 5. Making the Theory Implementable: Deterministic Decompression by Constraint Propagation + Backtracking

### 5.1 Deterministic State Representation

The deterministic solver maintains a compact state for each line (row, column, diagonal, and anti-diagonal) that enables
incremental constraint propagation as cells are assigned. This representation allows the solver to detect infeasibility
early, force deterministic assignments when only one value is possible, and verify completed rows against their SHA-256
digests without recomputing from scratch.

For each line $L$, the solver tracks four quantities: $u(L)$, the count of unknown cells on the line; $a(L)$, the count
of assigned ones; the required sum $S(L)$ from the corresponding cross-sum vector (LSM, VSM, DSM, or XSM); and the
residual $\rho(L) = S(L) - a(L)$.

A line is feasible iff

$$0 \leq \rho(L) \leq u(L).$$

When $\rho(L) = 0$, all unknown cells on $L$ are forced to $0$. When $\rho(L) = u(L)$, all unknown cells on $L$ are
forced to $1$. These are standard feasibility rules for cardinality constraints, and they can be applied repeatedly
until reaching a fixpoint.

Each assignment $CSM_{r,c} \leftarrow 0/1$ updates exactly four lines' $(u, a, \rho)$, enabling incremental propagation.

### 5.2 Integrating Row Hash Checks Deterministically

Row-hash constraints are most effective once a row becomes fully assigned. When $u(\text{row } r) = 0$, compute SHA-256
of the 511-bit row message and compare to $\text{LH}[r]$. A mismatch yields an immediate contradiction and triggers
backtracking. SHA-256 preprocessing rules (padding and parsing) must follow FIPS 180-4 exactly to ensure
interoperability (NIST, 2015).

This approach does *not* require representing SHA-256 as a boolean circuit; it relies on exact evaluation at the moment
the row becomes concrete, which is straightforward to implement and deterministic.

### 5.3 Canonical Branching to Support Enumeration

To enumerate solutions in lexicographic order, choose the first unassigned cell in row-major order:

$$(r^*, c^*) = \min\{(r,c) : CSM_{r,c} \text{ unassigned}\}$$

under lex order of indices. Then branch by assigning $0$ first, then $1$. This ensures that the first found complete
solution is $S_0$, the second is $S_1$, etc., assuming the solver is otherwise deterministic.

### 5.4 Pseudocode: Deterministic Enumerator

Below, `Propagate()` applies the feasibility rules to a queue until quiescence; it returns false if any line becomes
infeasible or any completed row hash mismatches.

```text
Algorithm 1: EnumerateSolutionsLex(C)
Input:  constraints C = (s, LSM, VSM, DSM, XSM, LH)
Output: stream of feasible matrices CSM in lex order

Initialize all CSM[r,c] = UNASSIGNED
Initialize line stats (u, a, ρ) for all rows, cols, diags, anti-diags
Q <- all lines
if not Propagate(Q): return

function DFS(next_cell_index):
    if all cells assigned:
        yield CSM
        return

    (r, c) <- first UNASSIGNED cell in row-major order

    for v in [0, 1]:          // lex: 0 before 1
        SaveUndoPoint()
        Assign(r, c, v)       // updates line stats, pushes affected lines into Q
        if Propagate(Q):
            DFS(next_cell_index + 1)
        UndoToSavePoint()

DFS(0)
```

### 5.5 Pseudocode: Decompression with DI

```text
Algorithm 2: DecompressWithDI(C')
Input:  C' = (LH, DI, LSM, VSM, DSM, XSM)
Output: S_DI

k <- 0
for CSM in EnumerateSolutionsLex(C):
    if k == DI:
        return CSM
    k <- k + 1
error: DI out of range (no such solution)
```

### 5.6 Pseudocode: Compression with Time-Bounded DI Discovery

```text
Algorithm 3: Compress(CSM, max_compression_time)
Input:  original matrix CSM, time limit T
Output: compressed payload C' or FAIL

StartTimer()

Compute LSM, VSM, DSM, XSM from CSM
Compute LH[r] = SHA256(row_r) for all r    // per FIPS 180-4

k <- 0
for Y in EnumerateSolutionsLex(C):
    if ElapsedTime() > T:
        return FAIL
    if Y == CSM:
        DI <- k
        return (LH, DI, LSM, VSM, DSM, XSM)
    k <- k + 1

// If enumeration ends without finding CSM, the constraints are inconsistent
return FAIL
```

This specification makes the compressor and decompressor behavior deterministic and compatible by construction. The only
non-deterministic externality is wall-clock time in compression failure; the output is either produced with a correct DI
or not produced at all.

## 6. Apple Silicon Implementation Strategy (CPU and GPU)

### 6.1 Design Goals and Constraints

Apple Silicon systems provide high-performance ARM64 CPUs and integrated GPUs accessible through Metal. Metal is Apple's
low-overhead API supporting compute kernels, buffers, and command submission, with its programming model and shading
language specified in official documentation (Apple, n.d.-a; Apple, n.d.-b; Apple, n.d.-c). The implementation should
preserve determinism of enumeration and must interoperate exactly with the format's SHA-256 definition.

Given $s = 511$, the raw matrix is only $511^2 = 261{,}121$ bits ($\approx 32$ KiB). Thus, the challenge is not storage
but search control and propagation speed.

### 6.2 CPU-Centric Core for Determinism

Branching search is inherently irregular: different branches cause different propagation cascades, and backtracking
requires undo logs. Such control flow is typically better suited to CPUs than GPUs. On Apple Silicon, the CPU
implementation can store each row as a 512-bit aligned bitset (8 × 64-bit words) for fast operations, maintain a compact
unknown mask per row, keep per-line residuals $(u, a, \rho)$ in small arrays, and maintain an undo stack recording
assignments and line-stat deltas for fast rollback.

Row-hash checking can use a constant-time, single-block SHA-256 implementation specialized for 511-bit messages, since
the padded message occupies exactly one 512-bit block under FIPS 180-4 rules. This can reduce overhead relative to a
general streaming hash, while remaining standards-compliant (NIST, 2015).

### 6.3 Bulk Propagation Primitives on GPU

Although full DFS is ill-suited to GPU execution, specific subroutines can be accelerated on the GPU, particularly when
the solver occasionally needs to recompute global statistics from scratch (e.g., after many incremental steps) or
evaluate many similar operations over structured memory.

Metal supports general compute workloads using buffers and kernels (Apple, n.d.-b). The Metal Shading Language is a
C++-family language for writing kernels (Apple, n.d.-c). Two GPU-appropriate tasks are:

1. **Line-stat recomputation:**
   Given current assignment/unknown masks, compute $u(L)$ and $a(L)$ for all $4s$ lines in parallel, then derive
   $\rho(L)$. This is dominated by reductions and bit-counts, which map well to GPU SIMD execution.

2. **Forced-assignment detection:**
   For each line, test $\rho(L) = 0$ or $\rho(L) = u(L)$ and mark affected cells as forced. Because each line touches
   511 cells, naive marking can be bandwidth-heavy; however, GPU kernels can emit "force proposals" into a buffer, which
   the CPU can merge deterministically (e.g., sort by cell index, resolve conflicts, then apply in fixed order).

Metal Performance Shaders (MPS) provides optimized kernels and guidance for efficient GPU compute pipelines (Apple,
n.d.-d). While MPS is not a direct CSP solver, its existence supports a design that offloads well-structured compute to
the GPU while keeping the solver's control logic on the CPU.

### 6.4 Deterministic CPU--GPU Coordination

To preserve determinism, the CPU must remain the single source of truth for:

- the branching order,
- the assignment order,
- conflict detection,
- undo/redo logic.

GPU kernels should be used only to compute *pure functions* of the current state (e.g., recomputed counts) and return
results that the CPU applies in a strictly defined order. This avoids non-deterministic race effects.

A typical pattern is:

1. CPU performs incremental propagation for a bounded number of steps.
2. CPU dispatches a GPU "audit" kernel to recompute line stats and optionally propose forced assignments.
3. CPU verifies consistency with its incremental state (debug mode) or uses the recomputed stats (release mode) as
   authoritative, then continues DFS.

This division exploits Apple Silicon's GPU for throughput while ensuring reproducible enumeration.

### 6.5 Practical Engineering of the Row Hash

As defined in Section 2.3, each row is hashed as a 512-bit (64-byte) message formed by appending a fixed trailing zero
bit to the 511-bit row. This ensures byte alignment, eliminating the need for bit-precise message construction. The
format must still define bit order within bytes (MSB-first by convention) to guarantee interoperability across
implementations.

FIPS 180-4's padding procedure appends a single '1' bit, then zeros, then the 64-bit message length (NIST, 2015). For a
512-bit message, the padded result occupies exactly two 512-bit blocks (1024 bits total). Because the input is
byte-aligned, standard SHA-256 library functions can be used directly without custom bit-level padding logic. This
simplifies implementation and ensures that LH behaves as a stable, interoperable constraint.

## 7. Complexity, Correctness, and the Role of Time-Bounded Failure

### 7.1 Correctness Under the Model

If `Compress` returns a payload $\mathcal{C}'$, then by construction $\mathcal{C}$ matches the sums and row hashes
computed from the original $CSM$, and $DI$ is the position of $CSM$ in the canonical enumeration of feasible solutions.
Thus the decompressor returns exactly $CSM$ when it enumerates in the same canonical order. The scheme shifts the burden
of ambiguity resolution into the DI and makes decompression deterministic even when $\mathcal{C}$ admits multiple
feasible reconstructions.

### 7.2 Why a Single Time Tolerance Is Defensible

Because solution enumeration can be intrinsically expensive in combinatorial systems, a time-bounded failure mode is a
rational design choice. Complexity theory provides strong reasons to avoid promising polynomial-time enumeration in
general (Valiant, 1979). Likewise, the AllSAT literature emphasizes that enumerating solutions is a distinct problem
class with its own algorithmic challenges (Kobler et al., 2025).

By specifying one global parameter `max_compression_time`, the system avoids multiple interacting limits that are hard
to reason about, gives users a straightforward "works/doesn't work" operational contract, and makes performance tunable
to available hardware, including Apple Silicon.

### 7.3 Security Posture of the Hash Constraint

The LH vector is not used here as a cryptographic authentication mechanism; it is used as a *constraint* that ties each
row's bits to a digest. Nonetheless, its intended uniqueness relies on standard hash security properties, standardized
in FIPS 180-4 and discussed in NIST guidance (NIST, 2015; Dang, 2012). An adversary able to engineer row-level
second-preimages under the sum constraints could induce ambiguity, but the DI mechanism still guarantees deterministic
decoding. Therefore, the DI reduces the system's reliance on cryptographic uniqueness for *correctness* (though not for
performance).

## 8. Relationship to Belief Propagation and Hybrid Inference

Message-passing methods (BP, generalized BP) provide approximate marginal beliefs on factor graphs and are widely used
when exact inference is intractable (Kschischang et al., 2001; Yedidia et al., 2002; Wainwright & Jordan, 2008). In the
CRSCE setting, BP can serve as a *heuristic guide* for branching: prioritize cells with extreme beliefs, or prefer
assignments that maximize local consistency scores.

However, two constraints dominate the design:

1. **Hard feasibility:**
   Exact sums must be satisfied, so residual-based propagation and feasibility checks remain primary.

2. **Deterministic enumeration:**
   The canonical DI requires a fixed, spec-defined exploration order; any BP-guided branching must not change the
   enumeration order if the DI semantics depend on lex order.

A practical compromise is to keep the enumeration order lexicographic (to define the DI), but use BP only for
*non-semantic optimizations* such as early detection of contradictions, or as an auxiliary scoring function that affects
pruning decisions without changing the canonical traversal. This preserves the meaning of the DI while allowing
heuristic acceleration.

## 9. Implementation Standards

### 9.1 Object-Oriented Design Requirements

The CRSCE compressor and decompressor must be implemented using pure object-oriented programming (OOP) with
polymorphism. This requirement ensures that the codebase remains extensible, testable, and maintainable as the algorithm
evolves. Specifically, this requirement ensures that new compressors and decompression solvers can be added or removed
over time. Concrete components --- the constraint propagation engine, the branching strategy, the hash verification
layer, and the enumeration controller --- should each be encapsulated behind well-defined interfaces, allowing
alternative implementations to be substituted without modifying the solver's control logic.

### 9.2 Polymorphism as an Architectural Principle

Polymorphism is not merely a stylistic preference; it is a structural requirement motivated by the algorithm's design.
The solver operates over four families of cross-sum constraints (LSM, VSM, DSM, XSM) that differ in their indexing
geometry but share identical residual-tracking and forcing logic. A polymorphic line-constraint interface allows all
four families to be processed uniformly by the propagation engine, eliminating redundant code paths and reducing the
surface area for implementation errors.

Similarly, the hash verification layer should be abstracted behind a common interface. Although the current
specification uses SHA-256, the design discussed in Appendix B.1 (segmented prefix hashes) and potential future adoption
of alternative hash functions both argue for a pluggable hash interface. Polymorphism ensures that such changes can be
introduced without restructuring the solver.

### 9.3 Interface Boundaries

The implementation should define clear interface boundaries between at minimum the following components: the constraint
store (managing line statistics $u$, $a$, $\rho$ and cell assignments), the propagation engine (applying forcing rules
and detecting infeasibility), the branching controller (selecting the next unassigned cell and managing the undo stack),
the hash verifier (computing and comparing row digests), and the enumeration controller (coordinating DFS traversal and
yielding solutions in canonical order). Each component should be independently testable against its interface contract,
enabling unit-level verification of correctness properties that are difficult to assess at the system level.

### 9.4 Rationale for Pure OOP

Procedural or mixed-paradigm implementations tend to couple algorithmic logic with data representation, making it
difficult to substitute components or introduce optimizations without cascading changes. For CRSCE, where the solver
must maintain strict determinism across implementations (Section 7.1) and where GPU-accelerated alternatives may replace
CPU-bound components (Section 6), polymorphic dispatch provides the natural mechanism for swapping implementations while
preserving behavioral contracts. The pure OOP requirement also facilitates conformance testing: any compliant
implementation can be validated against the interface specifications independently of its internal strategy.

## 10. Object Design

Section 9 established the requirement for pure object-oriented design with polymorphism. This section specifies the
top-level architecture, entry points, and principal types for the CRSCE compressor and decompressor programs.

### 10.1 Entry Points

The compressor and decompressor are separate programs sharing a common library of core types and solver components. Each
program defines a single entry point:

- `compress(input_stream, output_stream) → FAIL`
  reads raw input, partitions it into fixed-size blocks, compresses each block independently, and writes the compressed
  payload stream. The per-block time limit `max_compression_time` is read from the `MAX_COMPRESSION_TIME` environment
  variable (Appendix A.4), defaulting to 1,800 seconds. If any block exceeds this limit, the entire operation fails.

- `decompress(input_stream, output_stream) → ERROR`
  reads a compressed payload stream, reconstructs each block by solving the constraint system to $S_{\text{DI}}$, and
  writes the recovered bitstream. Decompression is deterministic and unbounded in time; it either succeeds or reports a
  malformed payload.

### 10.2 Core Data Types

The following value types represent the fundamental data structures of the CRSCE format.

**`CSM`** encapsulates the $s \times s$ binary matrix. Internally, each row is stored as a 512-bit aligned bitset (8 ×
64-bit words), supporting fast bitwise operations, popcount, and row extraction. The matrix provides indexed access via
$(r, c)$ and row-major serialization via $\text{vec}(CSM)$.

```text
+--------------------------------------+
|                CSM                   |
+--------------------------------------+
| - rows: uint64[s][8]                 |
| - s: uint16                          |
+--------------------------------------+
| + CSM(s: uint16)                     |
| + get(r: uint16, c: uint16): bit     |
| + set(r: uint16, c: uint16, v: bit)  |
| + getRow(r: uint16): uint64[8]       |
| + vec(): bitstring                   |
| + popcount(r: uint16): uint16        |
+--------------------------------------+
```

**`CrossSumVector`** is an abstract base type representing an ordered sequence of integer sums. The constructor accepts
the number of elements (size). The base type defines `set(r, c, v)` to accumulate a cell's value into the appropriate
sum and `get(r, c)` to retrieve the current sum for the line containing cell $(r, c)$. Each concrete subtype determines
which index to use: `RowSum` (LSM) indexes by $r$ only, `ColumnSum` (VSM) indexes by $c$ only, `DiagonalSum` (DSM)
indexes by both $r$ and $c$ via $d = c - r + (s - 1)$, and `AntiDiagonalSum` (XSM) indexes by both via $x = r + c$. The
base type also defines `cells(r, c)` returning the set of $(r, c)$ indices participating in the same line as the given
cell, with each subtype resolving the line identity from $r$, $c$, or both as appropriate. Finally, `len(k)` returns the
number of cells on line $k$, and the encoding width per element is derived from `len(k)`.

```text
  +------------------------------------------------+
  |       <<abstract>> CrossSumVector              |
  +------------------------------------------------+
  | # sums: uint16[]                               |
  | # size: uint16                                 |
  +------------------------------------------------+
  | + CrossSumVector(size: uint16)                 |
  | + set(r: uint16, c: uint16, v: bit): void      |
  | + get(r: uint16, c: uint16): uint16            |
  | + cells(r: uint16, c: uint16): set<(r,c)>      |
  | + len(k: uint16): uint16                       |
  | # lineIndex(r: uint16, c: uint16): uint16      |
  +------------------------------------------------+
          ^        ^        ^                   ^
          |        |        |                   |
  +-------+--+ +---+----+ +-+-----------+ +-----+-------+
  | RowSum   | | ColSum | |   DiagSum   | | AntiDiagSum |
  | (LSM)    | | (VSM)  | |   (DSM)     | | (XSM)       |
  +----------+ +--------+ +-------------+ +-------------+
  | index: r | | index:c| | d=c-r+(s-1) | | x=r+c       |
  +----------+ +--------+ +-------------+ +-------------+
```

**`LateralHash`** stores the vector of $s$ SHA-256 digests, one per row. It provides `compute(row_r)` to hash a 512-bit
row message (511 data bits plus trailing zero) and `verify(r, digest)` to compare against the stored value.

```text
+----------------------------------------------+
|              LateralHash                     |
+----------------------------------------------+
| - digests: uint8[s][32]                      |
| - s: uint16                                  |
+----------------------------------------------+
| + LateralHash(s: uint16)                     |
| + compute(row: uint64[8]): uint8[32]         |
| + store(r: uint16, digest: uint8[32]): void  |
| + verify(r: uint16, digest: uint8[32]): bool |
| + getDigest(r: uint16): uint8[32]            |
+----------------------------------------------+
```

**`CompressedPayload`** represents $\mathcal{C}' = (\text{LH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM},
\text{XSM})$ and provides serialization and deserialization methods that enforce the wire format byte-alignment
convention (Section 12.4): LH (byte-aligned), DI (uint8), followed by the bit-packed cross-sum vectors.

```text
+----------------------------------------------+
|           CompressedPayload                  |
+----------------------------------------------+
| - lh: LateralHash                           |
| - di: uint8                                  |
| - lsm: RowSum                               |
| - vsm: ColSum                               |
| - dsm: DiagSum                               |
| - xsm: AntiDiagSum                          |
+----------------------------------------------+
| + serialize(): bytestream                    |
| + deserialize(data: bytestream): void        |
| + getConstraintSet(): C                      |
| + getDI(): uint8                             |
+----------------------------------------------+
```

### 10.3 Solver Components

The solver architecture follows the interface boundaries defined in Section 9.3. Each component is an abstract interface
with one or more concrete implementations. The interfaces use several auxiliary types: `LineID` is an opaque identifier
for a specific line (row, column, diagonal, or anti-diagonal) within the constraint system; `CellState` is an
enumeration over `{UNASSIGNED, ZERO, ONE}`; `UndoToken` is an opaque handle returned by the branching controller to mark
a restore point on the undo stack; and `Assignment` is a record of `(r, c, value)` representing a single cell
assignment.

**`IConstraintStore`** manages the current partial assignment of the $s^2$ cells and the per-line statistics $(u, a,
\rho)$ for all $6s - 2$ lines. It is the single source of truth for the solver's state.

```text
+--------------------------------------------------+
|       <<interface>> IConstraintStore             |
+--------------------------------------------------+
| + assign(r: uint16, c: uint16, v: bit): void     |
| + unassign(r: uint16, c: uint16): void           |
| + getResidual(line: LineID): uint16              |
| + getUnknownCount(line: LineID): uint16          |
| + getAssignedCount(line: LineID): uint16         |
| + getLinesForCell(r: uint16, c: uint16): LineID[4]|
| + getCellState(r: uint16, c: uint16): CellState  |
+--------------------------------------------------+
```

**`IPropagationEngine`** applies forcing rules to a queue of affected lines until quiescence or infeasibility. When
$\rho(L) = 0$, it forces all unknowns on $L$ to 0; when $\rho(L) = u(L)$, it forces them to 1. It returns a boolean
indicating feasibility and records all forced assignments for undo purposes.

```text
+--------------------------------------------------+
|       <<interface>> IPropagationEngine           |
+--------------------------------------------------+
| + propagate(queue: LineID[]): bool               |
| + getForcedAssignments(): Assignment[]           |
| + reset(): void                                  |
+--------------------------------------------------+
```

**`IBranchingController`** selects the next unassigned cell in row-major order, manages the undo stack (save points,
rollback), and controls the branching order (0 before 1) to ensure canonical lexicographic enumeration.

```text
+--------------------------------------------------+
|       <<interface>> IBranchingController         |
+--------------------------------------------------+
| + nextCell(): (r: uint16, c: uint16) | null      |
| + saveUndoPoint(): UndoToken                     |
| + undoToSavePoint(token: UndoToken): void        |
| + branchOrder(): bit[]                           |
+--------------------------------------------------+
```

**`IHashVerifier`** abstracts the row-hash computation and comparison. The default implementation uses SHA-256 per FIPS
180-4 on 512-bit row messages. The interface admits alternative implementations (e.g., segmented prefix hashes per
Appendix B.1) without changing the solver's control flow.

```text
+--------------------------------------------------+
|       <<interface>> IHashVerifier                |
+--------------------------------------------------+
| + computeHash(row: uint64[8]): uint8[32]        |
| + verifyRow(r: uint16, row: uint64[8]): bool    |
| + setExpected(r: uint16, digest: uint8[32]): void|
+--------------------------------------------------+
```

**`IEnumerationController`** coordinates the DFS traversal by composing the constraint store, propagation engine,
branching controller, and hash verifier. It implements the `EnumerateSolutionsLex` generator (Algorithm 1), yielding
feasible solutions in canonical order. The compressor wraps this in a timed loop (Algorithm 3); the decompressor wraps
it in an indexed selection (Algorithm 2).

```text
+--------------------------------------------------+
|       <<interface>> IEnumerationController       |
+--------------------------------------------------+
| - store: IConstraintStore                        |
| - propagator: IPropagationEngine                 |
| - brancher: IBranchingController                 |
| - hasher: IHashVerifier                          |
+--------------------------------------------------+
| + enumerateSolutionsLex(): generator<CSM>        |
+--------------------------------------------------+
```

### 10.4 Compressor and Decompressor as Compositions

**`Compressor`** composes a `CSM`, a `CompressedPayload` builder, and an `IEnumerationController`. It computes the
cross-sum vectors and lateral hashes from the original matrix, then invokes the enumerator to discover the DI within the
time bound. On success, it assembles and serializes $\mathcal{C}'$.

```text
+--------------------------------------------------+
|                Compressor                        |
+--------------------------------------------------+
| - original: CSM                                  |
| - payload: CompressedPayload                     |
| - enumerator: IEnumerationController             |
+--------------------------------------------------+
| + Compressor(enumerator: IEnumerationController) |
| + compress(input: stream, output: stream): void  |
|            | FAIL                                 |
+--------------------------------------------------+
```

**`Decompressor`** composes a `CompressedPayload` parser and an `IEnumerationController`. It deserializes
$\mathcal{C}'$, reconstructs the constraint set $\mathcal{C}$, and invokes the enumerator to produce $S_{\text{DI}}$,
which it then deserializes into the recovered bitstream.

```text
+--------------------------------------------------+
|               Decompressor                       |
+--------------------------------------------------+
| - payload: CompressedPayload                     |
| - enumerator: IEnumerationController             |
+--------------------------------------------------+
| + Decompressor(enumerator: IEnumerationController)|
| + decompress(input: stream, output: stream): void|
|              | ERROR                              |
+--------------------------------------------------+
```

Both classes depend only on the abstract interfaces defined in Section 10.3, not on concrete implementations. This
ensures that the compressor and decompressor can be tested independently, and that alternative solver strategies (e.g.,
GPU-accelerated propagation per Section 6.3) can be injected without modifying the top-level control flow.

### 10.5 Observability (O11y)

Both the compressor and decompressor require structured observability to support debugging, performance analysis, and
operational monitoring. Rather than ad hoc logging scattered throughout the codebase, the design specifies a centralized
observability framework that records events asynchronously to a background thread, ensuring that instrumentation does
not block the solver's critical path.

**`TagValue`** is a discriminated union representing a single tag value. The supported types are: `string`, `bool`,
`int`, `int8`, `int16`, `int32`, `int64`, `uint`, `uint8`, `uint16`, `uint32`, `uint64`, `single`, and `double`. This
range covers all scalar types needed to describe solver state, performance counters, and diagnostic metadata without
resorting to string serialization of numeric values.

```text
+----------------------------------------------+
|              TagValue                        |
+----------------------------------------------+
| - type: enum { STRING, BOOL, INT, INT8,      |
|   INT16, INT32, INT64, UINT, UINT8, UINT16,  |
|   UINT32, UINT64, SINGLE, DOUBLE }           |
| - value: <union of above types>              |
+----------------------------------------------+
| + TagValue(v: string)                        |
| + TagValue(v: bool)                          |
| + TagValue(v: int64)                         |
| + TagValue(v: uint64)                        |
| + TagValue(v: double)                        |
| + ... (overloads for each supported type)    |
| + getType(): enum                            |
| + as<T>(): T                                 |
+----------------------------------------------+
```

**`Tag`** is a key-value pair where the key is a string and the value is a `TagValue`.

```text
+----------------------------------------------+
|                  Tag                         |
+----------------------------------------------+
| - key: string                                |
| - value: TagValue                            |
+----------------------------------------------+
| + Tag(key: string, value: TagValue)          |
| + getKey(): string                           |
| + getValue(): TagValue                       |
+----------------------------------------------+
```

**`Event`** is a structured, timestamped, tagged record. The timestamp is captured at the moment `event()` is called,
before the record is dispatched to the background thread. This guarantees that event timestamps reflect the caller's
wall-clock time, not the processing thread's dequeue time.

```text
+----------------------------------------------+
|                 Event                        |
+----------------------------------------------+
| - timestamp: uint64 (nanoseconds)            |
| - eventName: string                          |
| - tags: Tag[]                                |
+----------------------------------------------+
| + Event(name: string, tags: Tag[])           |
| + getTimestamp(): uint64                     |
| + getEventName(): string                     |
| + getTags(): Tag[]                           |
+----------------------------------------------+
```

**`IO11y`** is the observability interface. It provides `event()` for recording structured events and `counter()` for
incrementing named counters. The `event()` method captures the current timestamp, constructs an `Event` record, and
dispatches it asynchronously to a background processing thread. The `counter()` method accepts a counter name and
asynchronously passes it to the background thread, where a `map<string, int64>` maintains the running totals; each call
increments the named counter by one. The background thread is responsible for serialization, filtering, and output
(e.g., structured JSON to a log sink, metrics aggregation, or trace export). All formatting and routing decisions are
deferred to the background processor, keeping the caller's instrumentation to a single non-blocking call per event or
counter increment.

```text
+----------------------------------------------+
|         <<interface>> IO11y                  |
+----------------------------------------------+
| + event(name: string, tags: Tag[]): void     |
| + counter(name: string): void                |
| + flush(): void                              |
| + shutdown(): void                           |
+----------------------------------------------+
```

The `flush()` method blocks until all queued events and pending counter increments have been processed, and `shutdown()`
drains the queue and terminates the background thread. These are intended for use at program exit or during testing, not
during normal solver operation. Counter values can be retrieved after `flush()` for reporting or exported as tagged
events by the background processor.

**Initialization and lifecycle.** When either the compressor or decompressor program starts, it initializes the O11y
background thread before any solver work begins. The initialization generates a UUID `run_id` that uniquely identifies
the current execution. Every event and counter record produced during that execution is tagged with this `run_id`,
enabling downstream tools to segment, filter, and correlate observability data by individual runs.

The background thread writes to the file specified by the `OBSERVABILITY_LOG_FILE` environment variable. If the variable
is not set, the default path is `./log/<program_name>.log`, where `<program_name>` is `compress` or `decompress` as
appropriate. The log directory is created if it does not exist. The file is opened in append-only mode, ensuring that
records from prior runs are never overwritten and that concurrent or sequential executions accumulate a complete audit
trail. Each record is written as a single JSON object followed by a newline (JSON-L format). Event records contain the
fields `run_id`, `timestamp`, `type: "event"`, `event_name`, and `tags` (an object of key-value pairs). Counter records
contain `run_id`, `timestamp`, `type: "counter"`, `counter_name`, and `value` (the current int64 total). No record is
ever modified or deleted after being written.

The background thread runs for the lifetime of the program and periodically flushes queued events and pending counter
snapshots to disk on a configurable interval. The flush interval is determined by the `OBSERVABILITY_FLUSH_INTERVAL`
environment variable (Appendix A.4), which specifies the period in milliseconds. If the variable is not set, the default
interval is 1,000 ms. Between periodic flushes, the background thread processes enqueued messages as they arrive; the
periodic flush ensures that even during idle periods, any buffered records are written to disk within a bounded time
window. At program termination, `shutdown()` is called to drain the queue, execute a final flush, and close the log
file. If the program terminates abnormally, any records already flushed remain available in the log file for post-mortem
analysis.

```text
+----------------------------------------------+
|              O11yBackgroundThread             |
+----------------------------------------------+
| - runId: UUID                                |
| - logFile: file_handle                       |
| - queue: concurrent_queue<O11yMessage>       |
| - counters: map<string, int64>               |
| - programName: string                        |
| - flushIntervalMs: uint32                    |
+----------------------------------------------+
| + O11yBackgroundThread(programName: string)  |
| + getRunId(): UUID                           |
| + enqueue(msg: O11yMessage): void            |
| + run(): void                                |
| + drain(): void                              |
+----------------------------------------------+
```

The `O11yBackgroundThread` is an implementation detail, not part of the public `IO11y` interface. Callers interact only
with `IO11y`; the background thread is created and managed by the concrete `IO11y` implementation provided at program
startup.

All solver components (Section 10.3) and the top-level `Compressor` and `Decompressor` classes (Section 10.4) accept an
`IO11y` instance via constructor injection. This allows observability to be disabled entirely (via a no-op
implementation) for benchmarking, or redirected to different sinks for testing and production without modifying any
solver logic.

### 10.6 Argument Parser

Both binaries delegate argument parsing to a shared `ArgParser` class (Section 11.3, `common/ArgParser/`). The parser
processes command-line tokens left-to-right, recognizing `-in`, `-out`, `-h`, and `--help`. Unknown flags or a missing
value after `-in` or `-out` produce a parse error. Filesystem validation occurs after parsing: the input path is checked
for existence via `stat()`, and the output path is checked for non-existence. Failures raise typed exceptions
(`CliInputMissing`, `CliOutputExists`, `CliParseError`, `CliHelpRequested`, `CliNoArgs`) that the binary's `main()` maps
to the appropriate exit code and error message (Appendix A.3).

**`Options`** is a value type holding the parsed result of command-line arguments.

```text
+----------------------------------------------+
|                 Options                      |
+----------------------------------------------+
| + input: string                              |
| + output: string                             |
| + help: bool                                 |
+----------------------------------------------+
```

**`ArgParser`** encapsulates token-by-token parsing and filesystem validation. The two-argument constructor parses
immediately and throws on any error condition; the one-argument constructor defers parsing to an explicit `parse()`
call.

```text
+--------------------------------------------------+
|                  ArgParser                       |
+--------------------------------------------------+
| - programName: string                            |
| - opts: Options                                  |
+--------------------------------------------------+
| + ArgParser(programName: string)                 |
| + ArgParser(programName: string,                 |
|             args: span<char*>)                   |
| + parse(args: span<char*>): bool                 |
| + options(): const Options&                      |
| + usage(): string                                |
+--------------------------------------------------+
```

The `parse()` method returns `true` if the arguments are valid or the help flag is set, and `false` on unknown flags or
missing values. The two-argument constructor additionally validates the filesystem state and throws one of five typed
exceptions: `CliNoArgs` (no arguments provided), `CliHelpRequested` (help flag present), `CliParseError` (unknown flag
or missing value), `CliInputMissing` (input file does not exist), or `CliOutputExists` (output file already exists). All
five inherit from `CrsceException`.

## 11. Project Structure and Standards

The CRSCE implementation is organized as a C++23 CMake project targeting Apple Silicon. The directory layout separates
binary entry points, core implementation, public headers, tests, and build infrastructure into distinct top-level
directories. This section documents the canonical project structure to which all implementations must conform.

### 11.1 Top-Level Layout

```text
crsce/
├── cmd/                  Binary entrypoints
├── src/                  Core implementation
├── include/              Public headers (mirrors src/)
├── test/                 CTest suites
├── cmake/                Build system
│   ├── root.cmake        Root CMake orchestration
│   ├── pipeline/         Build/lint/test/cover stages
│   └── projects/         Per-binary CMake files
├── tools/                Standalone utilities and scripts
├── Makefile              Developer workflow targets
├── Makefile.d/           Makefile includes
├── CMakeLists.txt        Root CMake configuration
└── CMakePresets.json     Preset definitions
```

### 11.2 Binary Entrypoints (`cmd/`)

Each subdirectory under `cmd/` contains a `main.cpp` that instantiates the top-level class (Section 10.4) and wires
together the concrete implementations of the solver interfaces (Section 10.3). The two primary binaries are `compress`
and `decompress`. Additional utilities support testing and diagnostics.

```text
cmd/
├── compress/             Compression binary
├── decompress/           Decompression binary
├── hasher/               Standalone hashing utility
├── binDumper/            Binary dump inspector
├── testRunnerZeroes/     Test runner: all-zeros patterns
├── testRunnerOnes/       Test runner: all-ones patterns
├── testRunnerRandom/     Test runner: random patterns
├── testRunnerAlternating01/  Test runner: alternating 0/1
└── testRunnerAlternating10/  Test runner: alternating 1/0
```

### 11.3 Core Implementation (`src/`)

The `src/` directory is partitioned into three modules: `Compress/`, `Decompress/`, and `common/`. Each module maps
directly to the object design in Section 10.

```text
src/
├── Compress/             Compression pipeline
│   └── Cli/              Compress CLI handling
├── Decompress/           Decompression pipeline
│   ├── Cli/              Decompress CLI handling
│   ├── Decompressor/     Main decompressor orchestration
│   ├── Solvers/          Solver wrappers and selection
└── common/               Shared modules
    ├── ArgParser/         Command-line argument parsing
    ├── BitHashBuffer/     Bit buffer with hashing
    ├── Cli/               Common CLI utilities
    ├── Csm/               CSM matrix
    ├── CrossSum/          Cross-sum data structures
    ├── FileBitSerializer/ Bit-level file I/O
    ├── Format/            Format serialization
    │   └── CompressedPayload/ Compressed block payload (Section 10.2)
    ├── HasherUtils/       Hashing utilities
    ├── LateralHash/       Lateral hash data structures
    ├── O11y/              Observability (Section 10.5)
    ├── RowHashVerifier/   SHA-256 row hash verification
    ├── Util/              General utilities
    └── exceptions/        Exception definitions
```

### 11.4 Public Headers (`include/`)

The `include/` directory mirrors the `src/` structure exactly. Each class or interface defined in `src/` has a
corresponding header in `include/` under the same relative path. This separation enforces a clean distinction between
public API and private implementation, and allows dependent modules to include headers without exposing implementation
details.

```text
include/
├── Compress/             Compression pipeline headers
│   └── Cli/              Compress CLI handling headers
├── Decompress/           Decompression pipeline headers
│   ├── Cli/              Decompress CLI handling headers
│   ├── Decompressor/     Main decompressor orchestration headers
│   ├── Solvers/          Solver wrappers and selection headers
└── common/               Shared modules headers
    ├── ArgParser/         Command-line argument parsing headers
    ├── BitHashBuffer/     Bit buffer with hashing headers
    ├── Cli/               Common CLI utilities headers
    ├── Csm/               CSM matrix headers
    ├── CrossSum/          Cross-sum data structures headers
    ├── FileBitSerializer/ Bit-level file I/O headers
    ├── Format/            Format serialization headers
    │   └── CompressedPayload/ Compressed block payload headers
    ├── HasherUtils/       Hashing utilities headers
    ├── LateralHash/       Lateral hash data structures headers
    ├── O11y/              Observability (Section 10.5) headers
    ├── RowHashVerifier/   SHA-256 row hash verification headers
    ├── Util/              General utilities headers
    └── exceptions/        Exception definitions headers
```

### 11.5 Test Suites (`test/`)

Tests are organized by module and granularity. Each test file defines exactly one test function, registered in
`test/CMakeLists.txt`. Tests require both happy-path and failure-path coverage, and the project enforces a minimum of
95% line coverage via `make cover`.

```text
test/
├── Compress/             Compression pipeline tests
│   └── Cli/              Compress CLI handling tests
├── Decompress/           Decompression pipeline tests
│   ├── Cli/              Decompress CLI handling tests
│   ├── Decompressor/     Main decompressor orchestration tests
│   ├── Solvers/          Solver wrappers and selection tests
└── common/               Shared modules tests
    ├── ArgParser/         Command-line argument parsing tests
    ├── BitHashBuffer/     Bit buffer with hashing tests
    ├── Cli/               Common CLI utilities tests
    ├── Csm/               CSM matrix tests
    ├── CrossSum/          Cross-sum data structures tests
    ├── FileBitSerializer/ Bit-level file I/O tests
    ├── Format/            Format serialization tests
    │   └── CompressedPayload/ Compressed block payload tests
    ├── HasherUtils/       Hashing utilities tests
    ├── Helpers/           Helper functions for tests
    ├── LateralHash/       Lateral hash data structures tests
    ├── O11y/              Observability (Section 10.5) tests
    ├── RowHashVerifier/   SHA-256 row hash verification tests
    ├── Util/              General utilities tests
    └── exceptions/        Exception definitions tests
```

Test files are preceded with `unit_`, `integration_` and `e2e_`

### 11.6 Build System (`cmake/`)

The build system uses CMake with Ninja generator and C++23.

Presets are defined in `CMakePresets.json` and include `llvm-debug`, `llvm-release`, `arm64-debug`, `arm64-release`.

The Makefile auto-selects `llvm-debug` when Homebrew LLVM is available, otherwise `cmake-build-debug`.

The `cmake/pipeline/` directory contains modular build stages (build, clean, configure, lint, test, cover), and
`cmake/projects/` contains one CMake file per binary target.

The CMake implementation should remain modular using `*.cmake` files to keep the code DRY and readable.

There is a `Makefile` and smaller `Makefile.d/*.mk` files which provide a thin wrapper around CMake.  Any `make` target
should call CMake, where the majority of logic should be expressed.

```bash
make ready           # verify toolchain prerequisites
make ready/fix       # install missing prerequisites (Homebrew, LLVM, Python venv, Node)
make clean           # delete and recreate build/
make configure       # configure CMake (default preset: llvm-debug)
make build           # build all presets (auto-configures each)
make build/one       # build only PRESET (e.g., make build/one PRESET=llvm-debug)
make lint            # run all linters (md, sh, py, cpp/cppcheck + clang-tidy)
make test            # run CTest suite for PRESET
make cover           # enforce >=95% line coverage
make test/zeroes            # all-zeros input patterns
make test/ones              # all-ones input patterns
make test/random            # random input patterns
make test/alternating01     # alternating 0/1 patterns
make test/alternating10     # alternating 1/0 patterns
```

### 11.7 Coding Conventions

All source files use C++23, spaces only (no tabs), and a 120-column soft wrap. Every file requires a Doxygen-style
header:

```cpp
/**
 * @file <filename>
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 */
```

All Classes, Types, Functions, Methods and TEST(...) declarations and definitions are immediately preceded by a
Doxygen-style header:

```cpp
/**
 * @name myfunc
 * @brief example function
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
```

Class properties should be immediately preceded by a Doxygen-style header:

```cpp
/**
 * @name propName
 * @brief property description
 */
```

For any function or method which throws an exception, the Doxygen-style header should provide `@throws <exception>` as
well.

The following general rules apply to the entire project (and are enforced by the linter):

1. Entities are declared in .h files and defined in .cpp files.
2. Only one entity declared per .h file
3. Only one entity defined per .cpp file
4. Each test file defines exactly one test function.
5. The project enforces linting via `make lint` (Markdown, shell, Python, C++ via cppcheck and clang-tidy).
6. Test coverage is enforced via `make cover` at a 95% minimum threshold.

### 11.8 Exceptions

The project must not use generic exceptions such as `std::runtime_error()` directly. Instead, the project defines a
single base exception class, `CrsceException`, which inherits from `std::runtime_error`, and all application-specific
exceptions derive from it. This enables callers to catch `CrsceException` for uniform handling or catch a specific
derived type for targeted recovery. All exception classes are header-only and reside in `common/exceptions/` (Sections
11.3--11.5).

```text
std::runtime_error
└── CrsceException
    ├── CliNoArgs                      No CLI arguments provided
    ├── CliHelpRequested               Help flag (-h/--help) detected
    ├── CliParseError                  Unknown flag or missing value
    ├── CliInputMissing                Input file does not exist
    ├── CliOutputExists                Output file already exists
    ├── CompressTimeoutException       Compression exceeded time limit
    ├── CompressNonZeroExitException   Compress subprocess non-zero exit
    ├── DecompressTimeoutException     Decompression exceeded time limit
    ├── DecompressNonZeroExitException Decompress subprocess non-zero exit
    └── PossibleCollisionException     Decompressed output hash mismatch
```

The following requirements apply to all exception usage in the project:

1. Every exception thrown must be a `CrsceException` subtype; no raw `std::runtime_error`, `std::logic_error`, or other
   standard library exceptions may be thrown directly.
2. Each exception class occupies its own header file under `common/exceptions/`, following the one-entity-per-file
   convention (Section 11.7).
3. Exception classes follow the Rule of Zero, relying on `std::runtime_error` for copy/move semantics.
4. Every function or method that throws must document the exception in its Doxygen header using `@throws` (Section
   11.7).
5. Exception messages must be descriptive and include contextual information (e.g., the offending file path for
   `CliInputMissing`).

## 12. File Format Specification

This section specifies the on-disk layout for CRSCE v1 files. A CRSCE file consists of a single file header followed by
one or more compressed blocks. All multi-byte integer fields are little-endian unless otherwise noted. The format is
designed so that two independent implementations producing the same compressed output from the same input will generate
byte-identical files.

### 12.1 File Header

The file header is exactly 28 bytes and contains the metadata needed to parse and validate the remainder of the file.

```text
Offset  Size     Type     Field                    Description
------  -------  -------  -----------------------  ------------------------------------
 0       4       char[4]  magic                    ASCII "CRSC" (0x43 0x52 0x53 0x43)
 4       2       uint16   version                  Format version (1)
 6       2       uint16   header_bytes             Header size in bytes (28)
 8       8       uint64   original_file_size_bytes Original uncompressed file size
16       8       uint64   block_count              Number of compressed blocks
24       4       uint32   header_crc32             IEEE CRC-32 over bytes 0--23
```

The `header_crc32` field provides integrity verification for the header itself. A decoder must reject any file whose
CRC-32 does not match the computed value over the first 24 bytes.

### 12.2 Block Count and Last-Block Padding

Each block encodes exactly one $511 \times 511$ CSM derived from input bits. The number of blocks is determined by the
original file size:

$$
    \text{block\_count} = \left\lceil \frac{\text{original\_file\_size\_bytes} \times 8}{s^2} \right\rceil
$$

where $s = 511$ and $s^2 = 261{,}121$ bits per block. If the input bitstream does not fill the final block, the
remaining cells are zero-padded to produce a full $s \times s$ matrix. All 511 rows of the final block still generate LH
digests (hashing the zero-padded rows). During decompression, the `original_file_size_bytes` field determines how many
bits of the final block's reconstructed CSM contribute to the output; trailing zero-padded bits are discarded.

### 12.3 Bit Serialization Order

Input bytes are expanded to bits MSB-first: bit 7 of each byte is emitted first, bit 0 last. These bits fill the CSM in
row-major order, left-to-right within each row, top-to-bottom across rows. During decompression, the inverse mapping
applies: reconstructed CSM bits are packed MSB-first into output bytes.

Each row of the CSM contains $s = 511$ data bits. For hashing purposes, a fixed trailing zero bit is appended to form a
512-bit (64-byte) message, as defined in Section 2.3. This padding bit is not stored in the block payload; it is
implicit in the format.

### 12.4 Block Payload Layout

Each block payload is a fixed-length byte-aligned bitstream. The matrix dimension $s = 511$ is a format constant and is
not stored per block. The fields appear in the following order, matching the wire format tuple $\mathcal{C}' =
(\text{LH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM})$ defined in Section 4.2.

```text
Field   Elements    Bits/Element   Total Bits   Total Bytes   Encoding
------  ----------  ------------   ----------   -----------   --------
LH      511         256            130,816      16,352        32 bytes per digest, sequential
DI      1           8              8            1             uint8
LSM     511         9              4,599        ---           MSB-first packed bitstream
VSM     511         9              4,599        ---           MSB-first packed bitstream
DSM     2s-1=1,021  variable       8,185        ---           MSB-first, ceil(log2(len(d)+1))
XSM     2s-1=1,021  variable       8,185        ---           MSB-first, ceil(log2(len(x)+1))
------  ----------  ------------   ----------   -----------   --------
Total                              156,392      19,549
```

The total block payload is $156{,}392$ bits ($19{,}549$ bytes). The payload is naturally byte-aligned, so no trailing
padding bits are required. The total compressed file size is therefore $28 + (\text{block\_count} \times 19{,}549)$
bytes.

**LH (Lateral Hash).** The 511 SHA-256 digests are stored sequentially, each as 32 bytes, for a total of 16,352 bytes.
LH[0] corresponds to row 0, LH[510] to row 510.

**DI (Disambiguation Index).** An 8-bit unsigned integer selecting the intended solution from the canonical enumeration
(Section 4.2). Values 0--255 are valid; if the original matrix's position exceeds 255, compression fails.

**LSM and VSM (Row and Column Sums).** Each vector contains $s = 511$ elements. Each element requires $b = \lceil
\log_2(s + 1) \rceil = 9$ bits and is serialized MSB-first into a packed bitstream. The $511 \times 9 = 4{,}599$ bits
per vector are packed continuously; the final partial byte (if any) is zero-padded to the byte boundary only at the end
of the entire cross-sum region, not between individual vectors.

**DSM and XSM (Diagonal and Anti-Diagonal Sums).** Each vector contains $2s - 1 = 1{,}021$ elements corresponding to
straight (non-wrapping) diagonals. Element $k$ requires $\lceil \log_2(\text{len}(k) + 1) \rceil$ bits, where
$\text{len}(k) = \min(k + 1,\; s,\; 2s - 1 - k)$ as defined in Section 2.2. Elements are serialized MSB-first in index
order ($k = 0, 1, \ldots, 2s - 2$), packed continuously into the bitstream.

### 12.5 Cross-Sum Ranges and Validation

Each LSM and VSM entry must be in the range $[0, s]$ where $s = 511$. Each DSM entry at index $d$ must be in $[0,
\text{len}(d)]$, and each XSM entry at index $x$ must be in $[0, \text{len}(x)]$. A decoder should reject any block
containing out-of-range cross-sum values.

### 12.6 Block Acceptance Criteria

A block is accepted if and only if the reconstructed CSM simultaneously satisfies both conditions: it reproduces the
stored LSM, VSM, DSM, and XSM vectors exactly for all indices, and it recomputes per-row LH from the reconstructed rows
exactly ($\text{digest} = \text{SHA-256}(\text{row}_{64})$) for all $r \in \{0, \ldots, 510\}$. Any mismatch results in
rejection of that block. Decoders must fail hard by default and must not produce partial output.

### 12.7 File Size Validation

A decoder can validate file integrity before parsing blocks by checking:

$$
    \text{file\_size} = 28 + (\text{block\_count} \times 19{,}549)
$$

where `block_count` is read from the header. Any deviation indicates a truncated or corrupted file.

## 13. Conclusion

This paper formalized a disambiguated CRSCE decompression process that remains deterministic even if the base constraint
set admits multiple feasible reconstructions. The key idea is *canonical solution indexing*: define a strict
lexicographic order on feasible matrices, encode the zero-based DI of the intended solution as $S_{\text{DI}}$, and
discover that DI at compression time by running the decompressor as an enumerator constrained by the known original
matrix. The compressed payload $\mathcal{C}' = (\text{LH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM})$
is ordered for byte-alignment efficiency, with all fixed-width fields preceding the bit-packed cross-sum vectors.
Because enumeration can be expensive, the design includes exactly one tolerance parameter --- `max_compression_time` ---
beyond which compression fails, requiring a fallback to another algorithm.

We established that the probability of a non-unique reconstruction is approximately $10^{-40{,}199}$ under a
random-oracle model, combining the cross-sum collision barrier ($\sim 10^{-819}$) with the per-row SHA-256 collision
barrier ($\sim 10^{-39{,}380}$). Each row is hashed as a byte-aligned 512-bit message (the 511-bit row plus a fixed
trailing zero bit), simplifying implementation while preserving compliance with FIPS 180-4 (NIST, 2015).

We moved from theory to implementation by presenting a deterministic enumerator based on incremental feasibility
propagation for cardinality constraints, row-hash verification, and canonical branching to preserve lexicographic order.
For Apple Silicon, we argued for CPU-centric control flow with optional GPU acceleration for bulk recomputation tasks
using Metal, maintaining strict determinism by keeping all state changes and ordering decisions on the CPU.

The implementation is specified as pure object-oriented with polymorphism, ensuring that the constraint propagation
engine, hash verification layer, branching strategy, and enumeration controller are each encapsulated behind
well-defined interfaces. This architectural requirement supports extensibility, conformance testing, and the
substitution of alternative implementations --- including GPU-accelerated components --- without compromising the
determinism guarantees on which the DI mechanism depends.

## References

Apple. (n.d.-a). Metal developer documentation.

Apple. (n.d.-b). Performing calculations on a GPU (Metal).

Apple. (n.d.-c). Metal Shading Language Specification (PDF).

Apple. (n.d.-d). Metal Performance Shaders: Overview and tuning hints.

Dang, Q. (2012). Recommendation for applications using approved hash algorithms (NIST Special Publication 800-107
Revision 1). National Institute of Standards and Technology.

Jerrum, M., Sinclair, A., & Vigoda, E. (2004). A polynomial-time approximation algorithm for the permanent of a matrix
with nonnegative entries. *Journal of the ACM, 51*(4), 671--697.

Knuth, D. E. (2005). *The Art of Computer Programming, Volume 4, Fascicle 2: Generating All Tuples and Permutations*.
Addison-Wesley.

Kobler, J., et al. (2025). Disjoint projected enumeration for SAT and SMT without blocking clauses. *Artificial
Intelligence*.

Kschischang, F. R., Frey, B. J., & Loeliger, H.-A. (2001). Factor graphs and the sum-product algorithm. *IEEE
Transactions on Information Theory, 47*(2), 498--519.

National Institute of Standards and Technology. (2015). *Secure Hash Standard (SHS) (FIPS PUB 180-4)*.

Valiant, L. G. (1979). The complexity of computing the permanent. *Theoretical Computer Science, 8*(2), 189--201.

Wainwright, M. J., & Jordan, M. I. (2008). Graphical models, exponential families, and variational inference.
*Foundations and Trends in Machine Learning, 1*(1--2), 1--305.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2002). *Understanding belief propagation and its generalizations*
(Technical Report TR-2001-22). Mitsubishi Electric Research Laboratories.

## Appendix A. Command-Line Usage

The CRSCE project produces two primary binaries, `compress` and `decompress`, which share a common argument parser and
follow identical conventions for flags, validation, and exit codes.

### A.1 Compress

```text
usage: compress -in <file> -out <file>
```

The `compress` binary reads an uncompressed input file, partitions it into $511 \times 511$-bit blocks, compresses each
block independently per the format defined in Section 12, and writes the compressed output. If any block exceeds
`max_compression_time`, the entire operation fails and no output is produced.

**Flags:**

- `-in <path>` — Path to the input file (required). The file must exist.
- `-out <path>` — Path to the output file (required). The file must not already exist.
- `-h` or `--help` — Display usage information and exit.

### A.2 Decompress

```text
usage: decompress -in <file> -out <file>
```

The `decompress` binary reads a CRSCE-compressed file, reconstructs each block by solving the constraint system to
$S_{\text{DI}}$ (Section 5.5), and writes the recovered bitstream. Decompression is deterministic and unbounded in time;
it either succeeds or reports a malformed payload.

**Flags:**

- `-in <path>` — Path to the compressed input file (required). The file must exist.
- `-out <path>` — Path to the output file (required). The file must not already exist.
- `-h` or `--help` — Display usage information and exit.

### A.3 Exit Codes

Both binaries use the same exit code conventions.

| Code | Meaning                                                               |
|----- | --------------------------------------------------------------------- |
|0     |Success, or help displayed, or no arguments provided                   |
|2     |Parse error (unknown flag or missing value for -in/-out)               |
|3     |Filesystem validation error (input file missing or output file exists) |

When no arguments are provided, `compress` prints `"crsce-compress: ready"` to standard output and exits with code 0.
When `-h` or `--help` is present, both binaries print the usage string to standard output and exit with code 0. Parse
errors and filesystem validation errors are printed to standard error.

### A.4 Environment Variables

Both binaries recognize the following environment variables.

```text
Variable                      Default                      Description
----------------------------  ---------------------------  ----------------------------------
MAX_COMPRESSION_TIME          1800                         Max compression time per block (s)
OBSERVABILITY_LOG_FILE        ./log/<program_name>.log     Path to the append-only JSON-L log
OBSERVABILITY_FLUSH_INTERVAL  1000                         Periodic flush interval (ms)
```

The `MAX_COMPRESSION_TIME` variable specifies the wall-clock time limit, in seconds, for compressing a single block
including DI discovery (Section 4.4). If the variable is not set, the default is 1,800 seconds (30 minutes). If the
compressor exceeds this limit on any block, the entire operation fails and no output is produced. This variable is
recognized only by the `compress` binary; the `decompress` binary ignores it, as decompression is unbounded in time by
design (Section 10.1).

The `OBSERVABILITY_LOG_FILE` variable specifies the file to which the O11y background thread (Section 10.5) writes event
and counter records. If the variable is not set, the default path is `./log/compress.log` or `./log/decompress.log` as
appropriate. The log directory is created if it does not exist. The file is opened in append-only mode, ensuring that
records from prior runs are never overwritten.

The `OBSERVABILITY_FLUSH_INTERVAL` variable specifies the period, in milliseconds, at which the O11y background thread
flushes buffered events and counter snapshots to disk. If the variable is not set, the default interval is 1,000 ms.
Setting a shorter interval improves observability latency at the cost of more frequent I/O; setting a longer interval
reduces I/O overhead during high-throughput solver phases.

## Appendix B. Ideas Under Consideration

### B.1 Segmented Prefix Hashes for Early Solver Pruning

The current design stores a single full-row SHA-256 digest per row, which can only be verified once all $s = 511$ bits
of that row are assigned. This means the solver may pursue a doomed branch for hundreds of assignments before
discovering a hash mismatch at the end of the row.

A segmented prefix hash scheme would replace the single full-row digest with multiple shorter digests computed over
progressively longer prefixes of each row. For example, four prefix checkpoints at bit positions 128, 256, 384, and 511
would allow the solver to verify partial row assignments as each prefix is completed during left-to-right cell
assignment. A mismatch at the first checkpoint (128 bits) would prune the branch before assigning the remaining 383
bits, potentially eliminating a vast subtree of the search space.

The storage budget is comparable to shorter full-row hashes. Four 32-bit prefix digests per row cost $4 \times 32 \times
511 = 65{,}408$ bits --- the same as a single 128-bit full-row hash and half the current SHA-256 cost. The
per-checkpoint collision resistance of $2^{-32}$ is low in isolation but sufficient in context: the solver is checking a
single candidate prefix against a single stored digest among rows already constrained by four cross-sum families, and
any false positive at an early checkpoint is caught by subsequent checkpoints or the final digest.

The prefix digests need not use a cryptographic hash. A fast non-cryptographic function (e.g., xxHash3) would suffice
for the intermediate checkpoints, with the final full-row checkpoint using truncated SHA-256 for the stronger uniqueness
guarantee. This hybrid approach would preserve the FIPS 180-4 standardization narrative for the final constraint while
accelerating solver pruning at intermediate stages.

This idea remains under evaluation. The key open questions are: (a) the optimal number and placement of checkpoints
given the solver's row-major assignment order, (b) the interaction between prefix hash pruning and
constraint-propagation-driven forced assignments (which may complete row segments out of left-to-right order), and (c)
the impact on $C_r$ under various checkpoint/digest-size configurations.
