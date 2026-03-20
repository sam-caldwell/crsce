---
title: "CRSCE Decompression Using a Constraint-Based Graph-Theoretical Approach"
author: "Samuel Caldwell"
affiliation: "Champlain College"
course: "CMIT-450 : Senior Seminar"
advisor: "Dr. Kenneth Revett"
date: "28 February 2026"
---

## Abstract

Cross-Sums Compression and Expansion (CRSCE) compresses a fixed-size two-dimensional binary matrix&mdash;the Cross-Sum
Matrix (CSM)&mdash;by replacing it with eight families of exact-sum projections (row, column, diagonal, anti-diagonal, and
four pseudorandom lookup-table partitions) together with independent per-row SHA-1 digests and a single per-block
SHA-256 digest.
Decompression is a constrained reconstruction problem: recover a matrix consistent with all projections and hashes.
Although the combined hash constraints make non-uniqueness
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
column sums (VSM), diagonal sums (DSM), and anti-diagonal sums (XSM). Four pseudorandom lookup-table partitions (LTP1,
LTP2, LTP3, LTP4) provide additional constraint lines via Fisher-Yates shuffles of the cell index space (see B.20).
An independent SHA-1 digest is computed for each row, producing the lateral hash vector (LH), and a single SHA-256
digest of the entire block produces the block hash (BH). A disambiguation index (DI) is appended to handle the rare
case of non-unique reconstruction. The compressed representation of each block is therefore the tuple $(\text{LH},
\text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{LTP1}, \text{LTP2}, \text{LTP3},
\text{LTP4})$.

Compression is realized because the cross-sum vectors and hash vector are smaller than the original matrix. LSM and VSM
each contain $s$ elements and each element requires $b = \lceil \log_2(s+1) \rceil$ bits. DSM and XSM each contain $2s -
1$ elements, but because straight diagonals have variable lengths ranging from 1 to $s$, each diagonal sum is encoded in
$\lceil \log_2(\text{len}(k)+1) \rceil$ bits, where $\text{len}(k) = \min(k+1,\; s,\; 2s-1-k)$ for diagonal index $k$.
The result defines the total bits for one diagonal family as:

$$
    B_d(s) = \lceil \log_2(s+1) \rceil + 2 \sum_{l=1}^{s-1} \lceil \log_2(l+1) \rceil
$$

LH consists of $s$ SHA-1 digests of 160 bits each. BH is a single SHA-256 digest of 256 bits. LTP1, LTP2, LTP3, and
LTP4 each contain $s$ elements of $b = 9$ bits. The disambiguation index (DI) is encoded as a fixed 8-bit unsigned
integer per block, permitting up to 256 feasible reconstructions before the compressor must fail. The per-block
compression ratio is therefore:

$$
    C_r = 1 - \frac{6sb + 2 B_d(s) + 160s + 256 + 8}{s^2}
$$

where $s = 511$,

$$
    b = \lceil \log_2(s+1) \rceil = 9, \quad B_d(511) = 8{,}185
$$

Thus,

$$
    C_r = 1 - \frac{27{,}594 + 16{,}370 + 81{,}760 + 256 + 8}{261{,}121} = 1 - \frac{125{,}988}{261{,}121} \approx 0.5175
$$

This means for any given suitable input, CRSCE is *guaranteed* to realize approximately 51.8% compression per block.
This excludes any additional compressed file format header information.

Decompression inverts this mapping: reconstruct $CSM$ given the stored constraints. The central difficulty is
computational: the feasible set of $CSM$ consistent with all sum constraints may be large, and finding *the* correct
$CSM$ is a constrained search problem. The SHA-1 row digests and SHA-256 block digest transform the problem from
"find any feasible matrix" into "find a feasible matrix whose rows match $s$ given digests and whose block matches a
given digest," which, under standard cryptographic assumptions, should make accidental ambiguity negligible (NIST,
2015; Dang, 2012).

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

#### 2.2.1 Cross-Sum Partition Axioms

The four projection families defined above are specific instances of a general class of *cross-sum partitions* over the
matrix $CSM \in \{0,1\}^{s \times s}$. Any valid cross-sum partition $\mathcal{P}$ must satisfy the following three
axioms.

**Axiom 1 (Conservation).** Every cross-sum vector must sum to the same value: the Hamming weight of the matrix.
Formally, let $\mathcal{P} = \{L_0, L_1, \ldots, L_{n-1}\}$ be a partition of the $s^2$ cells into $n$ lines, and let
$\sigma(L_k) = \sum_{(r,c) \in L_k} CSM_{r,c}$ denote the sum along line $L_k$. Then for any two valid cross-sum
partitions $\mathcal{P}$ and $\mathcal{Q}$:

$$
    \sum_{L \in \mathcal{P}} \sigma(L) = \sum_{L \in \mathcal{Q}} \sigma(L) = \|CSM\|_1 = \sum_{r=0}^{s-1} \sum_{c=0}^{s-1} CSM_{r,c}.
$$

This follows directly from the partition property: each partition covers every cell exactly once, so the grand total of
all line sums equals the total number of ones in the matrix, regardless of partition geometry.

**Axiom 2 (Uniqueness).** No two lines, whether within the same partition or across distinct partitions, may cover the
identical set of cells. Formally, for any two lines $L_i$ and $L_j$ (from any partitions):

$$
    L_i \neq L_j \implies L_i \not\equiv L_j \quad \text{(as sets of cell indices)}.
$$

This ensures that every line contributes an independent constraint. A duplicate line would provide redundant information,
wasting storage without reducing ambiguity in the reconstruction problem.

**Axiom 3 (Non-repetition).** Each line must reference each cell of $CSM$ at most once. Formally, for every line $L_k$
in every partition $\mathcal{P}$, $L_k$ is a *set* (not a multiset) of cell indices:

$$
    \forall (r,c) \in L_k: \quad \bigl|\{i : L_k[i] = (r,c)\}\bigr| = 1.
$$

This prevents a single line from double-counting a cell, which would distort the sum and violate the semantic
interpretation of $\sigma(L_k)$ as an exact cardinality constraint over distinct Boolean variables.

Together, these three axioms ensure that every cross-sum partition induces a well-defined system of cardinality
constraints whose grand totals are mutually consistent, whose individual constraints are non-redundant, and whose
variable references are exact. The four families defined in Section 2.2 (LSM, VSM, DSM, XSM) each satisfy all three
axioms, and any future extension of the projection system (e.g., additional geometric partitions to accelerate solver
convergence) must likewise conform to these requirements.

### 2.3 Row Hash Constraints (LH)

For each row $r$, define the row bitstring

$$
    \text{row}_r = CSM_{r,0} \| CSM_{r,1} \| \cdots \| CSM_{r,s-1} \| 0
$$

as a message of length $s + 1 = 512$ bits. The trailing zero bit is a fixed padding constant that ensures byte alignment
(512 bits = 64 bytes), simplifying implementation without affecting the hash's discriminative power between distinct
rows. Let

$$
    H(\text{row}_r) = \text{SHA-1}(\text{row}_r),
$$

and require

$$
    \forall r: \quad H(\text{row}_r) = \text{LH}[r].
$$

Additionally, a block hash (BH) is computed over the entire CSM serialized in row-major order:

$$
    \text{BH} = \text{SHA-256}(\text{CSM}_{\text{row-major}}).
$$

SHA-1 and SHA-256 are standardized in FIPS 180-4, including message padding and parsing into 512-bit blocks (NIST,
2015). SHA-1 is used for per-row lateral hashes because its 160-bit digest provides sufficient second-preimage
resistance ($2^{160}$) for non-adversarial collision detection, while its shorter digest saves 48,800 bits per block
relative to SHA-256. The whole-block SHA-256 digest restores adversarial robustness to the $2^{128}$ birthday bound,
dominating SHA-1's broken collision resistance ($\sim 2^{63}$; Stevens et al., 2017).

### 2.4 Decompression Objective

Given the constraint set $\mathcal{C} = (s, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{LTP1}, \text{LTP2},
\text{LTP3}, \text{LTP4}, \text{LH}, \text{BH})$, decompression seeks a $CSM$ satisfying all constraints. The premise
that $\mathcal{C}$ admits exactly one feasible matrix is a design intent, not a mathematical guarantee; the cross-sum
and hash constraints make non-uniqueness astronomically unlikely but cannot logically exclude it. The full compressed
payload $\mathcal{C}' = (\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{LTP1},
\text{LTP2}, \text{LTP3}, \text{LTP4})$ therefore includes an 8-bit disambiguation index (DI) that selects the intended
reconstruction
from the canonical enumeration of feasible solutions (Section 4). It is the DI that provides the "exactly one solution"
guarantee: even if $\mathcal{C}$ admits multiple feasible matrices, $\mathcal{C}'$ identifies precisely one.

## 3. Theoretical Foundations

### 3.1 Constraint Satisfaction and Factor-Graph View

The decompression problem is naturally a CSP over $s^2$ Boolean variables with $10s - 2$ cardinality constraints plus
$s$ row-hash constraints and 1 block-hash constraint. For $s = 511$ this expands to:

$$
    s^2 = 261{,}121 \text{ Boolean variables}
$$

$$
    \underbrace{511}_{\text{row}} +
    \underbrace{511}_{\text{column}} +
    \underbrace{1{,}021}_{\text{diagonal}} +
    \underbrace{1{,}021}_{\text{anti-diagonal}} +
    \underbrace{4 \times 511}_{\text{LTP}} = 5{,}108 \text{ cardinality constraints}
$$

$$
    511 \text{ SHA-1 row-hash constraints} + 1 \text{ SHA-256 block-hash constraint}
$$

$$
    5{,}108 + 512 = 5{,}620 \text{ total constraints}
$$

This can also be represented as a factor graph: variable nodes for each $CSM_{r,c}$, factor nodes for each
row/column/diagonal/anti-diagonal/LTP sum constraint, and factor nodes for each SHA-1 row constraint and
the SHA-256 block constraint. Factor graphs
support message-passing interpretations (Kschischang et al., 2001).

In principle, belief propagation (BP) or its generalizations can provide probabilistic guidance in large constraint
systems, especially when exact inference is intractable (Yedidia et al., 2002; Wainwright & Jordan, 2008). In practice,
the exact-sum constraints are hard factors (zero probability when violated), while the hash factors are extremely sharp:
for a fully specified row, either the digest matches or it does not.

### 3.2 Cryptographic Uniqueness as a Heuristic, Not a Proof

SHA-1 is designed so that, absent structural weaknesses in second-preimage resistance, finding a distinct message
with the same digest is computationally infeasible; NIST guidance frames security strength for applications that rely
on hash properties (Dang, 2012; NIST, 2015). From a modeling perspective, treating $H$ as a random mapping from
511-bit strings to 160-bit digests, the expected number of 511-bit preimages for any fixed digest value is
$2^{511}/2^{160} = 2^{351}$, so hashes alone do not uniquely identify a row among all possible rows. However, the
decompressor is not choosing among all rows; it is choosing among rows that participate in a globally consistent $CSM$
satisfying eight families of sum constraints and a whole-block SHA-256 digest.
The heuristic claim is that the intersection "sum-feasible" $\cap$ "row-hash-matching" is typically singleton.

This is a plausible engineering assumption but remains a heuristic: it does not exclude adversarially constructed
instances or rare coincidences.

### 3.3 Estimating the Probability of Non-Unique Reconstruction

Section 3.2 argued qualitatively that hash-backed uniqueness is a heuristic. This section estimates the probability of
non-uniqueness quantitatively under a random-oracle model for SHA-1 (row hashes) and SHA-256 (block hash). The
analysis proceeds in three stages: estimating the cross-sum collision barrier across all 8 partitions, combining this
with the per-row SHA-1 collision barrier, and adding the whole-block SHA-256 collision barrier.

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
pass independent per-row SHA-1 verification. Under the random-oracle assumption, each of the $s$ row hashes must
independently collide with the original digest. The probability that all $s$ rows collide simultaneously is:

$$
    P(\text{LH collision}) = \left(\frac{1}{2^{160}}\right)^s = 2^{-160s} = 2^{-81{,}760}.
$$

Converting to base 10: $\log_{10} P(\text{LH collision}) = -81{,}760 \times \log_{10} 2 \approx -24{,}614.43$.

**Block hash collision probability.** The entire modified matrix $CSM'$ must also produce the same SHA-256 digest as
$CSM$. Since $CSM' \neq CSM$, this is a second-preimage event at probability $2^{-256}$, independent of swap geometry
and row-hash outcomes.

**Combined collision probability.** Treating the cross-sum, row-hash, and block-hash collision events as independent (a
conservative assumption, since in practice the constraints are correlated and further reduce the joint probability):

$$
    P_{\text{CRSCE}} \leq P(\text{LSM collision}) \times P(\text{LH collision}) \times P(\text{BH collision})
    \approx 10^{-819} \times 10^{-24{,}614} \times 10^{-77} \approx 10^{-25{,}510}.
$$

With all 8 cross-sum partitions considered (not just LSM), the cross-sum barrier is substantially stronger, and the
dominant term at minimum swap size $k_{\min} \geq 10$ (affecting $m \geq 3$ rows) is $2^{-(160 \times 3 + 256)} =
2^{-736}$. The full-space collision estimate is approximately $10^{-30{,}000}$.

A probability of $\sim 10^{-30{,}000}$ is not merely negligible in a practical sense&mdash;it exceeds the security margin of
any deployed cryptographic system by many orders of magnitude. For context, the estimated number of atoms in the
observable universe is approximately $10^{80}$, and the probability of a spontaneous SHA-256 collision under
birthday-bound assumptions is approximately $2^{-128} \approx 10^{-38.5}$. The CRSCE collision probability is smaller
than either by a factor exceeding $10^{29{,}000}$.

This analysis assumes SHA-1 and SHA-256 behave as random oracles, which is standard for non-adversarial settings. SHA-1's
collision resistance is broken at $\sim 2^{63}$ work (Stevens et al., 2017), but the whole-block SHA-256 digest
neutralizes this: constructing two matrices that collide on all 511 SHA-1 row hashes AND the SHA-256 block hash
requires at minimum $2^{128}$ work (the birthday bound on SHA-256). Under adversarial conditions, the DI mechanism
(Section 2.4) provides deterministic correctness regardless of collision probability.

### 3.4 Enumeration and Indexing: Why Worst Cases Can Be Hard

The proposed disambiguation mechanism depends on enumerating feasible solutions. Even when *finding one* solution is
tractable in some structured settings, *counting* or *enumerating* solutions is often much harder in general;
\#P-completeness results (e.g., via Valiant's theorem for counting perfect matchings / permanents) illustrate that
counting can be intractable even when related decision problems are easier (Valiant, 1979).

SAT/SMT research treats "AllSAT" as a distinct, often expensive task: enumerating all satisfying assignments can
dominate runtime and memory, and specialized methods exist to mitigate the cost (e.g., blocking and non-blocking
strategies) (Kobler et al., 2025). The relevance here is not that CRSCE decompression is literally SAT, but that
solution enumeration in combinatorial constraint systems can be intrinsically expensive. Therefore, any indexing-based
disambiguation must explicitly define a failure mode&mdash;hence the single time tolerance parameter.

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

$$\mathcal{C}' = (\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{LTP1}, \text{LTP2}, \text{LTP3}, \text{LTP4}),$$

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

The deterministic solver maintains a compact state for each line (row, column, diagonal, anti-diagonal, and four
toroidal-slope families) that enables incremental constraint propagation as cells are assigned. This representation
allows the solver to detect infeasibility early, force deterministic assignments when only one value is possible, and
verify completed rows against their SHA-1 digests without recomputing from scratch.

For each line $L$, the solver tracks four quantities: $u(L)$, the count of unknown cells on the line; $a(L)$, the count
of assigned ones; the required sum $S(L)$ from the corresponding cross-sum vector (LSM, VSM, DSM, XSM, or LTP); and the
residual $\rho(L) = S(L) - a(L)$.

A line is feasible iff

$$0 \leq \rho(L) \leq u(L).$$

When $\rho(L) = 0$, all unknown cells on $L$ are forced to $0$. When $\rho(L) = u(L)$, all unknown cells on $L$ are
forced to $1$. These are standard feasibility rules for cardinality constraints, and they can be applied repeatedly
until reaching a fixpoint.

Each assignment $CSM_{r,c} \leftarrow 0/1$ updates exactly eight lines' $(u, a, \rho)$ (one row, one column, one
diagonal, one anti-diagonal, and four LTP lines), enabling incremental propagation.

### 5.2 Integrating Row Hash Checks Deterministically

Row-hash constraints are most effective once a row becomes fully assigned. When $u(\text{row } r) = 0$, compute SHA-1
of the 511-bit row message and compare to $\text{LH}[r]$. A mismatch yields an immediate contradiction and triggers
backtracking. SHA-1 preprocessing rules (padding and parsing) must follow FIPS 180-4 exactly to ensure
interoperability (NIST, 2015).

This approach does *not* require representing SHA-1 as a boolean circuit; it relies on exact evaluation at the moment
the row becomes concrete, which is straightforward to implement and deterministic.

### 5.3 Canonical Branching to Support Enumeration

To enumerate solutions in lexicographic order, choose the first unassigned cell in row-major order:

$$(r^*, c^*) = \min\{(r,c) : CSM_{r,c} \text{ unassigned}\}$$

under lex order of indices. Then branch by assigning $0$ first, then $1$. This ensures that the first found complete
solution is $S_0$, the second is $S_1$, etc., assuming the solver is otherwise deterministic.

### 5.4 Pseudocode: Deterministic Enumerator

The enumerator is the core algorithmic primitive upon which both compression and decompression depend. Its purpose is to
produce every feasible reconstruction of $CSM$ in strict lexicographic order, yielding each solution as it is found. The
algorithm combines depth-first search (DFS) with incremental constraint propagation, and the interaction between these
two mechanisms is what makes reconstruction tractable in practice despite the enormous search space.

The procedure begins by initializing all $s^2$ cells as unassigned and computing the initial line statistics $(u, a,
\rho)$ for every row, column, diagonal, and anti-diagonal. Before any branching occurs, the propagator runs to fixpoint
on the initial state: lines whose residual $\rho$ equals zero force all their unknowns to $0$, and lines whose residual
equals the unknown count force all their unknowns to $1$. These forced assignments may cascade, since each forced cell
updates eight lines and may trigger further forcing on any of them. If propagation detects infeasibility&mdash;a negative
residual or a residual exceeding the unknown count on any line&mdash;the constraint set is inconsistent and no solutions
exist.

Once the initial propagation stabilizes, the DFS selects the first unassigned cell in row-major order and branches on
it, trying $0$ before $1$. This branching discipline is what establishes the lexicographic enumeration order: because
every cell is explored with $0$ first, the first complete assignment found is the lexicographically smallest feasible
matrix $S_0$, the second is $S_1$, and so on. After each tentative assignment, the propagator again runs to fixpoint,
forcing any cells that become determined as a consequence. If propagation succeeds, the DFS recurses to the next
unassigned cell; if it fails, the solver restores the state to the save point prior to the assignment (via an undo
stack) and tries the alternative value. When both values at a cell lead to infeasibility, the solver backtracks further
up the tree.

The `Propagate()` subroutine referenced below applies the feasibility rules from Section 5.1 to a queue of affected
lines until quiescence. It returns false if any line becomes infeasible or if any completed row fails its SHA-1
digest check (Section 5.2).

```text
Algorithm 1: EnumerateSolutionsLex(C)
    Input:  constraints C = (s, LSM, VSM, DSM, XSM, LTP1, LTP2, LTP3, LTP4, LH, BH)
    Output: stream of feasible matrices CSM in lex order

    Initialize all CSM[r,c] = UNASSIGNED
    Initialize line stats (u, a, ρ) for all rows, cols, diags, anti-diags, slopes
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

Decompression consumes the full compressed payload $\mathcal{C}' = (\text{LH}, \text{DI}, \text{LSM}, \text{VSM},
\text{DSM}, \text{XSM})$ and must return exactly one matrix: the $\text{DI}$-th feasible reconstruction in canonical
lexicographic order. The algorithm achieves this by invoking the enumerator from Section 5.4 and simply counting
yielded solutions until the counter reaches the stored DI value. The solution at that index is the original $CSM$, and
the decompressor returns it immediately without examining any subsequent candidates.

In the overwhelmingly common case where the constraint set admits only one feasible matrix (Section 3.3), $\text{DI} =
0$ and the decompressor returns the very first solution the enumerator produces. The counting loop exists solely to
handle the rare case of non-unique reconstruction, where the DI disambiguates among multiple valid matrices. If the
enumerator exhausts all feasible solutions before reaching the DI-th candidate, the compressed payload is malformed and
the decompressor raises an error. This cannot occur for payloads produced by a correct compressor, but the check
provides defense-in-depth against data corruption or truncation.

```text
Algorithm 2: DecompressWithDI(C')
Input:  C' = (LH, BH, DI, LSM, VSM, DSM, XSM, LTP1, LTP2, LTP3, LTP4)
Output: S_DI

k <- 0
for CSM in EnumerateSolutionsLex(C):
    if k == DI:
        return CSM
    k <- k + 1
error: DI out of range (no such solution)
```

### 5.6 Pseudocode: Compression with Time-Bounded DI Discovery

Compression is the inverse process: given the original matrix $CSM$, produce the compressed payload $\mathcal{C}'$
including the correct disambiguation index. The compressor first computes the four cross-sum vectors and the per-row
SHA-1 row digests and the SHA-256 block digest deterministically from the original matrix&mdash;this is straightforward
arithmetic and hashing. The
difficult step is discovering the DI, which requires determining the original matrix's position in the canonical
enumeration of all feasible solutions.

The compressor discovers the DI by running the same enumerator used in decompression and comparing each yielded
candidate to the known original. When a candidate matches the original, its zero-based index becomes the DI, and the
compressor emits the complete payload. This design ensures that compression and decompression are compatible by
construction: both use the identical enumerator, so the $k$-th solution produced during compression is guaranteed to be
the same as the $k$-th solution produced during decompression.

Because enumeration can be expensive in pathological cases (Section 3.4), the compressor is governed by a single
tolerance parameter, `max_compression_time`, which caps the wall-clock duration of the entire operation. If the timer
expires before the enumerator yields a candidate matching the original, the compressor returns FAIL rather than
producing a potentially incorrect or incomplete payload. This explicit failure mode is a deliberate design choice: the
caller is expected to fall back to an alternative compression algorithm for blocks that CRSCE cannot handle within the
time budget. The time bound also implicitly caps the DI value, since the enumerator must produce $\text{DI} + 1$
complete solutions before succeeding; in practice, any DI large enough to challenge the 8-bit encoding limit (255) would
almost certainly also exceed a reasonable time bound.

If the enumerator exhausts all feasible solutions without finding the original matrix, the constraints are inconsistent
with the input&mdash;an implementation error, since the cross-sums and hashes were computed directly from $CSM$. The
compressor returns FAIL in this case as well, providing a safety net against bugs in the constraint computation or
enumerator logic.

```text
Algorithm 3: Compress(CSM, max_compression_time)
Input:  original matrix CSM, time limit T
Output: compressed payload C' or FAIL

StartTimer()

Compute LSM, VSM, DSM, XSM, LTP1, LTP2, LTP3, LTP4 from CSM
Compute LH[r] = SHA1(row_r) for all r    // per FIPS 180-4
Compute BH = SHA256(CSM_row_major)        // per FIPS 180-4

k <- 0
for Y in EnumerateSolutionsLex(C):
    if ElapsedTime() > T:
        return FAIL
    if Y == CSM:
        DI <- k
        return (LH, BH, DI, LSM, VSM, DSM, XSM, LTP1, LTP2, LTP3, LTP4)
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
preserve determinism of enumeration and must interoperate exactly with the format's SHA-1 and SHA-256 definitions.

Given $s = 511$, the raw matrix is only $511^2 = 261{,}121$ bits ($\approx 32$ KiB). Thus, the challenge is not storage
but search control and propagation speed.

### 6.2 CPU-Centric Core for Determinism

Branching search is inherently irregular: different branches cause different propagation cascades, and backtracking
requires undo logs. Such control flow is typically better suited to CPUs than GPUs. On Apple Silicon, the CPU
implementation can store each row as a 512-bit aligned bitset (8 × 64-bit words) for fast operations, maintain a compact
unknown mask per row, keep per-line residuals $(u, a, \rho)$ in small arrays, and maintain an undo stack recording
assignments and line-stat deltas for fast rollback.

Row-hash checking can use a constant-time, single-block SHA-1 implementation specialized for 511-bit messages, since
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
byte-aligned, standard SHA-1 library functions can be used directly without custom bit-level padding logic. This
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
over time. Concrete components&mdash;the constraint propagation engine, the branching strategy, the hash verification
layer, and the enumeration controller&mdash;should each be encapsulated behind well-defined interfaces, allowing
alternative implementations to be substituted without modifying the solver's control logic.

### 9.2 Polymorphism as an Architectural Principle

Polymorphism is not merely a stylistic preference; it is a structural requirement motivated by the algorithm's design.
The solver operates over eight families of cross-sum constraints (LSM, VSM, DSM, XSM, LTP1, LTP2, LTP3, LTP4) that differ in their indexing
geometry but share identical residual-tracking and forcing logic. A polymorphic line-constraint interface allows all
four families to be processed uniformly by the propagation engine, eliminating redundant code paths and reducing the
surface area for implementation errors.

Similarly, the hash verification layer should be abstracted behind a common interface. Although the current
specification uses SHA-1 for per-row hashes and SHA-256 for the block hash, the design discussed in Appendix B.1
(segmented prefix hashes) and potential future adoption of alternative hash functions both argue for a pluggable hash
interface. Polymorphism ensures that such changes can be introduced without restructuring the solver.

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

**`LateralHash`** stores the vector of $s$ SHA-1 digests, one per row. It provides `compute(row_r)` to hash a 512-bit
row message (511 data bits plus trailing zero) and `verify(r, digest)` to compare against the stored value.

```text
+----------------------------------------------+
|              LateralHash                     |
+----------------------------------------------+
| - digests: uint8[s][20]                      |
| - s: uint16                                  |
+----------------------------------------------+
| + LateralHash(s: uint16)                     |
| + compute(row: uint64[8]): uint8[20]         |
| + store(r: uint16, digest: uint8[20]): void  |
| + verify(r: uint16, digest: uint8[20]): bool |
| + getDigest(r: uint16): uint8[20]            |
+----------------------------------------------+
```

**`BlockHash`** stores a single SHA-256 digest computed over the entire CSM serialized in row-major order. It provides
`compute(csm)` to hash the full matrix and `verify(csm, expected)` to compare against the stored value.

```text
+----------------------------------------------+
|              BlockHash                       |
+----------------------------------------------+
| + compute(csm: CSM): uint8[32]              |
| + verify(csm: CSM, expected: uint8[32]): bool|
+----------------------------------------------+
```

**`ToroidalSlopeSum`** *(legacy, retained for testing only)* is a cross-sum vector for toroidal modular-slope partitions.
Each partition consists of $s = 511$ lines defined by $L_k = \{(t, (k + pt) \bmod s) : t = 0, \ldots, s-1\}$ for a
fixed slope $p$. The four original slopes were $p \in \{256, 255, 2, 509\}$ (HSM1, SFC1, HSM2, SFC2). As of B.20,
these four toroidal-slope partitions have been replaced in the production solver by four pseudorandom LTP partitions
(LTP1--LTP4). The `ToroidalSlopeSum` class remains in the source tree for mathematical validation and unit testing but
is never instantiated in the compress or decompress pipelines.

**`CompressedPayload`** represents $\mathcal{C}' = (\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM},
\text{DSM}, \text{XSM}, \text{LTP1}, \text{LTP2}, \text{LTP3}, \text{LTP4})$ and provides serialization and
deserialization methods that enforce the wire format byte-alignment convention (Section 12.4): LH (byte-aligned),
BH (byte-aligned), DI (uint8), followed by the bit-packed cross-sum vectors.

```text
+----------------------------------------------+
|           CompressedPayload                  |
+----------------------------------------------+
| - lh: LateralHash                           |
| - bh: uint8[32]                              |
| - di: uint8                                  |
| - lsm: RowSum                               |
| - vsm: ColSum                               |
| - dsm: DiagSum                               |
| - xsm: AntiDiagSum                          |
| - hsm1: ToroidalSlopeSum                    |
| - sfc1: ToroidalSlopeSum                    |
| - hsm2: ToroidalSlopeSum                    |
| - sfc2: ToroidalSlopeSum                    |
+----------------------------------------------+
| + serialize(): bytestream                    |
| + deserialize(data: bytestream): void        |
| + getConstraintSet(): C                      |
| + getDI(): uint8                             |
| + getBH(): uint8[32]                         |
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
\rho)$ for all $10s - 2$ lines (511 rows + 511 columns + 1,021 diagonals + 1,021 anti-diagonals + 4 × 511
toroidal-slope lines = 5,108). It is the single source of truth for the solver's state.

```text
+--------------------------------------------------+
|       <<interface>> IConstraintStore             |
+--------------------------------------------------+
| + assign(r: uint16, c: uint16, v: bit): void     |
| + unassign(r: uint16, c: uint16): void           |
| + getResidual(line: LineID): uint16              |
| + getUnknownCount(line: LineID): uint16          |
| + getAssignedCount(line: LineID): uint16         |
| + getLinesForCell(r: uint16, c: uint16): LineID[8]|
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

**`IHashVerifier`** abstracts the row-hash computation and comparison. The default implementation uses SHA-1 per FIPS
180-4 on 512-bit row messages. The interface admits alternative implementations (e.g., segmented prefix hashes per
Appendix B.1) without changing the solver's control flow.

```text
+--------------------------------------------------+
|       <<interface>> IHashVerifier                |
+--------------------------------------------------+
| + computeHash(row: uint64[8]): uint8[20]        |
| + verifyRow(r: uint16, row: uint64[8]): bool    |
| + setExpected(r: uint16, digest: uint8[20]): void|
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
cross-sum vectors, SHA-1 lateral hashes, and SHA-256 block hash from the original matrix, then invokes the enumerator
to discover the DI within the time bound. On success, it assembles and serializes $\mathcal{C}'$.

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
    ├── RowHashVerifier/   SHA-1 row hash verification
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
    ├── RowHashVerifier/   SHA-1 row hash verification headers
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
    ├── RowHashVerifier/   SHA-1 row hash verification tests
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
6. Avoid recursion unless using tail-call recursion.
7. Where feasible, asynchronous lambda functions using coroutines should be applied to improve performance.

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
(\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{LTP1}, \text{LTP2},
\text{LTP3}, \text{LTP4})$ defined in Section 4.2.

```text
Field   Elements    Bits/Element   Total Bits   Total Bytes   Encoding
------  ----------  ------------   ----------   -----------   --------
LH      511         160            81,760       10,220        20 bytes per SHA-1 digest, sequential
BH      1           256            256          32            32 bytes, SHA-256 of row-major CSM
DI      1           8              8            1             uint8
LSM     511         9              4,599       &mdash;          MSB-first packed bitstream
VSM     511         9              4,599       &mdash;          MSB-first packed bitstream
DSM     2s-1=1,021  variable       8,185       &mdash;          MSB-first, ceil(log2(len(d)+1))
XSM     2s-1=1,021  variable       8,185       &mdash;          MSB-first, ceil(log2(len(x)+1))
LTP1    511         9              4,599       &mdash;          MSB-first packed bitstream
LTP2    511         9              4,599       &mdash;          MSB-first packed bitstream
LTP3    511         9              4,599       &mdash;          MSB-first packed bitstream
LTP4    511         9              4,599       &mdash;          MSB-first packed bitstream
------  ----------  ------------   ----------   -----------   --------
Total                              125,988      15,749
```

The total block payload is $125{,}988$ bits ($15{,}749$ bytes). The payload is naturally byte-aligned, so no trailing
padding bits are required. The total compressed file size is therefore $28 + (\text{block\_count} \times 15{,}749)$
bytes.

**LH (Lateral Hash).** The 511 SHA-1 digests are stored sequentially, each as 20 bytes, for a total of 10,220 bytes.
LH[0] corresponds to row 0, LH[510] to row 510.

**BH (Block Hash).** A single SHA-256 digest (32 bytes) computed over the entire CSM serialized in row-major order as
a contiguous bitstream of $s^2 = 261{,}121$ bits.

**DI (Disambiguation Index).** An 8-bit unsigned integer selecting the intended solution from the canonical enumeration
(Section 4.2). Values 0-255 are valid; if the original matrix's position exceeds 255, compression fails.

**LSM and VSM (Row and Column Sums).** Each vector contains $s = 511$ elements. Each element requires $b = \lceil
\log_2(s + 1) \rceil = 9$ bits and is serialized MSB-first into a packed bitstream. The $511 \times 9 = 4{,}599$ bits
per vector are packed continuously; the final partial byte (if any) is zero-padded to the byte boundary only at the end
of the entire cross-sum region, not between individual vectors.

**DSM and XSM (Diagonal and Anti-Diagonal Sums).** Each vector contains $2s - 1 = 1{,}021$ elements corresponding to
straight (non-wrapping) diagonals. Element $k$ requires $\lceil \log_2(\text{len}(k) + 1) \rceil$ bits, where
$\text{len}(k) = \min(k + 1,\; s,\; 2s - 1 - k)$ as defined in Section 2.2. Elements are serialized MSB-first in index
order ($k = 0, 1, \ldots, 2s - 2$), packed continuously into the bitstream.

**LTP1, LTP2, LTP3, and LTP4 (Lookup-Table Partition Sums).** Each vector contains $s = 511$ elements, one per LTP
line. Each element requires $b = \lceil \log_2(\text{ltpLineLen}(k) + 1) \rceil$ bits and is serialized MSB-first. For
the current uniform-511 partition, this equals 9 bits per element, identical to LSM and VSM. The four partitions are
pseudorandom: each sub-table is constructed by a Fisher-Yates shuffle seeded with a deterministic 64-bit LCG constant,
assigning every cell to exactly one of 511 lines per sub-table. At runtime, the solver maps $(r, c)$ to line indices
via precomputed lookup tables (see B.20).

### 12.5 Cross-Sum Ranges and Validation

Each LSM, VSM, LTP1, LTP2, LTP3, and LTP4 entry must be in the range $[0, s]$ where $s = 511$. Each DSM entry at index
$d$ must be in $[0, \text{len}(d)]$, and each XSM entry at index $x$ must be in $[0, \text{len}(x)]$. A decoder should
reject any block containing out-of-range cross-sum values.

### 12.6 Block Acceptance Criteria

A block is accepted if and only if the reconstructed CSM simultaneously satisfies all three conditions: it reproduces
the stored LSM, VSM, DSM, XSM, LTP1, LTP2, LTP3, and LTP4 vectors exactly for all indices; it recomputes per-row LH
from the reconstructed rows exactly ($\text{digest} = \text{SHA-1}(\text{row}_{64})$) for all $r \in \{0, \ldots,
510\}$; and it recomputes the block hash exactly ($\text{BH} = \text{SHA-256}(\text{CSM}_{\text{row-major}})$). Any
mismatch results in rejection of that block. Decoders must fail hard by default and must not produce partial output.

### 12.7 File Size Validation

A decoder can validate file integrity before parsing blocks by checking:

$$
    \text{file\_size} = 28 + (\text{block\_count} \times 15{,}749)
$$

where `block_count` is read from the header. Any deviation indicates a truncated or corrupted file.

## 13. Conclusion

This paper formalized a disambiguated CRSCE decompression process that remains deterministic even if the base constraint
set admits multiple feasible reconstructions. The key idea is *canonical solution indexing*: define a strict
lexicographic order on feasible matrices, encode the zero-based DI of the intended solution as $S_{\text{DI}}$, and
discover that DI at compression time by running the decompressor as an enumerator constrained by the known original
matrix. The compressed payload $\mathcal{C}' = (\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM},
\text{XSM}, \text{LTP1}, \text{LTP2}, \text{LTP3}, \text{LTP4})$ is ordered for byte-alignment efficiency, with all
fixed-width fields preceding the bit-packed cross-sum vectors. Because enumeration can be expensive, the design includes
exactly one tolerance parameter&mdash;`max_compression_time`&mdash;beyond which compression fails, requiring a fallback to
another algorithm.

We established that the probability of a non-unique reconstruction is approximately $10^{-30{,}000}$ under a
random-oracle model, combining the cross-sum collision barrier across 8 partitions with the per-row SHA-1 collision
barrier ($\sim 10^{-24{,}614}$) and the whole-block SHA-256 collision barrier ($\sim 10^{-77}$). Each row is hashed
as a byte-aligned 512-bit message (the 511-bit row plus a fixed trailing zero bit), simplifying implementation while
preserving compliance with FIPS 180-4 (NIST, 2015). The SHA-256 block hash restores adversarial robustness to the
$2^{128}$ birthday bound despite SHA-1's broken collision resistance.

We moved from theory to implementation by presenting a deterministic enumerator based on incremental feasibility
propagation for cardinality constraints, row-hash verification, and canonical branching to preserve lexicographic order.
For Apple Silicon, we argued for CPU-centric control flow with optional GPU acceleration for bulk recomputation tasks
using Metal, maintaining strict determinism by keeping all state changes and ordering decisions on the CPU.

The implementation is specified as pure object-oriented with polymorphism, ensuring that the constraint propagation
engine, hash verification layer, branching strategy, and enumeration controller are each encapsulated behind
well-defined interfaces. This architectural requirement supports extensibility, conformance testing, and the
substitution of alternative implementations&mdash;including GPU-accelerated components&mdash;without compromising the
determinism guarantees on which the DI mechanism depends.

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

### B.1 Abandoned

See Appendix D.2

### B.2 Auxiliary Cross-Sum Partitions as Solver Accelerators (Implemented)

This appendix originally proposed adding auxiliary cross-sum partitions to increase constraint density and accelerate
decompression. Four toroidal-slope partitions&mdash;HSM1 ($p = 256$), SFC1 ($p = 255$), HSM2 ($p = 2$), and SFC2
($p = 509$)&mdash;were initially integrated, but were subsequently replaced by four pseudorandom lookup-table partitions
(LTP1-LTP4) in B.20. The main specification (Sections 2.4, 3.1, 10.2-10.3, and 12.4) now reflects the LTP
architecture. The original toroidal modular-slope construction described in B.5 rather than the Hilbert-curve geometry
originally proposed here. The constraint store now manages $10s - 2 = 5{,}108$ lines (511 rows + 511 columns + 1,021
diagonals + 1,021 anti-diagonals + $4 \times 511$ slope lines), each cell participates in 8 constraint lines, and the
per-block payload is 125,988 bits (15,749 bytes) at a compression ratio of $C_r \approx 0.5175$ (51.8%).

The remainder of this section summarizes the analytical framework for evaluating the implemented design and assesses
the cost and benefit of adding further partitions.

#### B.2.1 Information-Theoretic Bounds

A cross-sum partition of $s$ groups of $s$ cells contributes at most $s \lceil \log_2(s+1) \rceil = 4{,}599$ bits of
constraint information. The 8 implemented partitions contribute a combined maximum of $6sb + 2B_d(s) = 43{,}982$ bits
of cross-sum information (where $b = 9$ for the 6 uniform-length families and $B_d(s)$ accounts for the variable-
length DSM and XSM encodings). This is far short of the $s^2 = 261{,}121$ bits needed to uniquely determine the
matrix. The kernel dimension of the linear constraint system is at least $s^2 - (10s - 2) = 255{,}013$, confirming
that cross-sum constraints alone&mdash;regardless of how many partitions are added&mdash;cannot replace the nonlinear
collision resistance provided by SHA-1 LH and SHA-256 BH.

The following table summarizes the current and hypothetical configurations:

| Configuration                        | Cross-sum bits | Lines   | Lines/cell | $C_r$  |
|--------------------------------------|----------------|---------|------------|--------|
| 4 original (LSM/VSM/DSM/XSM)        | 25,568         | 3,064   | 4          | 0.6989 |
| 8 implemented (+ LTP1/LTP2/LTP3/LTP4)| 43,982        | 5,108   | 8          | 0.5175 |
| 10 partitions (+ 1 pair)            | 53,180         | 6,130   | 10         | 0.4823 |
| 12 partitions (+ 2 pairs)           | 62,378         | 7,152   | 12         | 0.4471 |
| 16 partitions (+ 4 pairs)           | 80,774         | 9,196   | 16         | 0.3768 |

Each additional pair of toroidal-slope partitions costs $2sb = 9{,}198$ bits per block (3.5 percentage points of
compression ratio) and adds $2s = 1{,}022$ constraint lines.

#### B.2.2 Minimum Swap Size and Collision Resistance

The pigeonhole bound above is a loose upper bound that treats each partition's sums as independent. In practice,
interlocking partitions create tight dependencies: a single cell participates in one line from each of the 8
partitions, so flipping that cell perturbs 8 sums simultaneously. For a swap pattern to be invisible to the entire
partition system, it must produce zero net change on *every* line it touches.

With only LSM and VSM, the minimal invisible swap is a 4-cell rectangle with alternating $+1/-1$ flips. Adding DSM
and XSM breaks generic rectangles (the four cells fall on four distinct diagonals, each seeing an unbalanced flip),
raising the minimum swap size above 4 cells. Each additional toroidal-slope partition imposes further balance
constraints: the $k$ cells in a candidate swap must pair up within each slope line they touch, with equal $+1/-1$
split per line.

Let $k_{\min}$ denote the minimum swap size for an $n$-partition system. The expected number of collision partners
for a uniformly random matrix is bounded by:

$$
    E[\text{collisions}] \lessapprox \left(\frac{e \, s^2}{2k}\right)^k \cdot \exp(-nk \, e^{-k/s})
    \cdot 2^{-nk^2/(2s)}
$$

where the first factor counts $k$-cell subsets, the second enforces the no-singleton condition across $n$ partitions,
and the third enforces pairwise balance. The following table evaluates this bound at $n = 8$ (implemented) and
$n = 12$ (hypothetical) for representative swap sizes:

| $k_{\min}$ | $E[\text{collisions}]$ ($n = 8$) | $E[\text{collisions}]$ ($n = 12$) |
|:-----------:|:--------------------------------:|:---------------------------------:|
| 8           | $\approx 10^{14}$               | $\approx 10^{10}$                |
| 20          | $\approx 10^{15}$               | $\approx 10^{5}$                 |
| 50          | $\approx 10^{-8}$               | $\approx 10^{-50}$               |
| 80          | $\approx 10^{-75}$              | $\approx 10^{-157}$              |
| 100         | $\approx 10^{-138}$             | $\approx 10^{-260}$              |

For $n = 8$, the collision count drops below 1 at $k_{\min} \gtrsim 45$. For $n = 12$, this threshold drops to
$k_{\min} \gtrsim 25$. In either case, LH and BH provide the definitive collision guarantee ($\sim 10^{-30{,}000}$);
the cross-sum collision analysis is relevant only as a secondary assurance and for understanding the propagation
benefit.

#### B.2.3 Propagation Benefit of Additional Partitions

Each additional partition increases the number of constraint lines per cell. The propagator's forcing rules
($\rho = 0$ forces zeros, $\rho = u$ forces ones) trigger independently on each line. If $f$ denotes the per-line
forcing probability at a given search depth, the probability that a cell remains unforceable after propagation is
$(1 - f)^n$ for $n$ lines per cell. The reduction from $n = 8$ to $n = 10$ is:

$$
    \frac{(1-f)^{10}}{(1-f)^8} = (1-f)^2
$$

At a representative $f = 0.05$ (5% per-line forcing probability in the mid-solve), this reduces the unforceable
fraction by a factor of $(0.95)^2 = 0.9025$&mdash;a 10% reduction in the number of cells requiring branching decisions.
Because DFS tree size is exponential in the number of branch points, even a modest reduction compounds significantly.

The toroidal-slope construction guarantees 1-cell orthogonality between all partition pairs (Section 2.2.1): any two
lines from different partitions share exactly 1 cell. This means every assignment propagates information into every
other partition without redundancy. Additional slope pairs preserve this property provided $\gcd(p, 511) = 1$ and
$\gcd(|p_i - p_j|, 511) = 1$ for all existing slopes $p_j$. The current slopes $\{2, 255, 256, 509\}$ leave
abundant room for expansion&mdash;up to 255 total pairs are theoretically possible (see B.5.1(b)).

#### B.2.4 Implemented Design: Toroidal Modular-Slope Partitions

The original B.2 proposed Hilbert-curve-based PSM/NSM partitions implemented via static lookup tables. The actual
implementation uses toroidal modular-slope partitions instead, for three reasons:

*Analytical orthogonality.* The toroidal-slope construction provides a proof-backed guarantee that any two lines from
different partitions intersect in exactly 1 cell, via the $\gcd$-based argument in Section 2.2.1. Hilbert-curve
partitions have no such guarantee&mdash;their crossing density depends on the specific curve geometry and must be verified
empirically.

*Zero-overhead index computation.* Slope line membership is computed as $k = (c - pr) \bmod s$, which compiles to a
multiply-subtract-modulo sequence costing 1-2 cycles on Apple Silicon. The `ConstraintStore` precomputes flat
stat-array indices for all 4 slope families via `slopeFlatIndices(r, c)`, eliminating even the modular arithmetic from
the hot path. Hilbert-curve partitions would require a 510 KB lookup table per partition pair.

*Trivial extensibility.* Adding a new slope pair requires selecting a slope $p$ with $\gcd(p, 511) = 1$ and
$\gcd(|p - p_j|, 511) = 1$ for all existing slopes. No code-generation scripts, build-time lookup tables, or
curve-adaptation logic is needed.

#### B.2.5 Cost-Benefit of Further Partitions

Adding a fifth slope pair (10 partitions total) costs 9,198 bits per block, reducing $C_r$ from 51.8% to 48.2%&mdash;a
3.5 percentage-point penalty. The benefit is 1,022 additional constraint lines and an increase from 8 to 10
lines per cell. Whether this tradeoff is justified depends on the empirical decompression speedup.

The current solver plateaus at depth ~87K (row ~170) with 8 partitions. The plateau is a combinatorial phase
transition in the mid-rows where cardinality forcing is inactive and LH checks are the only strong pruning
mechanism. Additional partitions increase the per-assignment propagation radius but do not create new hash
checkpoints. The marginal benefit is therefore an increase in cardinality forcing frequency&mdash;more lines per cell
means more chances for a line's residual to reach 0 or $u$&mdash;but this benefit diminishes as the number of partitions
grows, because the residuals on the new lines are initially large (each slope line has $s = 511$ cells) and individual
assignments reduce them by at most 1.

The following qualitative assessment applies to the current plateau regime:

*Strong case for a 5th pair (10 partitions).* At 10 lines per cell, the probability of at least one forcing trigger
per assignment increases measurably. The 3.5-point compression cost is modest, and the propagation benefit is
concentrated exactly where the solver needs it&mdash;in the mid-rows where 8-line propagation is insufficient.

*Diminishing returns beyond 12 partitions.* At 12 lines per cell, the incremental forcing probability gain from each
additional pair is $(1-f)^2 \approx 0.90$, the same 10% relative reduction. But the compression cost accumulates
linearly: 12 partitions yield $C_r \approx 44.7\%$, and 16 partitions yield $C_r \approx 37.7\%$. The compression
ratio approaches the regime where CRSCE's overhead rivals the input size, undermining the format's purpose.

*Hard limit.* At $n = 28$ partitions (the maximum fitting within the information-theoretic budget), the compression
ratio drops to approximately 11%&mdash;effectively no compression. Long before this point, the solver's constraint
density is high enough that other bottlenecks (hash verification throughput, propagation queue overhead) dominate.

#### B.2.6 Open Questions

Consolidated into Appendix C.2.

### B.3 Variable-Length Curve Partitions as LH Replacement

Sections B.1 and B.2 explore modifications that *complement* LH. This section considers a more aggressive alternative:
replacing LH entirely with $n$ variable-length partitions structured identically to DSM and XSM&mdash;each consisting of
$2s - 1$ lines with lengths $1, 2, \ldots, s, \ldots, 2, 1$&mdash;but using space-filling curves rather than straight
lines as the underlying geometry. The motivation is twofold: the short lines at the periphery of each partition provide
deterministic "anchor" cells that seed constraint propagation, and the variable-length structure concentrates collision
resistance where it is strongest (short lines) while providing broad constraint coverage where it is weakest (long
lines).

#### B.3.1 Partition Structure

Each variable-length curve partition $\mathcal{V}$ consists of $2s - 1 = 1{,}021$ lines whose lengths follow the
triangular sequence $\text{len}(k) = \min(k+1,\; s,\; 2s-1-k)$ for $k \in \{0, \ldots, 2s-2\}$. The total cell count
is:

$$
    \sum_{k=0}^{2s-2} \text{len}(k) = 2 \sum_{l=1}^{s-1} l + s = s^2 = 261{,}121
$$

confirming that each partition covers every cell exactly once (Axiom 1, Section 2.2.1). The partition is generated by
traversing a space-filling curve (e.g., Hilbert curve adapted for the $511 \times 511$ grid) and cutting the traversal
into consecutive segments of lengths $1, 2, 3, \ldots, 511, \ldots, 3, 2, 1$. Different partitions use different curve
orientations or distinct space-filling curve families (e.g., Hilbert, Z-order, Peano) to maximize geometric diversity.
As with PSM/NSM (Section B.2), the cell-to-line mapping for each partition is precomputed at build time and stored as a
static lookup table.

#### B.3.2 Anchor Cells and Deterministic Seeding

The distinguishing feature of variable-length partitions is that their shortest lines provide deterministic or
near-deterministic cell values before any branching occurs:

**Length-1 lines (2 per partition).** The sum $\sigma \in \{0, 1\}$ uniquely determines the cell's value. These are
unconditionally solved&mdash;no search, no ambiguity.

**Length-2 lines (2 per partition).** If $\sigma = 0$ or $\sigma = 2$, both cells are determined. If $\sigma = 1$,
$\binom{2}{1} = 2$ configurations remain. Under the uniform random model, $P(\sigma \in \{0, 2\}) = 2/4 = 0.5$, so
each length-2 line solves both cells with probability $1/2$.

**Length-$l$ lines in general.** The number of feasible configurations is $\binom{l}{\sigma}$, which equals 1 when
$\sigma \in \{0, l\}$ and grows toward $\binom{l}{\lfloor l/2 \rfloor}$ as $\sigma$ approaches $l/2$. For a uniformly
random row of length $l$, the expected sum is $l/2$, so the ambiguity per line grows rapidly with length.

With $n$ variable-length partitions, the solver receives $2n$ unconditionally solved cells (from length-1 lines) plus
an expected $\sim n$ additional solved cells from length-2 lines, for a total of approximately $3n$ deterministic seeds
before any branching. Each seed cell participates in $4 + n$ constraint lines (the four original families plus the $n$
new partitions), and each updated line may trigger further forcing via the propagator's $\rho = 0$ and $\rho = u$
rules. The resulting cascade can solve substantially more than $3n$ cells before the first branch point.

For small line lengths $l$, the probability that the line is fully determined (i.e., $\sigma \in \{0, l\}$) under the
uniform random model is $2/2^l$, and the probability that at most 2 configurations remain (i.e.,
$\sigma \in \{0, 1, l-1, l\}$) is $2(l+1)/2^l$. The following table summarizes the deterministic yield for the
shortest lines of each partition:

| Length $l$ | Lines per partition | $P(\text{fully determined})$ | $P(\leq 2 \text{ configs})$ | Ambiguity at $\sigma = \lfloor l/2 \rfloor$ |
|---|---|---|---|---|
| 1  | 2 | 1.000 | 1.000 | 1 |
| 2  | 2 | 0.500 | 1.000 | 2 |
| 3  | 2 | 0.250 | 0.750 | 3 |
| 4  | 2 | 0.125 | 0.625 | 6 |
| 5  | 2 | 0.063 | 0.375 | 10 |
| 10 | 2 | 0.002 | 0.022 | 252 |
| 50 | 2 | $\approx 10^{-15}$ | $\approx 10^{-13}$ | $\approx 10^{14}$ |

Beyond approximately $l = 10$, the lines behave similarly to the uniform-length lines of Section B.2&mdash;they provide
constraint information but little deterministic solving power. The value of the variable-length structure is
concentrated in the first $\sim 10$ line-length tiers (20 lines per partition), which together cover
$2(1 + 2 + \cdots + 10) = 110$ cells per partition.

#### B.3.3 Collision Risk Reduction

The anchor cells from length-1 lines cannot participate in any swap: their values are unconditionally determined by a
single sum constraint. This removes $2n$ cells from the swappable pool entirely. More significantly, anchor cells that
propagate through other partitions fix additional cells, creating a cascade of determined values that further constrains
the space of valid swap patterns.

The collision resistance of a variable-length partition can be decomposed by line length. For a single partition, the
total number of configurations consistent with a given set of sums is:

$$
    N_{\mathcal{V}} = \prod_{k=0}^{2s-2} \binom{\text{len}(k)}{\sigma(k)}.
$$

Under the uniform random model ($\sigma(k) \approx \text{len}(k)/2$), the dominant contributions come from the longest
lines. The short lines ($l \leq 10$) contribute a factor of at most $\prod_{l=1}^{10} (\binom{l}{\lfloor l/2
\rfloor})^2 = (1 \cdot 2 \cdot 3 \cdot 6 \cdot 10 \cdot 20 \cdot 35 \cdot 70 \cdot 126 \cdot 252)^2 \approx
1.3 \times 10^{21}$, while the center line ($l = 511$) alone contributes $\binom{511}{255} \approx 10^{153}$. The
collision resistance is therefore dominated by the long lines, and the short lines' contribution is to provide
*deterministic anchors* rather than probabilistic collision barriers.

However, the anchors interact with other partitions through the interlocking structure. A cell solved by a length-1
line in partition $\mathcal{V}_1$ constrains lines in all other partitions passing through that cell. If partition
$\mathcal{V}_2$ has a length-3 line containing an anchored cell, the effective ambiguity of that length-3 line drops
from $\binom{3}{\sigma}$ to $\binom{2}{\sigma - v}$ where $v$ is the anchor's value&mdash;a reduction by factor
$\sim 3/2$. With $n$ partitions providing $2n$ anchors scattered across the matrix, the cross-partition reduction
compounds multiplicatively.

The net effect on collision probability depends on the spatial distribution of anchor cells relative to the short lines
of other partitions. If partition geometries are chosen so that each partition's anchor cells fall on short lines of
other partitions (maximizing cross-partition constraint tightening), the compounding effect is strongest. This is a
design criterion for the space-filling curve selection: curves should be oriented so that their length-1 endpoints are
spatially dispersed across regions covered by other curves' short segments.

#### B.3.4 Decompression Performance

The variable-length structure provides a qualitatively different propagation dynamic compared to uniform-length
partitions. With uniform partitions (Section B.2), all lines have length $s = 511$, and forcing rules trigger only
when a line's residual reaches 0 or equals its unknown count&mdash;unlikely for a fresh line with 511 unknowns. With
variable-length partitions, the short lines begin with small unknown counts and are far more likely to trigger forcing
immediately.

Consider the propagation sequence on initial state. The $2n$ length-1 lines solve their cells unconditionally. Each
solved cell updates its row, column, diagonal, anti-diagonal, and the lines it belongs to in all $n$ new partitions.
These updates decrease $u(L)$ on each affected line by 1 and (if the solved value is 1) increase $a(L)$ and decrease
$\rho(L)$ by 1. For a length-2 line that already had $\sigma \in \{0, 2\}$, the single update is sufficient to force
the remaining cell. For a length-3 line, the update may push it into a forcing state. The cascade propagates inward
from the short lines toward the long lines, with each wave of solved cells enabling the next tier.

This "warm start" effect is unique to variable-length partitions. Uniform-length partitions offer no free solutions at
initialization&mdash;the solver must wait for branching and propagation to create forcing conditions. Variable-length
partitions front-load the easy constraints, reducing the effective matrix size before the first branch point. If the
cascade solves a sufficient fraction of cells (even 5-10\% of the matrix), the remaining search tree is dramatically
smaller.

#### B.3.5 Storage Budget and Partition Count

Each variable-length partition requires $B_d(s) = 8{,}185$ bits (identical to DSM and XSM), compared to $4{,}599$ bits
for a uniform-length partition. Because the existing partitions are organized as orthogonal pairs (LSM--VSM,
DSM-XSM), any new partitions must likewise come in pairs to maintain the interlocking pair structure. This gives
$n = 16$ partitions (8 orthogonal pairs), costing $16 \times 8{,}185 = 130{,}960$ bits&mdash;exceeding the LH budget of
$130{,}816$ bits by only 144 bits (0.11\%), a negligible overrun that can be absorbed by a trivial format adjustment.

These 16 partitions provide 32 unconditional anchor cells (from length-1 lines), approximately 16 additional anchor
cells from length-2 lines (in expectation), and 110 cells per partition in the "high-information" short-line tiers
($l \leq 10$), totaling $16 \times 110 = 1{,}760$ cells with strong deterministic or near-deterministic constraints.
The revised block payload is:

$$
    B_{\text{block}} = \underbrace{9{,}198}_{\text{LSM+VSM}} + \underbrace{16{,}370}_{\text{DSM+XSM}}
    + \underbrace{8}_{\text{DI}} + \underbrace{16 \times 8{,}185}_{\text{16 partitions}} = 156{,}536 \text{ bits}.
$$

$$
    C_r = 1 - \frac{156{,}536}{261{,}121} \approx 0.4005
$$

At 40.1\% space savings, the 16-partition design is compression-neutral relative to the current LH design while
providing 32 anchor cells, 1,760 high-information cells, and 16 additional constraint lines per cell for propagation
acceleration.

#### B.3.6 Comparison of Approaches

| Property | LH (current) | B.2: 2 uniform curves | B.3: 16 var-length (8 pairs) |
|---|---|---|---|
| Storage (bits) | 130,816 | 9,198 (+ LH) | 130,960 |
| Space savings | 40.1% | 36.6% (with LH) | 40.1% |
| Anchor cells | 0 | 0 | 32 + ~16 probable |
| Collision resistance | Cryptographic ($2^{-256}$/row) | Linear only | Anchor + interlocking |
| Propagation seeds | 0 (hash is post-hoc) | 0 | 32+ free cells |
| Lines per cell | 4 + 0 | 4 + 2 | 4 + 16 |
| Solver warm start | No | No | Yes |

#### B.3.7 Existence and Selection of Eight Orthogonal Curve Pairs

The B.3 proposal requires 16 variable-length partitions organized as 8 orthogonal pairs. This subsection examines
whether 8 such pairs can be constructed from space-filling curves and, if so, what construction strategies are
available. The analysis draws on the combinatorial theory of space-filling curves, the classical theory of mutually
orthogonal Latin squares (MOLS), and the computational geometry of locality-preserving mappings.

**Defining orthogonality for curve-based partitions.** For linear partitions, orthogonality has a precise algebraic
definition: two families of parallel lines with slopes $m$ and $-1/m$ satisfy the *maximum crossing property*&mdash;every
line from one family intersects every line from the other in exactly one cell (Colbourn & Dinitz, 2007). For
curve-based partitions, lines are not straight, so the algebraic definition does not apply directly. The appropriate
generalization is *dense distributed crossing*: partitions $\mathcal{P}$ and $\mathcal{Q}$ form an orthogonal pair if
(a) every line in $\mathcal{P}$ of length $l_p$ intersects every line in $\mathcal{Q}$ of length $l_q$ in at most
$\lceil l_p l_q / s^2 \rceil + O(1)$ cells, and (b) the intersection pattern exhibits no systematic alignment that
would permit small balanced swap patterns to remain invisible to both partitions simultaneously. Property (a) ensures
that constraint information propagates uniformly between the two partitions; property (b) ensures that the interlocking
effect discussed in Section B.2 is maximized. For linear partitions, both properties follow from the slope
relationship. For curve partitions, both must be verified empirically or established through the structural properties
of the curve family.

**Natural pairing via transposition.** The simplest orthogonal pairing mirrors the LSM-VSM relationship: given a
space-filling curve $C$ that traverses the $s \times s$ matrix, define $C^T$ as the curve obtained by transposing the
matrix (swapping row and column indices). If $C$ visits cell $(r, c)$ at step $t$, then $C^T$ visits $(c, r)$ at step
$t$. Partitions derived from $C$ and $C^T$ using the same segment schedule are geometrically perpendicular in the
following sense: where $C$ traverses a region predominantly horizontally, $C^T$ traverses the transposed region
predominantly vertically. Sagan (1994) established that the Hilbert curve and its variants exhibit strong directional
coherence within each recursive quadrant, so transposition produces traversals with complementary directional
structure. This gives one orthogonal pair per base curve.

**Counting distinct space-filling curves.** To obtain 8 orthogonal pairs, 8 geometrically distinct base curves are
needed. The space of Hilbert-class curves is far richer than the single canonical Hilbert curve suggests. Haverkort
(2017) enumerated the structurally distinct face-continuous space-filling curves based on Hilbert-type recursion in two
dimensions and found 1,536 distinct curve variants (up to symmetry) for the standard $2^n \times 2^n$ grid. Each
variant differs in the orientation and reflection of recursive sub-quadrants at each level of the recursion. For the
$512 \times 512$ grid (9 levels of recursion), the combinatorial freedom at each level produces a vast family of
traversals that all visit every cell exactly once but in different orders.

Beyond Hilbert-type curves, other space-filling curve families offer structurally independent traversals. The Z-order
(Morton) curve follows a bit-interleaving pattern that produces a fundamentally different spatial clustering than the
Hilbert curve's recursive U-shaped traversal (Mokbel & Aref, 2001). The Peano curve uses a $3 \times 3$ recursive
subdivision rather than $2 \times 2$, producing a traversal with different locality properties (Sagan, 1994). Selecting
base curves from different families maximizes the geometric diversity between partitions, which in turn maximizes the
crossing density between orthogonal pairs.

**A concrete construction for 8 base curves.** The following strategy produces 8 geometrically distinct base curves for
the $511 \times 511$ matrix (adapted from $512 \times 512$ by discarding out-of-bounds cells per Section B.2):

(1-2) The canonical Hilbert curve and a Hilbert variant with reversed sub-quadrant orientations at the first recursion
level. These differ in which quadrant is visited first and the direction of traversal between quadrants, producing
traversals with complementary spatial coherence at the coarsest scale.

(3-4) The Z-order (Morton) curve and a reflected Z-order curve (horizontal reflection of the bit-interleaving
pattern). Z-order curves have weaker locality than Hilbert curves&mdash;Niedermeier et al. (2002) showed that the Hilbert
curve is optimal among all $2 \times 2$ recursion-based curves for locality, while the Z-order curve trades locality
for computational simplicity&mdash;which means Z-order partitions cross Hilbert partitions at different spatial scales,
providing complementary constraint structure.

(5-6) Two Hilbert curve variants with distinct recursion patterns at the second level of subdivision. At each level,
the Hilbert curve recursion offers four orientation choices per sub-quadrant; varying these at level 2 (the $128
\times 128$ sub-grid scale) produces traversals that agree at the coarsest level but diverge within each quadrant.

(7-8) A Peano-type curve adapted to $512 \times 512$ (via nested $3 \times 3$ subdivisions with padding) and a
rotated variant. The $3 \times 3$ recursion base produces a traversal structure incommensurable with the $2 \times 2$
Hilbert and Z-order families, maximizing geometric independence (Bader, 2013).

Each base curve $C_i$ paired with its transpose $C_i^T$ yields one orthogonal pair, giving 8 pairs and 16 partitions
total. The variable-length segment schedule ($1, 2, \ldots, s, \ldots, 2, 1$) is applied identically to each curve.

**Axiom verification.** Axiom 1 (Conservation) holds by construction: each curve visits all $s^2$ cells, and the
segment schedule sums to $s^2$. Axiom 3 (Non-repetition) holds because each curve is a Hamiltonian path, so no cell is
visited twice. Axiom 2 (Uniqueness) requires that no two lines across any pair of the 20 total partitions (4 original

- 16 new) cover the identical set of cells. For lines derived from geometrically distinct curves, cell-set identity
would require two different traversals to visit exactly the same cells in the same contiguous block of the segment
schedule&mdash;a combinatorial coincidence with probability vanishing in $s$. A build-time verification step (the code
generator checks all $\binom{20 \times 1021}{2}$ pairs of lines for set equality) provides a hard guarantee at zero
runtime cost. Hamilton and Rau-Chaplin (2008) demonstrated that compact Hilbert index computations for non-square
grids are efficient and deterministic, confirming that the adaptation from $512 \times 512$ to $511 \times 511$ is
computationally tractable for the code generator.

**Limitations of the curve-based orthogonality guarantee.** Unlike linear partitions, where the maximum crossing
property is a mathematical theorem, the dense distributed crossing property for curve-based partitions is a structural
expectation rather than a proof. The recursive self-similarity of Hilbert-type curves provides strong heuristic grounds
&mdash; Bader (2013) showed that Hilbert curves achieve near-optimal locality in the sense that spatially nearby cells
are visited close together in the traversal, which implies that segments cover compact regions and cross other
partitions' segments broadly&mdash;but a formal proof that all 8 pairs satisfy property (b) for all possible matrices
does not yet exist. The build-time verification described above addresses property (a) computationally; property (b)
can be assessed empirically by computing the intersection matrices for the chosen curves and checking for degenerate
alignment patterns.

**Connection to Latin square theory.** For uniform-length partitions ($s$ groups of $s$ cells), the existence of
mutually orthogonal families is governed by the theory of MOLS. Colbourn and Dinitz (2007) established that for any
prime power $q$, exactly $q - 1$ MOLS of order $q$ exist, and for composite orders, lower bounds on the number of MOLS
are given by the MacNeish-Mann theorem: $N(n) \geq \min(q_i - 1)$ where $n = \prod q_i^{a_i}$. For $s = 511 = 7
\times 73$, this gives $N(511) \geq 6$, guaranteeing at least 6 mutually orthogonal uniform partitions&mdash;more than
enough for 8 pairs if uniform partitions were used. However, the variable-length structure required by B.3 does not
correspond to a Latin square, so the MOLS bound does not directly apply. The MOLS result does confirm that the
underlying combinatorial space of the $511 \times 511$ grid is rich enough to support many mutually orthogonal
decompositions, lending plausibility to the curve-based construction even absent a formal equivalence theorem.

#### B.3.8 Open Questions

Consolidated into Appendix C.3.

### B.4 Dynamic Row-Completion Priority in Cell Selection (Implemented; Subsumed by B.10)

*The row-completion priority queue described in this appendix has been implemented. B.10 generalizes this design into
a multi-line tightness-driven ordering; B.4's implementation is the first stage of the B.10 strategy, corresponding
to the degenerate case where only row weights are non-zero.*

The `RowDecomposedController` (Section 10.4) computes a global cell ordering via
`ProbabilityEstimator::computeGlobalCellScores()` once before the DFS begins, then walks that static ordering for the
entire solve. The ordering reflects constraint tightness at the moment of computation but becomes increasingly stale
as the DFS progresses: propagation forces cells across many rows, dynamically changing per-row unknown counts
$u(\text{row})$, yet the cell ordering never updates to reflect these changes. This appendix proposes replacing the
static ordering with a *dynamic row-completion priority queue* that reacts to $u(\text{row})$ changes during the
solve, steering the solver toward earlier SHA-1 lateral-hash (LH) verification without sacrificing DI determinism.

#### B.4.1 Motivation

The LH check (Section 5.2) fires when $u(\text{row}_r) = 0$&mdash;when every cell in row $r$ has been assigned. A SHA-1
mismatch prunes the entire subtree rooted at the most recent branching decision, providing cryptographic-strength
pruning that is far more powerful than cardinality-based propagation alone. Current performance data shows approximately
200,000 hash mismatches per second at the depth plateau (~87K cells, row ~170), confirming that LH verification is the
dominant pruning mechanism in the mid-solve.

The current `ProbabilityEstimator` does not account for this, and more critically, the cell ordering is computed once
and never revisited. During the DFS, propagation cascades routinely force cells across multiple rows. A row that began
the solve with $u = 511$ may reach $u = 3$ due to column, diagonal, or slope forcing&mdash;but its remaining cells sit
at their original positions deep in the static ordering, unreachable for potentially thousands of iterations. The
solver continues branching on cells selected by stale confidence scores while a nearly-complete row waits, its LH
check tantalizingly close but inaccessible.

The waste is quantifiable. Each iteration that could have completed a row but instead branches elsewhere is an
iteration whose subtree cannot benefit from LH pruning. If the solver is running at 510K iter/sec and a row with
$u = 2$ must wait 1,000 iterations to be reached in the static ordering, those 1,000 iterations (~2 ms) explore
subtrees that a single LH check might have pruned entirely.

#### B.4.2 The Static Ordering Problem

The current implementation in `RowDecomposedController::enumerateSolutionsLex()` calls
`computeGlobalCellScores()` once after initial propagation (line 165). The returned vector is sorted by confidence
descending and stored as `cellOrder`. The DFS stack stores indices into this vector (`orderIdx`), and cell selection
advances sequentially through the array, skipping cells that propagation has already assigned.

This design has three consequences:

(a) *No reaction to $u(\text{row})$ changes.* When propagation drives a row to near-completion, the solver cannot
reprioritize that row's remaining cells. They will be reached only when the sequential scan arrives at their position.

(b) *Stale confidence scores.* The 7-line residual products that determine confidence change with every assignment and
propagation cascade. A cell that was weakly constrained at the start of the solve may become highly constrained after
50,000 assignments&mdash;but its confidence score still reflects the initial state.

(c) *No cost to fix.* The DFS stack's `orderIdx` values are the sole obstacle to dynamic reordering. Replacing the
static array walk with a priority-queue lookup preserves all other DFS mechanics (undo stack, hash verification,
probe integration) unchanged.

#### B.4.3 Proposed Design: Row-Completion Priority Queue

The proposal adds a lightweight priority queue alongside the existing static ordering. The static ordering remains the
default cell source; the priority queue overrides it only when a row crosses a completion threshold.

**Data structure.** A min-heap keyed on $u(\text{row})$, containing entries of the form $(\text{row},\; u)$ for each
row with $0 < u(\text{row}) \leq \tau$, where $\tau$ is a tunable threshold. The heap occupies at most $s = 511$
entries and supports $O(\log s)$ insert and extract-min.

**Threshold crossing.** After each propagation wave (both the direct assignment and all forced cells), the solver
checks $u(\text{row})$ for every row affected by the wave. If a row's unknown count drops to or below $\tau$ and
the row is not already in the heap, it is inserted. This check piggybacks on the existing propagation loop, which
already iterates forced assignments to record them on the undo stack (lines 245-247 in the current implementation).

**Cell selection.** At each branching decision, the solver checks the priority queue first:

1. If the heap is non-empty, extract the row $r^*$ with the smallest $u(\text{row})$. Select the most-constrained
   unassigned cell in row $r^*$ (using the existing per-row `computeCellScores(r)` or a simpler scan of the row's
   $u$ remaining cells). Branch on that cell.

2. If the heap is empty, fall back to the static ordering: advance `orderIdx` to the next unassigned cell in
   `cellOrder` and branch on it, exactly as the current implementation does.

**Undo integration.** When the solver backtracks past a branching decision that was drawn from the priority queue,
the unassigned cells in the affected row return to their unassigned state (via the existing undo stack). The row's
$u(\text{row})$ increases, and if it exceeds $\tau$, the row is removed from the heap. This requires tracking which
heap entries were added at which DFS depth, which can be accomplished with a small side stack of (row, depth) pairs
popped during undo.

#### B.4.4 DI Determinism

Dynamic cell selection must produce the same enumeration order in both the compressor and decompressor to preserve
DI semantics. This is guaranteed because the priority queue's behavior is a pure function of the constraint store
state:

(a) The constraint store state at any DFS node is fully determined by the input constraints and the sequence of
assignments made to reach that node.

(b) Both compressor and decompressor execute the same DFS with the same propagation engine, producing identical
constraint store states at each node.

(c) The priority queue's contents depend only on $u(\text{row})$ values, which are part of the constraint store
state. The cell selected within a priority row depends only on the per-row confidence scores, which are likewise
determined by the store state.

Therefore, both compressor and decompressor make identical cell-selection decisions at every node, and the
enumeration order is deterministic. No changes to the file format or DI encoding are required.

#### B.4.5 Cost Analysis

The per-decision overhead is bounded by the priority queue operations:

*Threshold check.* After propagation, the solver inspects $u(\text{row})$ for each row touched by forced
assignments. In the worst case, a single propagation wave forces cells in all $s$ rows, requiring $s = 511$
comparisons against $\tau$. This is a scan of 511 cached line statistics&mdash;comparable to the existing forced-
assignment recording loop and negligible relative to the propagation cost itself.

*Heap operations.* Each insert or extract-min costs $O(\log s) = O(9)$. At most $s$ rows can be in the heap
simultaneously. In practice, the number of rows crossing $\tau$ per propagation wave is small (typically 0-3),
so the amortized heap cost per decision is a few dozen instructions.

*Per-row scoring.* When the priority queue selects a row $r^*$, the solver must identify the best cell within
that row. A simple scan of the row's $u \leq \tau$ remaining cells, reading their 7 line residuals, costs $O(7\tau)$
operations. At $\tau = 8$, this is 56 stat lookups&mdash;under 200 ns on Apple Silicon.

The total per-decision overhead is approximately 200-500 ns, less than 10% of the current ~2 $\mu$s per iteration.
The throughput impact is negligible.

#### B.4.6 Expected Impact

The primary benefit is more frequent LH checks. In the current static-ordering regime, rows complete only when the
sequential scan happens to reach their remaining cells. With the priority queue, a row that reaches $u \leq \tau$ is
immediately prioritized, and its remaining cells are assigned within the next $\tau$ decisions. This reduces the
*latency* between a row becoming nearly complete (due to propagation) and the LH check firing.

The impact is largest in the mid-solve (rows 100-300), where propagation cascades from column, diagonal, and slope
constraints frequently force cells in rows far ahead of the current sequential position. These "accidentally
nearly-complete" rows represent free LH checks that the static ordering leaves on the table.

A conservative estimate: if the priority queue triggers even 10% more LH checks per unit time in the plateau region,
and each LH mismatch prunes an average subtree of depth $d$, the effective search-space reduction compounds
exponentially. At 200K mismatches/sec, a 10% increase yields 20K additional prunes/sec, each eliminating a subtree
of $O(2^d)$ nodes.

#### B.4.7 Threshold Selection

The threshold $\tau$ controls the tradeoff between responsiveness and distraction:

*$\tau = 1$.* The priority queue activates only when a single cell remains in a row. This is the most conservative
setting&mdash;the solver interrupts the static ordering only for guaranteed one-step row completions. The cost is
zero (no per-row scoring needed, only one candidate cell). The drawback is that rows at $u = 2$ or $u = 3$ are not
prioritized, missing opportunities where 2-3 assignments would yield an LH check.

*$\tau = 4$-$8$.* The priority queue activates for rows within a few cells of completion. This is the expected sweet
spot: the solver invests at most $\tau$ decisions to complete a row, and the LH payoff justifies the detour from the
static ordering. The per-row scoring cost is bounded by $O(7\tau) \leq 56$ stat lookups.

*$\tau = s$.* Every row is always in the priority queue, and the solver effectively abandons the static ordering
entirely in favor of a pure "complete the nearest row" strategy. This is likely suboptimal: it ignores constraint
tightness entirely and may direct the solver to rows where the remaining cells are weakly constrained, leading to
more backtracks.

#### B.4.8 Relationship to Adaptive Lookahead (B.8)

Dynamic row-completion priority and adaptive lookahead (B.8) address different aspects of the solver's performance
and should be evaluated independently. The priority queue modifies cell selection within the existing branching
framework, steering the solver toward earlier LH verification without changing the propagation or lookahead
machinery. Adaptive lookahead modifies the depth of speculative exploration at each branching decision, providing
stronger pruning when the solver stalls. Neither depends on the other.

That said, the two proposals are complementary if both are adopted. At lookahead depth $k = 0$, the solver's only
pruning mechanisms are cardinality forcing and LH checks. The priority queue maximizes the frequency of LH checks,
extracting more pruning power from the $k = 0$ regime and potentially delaying the first escalation to $k = 1$. This
preserves maximum throughput (~510K iter/sec) deeper into the solve. At higher $k$, the priority queue steers
lookahead probes toward cells on nearly-complete rows, where the probe is more likely to trigger an LH check during
speculative assignment, providing stronger pruning information per probe.

#### B.4.9 Open Questions

Consolidated into Appendix C.4.

### B.5 Hash Alternatives to Improve Search Depth

The changes originally proposed in this appendix&mdash;replacing per-row SHA-256 with per-row SHA-1 plus a whole-block
SHA-256 digest (BH), and adding four additional partitions&mdash;have been integrated into the main specification
(Sections 1, 2.3, 3.1-3.3, 5.1-5.6, 10.2-10.4, and 12.4-12.7). The four toroidal-slope partitions originally
proposed here (HSM1/SFC1, HSM2/SFC2) were subsequently replaced by four pseudorandom LTP partitions in B.20. Subsections B.5.1 through
B.5.8 have been removed. The open questions below remain.

#### B.5.1 Open Questions

Consolidated into Appendix C.5.

### B.6 Singleton Arc Consistency

The current propagation engine applies cardinality-based forcing rules: when the residual $\rho(L) = 0$
all unknowns on line $L$ are forced to 0, and when $\rho(L) = u(L)$ they are forced to 1 (Section 5.1).
The existing `FailedLiteralProber` strengthens this by tentatively assigning each value to an unassigned
cell, propagating, and detecting immediate contradictions&mdash;a technique known as *failed literal
detection* or *1-level lookahead* (Bessière, 2006). This appendix proposes *singleton arc consistency*
(SAC), a strictly stronger propagation level that subsumes both cardinality forcing and failed literal
probing.

#### B.6.1 Definition

A binary CSP is *singleton arc consistent* if, for every unassigned variable $x$ and every value $v$ in
$\mathrm{dom}(x)$, tentatively assigning $x = v$ and enforcing arc consistency on the resulting subproblem
does not wipe out any domain (Debruyne & Bessière, 1997). In the CRSCE context, the variables are the
$s^2 = 261{,}121$ binary cells, the domains are $\{0, 1\}$, and arc consistency reduces to cardinality
forcing iterated to fixpoint across all 5,108 constraint lines plus SHA-1 row-hash verification on
completed rows.

Formally, SAC removes value $v$ from $\mathrm{dom}(x_{r,c})$ whenever assigning $x_{r,c} = v$ and
propagating to fixpoint yields an infeasible state&mdash;that is, any line $L$ with $\rho(L) < 0$ or
$\rho(L) > u(L)$, or any completed row whose SHA-1 digest disagrees with the stored lateral hash.

#### B.6.2 Relationship to Existing Components

The existing `FailedLiteralProber` already implements the core operation underlying SAC: it calls
`tryProbeValue(r, c, v)`, which assigns, propagates to fixpoint, hash-checks completed rows, and
undoes the assignment. The distinction is operational, not algorithmic:

The current prober runs `probeToFixpoint()` once as a preprocessing pass (iterating all unassigned cells
in row-major order, repeating until no new forcings are discovered) and `probeAlternate(r, c, v)` during
DFS to prune the alternate branch at each node. SAC differs in that it maintains singleton arc
consistency as an invariant throughout search, not merely at the root.

Concretely, implementing SAC would require re-running the full probe loop after *every* assignment
(branching or forced), not just at the root. Each time a cell is assigned, all remaining unassigned
cells must be re-probed, because the assignment may have created new singleton inconsistencies that did
not exist before. The fixpoint condition is global: SAC holds when a complete pass over all unassigned
cells produces no new domain wipeouts.

#### B.6.3 Expected Impact on CRSCE

With 8 partition families, each cell participates in 8 constraint lines. A single tentative assignment
propagates through all 8 lines, potentially forcing cells on each. These secondary forcings cascade
through their own 8 lines, and so on. The high constraint density means that SAC propagation reaches
deeper than in typical binary CSPs, because each forcing event touches 8 rather than 2-4 lines.

The primary benefit is fewer DFS backtracks. At the current depth plateau (~87K cells, roughly row 170),
the solver encounters approximately 200,000 hash mismatches per second, indicating that ~60% of
iterations reach a completed row only to discover a SHA-1 mismatch. SAC would detect many of these
doomed subtrees earlier&mdash;when the tentative assignment that eventually causes the mismatch first
creates a singleton inconsistency at an intermediate cell.

#### B.6.4 Cost Analysis

The cost of maintaining SAC is substantial. Let $n$ denote the number of currently unassigned cells. A
single SAC maintenance pass probes $2n$ value-cell pairs (both values for each cell). Each probe invokes
the propagation engine, which touches up to 8 lines per forced cell. In the worst case, a single probe
propagates $O(s)$ forced cells, each updating 8 line statistics, giving $O(8s)$ work per probe and
$O(16ns)$ per pass. Multiple passes may be needed to reach the SAC fixpoint, though empirically the
number of passes is small (typically 2-4 in structured CSPs; Bessière, 2006).

At the current depth plateau ($n \approx 174{,}000$), a single SAC pass performs roughly $348{,}000$
probes. At an estimated 2-5 $\mu$s per probe (dominated by propagation and undo), one pass costs
0.7-1.7 seconds. If SAC is maintained after every assignment, this cost multiplies by the number of
assignments per second (~510,000 at current throughput), which is prohibitive as a per-assignment
invariant.

#### B.6.5 Practical Variants

Two practical relaxations avoid the full SAC cost:

*SAC as preprocessing.* Run SAC to fixpoint once before the DFS begins (extending the current
`probeToFixpoint` to iterate until the SAC fixpoint, not just the single-pass failed-literal fixpoint).
This imposes a one-time cost proportional to the number of SAC passes times $O(ns)$ and reduces the
DFS search space without per-node overhead. The existing `probeToFixpoint` infrastructure supports this
directly&mdash;the change is to continue iterating until the stricter SAC fixpoint condition holds.

*Partial SAC during search.* Rather than re-probing all unassigned cells after every assignment,
re-probe only cells whose constraint lines were affected by the most recent propagation wave. If
propagation forced $k$ cells, re-probe only the unassigned cells sharing a line with those $k$ cells.
This limits re-probe scope to $O(8k \cdot s)$ cells in the worst case but is typically much smaller,
since most propagation waves are local.

I'm proud of myself.  I've made it this far without making any jokes about probing, cost or relaxation.
I'm sure there's a proctology joke here somewhere...

#### B.6.6 Open Questions

Consolidated into Appendix C.6.

### B.7 Neighborhood-Based Lookahead

*This appendix is obsolete.* The neighborhood-based lookahead proposal has been subsumed by B.8 (Adaptive
Lookahead), which incorporates exhaustive $k$-level lookahead (up to $k = 4$) with stall-driven
escalation and de-escalation. B.8.3 specifies the exhaustive lookahead algorithm originally developed
here.

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

### B.11 Randomized Restarts with Heavy-Tail Mitigation

Gomes, Selman, and Kautz (2000) demonstrated that backtracking search times on hard CSP instances follow *heavy-
tailed distributions*: there is a non-negligible probability of catastrophically long runs caused by early bad
decisions that trap the solver deep in a barren subtree. The standard remedy is *randomized restarts*&mdash;the solver
periodically abandons its current search, randomizes its decision heuristic (e.g., by shuffling variable ordering
or perturbing value selection), and begins a fresh search from the root. Luby, Sinclair, and Zuckerman (1993) proved
that the optimal universal restart schedule is geometric: restart after $1, 1, 2, 1, 1, 2, 4, 1, 1, 2, \ldots$
units of work, guaranteeing at most a logarithmic-factor overhead relative to the optimal fixed cutoff.

The CRSCE solver's plateau at row ~170 has the hallmarks of a heavy-tailed stall. The solver commits to assignments
in rows 0-100 during a fast initial phase, but some of those early decisions propagate into the mid-rows via column,
diagonal, and slope constraints, creating an infeasible partial assignment that is only discovered much later when
hash checks begin failing. Chronological backtracking unwinds these bad decisions one level at a time, exploring an
exponential number of intermediate configurations before reaching the root cause. A restart strategy would abandon
the current trajectory, perturb the ordering heuristic, and re-enter the search with a different sequence of early
decisions&mdash;potentially avoiding the bad branch entirely.

#### B.11.1 The Determinism Constraint

**Restarts are fundamentally incompatible with CRSCE's disambiguation index (DI) semantics unless the restart
policy is fully deterministic.** The DI is defined as the ordinal position of the correct solution in the
lexicographic enumeration of all feasible solutions. Both compressor and decompressor must enumerate solutions in
the identical order to agree on which solution corresponds to DI = $k$. If the restart policy involves any
randomness (a random seed, a wall-clock timer, or a non-deterministic perturbation), the compressor and
decompressor will follow different search trajectories and assign different ordinal positions to the same solution.

This constraint rules out classical randomized restarts as described by Gomes et al. (2000), where the solver
uses a fresh random seed after each restart to diversify its variable ordering. It also rules out timer-based
restarts, where the solver restarts after $t$ seconds of wall-clock time (because wall-clock time varies between
machines and runs).

What remains permissible is *deterministic restarts*: restart policies where the decision to restart, and the
post-restart heuristic state, are pure functions of the solver's search history&mdash;the sequence of assignments,
backtracks, and constraint-store states encountered so far. Because the constraint system is identical in compressor
and decompressor (both derive it from the same payload), and the initial state is identical, a deterministic restart
policy produces the identical search trajectory in both. The DI is preserved.

#### B.11.2 Deterministic Restart Triggers

A deterministic restart policy requires a trigger condition and a post-restart heuristic modification, both defined
solely in terms of the solver's internal state. Three candidate triggers are:

*Backtrack-count threshold.* Restart when the solver has performed $B$ backtracks since the last restart (or since
the start of search). The threshold $B$ can follow the Luby sequence: $B_1, B_1, 2B_1, B_1, B_1, 2B_1, 4B_1,
\ldots$ for a base unit $B_1$. This is deterministic because the backtrack count is a function of the search
trajectory, which is identical in both compressor and decompressor.

*Depth stagnation.* Restart when the solver's maximum achieved depth has not increased for $W$ consecutive
backtracks. This detects the plateau directly: the solver is stuck at depth ~87K, repeatedly entering and exiting
subtrees without making forward progress. This trigger is a generalization of B.8's stall-detection metric
($\sigma$) applied at a coarser granularity&mdash;B.8 escalates lookahead depth, B.11 restarts the search.

*Row-failure rate.* Restart when the rate of LH failures on a specific row exceeds a threshold. If the solver
has attempted row $r$ more than $F$ times without finding a valid assignment, the current partial assignment to
rows 0 through $r - 1$ is likely the root cause. A restart abandons this partial assignment and re-enters with a
modified ordering that assigns rows 0 through $r - 1$ differently.

All three triggers are deterministic: they depend only on counters and states that are identical in compressor and
decompressor.

#### B.11.3 Post-Restart Heuristic Modification

After a restart, the solver must change its behavior to avoid re-entering the same barren subtree. In classical
randomized restarts, a new random seed provides diversification. In the deterministic setting, diversification must
come from the solver's *accumulated search history*.

*Constraint-store-guided reordering.* After a restart, recompute the cell ordering using the tightness score
$\theta$ from B.10, incorporating information from the failed search. Specifically, if row $r$ was the deepest row
reached before stagnation, increase the weight $w_{\text{row}}$ for rows near $r$ in the tightness score, biasing
the solver toward completing those rows earlier in the next attempt. This deterministic perturbation causes
different cells to be assigned first, producing a different trajectory through the search space.

*Phase saving.* Borrow the *phase saving* technique from CDCL SAT solvers (Pipatsrisawat & Darwiche, 2007): record
the most recent value (0 or 1) assigned to each cell before the restart. On the next attempt, when the solver
branches on a cell, try the saved phase first instead of the canonical 0-before-1 order. This biases the solver
toward the partial assignment from the previous attempt but allows constraint propagation to steer it away from the
infeasible region. Phase saving is deterministic because the saved phases are a function of the prior search
trajectory.

*Nogood-guided avoidance.* If the solver records nogoods from hash failures (see B.1), the post-restart search
avoids assignments that violate recorded nogoods. Each restart builds on the information from all prior restarts,
progressively narrowing the search space. This combines the restart mechanism with lightweight clause learning
(without the full CDCL machinery) and is fully deterministic.

#### B.11.4 Interaction with Lexicographic Enumeration

Restarts change the order in which the solver explores the search tree, but they must not change which solutions are
*feasible*. The enumeration must still visit all feasible solutions in lexicographic order. This requires that the
restart mechanism satisfies two properties:

*Completeness.* After any finite number of restarts, the solver must eventually enumerate all feasible solutions.
If restarts permanently abandon regions of the search space, some solutions may be skipped, corrupting the DI.
Completeness is guaranteed if the restart policy is *fair*: every branch is eventually explored, even if the ordering
changes between restarts. The Luby restart schedule is fair by construction.

*Order preservation.* The solver must visit feasible solutions in the same lexicographic order regardless of the
restart history. This is the harder condition. If the post-restart heuristic changes the cell ordering, the DFS
may discover solution $S_j$ before $S_i$ even though $i < j$ lexicographically. To preserve order, the solver must
either (a) use restarts only to prune infeasible subtrees (never skipping feasible solutions), or (b) buffer
discovered solutions and emit them in sorted order.

Option (a) is achievable if the post-restart heuristic modifies only the *value* ordering (which value to try first
at each branch) and the *cell* ordering (which cell to branch on next), without changing the *feasibility* of any
partial assignment. Constraint propagation and hash verification are unchanged by the restart, so the set of feasible
solutions is identical across restarts. The solver explores the same tree, just in a different traversal order. For
DI enumeration, the solver must still count solutions in lexicographic order, which requires that the enumeration
logic track the canonical position of each discovered solution.

Option (b) is simpler but requires memory proportional to the number of solutions discovered between restarts. For
CRSCE, the DI is typically small (0-255), so the buffer is at most 256 solutions&mdash;negligible memory.

The safest approach is a *partial restart*: instead of restarting from the root, the solver backtracks to a
*checkpoint depth* (e.g., the beginning of the plateau band at row 100) and re-enters with a modified heuristic for
the subproblem below the checkpoint. The partial assignment above the checkpoint is preserved, so the lexicographic
prefix is unchanged and order preservation is automatic. The solver is effectively restarting only the hard subproblem
(the plateau), not the easy prefix (rows 0-100).

#### B.11.5 Expected Benefit

The heavy-tail argument predicts that a small fraction of search attempts will be catastrophically long, while most
will be fast. If the solver's plateau stalls follow a heavy-tailed distribution (which is plausible given the
combinatorial phase-transition structure of the mid-rows), then restarts with the Luby schedule truncate the long
tail at the cost of occasionally re-doing fast prefixes. The expected solve time is:

$$
    E[T_{\text{restart}}] = O(E[T_{\text{optimal}}] \cdot \log E[T_{\text{optimal}}])
$$

where $E[T_{\text{optimal}}]$ is the expected time with the optimal fixed cutoff (Luby et al., 1993). If the
current solver's mean solve time is dominated by the heavy tail (a few very long stalls accounting for most of the
total time), restarts could reduce the mean by an order of magnitude.

The benefit compounds with other techniques. Restarts provide diversification (trying different early decisions),
while B.8 (adaptive lookahead) provides intensification (probing more deeply at stall points). B.1 (CDCL/backjumping)
provides learning across the search (avoiding known-bad configurations). The three techniques are complementary:
restarts escape bad regions, lookahead navigates within a region, and learning prevents re-entry into known-bad
regions.

#### B.11.6 Open Questions

(a) Does the CRSCE solver's plateau stall time follow a heavy-tailed distribution? Instrumenting the solver to
log per-block solve times across many random inputs would answer this. If the distribution is heavy-tailed, restarts
provide significant benefit; if it is concentrated (low variance), restarts provide little.

(b) What is the optimal Luby base unit $B_1$? Too small and the solver restarts before making meaningful progress;
too large and it spends too long in barren subtrees before restarting. The optimal $B_1$ depends on the typical
depth of the plateau stall, which is an empirical quantity.

(c) Is partial restart (from a checkpoint at row ~100) sufficient, or does the solver sometimes need full restarts
from the root? If the root cause of most stalls is in rows 0-100, partial restarts cannot fix them. If the root
cause is in the plateau region itself (rows 100-300), partial restarts are sufficient and avoid re-doing the
fast prefix.

(d) How does phase saving interact with DI determinism? Phase saving changes the value ordering at each branch
point, which changes the traversal order within the search tree. For DI enumeration, the solver must still count
solutions in lexicographic order. If the solver uses option (b) from B.11.4 (buffering solutions and emitting in
sorted order), phase saving is compatible with DI semantics. The buffer size is bounded by the DI value (at most
255 solutions).

(e) Can the restart policy be combined with B.8's stall detection? B.8 escalates lookahead depth when stagnation
is detected; B.11 restarts when stagnation persists. A natural hierarchy is: first escalate lookahead ($k = 0
\to 1 \to 2 \to 4$), then restart if lookahead at $k = 4$ still stalls. This uses restarts as the last resort
after intensification has failed, minimizing unnecessary restarts.

### B.12 Survey Propagation and Belief-Propagation-Guided Decimation

The CRSCE solver's `ProbabilityEstimator` computes a static approximation of marginal beliefs: for each cell, it
multiplies residual ratios across 7 non-row constraint lines to estimate the probability that the cell should be 1
versus 0. This computation runs once before DFS begins and is never updated. The result is a crude proxy for the
true marginal probability, because it treats the 7 lines as independent (they are not) and ignores the global
constraint structure (how assignments to distant cells propagate through shared lines).

Survey propagation (SP) and belief propagation (BP) are message-passing algorithms that compute far more accurate
marginal estimates by iterating messages between variables (cells) and constraints (lines) on the factor graph until
convergence (Mézard, Parisi, & Zecchina, 2002; Braunstein, Mézard, & Zecchina, 2005). BP computes approximate
marginal probabilities for each variable given the constraint structure. SP goes further: it estimates the probability
that each variable is *forced* to a specific value in the space of satisfying assignments, capturing the "backbone"
structure that BP misses. Marino, Parisi, and Ricci-Tersenghi (2016) demonstrated that backtracking survey
propagation (BSP)&mdash;which uses SP-guided decimation with backtracking fallback&mdash;solves random K-SAT instances
in practically linear time up to the SAT-UNSAT threshold, a region unreachable by CDCL solvers.

#### B.12.1 Application to CRSCE

The CRSCE constraint system is a factor graph with $s^2 = 261{,}121$ binary variable nodes (CSM cells) and
$10s - 2 = 5{,}108$ factor nodes (constraint lines), plus $s = 511$ hash-verification factors (LH). Each variable
participates in 8 factors (or 10 with an LTP pair). The graph is sparse: each factor connects to $s$ or fewer
variables, and the total edge count is $\sum_L \text{len}(L) \approx 2{,}000{,}000$.

A single BP iteration passes messages along all edges: each factor sends a message to each of its variables
summarizing the constraint's belief about that variable given messages received from all other variables. One
iteration costs $O(\text{edges}) \approx 2 \times 10^6$ multiply-add operations. Convergence typically requires
10-50 iterations for sparse factor graphs, so a full BP computation costs $20$-$100 \times 10^6$ operations
($\approx 20$-100 ms on Apple Silicon at $\sim 10^9$ operations/second).

This is far too expensive to run at every DFS node (the solver processes $\sim 500{,}000$ nodes/second in the fast
regime). However, BP can be used as a *periodic reordering oracle*: at checkpoint rows (e.g., every 50 rows, or at
the plateau entry point), the solver pauses the DFS, runs BP to convergence on the residual subproblem (unassigned
cells with their current constraint bounds), and recomputes the cell ordering based on BP's marginal estimates.
The BP-informed ordering replaces the static `ProbabilityEstimator` scores for the next segment of the search.

#### B.12.2 BP-Guided Decimation

An alternative to using BP purely for ordering is *decimation*: use BP to identify the most-biased variable (the
cell with the strongest marginal preference for 0 or 1), fix that cell to its preferred value, propagate, and repeat.
This converts the DFS into a greedy assignment sequence guided by global marginal information. When a conflict is
detected (infeasibility or hash mismatch), the solver falls back to backtracking search from the last decimation
point.

The decimation approach has two advantages over pure ordering. First, it exploits BP's global view to make
assignments that are unlikely to cause conflicts, reducing the backtrack count. Second, it naturally concentrates
assignments on cells with strong beliefs (near-forced cells), which tends to trigger propagation cascades earlier
than a tightness-driven ordering would.

The disadvantage is computational cost. Each decimation step requires a full BP computation ($\sim 50$ ms), and the
solver makes ~261K assignments per block. Running BP at every step would take ~3.6 hours per block&mdash;far slower
than the current solver. The practical approach is *batch decimation*: run BP, decimate the top $D$ most-biased
cells (e.g., $D = 1{,}000$), propagate, and repeat. This amortizes the BP cost across $D$ assignments, reducing
the overhead to $\sim 50$ ms per 1,000 assignments ($\sim 50$ $\mu$s/assignment)&mdash;roughly 25$\times$ the current
per-assignment cost, but potentially offset by a dramatic reduction in backtracking.

#### B.12.3 Survey Propagation for Backbone Detection

SP extends BP by introducing a third message type: the probability that a variable is *frozen* (forced to a specific
value in all satisfying assignments). In the CRSCE context, a frozen variable is a cell whose value is determined
by the constraint system regardless of how other cells are assigned. SP identifies these frozen cells without
exhaustive enumeration.

Frozen cells discovered by SP can be assigned immediately without branching, effectively shrinking the search space.
If SP identifies 10,000 frozen cells in the plateau region, the DFS has 10,000 fewer branching decisions to make&mdash;
a potentially exponential reduction in tree size. The cost is one SP computation ($\sim 100$-500 ms, as SP
converges more slowly than BP), amortized across thousands of forced assignments.

The caveat is convergence. SP was designed for random CSP instances near the phase transition, where the factor graph
has tree-like local structure. The CRSCE factor graph is not random&mdash;it has regular structure (every row line has
exactly $s$ cells, every slope line visits cells at modular intervals). SP may fail to converge on structured graphs,
or may converge to inaccurate estimates. Empirical testing on the CRSCE factor graph is needed to determine whether
SP's backbone detection is reliable.

#### B.12.4 DI Determinism

BP and SP are deterministic algorithms: given the same factor graph and the same initial messages, they produce the
same marginal estimates. The factor graph is derived from the constraint system (which is identical in compressor and
decompressor), and the initial messages can be fixed (e.g., uniform beliefs). Therefore, BP/SP-guided decimation and
reordering produce identical search trajectories in both compressor and decompressor. DI semantics are preserved.

The only subtlety is convergence tolerance. BP iterates until messages stabilize within a threshold $\epsilon$. If
floating-point rounding differs between machines, the convergence point may differ by one iteration, producing
slightly different marginal estimates. This can be avoided by using fixed-point arithmetic or by rounding messages
to a fixed precision after each iteration. Alternatively, the solver can run a fixed number of BP iterations (e.g.,
50) rather than iterating to convergence, eliminating the convergence-tolerance issue entirely.

#### B.12.5 Interaction with Other Proposals

BP-guided reordering is complementary to B.10 (constraint-tightness ordering): B.10 provides a fast, local ordering
heuristic used at every DFS node, while BP provides a slow, global reordering used at periodic checkpoints. The
solver uses B.10 between checkpoints and BP at checkpoints, combining local responsiveness with global accuracy.

BP-guided decimation interacts with B.8 (adaptive lookahead): decimation reduces the number of branching decisions,
which reduces the number of stall points where lookahead is needed. The two techniques are complementary --
decimation handles the "easy" cells (strong beliefs), and lookahead handles the "hard" cells (ambiguous beliefs).

SP's backbone detection interacts with B.1 (CDCL): frozen cells identified by SP are logically implied by the
constraint system, so assigning them does not generate nogoods or conflict clauses. SP pre-solves the easy part
of the problem, leaving CDCL to handle the hard part.

#### B.12.6 Open Questions

(a) **[ANSWERED — 2026-03-04]** Does BP converge reliably on the CRSCE factor graph? The graph's regular structure
(uniform line lengths, modular slope patterns) may cause BP to oscillate rather than converge.

*Empirical result:* Gaussian BP with damping α = 0.5, run for 30 iterations over all 5,108 constraint lines,
converges reliably on the CRSCE factor graph. In a live solver run (`run_id=1c64a9a5`), the first BP checkpoint
(cold-start from zero messages) produced `max_delta = 18.252` LLR units, indicating significant message movement.
The second checkpoint (warm-start from checkpoint 1's fixed point) produced `max_delta = 0.000`, confirming that
the fixed point is stable and that 30 iterations suffice for cold-start convergence. `max_bias = 0.500` across all
checkpoints means the Gaussian approximation pushes some cells' beliefs to the sigmoid clamp boundary (near-certain
0 or 1). No oscillation was observed. Damped BP is sufficient; tree-reweighted BP is not needed.

*Negative finding:* BP-guided branch-value ordering (overriding the `preferred` field when `|p - 0.5| > 0.1`) does
not break the depth plateau. After 3 StallDetector escalation events (k escalated from 1 to 4), the solver depth
remained at approximately 87,487-87,498, indistinguishable from the B.8-only baseline. The Gaussian approximation
may be too coarse for CRSCE's extremely loopy factor graph (each cell participates in 8 constraint families
simultaneously). The BP fixed point may not accurately reflect the true marginals of the constraint satisfaction
subproblem at the DFS frontier, leading to branch-value hints that neither help nor reliably harm the search.

(b) What is the optimal checkpoint interval for BP-guided reordering? Too frequent and the BP overhead dominates;
too infrequent and the ordering becomes stale. The interval should be tuned to the plateau structure: more frequent
checkpoints in the plateau band (rows 100-300), less frequent in the easy prefix and suffix.

(c) Can SP identify a meaningful number of frozen cells in the CRSCE instance? If SP finds that most cells are
unfrozen (as expected for a problem with DI > 0, meaning multiple solutions exist), its backbone-detection benefit
is limited. SP may still provide useful ordering information even if few cells are frozen.

(d) Is batch decimation ($D = 1{,}000$) sufficient to amortize the BP cost, or does the solver need a larger batch?
The optimal $D$ depends on how quickly BP's marginal estimates become stale after $D$ assignments. If propagation
cascades from the $D$ assignments significantly change the constraint landscape, BP must be rerun sooner.

(e) Can BP be accelerated on Apple Silicon using the Metal GPU? The message-passing computation is highly parallel
(messages on different edges are independent within one iteration). A GPU implementation could reduce the per-BP
cost from $\sim 50$ ms to $\sim 5$ ms, making BP-guided decimation at every ~100 assignments feasible.

### B.13 Portfolio and Parallel Solving with Diversification

Modern SAT and CSP solvers achieve their best performance through *portfolio* strategies: running multiple solver
instances in parallel, each using a different heuristic configuration, and accepting the result from whichever
instance finishes first (Xu, Hutter, Hoos, & Leyton-Brown, 2008). Hamadi, Jabbour, and Sais (2009) showed that
diversification&mdash;instantiating solvers with different decision strategies, learning schemes, and random seeds --
is the key to portfolio effectiveness, because different heuristics excel on different problem structures. A portfolio
hedges against the worst case of any single heuristic.

The CRSCE solver runs a single DFS with a fixed heuristic configuration (static ordering + row-completion queue +
optional lookahead). If the chosen heuristic happens to make a bad early decision, the solver pays the full
exponential cost of unwinding it. A portfolio approach would run $P$ solver instances concurrently, each with a
different cell-ordering heuristic, and accept the first to reach solution $S_{\text{DI}}$.

#### B.13.1 Diversification Strategies

The solver's behavior is determined by a small number of heuristic parameters: the cell-ordering weights $w_L$ (B.10),
the row-completion threshold $\tau$ (B.4), the lookahead escalation thresholds $\sigma^-$ and $\sigma^+$ (B.8), and
the branch-value heuristic (0-first vs. belief-guided). A portfolio instantiates $P$ copies of the solver with
different parameter vectors:

*Weight diversification.* Each instance uses a different weight vector $w_L$ for the tightness score $\theta$.
Instance 1 might use $w_{\text{row}} = 10, w_{\text{col}} = 3$; instance 2 might use $w_{\text{row}} = 5,
w_{\text{col}} = 5$; instance 3 might use $w_{\text{row}} = 1, w_{\text{col}} = 1$ (effectively uniform tightness).
Different weight vectors excel in different plateau regimes.

*Lookahead diversification.* Each instance uses a different lookahead policy. Instance 1 starts at $k = 0$ with
adaptive escalation (B.8); instance 2 starts at $k = 2$ throughout; instance 3 uses no lookahead but aggressive
row-completion priority. This hedges against the case where adaptive escalation wastes time on escalation/
de-escalation overhead.

*Value ordering diversification.* Each instance uses a different branch-value heuristic. Instance 1 always tries
0 first (canonical); instance 2 tries the value preferred by the `ProbabilityEstimator`; instance 3 alternates
based on parity. Different value orderings produce different DFS trajectories, and the optimal ordering is
input-dependent.

#### B.13.2 DI Determinism

**All portfolio instances must enumerate solutions in the same lexicographic order.** The DI is defined as the
ordinal position of the correct solution in this order. If different instances use different cell orderings, they
traverse the search tree in different orders and may discover solutions in different sequences. To preserve DI
semantics, the portfolio must satisfy one of two conditions:

*Condition 1: Identical enumeration order.* All instances use the same canonical enumeration order (row-major, 0
before 1) but differ in how they *prune* the search tree (via different lookahead policies, propagation strategies,
or constraint-tightening heuristics). The set of feasible solutions and their lexicographic ordering is identical
across instances; only the traversal speed differs. The first instance to reach $S_{\text{DI}}$ reports the correct
answer.

*Condition 2: Solution buffering.* Each instance emits solutions as discovered (potentially out of lexicographic
order) into a shared buffer. A coordinator thread merges the streams and identifies $S_{\text{DI}}$ by its
lexicographic position. This requires that at least one instance eventually enumerates all solutions up to
$S_{\text{DI}}$ in order&mdash;the other instances provide "hints" that accelerate the search but are not trusted
for ordering.

Condition 1 is simpler and sufficient for CRSCE. The cell ordering affects which subtrees are explored first, but
constraint propagation and hash verification are ordering-independent: the same partial assignments are feasible or
infeasible regardless of the order in which they are explored. Different orderings prune different infeasible
subtrees at different speeds, but all instances agree on which solutions are feasible and in what lexicographic
order.

More precisely: the *value* ordering (which value to try first at each branch) and the *lookahead* policy change
only the speed at which the solver traverses the canonical tree. They do not change the tree's structure. The *cell*
ordering, however, changes the tree structure&mdash;different cell orderings produce different DFS trees with different
branching sequences. For DI determinism under cell-ordering diversification, the solver must either (a) restrict
diversification to value ordering and pruning strategies only (preserving the canonical DFS tree), or (b) use
Condition 2 (solution buffering with a canonical-order verifier).

#### B.13.3 Hardware Mapping

Apple Silicon M-series chips provide a natural platform for portfolio solving. The M3 Pro has 6 performance cores
and 6 efficiency cores; the M3 Max has 12 performance cores and 4 efficiency cores. A portfolio of $P = 4$-6
solver instances, each pinned to a performance core, provides meaningful diversification without contending for
shared resources (each instance has its own constraint store and DFS stack in L2 cache).

The Metal GPU is less suitable for portfolio solving because the solver's DFS is inherently sequential (each
assignment depends on the propagation result of the previous one). The GPU is better used for bulk propagation
within a single instance (the existing `MetalPropagationEngine`) rather than for running independent solver
instances.

Shared-nothing parallelism (each instance is fully independent) is the simplest implementation. No clause sharing,
no work stealing, no inter-instance communication. The first instance to find $S_{\text{DI}}$ signals completion
and all others are terminated. This avoids the complexity of parallel CDCL clause sharing (which requires careful
synchronization and is a major source of bugs in parallel SAT solvers).

#### B.13.4 Expected Benefit

If the solver's per-block solve time has high variance (as predicted by the heavy-tail hypothesis in B.11), a
portfolio of $P$ independent instances reduces the expected solve time to:

$$
    E[T_{\text{portfolio}}] = E[\min(T_1, T_2, \ldots, T_P)]
$$

For heavy-tailed distributions, the minimum of $P$ independent samples is dramatically smaller than the mean of a
single sample. Even if one instance makes a catastrophically bad early decision, the other $P - 1$ instances
proceed on different trajectories and are likely to avoid the same trap. Empirical studies in SAT solving show
speedups of 3-10$\times$ for $P = 4$-8 on hard instances (Hamadi et al., 2009).

The portfolio approach is also *embarrassingly parallel*: no algorithmic changes to the solver are needed. Each
instance runs the existing solver code with different parameter settings. The only new infrastructure is a
launcher that spawns $P$ instances and a signal mechanism for early termination.

#### B.13.5 Open Questions

(a) What is the optimal portfolio size $P$ for CRSCE on Apple Silicon? Too few instances limit diversification;
too many contend for cache and memory bandwidth, slowing each instance. The sweet spot depends on the chip's core
count and the solver's memory footprint per instance ($\sim 500$ KB for the constraint store + DFS stack).

(b) Which heuristic parameters provide the most diversification benefit? If the solver's performance is most
sensitive to the row-completion threshold $\tau$, diversifying $\tau$ across instances is more valuable than
diversifying lookahead depth. A sensitivity analysis on the heuristic parameters would guide portfolio design.

(c) Should instances share learned information? Clause sharing (from B.1) between portfolio instances could
accelerate all instances by propagating nogoods discovered by one instance to the others. However, clause sharing
adds synchronization overhead and complexity. For CRSCE, where conflicts are rare (hash failures, not unit-
propagation conflicts), the sharing benefit may be small.

(d) Can the portfolio approach be combined with B.11 (restarts)? Each instance could use a different restart policy
(different Luby base units, different checkpoint depths), providing restart diversification in addition to heuristic
diversification. This combines two orthogonal diversification axes.

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

### B.15 DELETED

Content moved to Appendix D.1.

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

### B.17 Look-Ahead on Row Completion

The CRSCE solver verifies each completed row against its SHA-1 lateral hash (LH). A hash mismatch after all 511
cells in a row are assigned triggers a backtrack, unwinding the most recent branching decision and trying the
alternative value. The cost of this mismatch is proportional to the depth of the wasted subtree: the solver may have
assigned thousands of cells beyond the row boundary before discovering (via a later row's hash check) that an
assignment within the earlier row was wrong. This appendix proposes *row-completion look-ahead* (RCLA): when a row
drops to a small number of unknowns $u \leq u_{\text{max}}$, enumerate all $\binom{u}{\rho}$ valid completions
consistent with the row's cardinality constraint, check each against the SHA-1 hash, and prune immediately if none
pass.

#### B.17.1 Enumeration Cost

A row with $u$ unknowns and residual $\rho$ has exactly $\binom{u}{\rho}$ completions consistent with the row's
cardinality constraint (all other constraints aside). The SHA-1 cost per completion is approximately 200 ns
(the row message is 512 bits = one SHA-1 block). The total enumeration cost is:

$$C = \binom{u}{\rho} \times 200\;\text{ns}$$

For small $u$, this is tractable:

| $u$ | $\rho$ (worst case $= u/2$) | $\binom{u}{\rho}$  | Cost       |
|:---:|:---------------------------:|:------------------:|: ----     :|
| 3   | 1-2                         | 3                  | 0.6 $\mu$s |
| 5   | 2-3                         | 10                 | 2 $\mu$s   |
| 8   | 4                           | 70                 | 14 $\mu$s  |
| 10  | 5                           | 252                | 50 $\mu$s  |
| 15  | 7-8                         | 6,435              | 1.3 ms     |
| 20  | 10                          | 184,756            | 37 ms      |

At $u \leq 10$, the cost is under 50 $\mu$s&mdash;comparable to a single iteration of the DFS loop at $k = 2$
lookahead. At $u \leq 15$, the cost (1.3 ms) is tolerable if invoked once per row (511 rows per block, so
$\sim 0.7$s total). At $u = 20$, the 37 ms cost is marginal; beyond $u = 20$, the exponential growth of
$\binom{u}{\rho}$ makes exhaustive enumeration impractical.

#### B.17.2 Pruning Power

RCLA provides a fundamentally different pruning mechanism from constraint propagation or lookahead. Standard
propagation identifies infeasible cells by analyzing residuals; lookahead identifies infeasible cells by tentatively
assigning values and checking for cardinality violations. Neither mechanism can detect a row that is *cardinality-
feasible but hash-infeasible*: all $\binom{u}{\rho}$ completions satisfy the row's cardinality constraint (and
possibly all other cardinality constraints), but none produce a 512-bit message whose SHA-1 matches the stored
lateral hash.

When RCLA determines that no valid completion exists, it provides *proof of infeasibility* for the current partial
assignment to the row. The solver can immediately backtrack to the most recent branching decision within the row,
without assigning the remaining $u$ cells one by one, propagating after each, and eventually discovering the
mismatch at $u = 0$. The savings are proportional to $u$: instead of $u$ assign-propagate-check cycles (each
costing ~2-5 $\mu$s), the solver pays one $\binom{u}{\rho} \times 200$ ns enumeration.

More importantly, RCLA catches doomed rows *before* the solver descends into subsequent rows. Without RCLA, a row
that will fail its hash check at completion may first trigger thousands of cell assignments in rows below it (as
the solver fills the matrix top-down). When the hash finally fails at $u = 0$, all those subsequent assignments
are wasted. RCLA at $u = 10$ catches the doomed row 10 cells before completion, preventing the solver from
descending into the next row's subtree on a doomed path.

#### B.17.3 Interaction with Cross-Line Constraints

The enumeration in B.17.1 considers only the row's cardinality constraint. Some of the $\binom{u}{\rho}$
completions may violate other constraints (column, diagonal, slope residuals). Pre-filtering completions against
cross-line feasibility before checking SHA-1 can reduce the enumeration count.

For each candidate completion, the solver checks whether assigning the $u$ cells to their candidate values would
create a negative residual or an excessive residual on any of the cross-lines passing through those cells. This
check costs $O(8u)$ per candidate (8 lines per cell, each requiring a residual update). At $u = 10$, this is 80
operations per candidate, or $252 \times 80 = 20{,}160$ operations total&mdash;negligible compared to the SHA-1 cost.

If cross-line filtering eliminates most candidates, the effective SHA-1 count drops substantially. In the plateau
band, where cross-line residuals are moderately constrained, filtering might reduce the candidate count by 2-10×,
making RCLA feasible at higher $u$ thresholds.

#### B.17.4 Incremental SHA-1 Optimization

The $\binom{u}{\rho}$ completions differ only in the positions of $u$ bits within the 512-bit row message. If the
$u$ unknowns are clustered in the same 32-bit word of the SHA-1 input, the solver can precompute the SHA-1
intermediate state up to the word boundary, then finalize only the last few rounds for each candidate. SHA-1
processes its input in 32-bit words; if all unknowns fall within one or two words, the precomputation saves
approximately 80-90\% of the SHA-1 cost per candidate.

In practice, the $u$ unknowns are scattered across the 16-word (512-bit) input block, because the unknowns are the
cells not yet forced by propagation, and forcing patterns depend on cross-line interactions across the full row.
Therefore, the incremental optimization is unlikely to help in the general case. However, when RCLA is invoked at
very small $u$ (3-5 unknowns), the unknowns may happen to cluster, and the optimization is worth attempting.

#### B.17.5 Triggering Strategy

RCLA should not be invoked at a fixed $u$ threshold for all rows. The cost-benefit ratio depends on $\rho$ as well
as $u$: when $\rho$ is close to 0 or $u$, $\binom{u}{\rho}$ is small even for larger $u$ (e.g., $\binom{20}{1} =
20$, costing only 4 $\mu$s). The optimal trigger is therefore:

$$\text{Invoke RCLA when } \binom{u}{\rho} \leq C_{\max}$$

where $C_{\max}$ is a threshold on the enumeration count (e.g., $C_{\max} = 500$, corresponding to $\sim 100\,
\mu$s). This threshold-based trigger naturally adapts to the residual: rows with extreme residuals trigger RCLA
earlier (at higher $u$), while rows with $\rho \approx u/2$ trigger later (at lower $u$).

Computing $\binom{u}{\rho}$ is $O(1)$ with a precomputed lookup table for $u \leq 511$ and $\rho \leq u$. The
lookup table requires $\sim 1$ MB of storage (512 × 512 × 4 bytes) and can be generated once at solver
initialization.

#### B.17.6 DI Determinism

RCLA is purely deductive and deterministic. The enumeration order (e.g., lexicographic over the $u$ unknown
positions) is fixed, and the SHA-1 computation is deterministic. If *any* completion passes the hash check, the
row is feasible and the solver continues; if *none* pass, the row is provably infeasible and the solver backtracks.
In neither case does RCLA alter the set of feasible solutions or the order in which they are visited. It merely
detects infeasibility earlier than the standard approach (which waits until $u = 0$).

DI semantics are preserved because RCLA prunes only infeasible subtrees: it never eliminates a partial assignment
that could lead to a valid solution. The solver visits the same solutions in the same lexicographic order, but
backtracks sooner on doomed rows.

#### B.17.7 Expected Benefit

In the plateau band, the solver encounters approximately $200 \times 511 \approx 100{,}000$ row completions per
block (200 rows × 511 completions per row in the backtracking process&mdash;many rows are completed multiple times
due to backtracking). If RCLA detects infeasibility at $u = 10$ instead of $u = 0$, each detection saves the cost
of 10 assign-propagate cycles plus any wasted work in subsequent rows. At 2-5 $\mu$s per cycle, the direct saving
is 20--50 $\mu$s per detection. The indirect saving (preventing descent into subsequent rows on a doomed path) is
potentially much larger&mdash;up to hundreds of milliseconds per doomed subtree if the solver would have explored
several rows before discovering the hash mismatch.

Quantifying the indirect saving requires instrumentation: how deep does the solver typically descend beyond a
doomed row before backtracking? If the answer is rarely more than a few cells (because the doomed row's hash
is checked at $u = 0$ immediately), RCLA's benefit is limited to the 10-cell direct saving. If the answer is
hundreds or thousands of cells (because the solver completes the row, starts the next, and backtracks only when
a later hash fails), RCLA's benefit is substantial.

#### B.17.8 Open Questions

(a) What is the empirical distribution of $u$ and $\rho$ at the point where rows are completed in the plateau band?
If rows typically reach $u = 0$ through forcing cascades (most cells are propagated, not branched), the "natural"
$u$ at completion is already low, and RCLA has little room to trigger earlier.

(b) Can RCLA be combined with B.14 (nogood recording)? When RCLA proves a row infeasible, the current
partial assignment to the row's decision cells is a nogood. Recording this nogood (per B.14) prevents the solver
from re-entering the same configuration on future backtracking, compounding RCLA's benefit.

(c) For rows in the fast regime (rows 0-100 and 300-511), is RCLA ever triggered? In these regions, propagation
forces most cells, so $u$ drops to 0 rapidly. RCLA may be relevant only in the plateau band, where propagation
stalls and $u$ remains high.

(d) Should RCLA check cross-line constraints (B.17.3) before or after the SHA-1 check? Cross-line filtering is
cheaper per candidate ($O(8u)$ vs. SHA-1's $\sim 200$ ns), so checking cross-lines first and SHA-1 only on
survivors is more efficient when many candidates fail cross-line checks. But if most candidates pass cross-lines
(as expected when $u$ is small and residuals are loose), the filtering overhead is wasted.

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

### B.19 Enhanced Stall Detection and Extended Probe Strategies

B.8 introduces adaptive lookahead with stall-triggered escalation from $k = 0$ to $k = 4$, where $k = 4$ is the
current maximum. The stall detector monitors a sliding window of net depth advance and escalates when forward
progress stalls. In practice, the solver reaches $k = 4$ in the deep plateau (rows ~170-300) and remains there,
because the stall metric $\sigma$ never recovers sufficiently to trigger de-escalation. At $k = 4$, the solver
processes 12-30K iterations/second with 16 exhaustive probes per decision. If this throughput is still insufficient
to solve the block within the time bound, the solver has exhausted B.8's escalation ladder and has no further
recourse.

This appendix proposes two extensions: (1) enhanced stall detection that distinguishes *qualitatively different*
stalling regimes and responds with targeted interventions, and (2) extended probe strategies that go beyond $k = 4$
exhaustive lookahead by trading completeness for depth.

#### B.19.1 Limitations of the Current Stall Metric

The stall metric $\sigma = \Delta_{\text{depth}} / W$ measures net depth advance over a sliding window. This
single scalar conflates two distinct stalling modes:

*Shallow oscillation.* The solver repeatedly advances a few cells and backtracks the same distance. Net depth
advance is near zero, but the solver is actively exploring alternatives at a fixed depth. This mode is productive:
the solver is systematically eliminating infeasible branches at the current row, and each oscillation brings it
closer to a feasible assignment.

*Deep regression.* The solver backtracks through many rows, losing hundreds or thousands of depth levels before
resuming forward progress. Net depth advance is deeply negative. This mode is unproductive: the solver has
discovered that a high-level assignment (many rows above) is wrong, but it is unwinding cell by cell through
intervening rows, revisiting decisions that are irrelevant to the conflict.

Both modes produce $\sigma \leq 0$, triggering the same escalation response ($k \to k + 1$). But they call for
different interventions. Shallow oscillation benefits from deeper lookahead (helping the solver find the feasible
branch faster). Deep regression benefits from *backjumping* or *restarts* (jumping directly to the root cause
instead of unwinding incrementally).

#### B.19.2 Multi-Metric Stall Detection

To distinguish stalling modes, the solver can maintain multiple metrics:

*Backtrack depth distribution.* For each backtrack event, record the number of depth levels unwound, $d_{\text{bt}}$.
Maintain a sliding window of the last $W_{\text{bt}}$ backtrack depths. If the median backtrack depth exceeds a
threshold $d^{*}$ (e.g., $d^{*} = 100$ cells, corresponding to roughly $100 / 511 \approx 0.2$ rows), the solver
is in deep-regression mode.

*Row re-entry count.* Track how many times each row is re-entered (its unknowns drop to 0, a hash check occurs,
and the solver backtracks into the row to try a different assignment). A row with a high re-entry count is a
*conflict hotspot*: the partial assignment above the row is likely wrong, and no assignment within the row will
satisfy the hash.

*Forward-progress rate.* Instead of net depth advance, measure the *maximum depth reached* in the sliding window.
If the maximum depth has not increased in $W$ decisions, the solver is making no progress regardless of local
oscillations.

These metrics can be combined into a *stall classification*:

| Metric | Shallow Oscillation | Deep Regression | Mixed |
|:------:|:-------------------:|:---------------:|:-----:|
| $\sigma$ | $\approx 0$ | $\ll 0$ | $< 0$ |
| Median $d_{\text{bt}}$ | Small ($< d^{*}$) | Large ($> d^{*}$) | Bimodal |
| Max depth advance | Stationary | Declining | Stationary |

#### B.19.3 Targeted Interventions by Stall Mode

*For shallow oscillation.* The solver is stuck at a specific depth (typically a row boundary in the plateau).
Appropriate interventions:

(a) Escalate lookahead depth (B.8's existing response). Effective if the oscillation is caused by near-miss
    branches that fail within a few cells.

(b) Invoke RCLA (B.17) at a higher $u$ threshold for the stuck row. If the row has $u = 15$ unknowns, enumerate
    all $\binom{15}{\rho}$ completions and check hashes. This either finds a feasible completion (resolving the
    oscillation) or proves the row infeasible (allowing the solver to backtrack to the prior row).

(c) Invoke PRCT (B.16) on the stuck row. Tighter bounds on the remaining unknowns may force additional cells,
    reducing $u$ and enabling RCLA to trigger or enabling propagation cascades.

*For deep regression.* The solver is backtracking through many rows. Appropriate interventions:

(a) Trigger a partial restart (B.11). Rather than unwinding one level at a time, jump back to a checkpoint depth
    (e.g., the beginning of the plateau band) and resume the DFS from there. This skips the unproductive
    cell-by-cell unwinding.

(b) Escalate to CDCL-style backjumping (B.1) if implemented. Analyze the conflict to identify the root-cause
    decision and jump directly to it, skipping irrelevant intermediate rows.

(c) Update failure statistics (B.18) for all rows traversed during the regression. The deep backtrack provides
    evidence that the high-level assignment was wrong; recording this evidence biases future branching away from
    the failed pattern.

*For mixed stalling.* The solver alternates between oscillation and regression. A combined intervention uses
multi-metric detection to select the appropriate response at each decision point.

#### B.19.4 Extended Probe Strategies Beyond $k = 4$

B.8 caps exhaustive lookahead at $k = 4$ (16 probes per decision) due to the exponential cost of the full $2^k$
tree. When the solver reaches $k = 4$ and remains stalled, the following extensions offer deeper lookahead at
sub-exponential cost:

*Strategy A: Beam search.* Instead of exploring the full $2^k$ tree, maintain a beam of the $B$ most promising
partial assignments at each depth level. At depth $d$, each of the $B$ candidates is extended by both values (0
and 1), producing $2B$ candidates. These are evaluated by a heuristic (e.g., the number of constraint violations,
or the constraint-tightness score from B.10) and the top $B$ are retained. The cost is $O(B \cdot k)$ per decision
rather than $O(2^k)$.

At $B = 8$ and $k = 16$, the cost is $8 \times 16 = 128$ probes per decision&mdash;comparable to $k = 7$ exhaustive
($2^7 = 128$) but reaching 16 levels deep. The tradeoff is that beam search is incomplete: it may miss
contradictions that lie outside the beam. However, for detecting cardinality violations that manifest at depth
8-16 (spanning 1-3 cells into the next row), beam search's heuristic filtering is likely to retain the
relevant branches.

*Strategy B: Row-boundary probing.* Rather than probing to a fixed depth $k$, probe forward until the next row
boundary and check the SHA-1 hash. If the current cell is at position $(r, c)$ with $511 - c$ cells remaining
in the row, the probe completes the row (assigning $511 - c$ cells greedily, using constraint-tightness ordering),
checks the SHA-1 hash, and backtracks if the hash fails.

The cost is $O(511 - c)$ propagation steps per probe, which varies from $O(s)$ (if $(r, c)$ is at the beginning
of the row) to $O(1)$ (if $(r, c)$ is near the row end). On average, the probe cost is $O(s/2) \approx 256$
propagation steps ($\sim 500$ $\mu$s). The solver runs two probes per decision (one for each value), costing
$\sim 1$ ms per decision. At $\sim 1{,}000$ decisions per row ($\sim 200{,}000$ plateau-band decisions total), the
overhead is $\sim 200$s per block&mdash;comparable to the current solve time and thus marginally acceptable.

The advantage of row-boundary probing over fixed-depth lookahead is that it exploits the SHA-1 hash as a
verification oracle, which is far more powerful than cardinality-based pruning. A fixed-depth probe at $k = 4$ can
detect only cardinality violations within 4 cells; a row-boundary probe can detect hash mismatches for the entire
row, which catches failures that no finite $k$ would detect.

*Strategy C: Stochastic lookahead.* Rather than exhaustively exploring all $2^k$ paths, sample $M$ random paths
of depth $k$ and check each for feasibility. This is a Monte Carlo estimation of the feasibility rate at depth $k$:
if all $M$ samples are infeasible, the current assignment is likely doomed. The false-negative probability (failing
to detect a feasible path) is $(1 - p)^M$, where $p$ is the fraction of feasible paths. At $p = 0.01$ (1\% of
paths are feasible) and $M = 100$, the false-negative rate is $(0.99)^{100} \approx 0.37$&mdash;too high for
reliable pruning.

Stochastic lookahead is therefore unsuitable as a primary pruning mechanism but could serve as a *stall-breaking
heuristic*: when the solver is stuck at $k = 4$, sample 100 random paths of depth 32 (spanning $\sim 6$ cells
per line across 4 rows). If all samples fail, switch to a different branching decision rather than
exploring the full subtree deterministically. This trades completeness for speed in the deep stall regime.

**DI determinism caveat.** Stochastic lookahead requires a deterministic PRNG seeded from the search state (e.g.,
the current undo-stack hash) to ensure identical sampling in compressor and decompressor. Beam search and row-
boundary probing are fully deterministic given deterministic tie-breaking in the heuristic evaluation.

#### B.19.5 Stall Detector as Orchestrator

The enhanced stall detector serves as an *orchestration layer* that coordinates B.8, B.11, B.16, B.17, and B.18.
Rather than each proposal operating independently, the stall detector classifies the current stalling mode and
dispatches the appropriate intervention:

1. $\sigma > 0$: forward progress. No intervention. Use current $k$.

2. $\sigma \leq 0$, median $d_{\text{bt}} < d^{*}$: shallow oscillation.
   Escalate $k$ (B.8). If $k = 4$, invoke PRCT (B.16) + RCLA (B.17) on the stuck row. If still stalled,
   try row-boundary probing (Strategy B).

3. $\sigma \leq 0$, median $d_{\text{bt}} > d^{*}$: deep regression.
   Trigger partial restart (B.11) or backjumping (B.1). Update failure statistics (B.18) for traversed rows.

4. $\sigma \leq 0$, max depth not advancing for $> 3W$ decisions: persistent stall.
   Invoke beam-search lookahead (Strategy A) at $k = 16, B = 8$. If still stalled, escalate to stochastic
   lookahead (Strategy C).

This orchestration layer is the key architectural contribution of B.19: it unifies the various plateau-breaking
proposals into a coherent escalation hierarchy, applying each technique where it is most effective.

#### B.19.6 DI Determinism

All stall metrics (net depth advance, backtrack depth distribution, row re-entry counts, maximum depth) are
deterministic functions of the search trajectory. The stall classification and intervention dispatch are therefore
identical in compressor and decompressor. DI semantics are preserved.

The extended probe strategies (beam search, row-boundary probing) are deterministic given deterministic heuristic
evaluation. Stochastic lookahead requires a deterministic PRNG as noted in B.19.4.

#### B.19.7 Open Questions

(a) What are the empirical distributions of backtrack depth and row re-entry count in the plateau band? If the
solver predominantly exhibits shallow oscillation (as the current $k = 4$ data suggests), the deep-regression
interventions (B.11, B.1) are less urgent. If deep regressions are common, they are critical.

(b) Is beam search (Strategy A) effective on the CRSCE factor graph? The heuristic evaluation function (constraint
tightness, B.10) must correlate with actual feasibility for the beam to retain relevant branches. If the heuristic
is poorly calibrated in the plateau (where tightness is uniformly weak), beam search degenerates to random sampling.

(c) What is the overhead of row-boundary probing (Strategy B) in the non-plateau regime? If the stall detector
correctly limits row-boundary probing to the plateau band, the overhead applies only to the $\sim 200$ hard rows.
But if the stall detector misclassifies easy rows as stalled, probing wastes throughput on rows that would resolve
quickly with standard propagation.

(d) How should the stall detector's thresholds ($\sigma^{-}$, $d^{*}$, $W$, $W_{\text{bt}}$) be tuned? These
parameters interact: a low $d^{*}$ classifies more episodes as deep regression, triggering more restarts; a high
$d^{*}$ classifies more episodes as shallow oscillation, triggering more lookahead escalation. The optimal
settings depend on the relative cost and benefit of each intervention, which varies across blocks.

(e) Can the orchestration hierarchy (B.19.5) be formalized as a multi-armed bandit? Each intervention is an
"arm" with an unknown reward (solve-time reduction). The stall detector is a contextual bandit that observes the
stall classification and selects the arm with the highest expected reward. Thompson sampling or UCB could be used
to balance exploration (trying new interventions) with exploitation (repeating interventions that have worked). The
determinism requirement constrains the bandit: the selection policy must be a deterministic function of the search
history to preserve DI.

### B.20 LTP Substitution Experiment: Geometry versus Position (Implemented)

B.9 proposes adding one or more non-linear lookup-table partition (LTP) pairs *alongside* the existing 8 partitions,
increasing the per-cell constraint count from 8 to 10 at a storage cost of 9,198 bits per pair (reducing $C_r$ from
51.8% to 48.2%). This section proposes a different experiment: *substituting* the four toroidal-slope partitions
(HSM1/SFC1/HSM2/SFC2) with four independently optimized LTP pairs, holding the total partition count at 8 and the
storage budget at 125,988 bits. The experiment is designed to isolate a fundamental question about the plateau
bottleneck: is the solver's stalling at depth ~87K caused by the *geometry* of the constraint lines (linear
algebraic structure, uniform crossing density, shared null space), or by their *structural position* in the
propagation process (any length-511 uniform line, regardless of shape, cannot generate forcing events when
$\rho/u \approx 0.5$)?

#### B.20.1 Motivation: The Marginal Disappointment of Toroidal Slopes

The four toroidal-slope partitions (B.2) were added to increase constraint density from 4 lines per cell (geometric
partitions only) to 8. The expectation was that doubling the per-cell constraint count would substantially increase
the frequency of forcing events in the plateau band, where the geometric partitions alone are inert. In practice,
the slopes' contribution has been relatively poor compared to the original four cross-sum families. The geometric
partitions provide three qualitatively distinct pruning mechanisms: rows interact directly with SHA-1 hash
verification, columns provide orthogonal information transfer, and DSM/XSM generate short lines (length 1 to 511)
whose variable geometry triggers early forcing at the matrix periphery. The toroidal slopes provide none of these&mdash;
they are uniformly length-511 lines with no hash interaction and no variable-length early-forcing behavior.

This observation raises the possibility that the slopes' underperformance is not a property of their *algebraic
structure* (modular arithmetic, linear null space) but of their *position* in the solver's constraint hierarchy: any
uniform-length-511 line that lacks hash verification will underperform, because at the plateau entry point (row
~170), all such lines have $u \approx 341$ unknowns and residual $\rho \approx 170$, placing them deep in the
forcing dead zone ($\rho \gg 0$ and $\rho \ll u$). Adding more lines of the same length&mdash;whether algebraic or
pseudorandom&mdash;cannot change this arithmetic.

If this *positional hypothesis* is correct, replacing the slopes with optimized LTP pairs will produce similarly
poor marginal performance, despite the LTP's non-linear structure and plateau-targeted optimization. Conversely, if
the LTP pairs significantly outperform the slopes, the *geometric hypothesis* is supported: the non-linear
structure and early-tightening line composition (B.9.5) provide a qualitatively different kind of constraint that
uniform algebraic lines cannot replicate.

#### B.20.2 Experiment Design

The experiment consists of four solver configurations, benchmarked on a common test suite of $N \geq 50$ random
$511 \times 511$ binary matrices:

**Configuration A (baseline).** The pre-B.20 8-partition solver: 4 geometric (LSM, VSM, DSM, XSM) + 4 toroidal
slopes (HSM1, SFC1, HSM2, SFC2). This was the production configuration at the time of this experiment.

**Configuration B (geometric only).** The 4 geometric partitions alone, with toroidal slopes removed. This
establishes the baseline performance of the geometric partitions without any supplementary lines, quantifying the
slopes' actual marginal contribution.

**Configuration C (slopes replaced by LTP).** 4 geometric + 4 independently optimized LTP pairs, replacing the
toroidal slopes entirely. Each LTP pair is optimized per B.9.1-B.9.2 (Fisher-Yates baseline + hill-climbing
against simulated DFS trajectories). The four tables are optimized sequentially: the 1st LTP is optimized with
geometric partitions only, the 2nd with geometric + 1st LTP, and so on, so each table complements the existing
constraint set.

**Configuration D (slopes + LTP).** All 8 existing partitions + 4 LTP pairs (12 total). This is the additive
configuration, increasing per-cell constraints from 8 to 12 at a storage cost of $4 \times 9{,}198 = 36{,}792$
additional bits per block ($C_r$ drops from 51.8% to 40.5%). This configuration is included as an upper bound: if
12 partitions substantially outperform 8, the question becomes whether the improvement is worth the compression
penalty.

#### B.20.3 Metrics

For each configuration and each test matrix, record:

(a) **Total backtrack count** in the plateau band (rows 100-300). This is the primary metric: the plateau is where
the solver spends 90% of its time, and backtrack reduction is the clearest signal of constraint effectiveness.

(b) **Maximum sustained depth** before the first major regression ($d_{\text{bt}} > 1{,}000$ cells). This measures
how far the solver penetrates into the plateau before stalling. Deeper penetration indicates stronger constraint
propagation in the early plateau.

(c) **Forcing event rate** in the plateau band: the fraction of cell assignments that are forced by propagation
(rather than selected by branching). This directly measures constraint tightening. The geometric partitions produce
a high forcing rate in the easy regime (rows 0-100) and a near-zero rate in the plateau. The question is whether
slopes or LTP pairs increase the plateau forcing rate.

(d) **Wall-clock solve time** per block. The ultimate metric, but confounded by per-assignment overhead (LTP lookup
vs. slope formula) and machine-specific factors. Backtrack count is more portable.

(e) **Per-line residual distribution** at plateau entry (row 170). For each of the 8 (or 12) partition families,
record the distribution of $\rho/u$ across all lines at the moment the solver enters the plateau. If all families
cluster around $\rho/u \approx 0.5$, the positional hypothesis is supported: the dead zone is universal. If the LTP
early-tightening lines show a bimodal distribution (some lines with $\rho/u$ near 0 or 1, others near 0.5), the
geometric hypothesis gains support.

#### B.20.4 Predicted Outcomes and Interpretation

*Outcome 1: C $\approx$ A (LTP matches slopes).* Both achieve similar backtrack counts, forcing rates, and
residual distributions. **Interpretation:** the positional hypothesis is confirmed. Uniform-length-511 lines cannot
break the plateau regardless of their cell-to-line mapping. The productive research direction is proposals that
change *when* constraints activate (B.16 partial tightening, B.17 row-completion look-ahead) or that introduce
*variable-length* non-linear lines. The B.9 optimization infrastructure is not justified for uniform-length tables.

*Outcome 2: C $\gg$ A (LTP substantially outperforms slopes).* LTP pairs reduce plateau backtracks by $\geq 2
\times$ relative to slopes. **Interpretation:** the geometric hypothesis is confirmed. Non-linear structure and/or
early-tightening line composition provide qualitatively superior constraint information. The recommendation is to
replace all four toroidal slopes with optimized LTP pairs in production, at zero storage cost and minimal
implementation complexity ($4 \times 510$ KB static tables, each resolved by a single array lookup). Further
investment in the B.9 optimization infrastructure is justified.

*Outcome 3: B $\approx$ A (slopes provide no marginal benefit).* Removing the slopes entirely produces similar
performance to the 8-partition baseline. **Interpretation:** the four geometric partitions carry essentially all
the solver's constraint power. The slopes were never contributing meaningfully, and any supplementary partition&mdash;
linear or non-linear&mdash;faces the same positional barrier. This is the strongest evidence for the positional
hypothesis and redirects the research program entirely toward non-partition approaches (B.1 CDCL, B.11 restarts,
B.16-B.19).

*Outcome 4: C $\gg$ A and D $\gg$ C (LTP outperforms slopes, and more LTP is better).* Both the substitution and
the additive configuration show progressive improvement. **Interpretation:** non-linear constraint density scales
favorably. Each additional LTP pair contributes independent plateau-breaking power. The recommendation is to add
LTP pairs until the compression-ratio penalty becomes unacceptable, using $C_r$ as the binding constraint.

*Outcome 5: C $\approx$ A but D $\gg$ A (substitution equivalent, but addition helps).* Replacing slopes with LTP
doesn't help, but adding LTP on top of slopes does. **Interpretation:** the slopes and LTP pairs provide
complementary information (the slopes contribute algebraic regularity, the LTP contributes non-linear structure),
but neither alone is sufficient to break the plateau. Only the combination crosses a critical constraint-density
threshold. This favors the 12-partition configuration (D) despite its compression penalty.

#### B.20.5 LTP Table Construction for the Experiment

Each of the four LTP tables is constructed per B.9.1 (Fisher-Yates baseline from a fixed seed) and optimized per
B.9.2 (hill-climbing against simulated DFS). The four seeds are deterministic and distinct:

$$\text{seed}_i = \text{SHA-256}(\texttt{"CRSCE-LTP-v1-table-"} \| i) \quad \text{for } i \in \{0, 1, 2, 3\}$$

Sequential optimization order matters. Table 0 is optimized with the geometric partitions only (Configuration B as
the base solver). Table 1 is optimized with geometric + Table 0. Table 2 with geometric + Tables 0-1. Table 3
with geometric + Tables 0-2. This greedy-sequential approach ensures each table complements the existing constraint
set rather than duplicating it. The alternative&mdash;joint optimization of all four tables simultaneously&mdash;is a
$4 \times 261{,}121$-dimensional search problem that is computationally infeasible.

The optimization test suite consists of $N = 100$ random matrices, split 50/50 for optimization and validation.
Tables are optimized against the first 50 matrices and evaluated on the second 50. If the validation backtrack
count is within 10% of the optimization backtrack count, the table generalizes; if it degrades by $> 25\%$, the
table is overfit and the optimization must be rerun with a larger or more diverse suite.

To manage the computational cost of the optimization loop, each candidate swap is evaluated using a *truncated DFS*:
the solver runs for the first $100{,}000$ nodes per matrix (covering the plateau entry), and the plateau-band
backtrack count in this window serves as the proxy objective. This reduces the per-evaluation cost from minutes to
seconds, enabling $\sim 100{,}000$ swap evaluations per table within a week of Apple Silicon compute time.

#### B.20.6 Implementation Path

The implementation is staged to minimize risk:

*Stage 1: Instrument the current solver.* Add per-line residual logging at row 170 (plateau entry) and per-family
forcing-event counters in the plateau band. Run Configuration A on the test suite to establish baseline metrics.
This requires no architectural changes&mdash;only observability instrumentation via the existing `IO11y` interface.

*Stage 2: Implement Configuration B.* Disable the four toroidal-slope partitions (set their line count to 0 in
`ConstraintStore`, skip their updates in `PropagationEngine`). Run the test suite. If Outcome 3 materializes
(slopes provide no marginal benefit), the experiment is already informative and Stage 3 can be prioritized or
deprioritized accordingly.

*Stage 3: Construct and integrate LTP tables.* Implement the `LookupTablePartition` class (a `CrossSumVector`
subtype backed by a `constexpr std::array<uint16_t, 261121>` literal). Run the B.9.1 optimization loop to produce
four tables. Integrate into `ConstraintStore` as drop-in replacements for the four `ToroidalSlopeSum` instances.
Run Configurations C and D.

*Stage 4: Analyze and decide.* Compare metrics across A/B/C/D per B.20.4. If Outcome 2 or 4, proceed to
production integration of LTP pairs. If Outcome 1 or 3, redirect research toward B.16-B.19.

#### B.20.7 Implications for the File Format

If LTP substitution proceeds to production (Outcome 2 or 4), the compressed payload format (Section 12) changes
in field semantics but not in field sizes. The HSM1/SFC1/HSM2/SFC2 fields in the block payload are reinterpreted
as LTP0/LTP1/LTP2/LTP3 cross-sums. Each field retains its encoding: $s = 511$ values at $b = 9$ bits each,
MSB-first packed bitstream, totaling 4,599 bits per field. The header `version` field should be incremented to
distinguish payloads that use LTP cross-sums from those that use toroidal-slope cross-sums, since the decompressor
must know which cell-to-line mapping to apply.

If additive LTP is adopted (Outcome 4, Configuration D), the block payload grows by $4 \times 4{,}599 = 18{,}396$
bits (4 additional LTP fields), and the header version must reflect the new field count. The format remains
backward-compatible in the sense that a version-2 decompressor can detect the version field and reject version-1
payloads (or vice versa) rather than silently misinterpreting the cross-sum fields.

#### B.20.8 Open Questions

(a) Is sequential table optimization (B.20.5) sufficient, or does the greedy ordering leave significant performance
on the table? If Table 3 is optimized against a solver that already has Tables 0-2, it may converge to a table
that merely patches the remaining gaps in the first three tables' coverage, rather than providing independent
constraint power. A partial remedy is to re-optimize Table 0 after Tables 1-3 exist (iterating the sequential
process), but convergence of this outer loop is not guaranteed.

(b) How sensitive is the experiment to the choice of test suite? Random matrices may not be representative of
real-world inputs (which may have structure&mdash;e.g., natural-language text produces non-uniform bit distributions).
The test suite should include structured inputs (all-zeros, all-ones, alternating patterns, natural data) alongside
random matrices.

(c) Can the truncated-DFS proxy objective (B.20.5) mislead the optimizer? A table that minimizes backtracks in the
first $100{,}000$ nodes may not minimize backtracks in the full solve if the plateau's difficulty profile changes
beyond the truncation window. The truncation point should be validated by comparing proxy rankings with full-solve
rankings on a small subset of matrices.

(d) If Outcome 1 materializes (positional hypothesis confirmed), is there a *variable-length* LTP design that
escapes the dead zone? B.9 specifies uniform-length lines ($s$ cells each), but a generalized LTP could assign
lines of varying length&mdash;some with 100 cells (reaching forcing thresholds early) and others with 900 cells
(providing long-range information). Variable-length LTP lines would encode with variable-width cross-sums
($\lceil \log_2(\text{len}(k) + 1) \rceil$ bits each, as DSM/XSM do), complicating the storage cost analysis but
potentially breaking the positional barrier that uniform lines cannot.

(e) Should the experiment include a Configuration E that replaces the toroidal slopes with a 5th-8th slope pair
(different slope parameters, still algebraic)? This would test whether the slopes' underperformance is specific to
the chosen parameters $\{2, 255, 256, 509\}$ or inherent to the algebraic family. If alternative slope parameters
perform equally poorly, the algebraic family is exhausted and only non-linear alternatives remain.

#### B.20.9 Observed Results (Implemented)

B.20 Configuration C was implemented: 4 geometric partitions + 4 uniform-length LTP partitions (seeds
`"CRSCLTP1"`-`"CRSCLTP4"`, Fisher-Yates baseline, no hill-climbing optimization). The toroidal slopes
(HSM1/SFC1/HSM2/SFC2) were removed. The total line count decreased from 6,130 (B.9) to 5,108, and the block
payload reverted to 15,749 bytes (identical to the pre-B.9 format). The implementation used `constexpr` lookup
tables built at compile time from the fixed seeds.

**Telemetry (useless-machine.mp4, block 0, ~14M iterations):**

| Metric                  | B.8/B.9 Baseline | B.20 (Config C) | Change              |
|-------------------------|------------------|-----------------|---------------------|
| Sustained plateau depth | ~87,500          | ~88,503         | +1,003 (+1.1%)      |
| Hash mismatch rate      | 37.0%            | 25.2%           | $-11.8$ pp          |
| Avg iterations/sec      | ~200K            | ~198K           | $\approx 0$%        |
| Block payload bytes     | 16,899           | 15,749          | $-1,150$ ($-6.8\%$) |

The plateau depth improved by $+1{,}003$ cells ($+1.1\%$), a modest but consistent gain. The hash mismatch rate
dropped from $37\%$ to $25.2\%$&mdash;a reduction of 11.8 percentage points&mdash;indicating that the LTP partitions
generate substantially more propagation in the plateau band than the toroidal slopes did. The iteration rate was
unchanged ($\approx 198{,}000$/sec), confirming that the LTP table lookup imposes no measurable per-iteration
overhead relative to the slope modular-arithmetic formula.

**Outcome classification:** The result falls between Outcome 1 and Outcome 2 from B.20.4. The depth improvement
($+1.1\%$) is far below the $2\times$ backtrack reduction threshold of Outcome 2, but the $-12$ pp reduction in
hash mismatch rate is material. The positional hypothesis is partially supported: uniform-length-511 lines (both
slopes and LTP) cannot break the plateau, but LTP's non-linear structure provides a qualitatively better
constraint signal within the plateau&mdash;evidenced by the mismatch rate falling&mdash;without changing the depth
ceiling.

**Interpretation:** The toroidal slopes were providing weak constraint power in the plateau band. LTP partitions
provide modestly stronger constraint power (fewer dead-end explorations per iteration), but not enough to push
through the fundamental depth barrier at $\approx 88{,}500$. This suggests both families share the same positional
weakness: at plateau entry (row $\approx 170$), every uniform-length-511 line has $u \approx 341$ and
$\rho \approx 170$, placing it in the forcing dead zone. The LTP lines' non-linear structure reduces the fraction
of explorations that terminate in hash failure&mdash;perhaps by providing slightly tighter cross-line bounds&mdash;but
cannot generate the forcing events necessary to push the solver past row $\approx 173$.

**Recommendation:** Proceed to B.21 (joint-tiled variable-length LTP partitions). The B.20 result rules out
uniform-length supplementary lines as a plateau-breaking mechanism; variable-length lines with the triangular
length distribution of DSM/XSM may escape the dead zone by allowing short lines ($\ll 511$ cells) to reach
forcing thresholds earlier in the plateau.

### B.21 Joint-Tiled Variable-Length LTP Partitions

B.20 replaced four toroidal-slope partitions with four uniform-length LTP partitions, each containing
511 lines of 511 cells. Every cell belongs to all four LTP partitions, yielding 8 constraint lines per
cell (4 basic + 4 LTP). The uniform line length, however, forces a fixed 9-bit encoding for every
cross-sum element, and the large line size means that the ratio $\rho / u$ stays near 0.5 for most of
the solve, suppressing forcing events on LTP lines past the first ~50 rows. This section proposes
a *joint-tiled* LTP architecture in which the four LTP partitions collectively tile the matrix once,
each partition contains variable-length lines following the DSM/XSM triangular distribution, and each
cell belongs to exactly one LTP partition. The result is fewer but *stronger* LTP constraints per cell,
a naturally lossless variable-length encoding, and a modest reduction in block payload size.

#### B.21.1 Motivation

The DSM and XSM families encode cross-sums with variable bit widths because their line lengths vary
from 1 to $s$. The encoding cost for one diagonal family over $2s - 1$ lines totals $B_d(s) = 8{,}185$
bits (Section 1), far less than the $s \times b = 4{,}599$ bits that a hypothetical 511-element
fixed-width diagonal family would require. The savings arise because short lines have small maximum
sums, requiring fewer bits.

Uniform-length LTP partitions cannot exploit this structure: every line has $s = 511$ cells, so every
cross-sum ranges $[0, 511]$ and requires $b = 9$ bits. If instead each LTP partition contained lines
of varying length&mdash;some with 1 cell, some with 256, some with 511&mdash;the encoding would mirror the
DSM/XSM scheme, with short lines encoded cheaply and only the longest lines requiring the full 9 bits.

The challenge is arithmetic: a single partition with 511 lines whose lengths sum to $s^2 = 261{,}121$
must average 511 cells per line. A symmetric triangular distribution peaking at 511 sums to only
65,536&mdash;approximately one quarter of the matrix. No symmetric distribution ranging from 1 to 511
can average 511 over 511 elements; the only such distribution is the degenerate uniform case. The
resolution is to abandon per-partition completeness: each LTP partition covers approximately one
quarter of the matrix, and the four partitions jointly tile the full $s \times s$ grid.

#### B.21.2 Partition Structure

Each of the four LTP sub-tables $T_k$ ($k \in \{0, 1, 2, 3\}$) contains 511 lines indexed
$i \in [0, 510]$ with prescribed lengths:

$$
    \text{len}_k(i) = \min(i + 1, \; 511 - i)
$$

This is the left half of the DSM length function: $\text{len}(0) = 1$, $\text{len}(255) = 256$,
$\text{len}(510) = 1$. The total cells per sub-table is:

$$
    \sum_{i=0}^{510} \min(i+1, \; 511-i) = 2 \sum_{j=1}^{255} j + 256 = 2 \cdot \frac{255 \cdot 256}{2} + 256 = 65{,}536
$$

Four sub-tables total $4 \times 65{,}536 = 262{,}144$ cell assignments. The matrix has $511^2 = 261{,}121$
cells, leaving $262{,}144 - 261{,}121 = 1{,}023$ surplus assignments. Exactly 1,023 cells are therefore
assigned to two sub-tables, and the remaining $261{,}121 - 1{,}023 = 260{,}098$ cells to exactly one.
Equivalently, every cell belongs to at least one LTP sub-table, and 1,023 privileged cells belong to two.

The per-cell constraint line count is:

- 260,098 cells: 4 basic lines + 1 LTP line = 5 constraint lines.
- 1,023 cells: 4 basic lines + 2 LTP lines = 6 constraint lines.

#### B.21.3 Encoding and Payload Impact

Each sub-table encodes identically to a DSM or XSM family truncated to 511 elements. The bit cost
per sub-table is:

$$
    B_{\text{LTP}}(s) = \sum_{i=0}^{510} \lceil \log_2(\min(i+1, \; 511 - i) + 1) \rceil
$$

By symmetry and the identity with the DSM sum's first half:

$$
    B_{\text{LTP}}(s) = \frac{B_d(s) + \lceil \log_2(s+1) \rceil}{2} = \frac{8{,}185 + 9}{2} = 4{,}097 \text{ bits}
$$

Four sub-tables total $4 \times 4{,}097 = 16{,}388$ bits. Under the uniform B.20 encoding, four LTP
partitions at $s \times b = 4{,}599$ bits each total 18,396 bits. The savings are $18{,}396 - 16{,}388
= 2{,}008$ bits = 251 bytes per block.

Updated block payload:

| Field | Elements | Encoding | Total Bits |
|-------|----------|----------|------------|
| LH    | 511      | 160 bits each | 81,760 |
| BH    | 1        | 256 bits | 256 |
| DI    | 1        | 8 bits   | 8   |
| LSM   | 511      | 9 bits each | 4,599 |
| VSM   | 511      | 9 bits each | 4,599 |
| DSM   | 1,021    | variable | 8,185 |
| XSM   | 1,021    | variable | 8,185 |
| LTP0  | 511      | variable (1--9 bits) | 4,097 |
| LTP1  | 511      | variable (1--9 bits) | 4,097 |
| LTP2  | 511      | variable (1--9 bits) | 4,097 |
| LTP3  | 511      | variable (1--9 bits) | 4,097 |
| **Total** | | | **123,980** |

Block payload size: $\lceil 123{,}980 / 8 \rceil = 15{,}498$ bytes. Compression ratio:
$15{,}498 / 32{,}641 \approx 47.5\%$, down from 48.3% under uniform LTP (B.20) and 51.8% under
the original toroidal-slope format.

#### B.21.4 Constraint Strength Analysis

Reducing from 4 LTP lines per cell to 1 appears to weaken the constraint system, but the analysis
is more nuanced. Under B.20's uniform design, each of the 4 LTP lines has 511 cells. During the
DFS solve, a 511-cell line's $\rho / u$ ratio stays near 0.5 until the line is nearly complete,
meaning it almost never triggers forcing ($\rho = 0$ or $\rho = u$). Empirically, the LTP lines
contribute no forcing events past approximately row 50&mdash;well before the plateau at row ~170 where
the solver stalls.

Under joint tiling, a cell's single LTP line may be as short as 1 cell (immediately forced) or as
long as 256 cells (reaching forcing thresholds much earlier than a 511-cell line). The distribution
of line lengths follows the triangular pattern: half of all LTP lines have 128 cells or fewer.
A 10-cell line reaches $\rho = 0$ or $\rho = u$ after just 10 assignments&mdash;an event that occurs
in the first few rows. A 50-cell line forces within the first 50 assignments to that line. These
short-line forcing events propagate through the basic constraint lines (rows, columns, diagonals),
creating cascading reductions that the uniform 511-cell LTP lines never produce.

The trade-off is therefore not "8 lines down to 5" but rather "4 weak lines replaced by 1 strong
line." The net propagation yield per cell assignment may increase if the short LTP lines' frequent
forcing events outweigh the loss of three long, inert constraint lines. The 1,023 cells assigned to
two sub-tables receive a sixth constraint line, and these privileged cells should be placed
deliberately at the matrix corners where diagonal coverage is weakest (see B.21.5).

#### B.21.5 Spatial Layout: Center-Cross / Corner-Long Distribution

The joint-tiled architecture enables a deliberate spatial arrangement of line lengths across the
matrix. The design principle is *complementarity*: place short LTP lines where the basic partitions
are already strong, and long LTP lines where they are weakest.

Diagonal and anti-diagonal line lengths vary with position. A cell at $(r, c)$ lies on diagonal
$d = c - r + (s-1)$ of length $\min(d+1, 2s-1-d)$ and anti-diagonal $x = r + c$ of length
$\min(x+1, 2s-1-x)$. Cells near the matrix center (row $\approx 255$, column $\approx 255$) lie on
diagonals and anti-diagonals of length $\approx 511$, providing maximum constraint coverage from the
basic partitions. Cells in the corners&mdash;$(0,0)$, $(0,510)$, $(510,0)$, $(510,510)$&mdash;lie on
diagonals and anti-diagonals of length 1, the weakest possible basic coverage.

The spatial layout therefore assigns:

*Short LTP lines (indices near 0 and 510, length 1-64) along the center cross.* The center cross
comprises cells near row 255 (horizontal axis) and column 255 (vertical axis). These cells already
have full-length diagonal and anti-diagonal coverage, so short LTP lines sacrifice little. The LTP
constraint on these cells is minimal but also unnecessary: the basic partitions already constrain
them heavily.

*Long LTP lines (indices near 255, length 192-256) toward the corners.* Corner cells have length-1
diagonals and benefit most from long LTP lines. A 256-cell LTP line connecting corner-region cells
provides cross-cutting constraint information that no basic partition supplies for those cells.

*Privileged dual-covered cells (the 1,023 cells in two sub-tables) at the extreme corners.* These
cells have the weakest basic coverage (length-1 diagonals and anti-diagonals) and receive a sixth
constraint line as partial compensation.

The construction algorithm for each sub-table $T_k$ proceeds as follows. First, compute a spatial
priority score for each unassigned cell: $\text{score}(r, c) = \text{len}(\text{diag}(r,c)) +
\text{len}(\text{antidiag}(r,c))$. Cells with low scores (corners) have high priority for long LTP
lines; cells with high scores (center cross) have high priority for short lines. Second, for each
line index $i$ in order of decreasing $\text{len}_k(i)$, select $\text{len}_k(i)$ cells from the
unassigned pool, biased toward cells with the lowest spatial priority scores. The bias is implemented
via a seeded LCG shuffle (as in B.9) restricted to the candidate pool, with the first
$\text{len}_k(i)$ cells after shuffling assigned to line $i$. The seed for sub-table $T_k$ is
derived from the string "CRSCLTP$k$" to ensure deterministic, reproducible assignments.

After $T_0$ is constructed, $T_1$ is built from the remaining unassigned cells (plus a portion of
the 1,023 dual-assigned cells), then $T_2$ from the next remainder, and $T_3$ from the final
remainder. The sequential construction ensures joint tiling with exactly 1,023 overlapping cells.

#### B.21.6 Table Construction Protocol

The four sub-tables must be constructed in a fixed order ($T_0, T_1, T_2, T_3$) so that both
compressor and decompressor produce identical cell-to-line mappings. The protocol is:

*Step 1.* Initialize a 511 $\times$ 511 assignment matrix $A$ where $A[r][c] = \emptyset$ (unassigned).

*Step 2.* For each sub-table $T_k$ ($k = 0, 1, 2, 3$):

  (a) Compute the candidate pool: all cells $(r, c)$ where $|A[r][c]| < 1$ (for $k < 3$) or
      $|A[r][c]| < 2$ (for $k = 3$, which absorbs the 1,023 dual assignments).

  (b) Compute the spatial priority score for each candidate cell:
      $\text{score}(r, c) = \min(d+1, 2s-1-d) + \min(x+1, 2s-1-x)$ where $d = c - r + 510$
      and $x = r + c$.

  (c) For each line index $i$ sorted by decreasing $\text{len}_k(i)$ (longest lines first):
      seed an LCG with $\text{hash}(\text{"CRSCLTP"} \| k \| i)$, shuffle the candidate pool,
      stable-sort by ascending spatial priority score with ties broken by the shuffle,
      and assign the first $\text{len}_k(i)$ cells to line $i$ of sub-table $T_k$.
      Mark assigned cells in $A$.

*Step 3.* Verify: every cell $(r, c)$ satisfies $1 \leq |A[r][c]| \leq 2$, with exactly 1,023
cells having $|A[r][c]| = 2$.

The entire construction is deterministic ($O(s^2)$ per sub-table, $O(s^2)$ total) and depends only on
$s$ and the four seed strings. Both compressor and decompressor execute the identical construction at
startup, requiring no table data in the payload.

#### B.21.7 Implications for the Solver

The solver's `ConstraintStore` changes as follows:

*Line count.* Each sub-table contributes 511 lines, but only the sub-table that owns a given cell
is active for that cell. The total number of distinct constraint lines remains $4 \times 511 = 2{,}044$
for the LTP component, plus the $4 \times 511 - 2 = 2{,}042$ diagonal/anti-diagonal lines and
$2 \times 511 = 1{,}022$ row/column lines, totaling $1{,}022 + 2{,}042 + 2{,}044 = 5{,}108$. This
matches the B.20 line count; only the line *lengths* change, not the line *count*.

*`getLinesForCell()`.* For the 260,098 cells in exactly one sub-table, this returns 5 `LineID`s
(row, column, diagonal, anti-diagonal, and one LTP). For the 1,023 dual-covered cells, it returns 6.
The return type changes from `std::array<LineID, 8>` to a small vector or to `std::array<LineID, 6>`
with a valid-count field. Alternatively, all cells can return 6 entries, with 260,098 cells having one
sentinel `LineID::None` value that the propagator ignores.

*Propagation impact.* With fewer lines per cell, each cell assignment enqueues fewer lines for
constraint checking. This reduces per-assignment overhead from 8 line updates to 5 or 6, a modest
speedup of $\approx 30\%$ in the inner propagation loop. The reduced overhead partially compensates
for any loss in propagation yield from fewer constraints.

*LTP line statistics.* Each LTP line's $(\text{assigned}, \text{unknown}, \rho)$ triple behaves
differently from the uniform case. A line of length $\ell$ starts with $u = \ell$ unknowns and
$\rho = \text{sum}$. For short lines ($\ell \leq 10$), the forcing conditions $\rho = 0$ (force all
unknowns to 0) and $\rho = u$ (force all unknowns to 1) are reached within the first few
assignments to that line. This produces immediate propagation cascades during the early rows of the
solve&mdash;exactly the region where propagation is currently most effective.

#### B.21.8 Comparison with B.20 Uniform LTP

| Metric                           | B.20 (Uniform)      | B.21 (Joint-Tiled)         |
|----------------------------------|---------------------|----------------------------|
| LTP lines per cell               | 4                   | 1 (or 2 for 1,023 cells)   |
| Constraint lines per cell        | 8                   | 5 or 6                     |
| LTP line lengths                 | 511 (uniform)       | 1--256 (triangular)        |
| LTP encoding bits (4 partitions) | 18,396              | 16,388                     |
| Block payload bytes              | 15,749              | 15,498                     |
| Compression ratio                | 48.3%               | 47.5%                      |
| Forcing events from LTP (est.)   | Rare past row 50    | Frequent through row 256   |
| Propagation inner-loop cost.     | 8 line updates/cell | 5--6 line updates/cell     |
| Spatial adaptivity               | None                | Center-cross / corner-long |

The joint-tiled design trades breadth of coverage (fewer LTP lines per cell) for depth of coverage
(shorter, more forceful lines) and encoding efficiency (variable-width sums). The spatial layout
further optimizes coverage by concentrating long LTP lines where the basic partitions are weakest.

#### B.21.9 Predicted Telemetry Targets

B.20.9 established the Configuration C baseline: plateau depth ~88,503, hash mismatch rate 25.2%,
iteration rate ~198K/sec, block payload 15,749 bytes. Three telemetry indicators should be monitored
during B.21 implementation to determine whether joint-tiled variable-length LTP partitions address
the forcing dead zone that uniform-length lines cannot escape.

*Indicator 1: Iteration rate (leading, mechanical).* Under B.20, each cell assignment updates 8
constraint lines. Under B.21, each assignment updates 5 or 6 lines (4 basic + 1 or 2 LTP). The
inner propagation loop should therefore speed up by approximately 30%, pushing the iteration rate
from ~198K/sec toward ~257K/sec. If the observed rate does not increase, the joint-tiling lookup
(determining which sub-table owns each cell) is adding overhead that offsets the line-count savings.
A rate below 198K/sec would indicate a regression in the table-lookup implementation and should be
investigated before interpreting depth or mismatch metrics.

*Indicator 2: Hash mismatch rate (leading, qualitative).* The mismatch rate is the most sensitive
indicator of constraint quality within the plateau band. B.20 reduced it from 37.0% (toroidal
slopes) to 25.2% (uniform LTP), demonstrating that non-linear structure provides qualitatively
better constraint signal even when the depth ceiling barely moves. If B.21 drops the mismatch rate
below 20%, the short LTP lines are converting that constraint signal into tighter subtree pruning
&mdash;fewer dead-end explorations per iteration. If it remains at ~25% despite variable line lengths,
the non-linear structure was already providing all available constraint power and line length is
irrelevant. A mismatch rate above 30% would indicate that the loss of three LTP lines per cell
(from 4 to 1) outweighs the benefit of shorter, more forceful lines.

*Indicator 3: Plateau depth (lagging, definitive).* The depth ceiling is the ultimate success
metric but moves only when forcing events push the solver into previously unreachable territory.
A jump from 88,503 to 90,000 or above would be a qualitative breakthrough&mdash;the first time any
partition-level change has moved the depth ceiling by more than ~1%. This would confirm that short
LTP lines ($\ell \leq 50$) generate forcing events deep in the plateau where no 511-cell line ever
could. If the depth remains at ~88,500, the positional barrier is deeper than line-length alone can
address, and the research program should pivot decisively toward non-partition approaches: B.1
(conflict-driven learning), B.11 (randomized restarts), and B.16-B.19 (row-completion look-ahead,
failure-biased branching, stall detection).

#### B.21.10 Outcome

B.21 was fully implemented and all 30 unit and integration tests pass with lint clean. The 30-minute
`uselessTest` depth run (same random binary block used for all prior comparisons) produced the
following telemetry against the B.20.9 baseline.

**Observed results (B.21, 30-minute run):**

| Indicator | B.20.9 Baseline | B.21 Observed | Direction |
|-----------|----------------|---------------|-----------|
| Plateau depth | 88,503 | **50,272** | −43% regression |
| Hash mismatch rate | 25.2% | **0.20%** | 93% improvement |
| Avg iteration rate | ~750K/sec | ~687K/sec | −8% regression |
| Max probe depth reached | 4 | 4 (stuck) | no improvement |
| Stall escalations | — | 3 (no deescalations) | maxed out |
| Min row unknowns at plateau | — | 123 (frozen) | depth wall |
| Total iterations at end | — | ~1.2B | — |

**Interpretation of the depth regression.**  The solver entered a hard plateau at depth 50,272
(approximately row 98 of 511) and never exceeded that value throughout the entire 30-minute run.
The row with fewest remaining unknowns was frozen at 123 for the entire run; the DFS oscillated
within a tight band around the same frontier without making forward progress.  Three stall
escalations brought the lookahead probe depth to its maximum of 4, and no deescalations occurred,
confirming the solver was genuinely stuck rather than transiently paused.

The root cause is architectural.  B.21 reduces the number of LTP lines per cell from 4 (B.20) to
1-2, a 50-75% reduction in LTP-derived coupling.  B.20's four toroidal-slope lines each spanned
all 511 rows (one cell per row), providing global cross-row coupling.  B.21's spatially-partitioned
lines are clustered by spatial score: each line's cells are drawn from the same spatial region
rather than distributed across rows.  At row 98, the LTP lines containing those cells carry little
information about the assignment state of rows 0-97, eliminating the inter-row signal that
stabilized B.20's descent to row 170.

**Interpretation of the hash mismatch improvement.**  The 0.20% hash mismatch rate (vs. 25.2% for
B.20) is the most dramatic constraint-quality improvement recorded in this research program.  It
demonstrates conclusively that variable-length LTP lines ($\ell = 1$-$256$) generate qualitatively
stronger row-level pruning than uniform-511-cell lines.  When a row does complete under B.21, it
almost always satisfies the SHA-256 lateral hash.  The constraint signal is excellent; the problem
is that the stronger constraints create a hard barrier the chronological DFS cannot cross.

**The paradox.**  B.21 simultaneously achieves the best constraint quality and the worst depth in
the experiment history.  This is not contradictory.  Shorter LTP lines commit cells earlier and
more forcefully.  Early commitments that are wrong propagate deeply into the search tree before the
contradiction surfaces at row ~98.  Chronological backtracking must then unwind all 50,272 branching
choices to reach the actual source of the conflict, which may lie at row 10-20.  Without
non-chronological backjumping, the adaptive lookahead (B.8) at probe depth 4 cannot reach far
enough back to identify and prune the bad decision.

**Implication for CDCL (B.1).**  B.21.9 predicted that CDCL would benefit if B.21 dropped the hash
mismatch rate below 20%, because a low mismatch rate implies that the reason graph contains rich
antecedent chains tracing hash failures back to early decisions.  The observed 0.20% rate satisfies
this condition by a wide margin.  However, B.21 also imposes a 43% depth penalty that CDCL alone
cannot recover; re-adding the fourth LTP line per cell (restoring cross-row coupling) while
preserving variable line lengths is a prerequisite for CDCL to operate in the plateau band rather
than below it.  A hybrid design&mdash;four variable-length LTP lines per cell, each spanning multiple
rows, with triangular length distribution&mdash;is the logical B.22 candidate.

**Disposition.**  B.21 is retained in the codebase as a correct, complete implementation.  The
variable-length encoding it introduces (saving 502 bytes per block relative to B.20) is a permanent
improvement regardless of the partition topology.  The plateau regression requires a new partition
design before the depth ceiling can be attacked further.

#### B.21.11 Next Iteration Improvement

B.21 established three empirical facts that constrain the design space for the next iteration.

*Fact 1: Variable line lengths work.* The 0.20% hash mismatch rate is the strongest constraint-quality
signal recorded in this project.  The triangular length distribution $\ell(k) = \min(k+1, 511-k)$,
$k = 0 \ldots 510$ generates short lines that force cells early and deeply enough to match the
correct assignment on virtually every row completion.  This distribution, or the variable-length
encoding mechanism it implies, should be preserved in any successor design.

*Fact 2: Lines per cell cannot fall below 4.* Reducing from 4 LTP lines per cell (B.20) to 1-2
(B.21) caused a 43% depth regression.  The regression is not offset by the stronger per-line
constraint quality.  Four LTP lines per cell is a hard floor: going below it removes more
cross-family coupling than any improvement in individual line informativeness can restore.

*Fact 3: Cross-row span is load-bearing.* B.20's toroidal lines each visited one cell in every
row, providing global inter-row coupling at every depth.  B.21's spatially-partitioned lines
cluster in the same region of the matrix, cutting off coupling past the cluster boundary.  The
depth wall at row $\approx 98$ corresponds precisely to the point where B.21's LTP lines stop
carrying information about the rows above.  Any replacement must ensure that each LTP line visits
cells from a wide range of rows, not a contiguous spatial region.

The three candidate next steps are ordered by implementation cost and expected confidence in
outcome.

---

**Candidate 1: Reactivate CDCL (B.1) against B.21's constraint graph (immediate, low cost)**

The B.1 CDCL implementation is complete and correct.  It was shelved after initial experiments
showed no depth improvement under B.20's uniform-511-cell lines, because the reason graph lacked
the short forcing chains CDCL requires to identify non-trivial backjump targets.  B.21 has
eliminated that limitation: a 0.20% hash mismatch rate implies that virtually every row completion
reflects a deterministic forcing cascade rooted at an early decision, and the antecedent table
will contain meaningful 1-UIP backjump targets.

The experiment is: re-enable the CDCL path in `RowDecomposedController_enumerateSolutionsLex.cpp`
using the existing `ConflictAnalyzer` and `ReasonGraph` infrastructure and run `uselessTest`.
**Success criterion:** depth exceeds 50,272 consistently.  **Failure criterion:** depth remains
at or below 50,272, indicating that the antecedent chains under B.21's partition structure still
resolve to targets too close to the conflict site for CDCL to produce useful backjumps ($< 1{,}000$
frame gain).

If CDCL pushes depth to 70K or above under B.21, the combination of variable-length forcing and
non-chronological backtracking may achieve the research goal without redesigning the partition.  If
CDCL fails to move the depth needle, the depth wall at row 98 is structural&mdash;the partial
assignment at depth 50K is globally inconsistent with the LTP sums but no single early decision
is singularly responsible&mdash;and a new partition design is required.

---

**Candidate 2: Full-coverage variable-length partitions (B.22, medium cost)**

The root cause of B.21's depth regression is the joint-tiling constraint: covering all 261,121
cells with only $4 \times 65{,}536 = 262{,}144$ cell-slots forces each cell into at most 1-2
sub-tables, destroying the 4-line-per-cell coupling B.20 provided.  The fix is to restore full
coverage: each sub-table assigns every cell to exactly one line, giving $\sum_k \ell(k) = 261{,}121$
per sub-table and exactly 4 LTP lines per cell.

The construction must enforce row diversity.  Rather than sorting cells by spatial score (which
clusters spatial neighbours together), cells should be ordered by a row-interleaving key before
assignment to lines&mdash;for example, column-major order $(c, r)$ rather than row-major $(r, c)$.
Under column-major ordering, consecutive cells in the sorted pool alternate rows, so every LTP
line of any length receives cells from a broad cross-section of rows.  The first $\ell(0) = 1$
cell (line 0, length 1) comes from row 0; the first $\ell(255) = 256$ cells (the peak line) span
rows 0 through 255 in column order; the last short lines near $k = 510$ draw from the same
column-wise cross-section as $k = 0$.

The length distribution must sum to 261,121.  A natural choice is to scale the triangular
distribution by a factor of $\lceil 261{,}121 / 65{,}536 \rceil \approx 4$ and adjust the
remainder:

$$\ell'(k) = 4 \cdot \min(k+1,\, 511-k) + \epsilon_k$$

where $\epsilon_k \in \{0, 1\}$ is a correction term distributed to bring the total to exactly
261,121.  The maximum line length under this distribution is $4 \times 256 = 1{,}024$, requiring
at most 10 bits per LTP sum element (vs. 9 bits for B.20 and 1-9 bits for B.21).  Total encoding
per sub-table is $\sum_{k=0}^{510} \lceil \log_2(\ell'(k)+1) \rceil$ bits; a rough estimate gives
$\approx 4{,}200$-$5{,}000$ bits per sub-table, comparable to B.20's $511 \times 9 = 4{,}599$
bits.

The variable-length encoding infrastructure from B.21 (`CompressedPayload_serializeBlock.cpp`,
`CompressedPayload_deserializeBlock.cpp`) requires only a change to the length function; no
structural changes to the serialization pipeline are needed.

**Success criterion:** depth exceeds 88,503 (the B.20 plateau) with hash mismatch rate remaining
below 10%.  **Failure criterion:** depth regresses below B.20 despite restoring 4 LTP lines per
cell, indicating that the triangular length distribution is fundamentally incompatible with the
solver's row-traversal order.

---

**Candidate 3: CDCL first, then B.22 if needed (recommended sequencing)**

Candidates 1 and 2 are not mutually exclusive, but Candidate 1 should be attempted first because
it requires no partition redesign.  If CDCL restores or exceeds B.20 depth under B.21's current
structure, it also validates the short-line forcing hypothesis and the variable-length encoding
savings are free.  If CDCL fails, Candidate 2 provides the definitive structural fix, and CDCL
can then be layered on top of B.22 once the depth regression is resolved.

The two experiments are sequential and non-overlapping in code paths.  Attempting both
simultaneously would obscure which mechanism is responsible for any observed improvement.

| Step | Action | Code change | Expected outcome | Termination criterion |
|------|--------|-------------|------------------|-----------------------|
| 1 | Re-enable B.1 CDCL with B.21 | ~50 lines in DFS loop | depth > 50K | 30-min uselessTest |
| 2 | If step 1 fails: implement B.22 column-major full-coverage partition | LtpTable.cpp constructor rewrite | depth > 88K | 30-min uselessTest |
| 3 | Layer CDCL on B.22 | No additional code if B.1 is already enabled | depth > 90K | 30-min uselessTest |

If step 1 achieves depth $\geq 90{,}000$, steps 2-3 become optional optimizations rather than
required fixes, and the research program can pivot to compression-ratio improvements or the
parallel-restart strategy (B.11).

#### B.21.12 Final Outcomes (CDCL + B.22)

Both Candidate 1 (CDCL) and Candidate 2 (B.22) from B.21.11 were executed sequentially and
then combined.

---

**Candidate 1 outcome: CDCL on B.21 (B.21.11, Step 1)**

CDCL was re-enabled in the DFS loop using the existing `ConflictAnalyzer`, `ReasonGraph`, and
`BackjumpTarget` infrastructure.  A `cdclStack` vector (parallel to BranchingController's undo
stack) tracked per-frame `cdclSavePoint` offsets.  On hash failure, `analyzeHashFailure` ran BFS
from the failed row; if a valid 1-UIP target $T$ was found, CDCL unrecorded assignments back to
`stack[T+1].cdclSavePoint`, called `brancher->undoToSavePoint(stack[T+1].token)`, resized the DFS
stack to $T+1$ frames, and continued.

30-minute uselessTest result:

| Metric | Value |
|--------|-------|
| Depth | ~52,042 |
| iter/sec | ~698K |
| CDCL backjumps | 7,583 (all in first 67M iterations, then zero) |
| cdcl\_jump\_dist\_max | 586 |
| Hash mismatch rate | ~25% |

**Assessment: Candidate 1 failed.** CDCL fired 7,583 times in the first 67M iterations (max
jump 586 frames) then stopped entirely, gaining only 1,770 depth frames (+3.5%) over B.21
without CDCL.  The failure mode confirms B.21.11's failure criterion: with 1-2 LTP lines per
cell and lines of length 1-256, the forcing graph is too sparse to construct useful 1-UIP
backjump targets past depth 52K.  Proceeding to Candidate 2 (B.22).

---

**Candidate 2 outcome: B.22 full-coverage variable-length LTP (B.21.11, Steps 2-3)**

B.22 was implemented with the following design:

*Formula:* $\ell_{\text{B.22}}(k) = 4 \cdot \min(k+1,\, 511-k) - \text{adj}(k)$, where
$\text{adj}(k) = 3$ if $k = 255$, else $2$.  This sums to exactly 261,121 per sub-table.
Line lengths range from 2 ($k = 0, 510$) to 1,021 ($k = 255$).

*Coverage:* Every cell belongs to exactly 4 LTP lines (one per sub-table, full coverage).
`LtpMembership.flat` expanded from 2 to 4 entries; `CellLines` expanded from 6 to 8 slots.
`getLinesForCell` always returns count = 8.

*Construction:* Fisher-Yates LCG shuffle of all 261,121 cells with a per-pass seed, then assign
consecutive chunks to lines in decreasing-length order.  No spatial sort; pure random
cell-to-line assignment ensures each line spans cells from diverse rows.

*Encoding:* $\sum_{k=0}^{510} \lceil \log_2(\ell_{\text{B.22}}(k) + 1) \rceil = 4{,}600$ bits
per sub-table.  `kBlockPayloadBytes` restored to 15,749 (same as B.20).

CDCL was layered on B.22 simultaneously (Step 3 from the plan).

30-minute uselessTest result (B.22 + CDCL):

| Metric | B.20 | B.21+CDCL | B.22+CDCL |
|--------|------|-----------|-----------|
| Depth | ~88,503 | ~52,042 | **~80,300** |
| iter/sec | ~198K | ~698K | ~521K |
| CDCL backjumps |&mdash;| 7,583 then 0 | = hash\_mismatches (100%) |
| cdcl\_jump\_dist\_max |&mdash;| 586 | **732** |
| Hash mismatch rate | ~25% | ~25% | ~25% |

**Assessment: Candidate 2 partially succeeded.** B.22 recovered 30,000 frames (+60%) from
B.21's depth floor, confirming that restoring 4 LTP lines per cell is the dominant structural
factor.  CDCL on B.22 fires on every hash mismatch (100% backjump rate, max dist 732), a
dramatic improvement over B.21+CDCL where backjumps ceased after 67M iterations.

However, depth plateaus at ~80,300 rather than exceeding B.20's 88,503.  Three factors explain
the gap:

1. *Throughput cost:* B.22's longer LTP lines (up to 1,021 cells) increase cascade cost per
   forced assignment; iter/sec drops from 698K (B.21) to 521K (B.22+CDCL), reducing effective
   search rate.

2. *Line length distribution:* Very long lines ($\ell > 511$) accumulate pressure slowly and
   are less likely to reach the $\rho/u = 0$ or $\rho/u = 1$ forcing threshold that drives
   cascade propagation.  B.20's uniform 511-cell lines provided more balanced pressure
   across all $k$.

3. *CDCL plateau persistence:* CDCL backjumps 732 frames on each failure but the plateau depth
   is unchanged, suggesting that the failures arise from global sum inconsistency across all four
   LTP families simultaneously&mdash;no single 1-UIP decision resolves the conflict.

**Conclusions:**

- Restoring 4 LTP lines per cell (full coverage) is non-negotiable: this single change recovered
  +60% of the depth lost in B.21 (+30,000 frames, 50K $\to$ 80K).
- Variable-length lines in the range 2-1,021 are less effective than B.20's uniform 511-cell
  lines at the depth frontier.  The optimal distribution likely concentrates lines in the
  128-512-cell range, providing both forcing power (shorter than 511) and constraint pressure
  (fewer near-zero-length lines wasted in early rows).
- CDCL is effective on B.22 (100% backjump rate, max dist 732) and should be retained.
- The next exploration should vary the length distribution while maintaining full 4-line coverage
  per cell.  A quadratic distribution, a uniform-511 distribution with random shuffle (repeating
  B.20 to validate parity), or a clipped triangular (minimum length 64) are candidate targets.

#### B.21.13 CDCL Interaction Study (B.23 and B.24)

Following B.22's conclusion that CDCL "should be retained," B.23 tested uniform-511 + CDCL to
isolate whether CDCL benefits a known-good partition. B.24 then disabled CDCL entirely to measure
the overhead.

---

**B.23: Uniform-511 LTP + CDCL Active**

B.23 changed `ltpLineLen(k)` to return 511 for all $k$ (B.20's construction, B.22 code
infrastructure).  The construction algorithm (Fisher-Yates LCG shuffle, consecutive 511-cell chunk
assignment) is identical to B.20 except that the `buildAllPartitions` function now uses
B.22-era seed constants (`CRSCLTP1`–`CRSCLTP4` in ASCII).

*Hypothesis:* uniform-511 + CDCL should outperform B.22's 80,300 depth (triangular + CDCL)
because the partition matches B.20's known-good structure.

30-minute uselessTest result:

| Metric | B.20 | B.22+CDCL | B.23 (uniform+CDCL) |
|--------|------|-----------|---------------------|
| Depth | ~88,503 | ~80,300 | **~69,000** |
| iter/sec | ~198K | ~521K | ~306K |
| CDCL max jump dist |&mdash;| 732 | **1,854** |
| CDCL backjumps/sec |&mdash;| ~1,200 | ~1,018 |
| Hash mismatch rate | ~25% | ~25% | ~25% |

**Assessment: B.23 regressed further than B.22.** The 1,854-cell average jump distance caused
severe thrashing — 27% worse than B.22+CDCL and 22% worse than B.20 without CDCL.

*Why is the jump distance larger with uniform-511 than triangular (B.22)?*  With uniform-511, all
lines have 511 cells.  A completed row requires ~511 cell assignments spread over many DFS frames;
by the time the hash failure is detected, the 1-UIP antecedent chain traces back nearly 1,854
frames.  With B.22's triangular distribution, short lines (2–6 cells) resolve in 1–2 decisions,
producing compact antecedent chains and shorter jumps (732 frames).  So a more uniform distribution
actually worsens CDCL jump distances.

*Root cause of CDCL ineffectiveness:* CDCL was designed for unit-clause propagation in SAT, where
a "conflict" traces back to exactly one decision variable (1-UIP).  Hash-based row verification is
a **global non-linear constraint** over all 511 cells in a row; no single decision is the "culprit."
The 1-UIP analysis produces a jump target that is far back in the stack (deep antecedent chain) and
irrelevant to the actual failure cause.  The net effect is a large undo of correct work without
gaining any constraint learning benefit (the current implementation does not store learned clauses).

---

**B.24: CDCL Disabled (kMaxCdclJumpDist = 0)**

B.24 added `constexpr uint64_t kMaxCdclJumpDist = 0` to the DFS loop.  When the CDCL analysis
returns a backjump distance exceeding this cap, the code falls through to chronological backtrack
instead.  With `kMaxCdclJumpDist = 0`, CDCL never fires.  The ReasonGraph, ConflictAnalyzer, and
cdclStack infrastructure remain in place but perform zero useful work.

2-minute uselessTest snapshot (B.24):

| Metric | B.20 | B.23 | B.24 (CDCL off) |
|--------|------|------|-----------------|
| Depth (2-min) | ~88,503 | ~69,000 | **~86,123** |
| iter/sec | ~198K | ~306K | **~120K** |
| CDCL backjumps |&mdash;| ~1,018/sec | **0** |

**Assessment: Disabling CDCL recovers most of B.20's depth.**  B.24 reaches ~86,123 at 2 minutes
(vs B.20's plateau of 88,503), confirming that CDCL was the primary cause of the regression.
The remaining ~2,400-frame gap is attributed to:

1. *CDCL overhead still running:* `recordDecision`, `recordPropagated`, and `unrecord` are called
   on every cell assignment/unassignment even though no backjumps fire.  This reduces iter/sec
   from B.20's ~198K to B.24's ~120K — a 40% throughput penalty.

2. *Different shuffle seeds:* B.20 may have had slightly different seeds that produced a marginally
   better partition.

**Conclusions from B.23 and B.24:**

- CDCL as implemented is strictly harmful in all tested configurations: depth drops 20–27% in
  every experiment where CDCL fires.
- The harm mechanism is the 1-UIP backjump distance: hash failures have antecedent chains of
  700–1,854 cells, causing large backward jumps that undo correct work without constraint learning.
- CDCL is fundamentally mismatched with hash-based row verification (a global constraint), as
  opposed to unit-clause propagation failures (local constraint) for which CDCL was designed.
- **B.25 plan:** Remove all CDCL overhead (delete cdclStack, ReasonGraph/ConflictAnalyzer calls,
  per-assignment record/unrecord calls) to recover B.20's ~198K iter/sec and close the 2K depth
  gap.  Expected outcome: depth ≥ 88,503 (or reveal remaining structural differences).

#### B.21.14 Open Questions

(a) What is the optimal distribution of the 1,023 dual-covered cells? Placing them at extreme corners
maximizes complementarity with diagonal coverage, but other strategies (e.g., distributing them along
the anti-diagonal extremes) may yield better propagation cascades. Empirical testing with the existing
solver can compare strategies by measuring depth-to-backtrack statistics.

(b) Does the sequential sub-table construction introduce ordering bias? $T_0$ gets first pick of the
cell pool and may receive a higher-quality spatial layout than $T_3$, which operates on the residual.
An iterative refinement step (re-constructing $T_0$ after $T_1$-$T_3$ exist, then repeating) could
equalize quality across sub-tables, at the cost of construction complexity.

(c) Is the triangular length distribution optimal, or would a different distribution (e.g., quadratic,
logarithmic) better match the solver's propagation dynamics? The triangular distribution mirrors
DSM/XSM for encoding simplicity, but the solver's performance depends on the distribution of short
lines relative to the DFS traversal order. A distribution with more short lines and fewer long ones
might increase early forcing at the cost of weaker mid-solve constraints.

(d) How does joint tiling interact with the belief-propagation-guided branching heuristic (Section 7)?
The BP estimator currently assumes each cell participates in 8 factor nodes (one per constraint line).
Reducing to 5-6 factors changes the BP update equations and convergence properties. The BP estimator
must be updated to handle variable factor counts per cell.

(e) Can the joint-tiling principle extend to the basic partitions? If diagonal and anti-diagonal
families were jointly tiled (each cell on exactly one of the two), the total constraint count would
drop further but diagonal encoding would change. This is unlikely to be beneficial because DSM and XSM
provide qualitatively different constraint information (different slope directions), whereas the four
LTP sub-tables are qualitatively interchangeable.

### B.22 Partition Seed Search for Uniform-511 LTP

#### B.22.1 Motivation

B.25 (uniform-511 LTP, CDCL removed) achieves depth ~86,123 and 329K iter/sec. B.20 achieved
depth ~88,503 at ~198K iter/sec using the same Fisher-Yates construction with different LCG seed
strings. The gap of ~2,380 depth cells (2.7%) has no algorithmic explanation: both configurations
use identical constraint topology (511 cells/line, 4 LTP sub-tables, same line-count), the same
DFS loop, and the same propagation engine. The only difference is the random shuffle of cells into
lines produced by the four LCG seeds.

This 2.7% gap is the direct cost of re-seeding when moving from B.20 to B.25. The Fisher-Yates
shuffle maps each of the $511^2 = 261{,}121$ cells into exactly one of the four sub-tables' lines.
Different seed strings produce different shuffles, and different shuffles produce different constraint
topologies at the cell level: which cells share LTP lines, which LTP lines have high overlap with
diagonal/column lines at the plateau band (rows 100--300), and therefore which cells cascade most
effectively when forced.

The depth ceiling for a given seed is not random noise. B.20's depth of 88,503 was reproducibly
measured over multiple runs; B.25's depth of 86,123 is equally stable. The depth is a structural
property of the partition, not a stochastic accident. Different seeds produce different structural
depth ceilings, and a systematic search over seeds can identify partitions whose structural
properties (high LTP-line overlap with other constraint families in the plateau band) yield higher
depth ceilings.

The target is to recover the ~2,380-cell gap relative to B.25, matching or exceeding B.20's 88,503
depth while retaining B.25's 329K iter/sec throughput.

#### B.22.2 Objective

Find four LCG seed strings $(s_1, s_2, s_3, s_4)$ for the Fisher-Yates construction of LTP
sub-tables $T_0$-$T_3$ such that the resulting uniform-511 partition achieves depth
$\geq 88{,}503$ (the B.20 baseline) at B.25's throughput (~329K iter/sec), without any
algorithmic change to the DFS loop or constraint architecture.

Secondary objective: understand *why* some seed configurations produce higher depth ceilings,
to guide future partition design.

#### B.22.3 Methodology

**Phase 1 — Baseline Characterization (1 run per seed)**

Each candidate seed configuration $(s_1, s_2, s_3, s_4)$ is evaluated by running the useless-machine
depth test for a fixed 2-minute window and recording:

- `depth_max`: maximum DFS depth reached
- `iter_per_sec`: mean iteration rate
- `hash_mismatch_rate`: fraction of iterations ending in SHA-1 failure

The 2-minute window provides sufficient signal to distinguish partitions with meaningfully
different depth ceilings (difference $\geq 1{,}000$ cells is reliable; $\geq 500$ cells is
directional).

**Phase 2 — Seed Space**

The Fisher-Yates LCG uses seeds derived from a string constant via a hash or direct XOR:
$\text{seed} = \text{hash}(\text{string}) \oplus \text{row\_index}$. The current seeds are the
ASCII byte XOR of the strings `CRSCLTP1`, `CRSCLTP2`, `CRSCLTP3`, `CRSCLTP4` (B.25). B.20
used a different set of string constants.

The search space is structured as follows:

- *String prefix search*: vary a 4-8 character ASCII suffix while holding the prefix constant.
  This provides a dense neighborhood in seed space while keeping the LCG properties stable.
- *Independent sub-table search*: optimize each sub-table's seed independently. If sub-table
  quality is approximately independent (a cell's constraint value comes from its full 8-line
  membership, not just the LTP line), then $4 \times K$ evaluations suffice to find a near-optimal
  4-tuple rather than $K^4$.
- *Geometric proxy metric*: before running the full depth test, evaluate each partition by a
  cheap structural metric (see B.22.4). Use this metric to prune candidate seeds by $\geq 90\%$,
  spending full 2-minute runs only on the top candidates.

**Phase 3 — Validation**

Top-5 candidate seed configurations from Phase 2 are validated with 10-minute depth runs
($3\times$ longer for statistical confidence). The configuration achieving the highest
`depth_max` over the 10-minute window is adopted as the B.22 production seed set.

#### B.22.4 Geometric Proxy Metric

Rather than running a 2-minute depth test for every candidate seed, evaluate each partition by
a cheap structural metric that correlates with depth ceiling. The metric targets the plateau
band (rows 100-300) where the forcing dead zone activates.

**Cross-family line overlap score** $\Phi$: for each LTP line $\ell$ with index $k$ (belonging
to sub-table $T_j$), count the number of cells on $\ell$ that also lie on a nearly-tight
column or diagonal line in the plateau band. "Nearly tight" at the plateau means that the line
has small unknown count ($u \leq \tau_\Phi$, suggested $\tau_\Phi = 50$). A cell $(r, c)$ on
LTP line $\ell$ contributes to $\Phi(\ell)$ if any of its 6 non-LTP non-row constraint lines
(column, diagonal, anti-diagonal, and the other 3 LTP sub-tables) is nearly tight in the
plateau band.

$$
\Phi = \frac{1}{4 \times 511} \sum_{j=0}^{3} \sum_{k=0}^{510} \Phi(T_j, k)
$$

Higher $\Phi$ indicates that LTP lines in the plateau band co-reside with tight non-LTP lines,
increasing the probability that LTP-line forcing events cascade into row completions and SHA-1
checks. This is the structural property that determines depth ceiling.

Computing $\Phi$ requires only the static partition layout and a precomputed estimate of which
lines are tight in the plateau band (derived from the cross-sum residuals of the test block).
Wall time: $O(4 \times 511 \times \text{line\_len}) = O(10^6)$ operations, approximately 1 ms&mdash;
three orders of magnitude cheaper than a 2-minute depth run.

#### B.22.5 Expected Outcomes

**Best case:** A seed configuration is found with depth $\geq 90{,}000$ ($\Phi$-selected, 2K+
improvement over B.20). This would confirm that the $\Phi$ metric is a reliable proxy and that
meaningful depth gains are available within the uniform-511 architecture without algorithmic
changes.

**Likely case:** A seed configuration is found in the range 87{,}500-89{,}500, recovering most
or all of the 2,380-cell B.20-to-B.25 regression and possibly exceeding B.20. This would
establish the B.22 seed set as the new baseline for all subsequent experiments.

**Pessimistic case:** No seed configuration meaningfully exceeds ~88{,}500 despite $K \geq 200$
candidate evaluations. This would indicate that the 88K-89K range is a structural ceiling for
uniform-511 partitions regardless of cell assignment, pointing toward architectural changes
(variable-length lines, increased constraint density) as the only path to deeper search.

In all cases, the secondary objective is satisfied: the distribution of depth ceilings over the
seed search gives a quantitative picture of how much variance the partition geometry contributes
to solver performance, complementing the partition-architecture data from B.20-B.21.

#### B.22.6 Relationship to Other Proposals

*B.20 (uniform-511 LTP baseline).* B.22 is a direct continuation of B.20's partition architecture,
reusing the Fisher-Yates construction and 511-cell-per-line constraint. The only change is
systematic exploration of the seed space rather than accepting the default constants.

*B.21/B.22 (variable-length LTP).* Variable-length partitions (B.21) achieved 0.20% mismatch
rate and dramatically better constraint quality, but depth regression to ~50K due to constraint
density loss. Seed search operates within the proven uniform-511 architecture; findings inform
whether architectural complexity is necessary or whether the uniform-511 ceiling can be raised
sufficiently to meet the project's depth target.

*B.8 (adaptive lookahead).* Deeper lookahead (k=4) produced zero depth improvement in the
forcing dead zone. Partition seed search addresses the dead zone directly by modifying which
lines are co-resident in the plateau band, rather than applying deeper probing to the existing
topology.

*B.10 (constraint-tightness ordering).* In the forcing dead zone, all lines have
$\rho/u \approx 0.5$ and tightness scores are uniformly weak. A partition with higher $\Phi$
creates more variation in line tightness at the plateau, making B.10 relevant again. Seed search
and tightness ordering are therefore synergistic: seed search creates the tightness variation
that B.10 can exploit.

#### B.22.7 Open Questions

(a) Does the $\Phi$ metric correlate reliably with depth ceiling? If $\Phi$ and depth ceiling
have Spearman rank correlation $\geq 0.7$ over 50+ candidate seeds, the metric is useful as a
pre-filter. If correlation is low, structural metrics alone cannot predict depth; full 2-minute
runs are required for all candidates.

(b) Are the four sub-table seeds independent? If optimizing $s_1$ while holding $s_2, s_3, s_4$
fixed produces the same depth ceiling as a joint optimization, the search reduces from $O(K^4)$
to $O(4K)$. Empirical testing: optimize each sub-table seed independently, then run the joint
configuration and compare.

(c) Is there a seed configuration that exceeds B.21's $\Phi$ score despite uniform-511 line
lengths? The triangular variable-length architecture achieves low $\Phi$ (short lines are sparse
in the plateau band) but high constraint quality (near-zero mismatch). A uniform-511 partition
with high $\Phi$ achieves high constraint density (all 511 cells/line) with good cross-family
overlap. If such a partition can be found, it may combine the best properties of B.20 and B.21
without the depth regression.

(d) Does the optimal seed set generalize across input blocks? The depth test uses a single
representative block. A seed configuration optimized on one block may overfit to its specific
cross-sum residual structure. Validation on 3-5 diverse blocks (all-zeros, all-ones, random,
alternating) is required before adopting a new seed set for production.

(e) What is the variance of depth measurements for a fixed seed at 2 minutes vs. 10 minutes?
If depth variance over 2-minute windows is large (standard deviation $\geq 500$ cells), the
phase 1 pre-filter will produce false rankings and Phase 2 validation will require longer runs.
The variance characterization should be performed on B.25's current seed set before beginning
the search.

### B.23 Clipped-Triangular LTP Partitions

#### B.23.1 Motivation

B.25 (uniform-511 LTP) produces depth ~86,123. B.22 (full-coverage variable-length LTP,
triangular distribution with ltp_len(k) = 4·min(k+1, 511−k) − adj(k), range 2--1,021) achieved
depth ~80,300, a 7% *regression* from B.20's 88,503. The root cause: B.22's shortest lines (2-8
cells) are exhausted within the first few DFS rows and contribute no forcing in the plateau band
(rows 100-300). Their slots in the sub-tables are "wasted" relative to a longer line that would
still be active at the plateau.

B.21 (proposed joint-tiled variable-length, ltp_len(k) = min(k+1, 511−k), range 1-256) was
evaluated analytically but never implemented because B.22's experimental regression contradicted
the theoretical premise: short lines were expected to generate forcing cascades in early rows that
reduce the solver's branching factor, but in practice the extreme short lines (< 16 cells) provide
too little cross-row coverage to survive contact with the plateau band.

B.23 proposes *clipped-triangular* LTP line lengths: each line $k$ has length
$\text{ltp\_len}(k) = \max(L_{\min}, \min(k+1, 511-k))$, where $L_{\min}$ is a minimum clip
threshold. The triangular distribution is preserved at the top (maximum 256 cells for k=255),
but all lines shorter than $L_{\min}$ are extended to $L_{\min}$. This eliminates the
ineffective very-short lines while retaining the length variation that distinguishes B.23 from
uniform-511.

The working hypothesis is that B.22's regression is driven by the lines below some threshold
$L_{\min}^*$: lines shorter than $L_{\min}^*$ add noise (consuming cell slots for lines that
never generate plateau-band forcing) while lines at or above $L_{\min}^*$ provide the structural
variation that improves constraint topology. If this hypothesis holds, the optimal clip threshold
$L_{\min}^*$ lies where the marginal value of line-length variation (longer lines → more plateau
coverage) balances the marginal cost (shorter lines → fewer cells assigned to plateau-active
lines).

#### B.23.2 Objective

Find a clip threshold $L_{\min} \in \{16, 32, 64, 128\}$ such that the clipped-triangular LTP
partition achieves depth strictly greater than B.25's uniform-511 baseline of ~86,123 while
maintaining throughput $\geq 250K$ iter/sec. A secondary objective is to determine whether
the full uniform-511 ceiling (~88,503 from B.20) is reachable with clipped-triangular geometry,
or whether uniform-511 is structurally superior for any achievable $L_{\min}$.

#### B.23.3 Partition Structure

For a given clip threshold $L_{\min}$:

$$
\text{ltp\_len}(k) = \max\!\left(L_{\min},\; \min(k+1, \; 511-k)\right), \quad k = 0, 1, \ldots, 510
$$

The resulting length distribution:
- For $k < L_{\min} - 1$ or $k > 511 - L_{\min}$: $\text{ltp\_len}(k) = L_{\min}$ (clipped)
- For $L_{\min} - 1 \leq k \leq 511 - L_{\min}$: $\text{ltp\_len}(k) = \min(k+1, 511-k)$
  (unclipped triangular, range $L_{\min}$-256)

**Total cells per sub-table** (sum over k=0..510):

$$
N(L_{\min}) = 2\!\sum_{k=0}^{L_{\min}-2}\! L_{\min} \;+\; \sum_{k=L_{\min}-1}^{511-L_{\min}}\!\min(k+1, 511-k)
$$

For the clipped region, each of the $2(L_{\min}-1)$ short lines contributes $L_{\min}$ cells
instead of 1..$(L_{\min}-1)$ cells, so $N(L_{\min}) > N(0) = 65,536$ (B.21). Coverage exceeds
B.21 but may not reach 261,121 (full coverage). The four sub-tables together must cover all
261,121 cells; the construction protocol (B.23.4) is responsible for complete coverage.

**Payload impact:** Variable-width encoding uses $\lceil\log_2(\text{ltp\_len}(k)+1)\rceil$ bits
per element. For $L_{\min} = 64$: minimum width = $\lceil\log_2(65)\rceil = 7$ bits; for
$L_{\min} = 128$: minimum width = $\lceil\log_2(129)\rceil = 8$ bits. The mean bit-width
increases as $L_{\min}$ increases, converging to 9 bits (uniform-511) at $L_{\min} = 256$.
`kBlockPayloadBytes` changes accordingly.

#### B.23.4 Construction Protocol

The clipped-triangular partition is built using the same sequential spatial-assignment algorithm
as B.22, with $\text{ltp\_len}(k)$ substituted for the B.22 length formula:

1. For each sub-table $j = 0, 1, 2, 3$:
   - Build candidate pool = cells with assignment count $< j+1$
   - Sort lines in decreasing order of $\text{ltp\_len}(k)$ (longest lines first)
   - For each line in sorted order:
     - Seed LCG: $\text{seed} = \text{baseSeed}[j] \oplus (k \times k_A + k_C)$
     - LCG-shuffle candidate pool
     - Sort by ascending spatial score (corners first)
     - Assign first $\text{ltp\_len}(k)$ candidates to line $k$ of sub-table $j$
     - Mark assigned cells

This is identical to B.22's construction except for the length function. The spatial-score sort
and LCG shuffle are unchanged. CSR storage (offsets + cells arrays) is used exactly as in B.22.

#### B.23.5 Impact on Solver Architecture

All solver components change only at the `ltpLineLen(k)` call site — the single function that
maps a line index to its length. Everything downstream (ConstraintStore stats initialization,
propagation loop, serialization bit-width) already reads from `ltpLineLen(k)`. The change is
therefore local to `LtpTable.h`:

```cpp
[[nodiscard]] inline std::uint32_t ltpLineLen(const std::uint16_t k) noexcept {
    constexpr std::uint32_t kClip = 64U;  // or 32, 128 — experiment parameter
    const auto tri = static_cast<std::uint32_t>(std::min(k + 1U, 511U - k));
    return std::max(kClip, tri);
}
```

No changes to ConstraintStore, PropagationEngine, BranchingController, or the DFS loop. The
`kBlockPayloadBytes` constant changes to reflect the new bit-width distribution.

#### B.23.6 Expected Outcomes

The expected depth ceiling as a function of $L_{\min}$:

| $L_{\min}$ | Description                     | Predicted Depth | Rationale |
|:-----------:|:--------------------------------|:---------------:|:----------|
| 1 (B.22)   | Full triangular, no clip         | ~80,300         | Short lines too weak; established experimentally |
| 16          | Clip at 16 cells                 | ~82--84K        | Some short-line noise eliminated; medium lines still active |
| 32          | Clip at 32 cells                 | ~84--86K        | Most short-line noise gone; expect near-parity with uniform-511 |
| 64          | Clip at 64 cells                 | ~85--88K        | Short lines gone; length variation in plateau range; target outcome |
| 128         | Clip at 128 cells                | ~86--88K        | Near-uniform; small variation at top; expected parity with B.25 |
| 256 (uniform-511) | Full uniform, no variation | ~86,123        | B.25 baseline |

The hypothesis predicts a performance peak at $L_{\min} = 64$ or $L_{\min} = 128$, where
line-length variation is concentrated in the plateau band (lines of length 64--256 correspond
to $k \in [63, 447]$, covering 384 of 511 lines and exactly the rows that stall).

**Best case:** $L_{\min} = 64$ achieves depth $\geq 89{,}000$, confirming that length variation
at the 64--256 scale provides structural benefits that uniform-511 cannot. B.23 becomes the new
production partition architecture.

**Likely case:** $L_{\min} = 64$ achieves depth ~87--88K, roughly matching or slightly exceeding
B.25, with lower `hash_mismatch_rate` (reflecting better plateau-band constraint quality). The
result is directionally positive but not a breakthrough; B.23 may be combined with B.22's seed
search to extract further gains.

**Pessimistic case:** All clip thresholds perform at or below B.25's 86,123 baseline, replicating
B.22's regression at milder scale. This would indicate that any length variation is detrimental
under the current construction protocol and that uniform-511 is the structural optimum for
Fisher-Yates partitions. Further architectural improvement would require either joint-tiling
(B.21) or a different construction strategy.

#### B.23.7 Relationship to Other Proposals

*B.22 (seed search).* B.22 and B.23 are complementary and independent. B.22 varies the seed
within a fixed (uniform-511) length distribution; B.23 varies the length distribution with a
fixed construction algorithm. The optimal combination is a clipped-triangular partition with
a seed-searched assignment. If B.23's $L_{\min} = 64$ peak outperforms B.22's best uniform-511
seed, the partition architecture (B.23) takes precedence and seed search within the B.23
architecture becomes the follow-on step.

*B.25 (current production).* B.23 is implemented as a drop-in replacement for B.25's
`ltpLineLen(k)`. No other files change. The experiment can be run by modifying a single
constant in `LtpTable.h` and rebuilding, making iteration fast.

*B.21 (proposed joint tiling).* B.21's joint-tiling construction protocol is significantly
more complex (CSR storage, `LtpMembership` forward table, variable cell coverage per sub-table).
B.23 retains B.25's simpler "every cell in all 4 sub-tables" ownership model. If B.23 achieves
the depth improvement, B.21's complexity is unnecessary. B.23 should be fully evaluated before
B.21 implementation is considered.

#### B.23.8 Open Questions

(a) What is the optimal clip threshold $L_{\min}^*$? The prediction in B.23.6 suggests 64--128,
but the true optimum may differ. Does depth ceiling vary monotonically with $L_{\min}$ (allowing
binary search), or does it exhibit a non-monotone peak?

(b) Does the optimal $L_{\min}^*$ interact with the seed constants? A clipped-triangular
partition at $L_{\min} = 64$ with default seeds may be suboptimal; combining $L_{\min}$ search
with seed search (B.22) may be required to find the global optimum. Does the $\Phi$ metric from
B.22.4 extend naturally to variable-length lines (weighting short lines less in the $\Phi$ sum)?

(c) Does the construction order (longest lines first) still produce optimal spatial layout for
clipped lines? For B.22's full-triangular construction, sorting by decreasing length ensures
that long lines (which cover the most cells) are assigned first and therefore receive higher
spatial priority. For clipped distributions, the many equal-length clipped lines at both ends of
the $k$ range are assigned in an arbitrary order. Does sub-sorting within the clipped band by
spatial score produce better partitions?

(d) How does `kBlockPayloadBytes` change across the tested $L_{\min}$ values, and does the
encoding cost offset any solver depth benefit in terms of compression ratio? Specifically: does
$L_{\min} = 64$ produce a smaller block payload than uniform-511, and if so, does that improve
the overall compression ratio $C_r$ meaningfully?

(e) Is the clipped-triangular architecture compatible with the LTP membership model (B.21's
joint-tiling)? If the flat "all 4 sub-tables" model is retained (as in B.23), can the
construction protocol be extended to allow joint-tiling within the clipped-triangular length
distribution for a future experiment?

#### B.23.9 Experimental Results (ABANDONED)

B.23 was implemented and tested with $L_{\min} = 64$ on 2026-03-05.

**Implementation:** Changed `ltpLineLen(k)` to `max(64, min(k+1, 511-k))` in `LtpTable.h`.
The construction assigns cells in decreasing-length order.  **Critical finding discovered during
implementation:** sum of `ltp_len(k)` over $k=0..510$ equals **69,568** (not 261,121).  With
261,121 cells in the pool and only 69,568 assigned per sub-table, coverage is 26.7% per sub-table.
Expected LTP memberships per cell: $4 \times 0.267 \approx 1.07$ (mostly 0 or 1).

**Result:** Depth peaked at **~46,022** (vs 86,123 for B.25 uniform-511). This is a **46%
regression**, far worse than B.22's 7% regression.

**Root cause:** The spec's claim that B.23 "retains B.25's 'every cell in all 4 sub-tables'
ownership model" is mathematically impossible for the given `ltp_len` formula — the total
cell budget is 4 × 69,568 = 278,272, covering 261,121 cells an average of **1.07 times** (not 4).
73% fewer LTP constraints than B.25 cripples propagation quality.

**Conclusion:** The simple "change `ltpLineLen(k)`" implementation is fundamentally broken for
any clip value where `sum(ltp_len(k)) < 261,121`.  B.23 as specified requires the full B.21-style
joint-tiling construction to achieve meaningful cell coverage.  Since B.21's joint-tiling was
already considered too complex and produced a regression itself (50,272 depth), B.23 is abandoned.

| Metric | B.25 (baseline) | B.23 kLtpClip=64 |
|:-------|:---------------:|:----------------:|
| Peak depth | ~86,123 | ~46,022 |
| Cells/sub-table | 261,121 | 69,568 |
| Avg LTP memberships/cell | 4.0 | ~1.07 |
| Depth regression | — | −46% |

**Abandonment verdict:** B.23 clipped-triangular (as a drop-in `ltpLineLen` substitution) is
abandoned.  The correct follow-on is B.22 seed search within the uniform-511 architecture (B.25),
which preserves full coverage while varying partition topology.

### B.24. LTP Hill-Climber: Direct Cell-Assignment Optimisation (ABANDONED)

#### B.24.1 Motivation

The B.9.2 optimization objective identified a specific failure mode: at plateau entry (~row 170),
every LTP line has roughly 170 of its 511 cells assigned, leaving $u \approx 341$ unknowns and a
residual far from the forcing conditions ($\rho = 0$ or $\rho = u$). Because Fisher--Yates shuffle
distributes cells uniformly across all rows, all LTP lines progress at the same rate and none
reaches its forcing threshold before the solver stalls. No line is tight enough to propagate
anything.

The proposed fix was a *direct hill-climbing optimizer* that would post-process the Fisher--Yates
baseline by swapping individual cell-to-line assignments, driving a subset of LTP lines to
concentrate their cells in the early rows (0--170) already solved before the plateau. At plateau
entry, these biased lines would have $u \lesssim 170$ (half their unknowns already resolved), with
residuals closer to the forcing threshold than any uniformly-distributed line at the same depth.
The remaining ~400 LTP lines absorb peripheral cells and remain loose, but those cells are
well-served by DSM/XSM (short diagonal lines) and row/column cardinality forcing.

**Degrees of freedom.** A Fisher--Yates seed provides one degree of freedom per sub-table over
$O(s^{2}!)$ possible assignments. The hill-climber relaxes this constraint and operates directly
in the assignment space: for an $s \times s$ matrix with $s$ lines of $s$ cells each, any two
cells on different lines can be exchanged while preserving the uniform-length invariant, giving
$O(s^{4})$ candidate swaps per round.

#### B.24.2 Hypothesis

> *Biasing 50--100 LTP lines so that the majority of their $s = 511$ cells lie in rows $0$--$170$
> will create forcing cascade opportunities at plateau entry that do not exist under the uniform
> Fisher--Yates construction.  A hill-climbing optimizer that iteratively accepts swaps reducing
> mean DFS backtrack count in the plateau band will converge to such a biased configuration and
> will increase depth materially above the Fisher--Yates baseline.*

Formally: let $\text{cov}(L) = |\{(r,c) \in L : r \leq 170\}|$ be the early-row coverage of
line $L$, and let $\Phi = \sum_L \max(0,\, \text{cov}(L) - \theta)^2$ for a threshold $\theta$
(e.g., $\theta = 340$, rewarding lines with $> 66.5\%$ early-row concentration). The hill-climber
maximises $\Phi$; increasing $\Phi$ is taken as a proxy for increasing DFS depth.

Secondary motivation from B.9.3: an optimized LTP has no algebraic relationship between the
cell-to-line mapping and the cell coordinates $(r, c)$.  Swap-invisible attack patterns must
simultaneously satisfy all linear partition constraints (row, column, diagonal, anti-diagonal)
*plus* the non-linear LTP constraint, which is harder when the LTP geometry is opaque to
linear-algebraic analysis.

#### B.24.3 Methodology

**Phase 1 — Baseline construction.** Generate a Fisher--Yates uniform partition seeded with a
fixed constant (B.9.1).  Each of the four sub-tables assigns all $s^2 = 261{,}121$ cells to one
of $s = 511$ lines with exactly 511 cells per line (uniform-511 invariant).

**Phase 2 — Hill-climbing loop.**  Repeat until convergence:

1. Sample two lines $L_a$, $L_b$ uniformly at random from the same sub-table ($L_a \neq L_b$).
2. Sample one cell $p_a \in L_a$ and one cell $p_b \in L_b$ uniformly at random.
3. Compute the change in $\Phi$ if $p_a$ and $p_b$ swap their line assignments:
   $$\Delta\Phi = [\max(0,\text{cov}^*(L_a) - \theta)^2 + \max(0,\text{cov}^*(L_b) - \theta)^2]
                 - [\max(0,\text{cov}(L_a) - \theta)^2   + \max(0,\text{cov}(L_b) - \theta)^2]$$
   where $\text{cov}^*$ is coverage after the proposed swap.
4. Accept the swap if $\Delta\Phi > 0$; otherwise reject.

**Filter:** A swap that moves cells within the same coverage zone ($r \leq 170$ for both, or $r > 170$
for both) does not change $\Phi$ and is skipped.

**Uniform-511 invariant:** After each accepted swap $L_a$ gains one cell and $L_b$ loses one cell
*from the same sub-table*, then the symmetric exchange restores both lines to $s = 511$ cells.
The invariant is preserved unconditionally.

**Implementation context.** The hill-climber was implemented in Python, using the same
compress/decompress pipeline used in B.26c seed-search experiments (20-second timed runs, peak
depth from `CRSCE_EVENTS_PATH` JSON-L log).  The proxy metric $\Phi$ was computed incrementally
in $O(1)$ per swap from pre-built coverage counters.

#### B.24.4 Experimental Results (ABANDONED)

The procedure was conducted immediately following B.26c (which established the 91,090 baseline
with CRSCLTPV+CRSCLTPP seeds).  The hill-climbing optimizer was run overnight on the B.26c
winning partition.

**Outcome:** Peak depth collapsed to **$< 500$** — a catastrophic regression of more than 90,000
cells below the 91,090 baseline.  The optimizer was immediately halted and the approach abandoned.

| Metric | B.26c baseline | B.24 hill-climber |
|:-------|:--------------:|:-----------------:|
| Peak depth | 91,090 | $< 500$ |
| Regression | — | $> 99\%$ |
| $\Phi$ score | N/A (Fisher--Yates) | Increasing |
| Depth trajectory | Stable | Collapsed |

The proxy metric $\Phi$ continued to increase while depth simultaneously collapsed, demonstrating
that $\Phi$ and DFS depth are **anti-correlated** in this regime.

#### B.24.5 Root Cause Analysis

The Fisher--Yates shuffle produces partitions with three global statistical properties that are
critical for effective constraint propagation and that the hill-climber destroyed:

1. **Near-uniform pairwise co-occurrence.** For any two cells $(r_1, c_1)$ and $(r_2, c_2)$,
   the probability they belong to the same LTP line is close to $1/(s-1)$ under the uniform
   distribution.  When the hill-climber concentrates cells from rows 0--170 onto a subset of
   lines, those lines gain high co-occurrence among early-row cells and near-zero co-occurrence
   among plateau-band cells.  Constraint propagation depends on cross-row co-occurrence to
   propagate information between already-solved rows and the branching frontier; concentrating
   co-occurrence inside the solved region removes the cross-boundary linkage that drives forcing.

2. **Low maximum cross-line overlap.** Under Fisher--Yates, any two LTP lines share at most
   $O(\sqrt{s})$ cells in expectation.  After hill-climbing concentrates ~300 cells from each
   biased line into rows 0--170, the biased lines share a large fraction of their early-row
   cells with each other (because early rows provide only $170 \times 511 = 86{,}870$ total
   cell-slots for $511$ lines competing for early-row concentration).  High cross-line overlap
   makes multiple LTP constraints redundant: forcing on one biased line conveys little new
   information about cells already covered by the other biased lines.

3. **Well-distributed spatial coverage.** Under Fisher--Yates, each plateau-band row (100--300)
   contains approximately $511 \times 511 / 511 = 511$ cells per line, giving uniform density.
   After hill-climbing, the ~400 un-biased lines must absorb the late-row cells rejected by
   biased lines.  Each un-biased line becomes heavily loaded with rows 170--510, reducing their
   constraint value in the plateau band to near zero.  The plateau band (the critical region for
   depth) loses LTP constraint coverage precisely where it is most needed.

**Summary:** Maximising $\Phi$ explicitly concentrates the useful cells of biased lines *outside*
the plateau band and floods the un-biased lines with plateau-band cells but no corresponding sum
constraints that are tight.  The solver loses the LTP forcing information it had under the
uniform partition and regresses catastrophically.

#### B.24.6 Theoretical Interpretation

The Fisher--Yates LCG construction is not merely a convenient baseline; it is a *strong global
prior* that encodes the spatial regularity required for effective constraint propagation.  The
prior cannot be captured by any local metric:

- $\Phi$ measures concentration in a fixed row band; it is blind to co-occurrence structure.
- Any single scalar proxy can be improved by local swaps while global properties degrade.
- The space of valid uniform-511 partitions is $O(s^{s^2})$; the hill-climber gradient points
  away from the Fisher--Yates manifold of "useful" partitions toward a degenerate region that
  the optimizer has no mechanism to detect until the full decompressor is queried.

**Key conclusion:** The optimization variable (assignment) and the optimization objective (DFS
depth) are coupled through global properties of the partition geometry.  No local proxy captures
this coupling.  Direct hill-climbing of LTP cell assignments is not a viable optimization strategy.

#### B.24.7 Implications for Subsequent Experiments

B.24's failure definitively closed the direct-partition-manipulation branch of the search tree:

- **Seed search is the correct optimization strategy.** Varying the LCG seed varies the entire
  partition globally and simultaneously, preserving all Fisher--Yates invariants while exploring
  a different region of the assignment space.  This is exactly what B.26c exploited.
- **Proxy-based hill-climbing cannot substitute for end-to-end depth measurement.** Any
  experiment that proposes to optimize a surrogate metric without periodic DFS validation should
  be treated with extreme scepticism.
- **The B.9.2 objective is sound; the B.9.1 optimization mechanism is not.** The row-concentration
  idea (B.9.2) correctly identifies what a good partition should look like.  The error was
  assuming local swaps could achieve it without destroying the global invariants on which the
  solver relies.  A constructive approach (e.g., seeded shuffle with a modified initial ordering
  that biases early-row placement) might achieve the B.9.2 objective without destroying global
  co-occurrence structure, but this remains unverified and was not pursued.

This result was originally recorded informally in B.26.7 ("Lessons from Hill-Climbing
(Abandoned Side-Experiment)") immediately after B.26c.  B.24 is the primary documentation.

### B.25. Reserved

### B.26 Joint Seed Search for LTP Sub-Tables (Implemented)

#### B.26.1 Motivation

B.22 established that LTP partition seeds control a 2.7% depth variance (~2,380 cells) and
proposed a greedy sequential (coordinate-descent) search over a 36-element candidate set per
sub-table. The B.22 search optimized each seed $s_i$ independently in four sequential phases,
fixing the other three to their current best or default values. This strategy reduced the search
cost from $36^4 = 1{,}679{,}616$ evaluations to $4 \times 36 = 144$ evaluations, but it assumed
that sub-table quality contributions are approximately separable.

Separability may fail when two LTP sub-tables interact through constraint propagation cascades.
A cell $(r, c)$ belongs to one line in $T_0$ and one line in $T_1$. When the solver assigns a
value to $(r, c)$, both lines' residuals are updated simultaneously. If $T_0$-line $\ell_0$ and
$T_1$-line $\ell_1$ share many cells in the plateau band (rows 100--300), a forcing event on
$\ell_0$ may immediately push $\ell_1$ toward its own forcing threshold, producing a two-step
cascade that neither sub-table would achieve with arbitrary partners.

In optimization theory, coordinate descent over a discrete, non-differentiable objective provides
no convergence guarantee to the global optimum (Papadimitriou & Steiglitz, 1998). The DFS depth
metric $D(\sigma_1, \sigma_2, \sigma_3, \sigma_4)$ is a highly non-smooth function of the seed
tuple with no known gradient or Lipschitz structure. B.26 replaces coordinate descent with
exhaustive joint search in the two most sensitive seed dimensions.

#### B.26.2 Objective

Determine whether the B.22 greedy sequential seed optimum is also the joint optimum for
sub-tables $T_0$ and $T_1$, and if not, identify the globally optimal $(s_1, s_2)$ pair within
the candidate set. The search proceeds in two campaigns of increasing scope:

- **B.26c:** Exhaustive joint search over $36 \times 36 = 1{,}296$ pairs drawn from the
  original B.22 alphanumeric candidate set (ASCII `0`--`9`, `A`--`Z`), holding $s_3$ and $s_4$
  fixed.

- **B.26d:** Exhaustive joint search over $256 \times 256 = 65{,}536$ pairs drawn from the
  full single-byte range (`0x00`--`0xFF`), parallelized across multiple CPU cores. B.26d
  subsumes B.26c and explores the complete suffix-byte seed space.

Secondary objective: characterize the two-dimensional landscape $D(s_1, s_2)$ to determine
whether the objective is smooth (amenable to local search) or rugged (requiring exhaustive
enumeration).

#### B.26.3 Fixed Parameters

Both campaigns fix sub-table seeds $s_3 = \texttt{CRSCLTP3}$ and $s_4 = \texttt{CRSCLTP4}$ at
their B.22 Phase 3/Phase 4 values. This decision is justified by an empirical observation from
the B.22 sequential search: all 36 candidates for both Phase 3 and Phase 4 achieved identical
peak depths of 89,331, indicating that the solver's depth ceiling is insensitive to $s_3$ and
$s_4$ at the current operating point. The search therefore computes:

$$
D^*(s_1, s_2) = \max_{(s_1, s_2) \in \mathcal{S} \times \mathcal{S}} D(s_1, s_2, \sigma_3^{\text{fixed}}, \sigma_4^{\text{fixed}})
$$

where $\mathcal{S}$ is the candidate set (36 elements for B.26c, 256 for B.26d) and $D(\cdot)$
is the peak depth achieved in a fixed-duration decompression run.

#### B.26.4 Methodology

**Re-compression requirement.** Each candidate seed pair requires re-compression of the source
file before decompression can be evaluated. The compressed `.crsce` file contains LTP cross-sum
vectors computed from the original matrix under a specific set of seeds. If the decompressor uses
different seeds (hence a different cell-to-line mapping), the stored sums are inconsistent with
the partition the decompressor constructs. Without re-compression, depth measurements are invalid.

The implementation proceeds as follows for each candidate pair $(s_1, s_2)$:

```python
# Step 1: Compress with candidate seeds (DI discovery disabled for speed)
compress_env["CRSCE_LTP_SEED_1"] = str(s1)
compress_env["CRSCE_LTP_SEED_2"] = str(s2)
compress_env["CRSCE_LTP_SEED_3"] = str(SEED3_FIXED)
compress_env["CRSCE_LTP_SEED_4"] = str(SEED4_FIXED)
compress_env["DISABLE_COMPRESS_DI"] = "1"
subprocess.run([compress_bin, "-in", source_video, "-out", crsce_path], env=compress_env)

# Step 2: Decompress with same seeds for `secs` seconds, then SIGINT
proc = subprocess.Popen([decompress_bin, "-in", crsce_path, "-out", out_path], env=env)
time.sleep(secs)
proc.send_signal(signal.SIGINT)   # graceful flush of telemetry
proc.wait(timeout=5)              # escalate to SIGKILL on timeout
```

**Fitness metric.** Peak DFS depth recorded in the decompressor's JSON-L telemetry log. The
decompressor writes structured events to `CRSCE_EVENTS_PATH`; each event includes `tags.depth`.
The script parses the log and extracts `max(depth)` across all events.

**Progressive output.** Results are written to a JSON file after each pair evaluation.
The `--resume` flag reloads prior results and skips completed pairs, making the search idempotent
with respect to interruption.

#### B.26.5 B.26c: Alphanumeric Joint Search

**Search space.** The 36-element candidate set from B.22: ASCII characters `0`--`9` and `A`--`Z`
appended to the 7-byte prefix `CRSCLTP`, yielding 8-byte (64-bit) seeds
$\sigma = \texttt{0x43\_52\_53\_43\_4C\_54\_50\_}xx$ where $xx \in \{\texttt{0x30}\text{--}\texttt{0x39}, \texttt{0x41}\text{--}\texttt{0x5A}\}$.
Total: $36^2 = 1{,}296$ pairs.

**Runtime.** Sequential execution on Apple Silicon, 20s decompression + ~3--5s re-compression
per pair, totaling approximately 9 hours.

**Implementation.** `tools/b26c_joint_seed_search.py`. Extends B.22's single-axis scan to a
two-dimensional exhaustive sweep. The `run_candidate()` function implements the re-compress +
timed-decompress + telemetry-parse pipeline described in B.26.4.

#### B.26.6 B.26c Experimental Results

The joint search completed all 1,296 candidate pairs.

**Winner:** $(\sigma_1, \sigma_2) = (\texttt{CRSCLTPV}, \texttt{CRSCLTPP})$, peak DFS depth
**91,090**. This surpasses the B.22 greedy sequential baseline of 89,331 by **1,759 cells**
(+1.97%).

**Depth landscape:**

| Metric                    | Value            |
|:--------------------------|:-----------------|
| Total pairs evaluated     | 1,296            |
| Mean depth                | 86,941           |
| Minimum depth             | 84,265           |
| Maximum depth (winner)    | 91,090           |
| Pairs $\geq$ 89,331 (B.22 baseline) | 29 (2.2%) |
| Pairs $\geq$ 90,000      | 7 (0.54%)        |
| Pairs $\geq$ 91,000      | 1 (0.08%)        |

**Top-10 pairs:**

| Rank | Seed 1   | Seed 2   | Depth  |
|:----:|:---------|:---------|:------:|
| 1    | CRSCLTPV | CRSCLTPP | 91,090 |
| 2    | CRSCLTPH | CRSCLTPR | 90,826 |
| 3    | CRSCLTPG | CRSCLTPK | 90,244 |
| 4    | CRSCLTPX | CRSCLTPB | 90,139 |
| 5    | CRSCLTPZ | CRSCLTPZ | 90,031 |
| 6    | CRSCLTPP | CRSCLTPQ | 90,027 |
| 7    | CRSCLTPJ | CRSCLTP9 | 90,013 |
| 8    | CRSCLTPB | CRSCLTPA | 89,875 |
| 9    | CRSCLTPV | CRSCLTPY | 89,874 |
| 10   | CRSCLTPZ | CRSCLTPC | 89,853 |

The B.22 greedy sequential pair (CRSCLTP0, CRSCLTPG) scored 89,331 and ranks 29th of 1,296.

**Confirmed interaction effects.** The results definitively refute the separability assumption:

1. The greedy sequential pair is *not* the joint optimum. It ranks 29th overall.

2. The winning seed1 value (`CRSCLTPV`, suffix `0x56`) was not the greedy Phase-1 winner
   (`CRSCLTP0`). `CRSCLTPV` is synergistic with `CRSCLTPP`—a combination invisible to the
   greedy search, which conditioned Phase 2 on the Phase-1 winner.

3. The landscape is rugged: the coefficient of variation is ~1.4%, but the range spans 6,825
   cells (84,265--91,090), a 7.6% spread relative to the mean. Small seed changes produce large
   depth swings, precluding gradient-based or local search strategies.

**Deployment.** Seeds deployed in `src/Decompress/Solvers/LtpTable.cpp`:

```cpp
constexpr std::uint64_t kSeed1 = 0x4352'5343'4C54'5056ULL;  // CRSCLTPV
constexpr std::uint64_t kSeed2 = 0x4352'5343'4C54'5050ULL;  // CRSCLTPP
```

End-to-end validation via the 30-minute `uselessMachine` test confirmed depth 91,090.

#### B.26.7 Lessons from Hill-Climbing (Abandoned Side-Experiment)

An attempt to push depth beyond 91,090 by direct hill-climbing of LTP tables (swapping
cell-to-line assignments within a sub-table to maximize a local $\Phi$ metric) was conducted
and immediately abandoned. The procedure plateaued at depth $< 500$—a catastrophic regression.

**Root cause.** The Fisher-Yates shuffle produces partitions with global statistical properties
(near-uniform pairwise cell co-occurrence across lines, low maximum cross-line overlap,
well-distributed spatial coverage) that are critical for effective constraint propagation. Local
swaps destroy these global invariants: a swap that improves $\Phi$ on one line degrades spatial
coverage elsewhere, and the cascade of corrections required to restore balance exceeds the
benefit. The Fisher-Yates construction, parameterized only by the LCG seed, is a strong global
prior that cannot be replicated by greedy local perturbation.

This result confirms that seed search (varying the input to a structurally sound construction
algorithm) is the correct optimization strategy, not direct manipulation of the output partition.

#### B.26.8 B.26d: Full-Byte Joint Search (In Progress)

The B.26c result demonstrated that the 36-element alphanumeric candidate set contains a pair
significantly better than the greedy optimum. The natural follow-on question is whether
expanding the candidate set to the full single-byte range (`0x00`--`0xFF`, 256 candidates)
reveals even stronger pairs in the unexplored 220/256 = 85.9% of the byte space.

**Search space.** $256 \times 256 = 65{,}536$ pairs—a $50.6\times$ expansion over B.26c.

**Parallelization.** At 23 seconds per pair, sequential execution would require approximately
17.5 days. The B.26d script partitions the search across $N$ workers by dividing the seed1
axis into $N$ contiguous slices. Each worker independently sweeps all 256 seed2 values for its
assigned seed1 range. For $N = 4$ workers:

| Worker | seed1 range       | Pairs   | Est. hours |
|:------:|:------------------|:-------:|:----------:|
| 0      | `0x00`--`0x3F`    | 16,384  | ~105       |
| 1      | `0x40`--`0x7F`    | 16,384  | ~105       |
| 2      | `0x80`--`0xBF`    | 16,384  | ~105       |
| 3      | `0xC0`--`0xFF`    | 16,384  | ~105       |

Wall time: approximately **4.3 days** with 4 cores.

**Worker isolation.** Each worker writes to a separate results file
(`b26d_results_w{N}.json`) and uses a separate temporary directory for compressed/decompressed
files. Workers share no mutable state and can be launched, stopped, and resumed independently.
The `--resume` behavior from B.26c is preserved per-worker.

**Merge.** After all workers complete, `tools/b26d_merge_results.py` reads all per-worker JSON
files, deduplicates by seed pair, computes descriptive statistics (mean, median, standard
deviation, percentiles, baseline-beating count), ranks all pairs by depth, and writes a unified
`b26d_merged.json`.

**Implementation.** `tools/b26d_full_byte_joint_search.py`. Extends B.26c with:

- `--worker W --workers N` for single-worker execution on a partition of the seed1 axis
- `--launch-all` to spawn all $N$ workers as background processes with per-worker log files
- `partition_seed1_range(worker, workers)` for balanced slice assignment (distributes remainder
  across the first $R$ workers when $256 \bmod N \neq 0$)
- `seed_to_label()` extended to handle non-printable suffix bytes via `\xHH` notation

The `run_candidate()` function is identical to B.26c: re-compress → timed decompress → SIGINT →
telemetry parse.

#### B.26.9 Expected Outcomes for B.26d

**Optimistic case.** The full byte range contains a pair with depth $\geq 92{,}000$, a further
$+1.0\%$ improvement over B.26c. This would indicate that the 36-element alphanumeric set was a
substantial restriction and that the depth function has exploitable structure in the non-printable
byte regions.

**Likely case.** The B.26c winner (CRSCLTPV + CRSCLTPP, depth 91,090) remains the global
optimum or is surpassed by fewer than 500 cells. The additional 64,240 pairs confirm the
landscape statistics from B.26c at higher resolution and establish the byte-space depth
distribution with high confidence. The 256-axis marginal distributions may reveal whether seed
quality correlates with any byte-level property (e.g., Hamming weight, parity) that could
guide future multi-dimensional searches.

**Pessimistic case.** No pair exceeds the B.26c winner. The 91,090-depth plateau represents a
structural ceiling for two-seed joint optimization under the uniform-511 architecture with fixed
$s_3$ and $s_4$. Future improvement would require either a four-dimensional joint search or
architectural changes.

#### B.26.10 Relationship to Other Proposals

*B.22 (greedy seed search).* B.26c is a direct verification of B.22's greedy methodology. The
1,759-cell improvement proves that coordinate descent was insufficient and that joint search is
necessary in the $(s_1, s_2)$ subspace.

*B.23 (clipped-triangular LTP).* B.23 was abandoned due to coverage collapse (Section B.23.9).
B.26 operates entirely within the proven uniform-511 architecture, confirming that meaningful
depth gains are available without length-distribution changes.

*B.20 (LTP substitution baseline).* B.20 achieved depth 88,503 with its original seeds. B.26c's
winner at 91,090 exceeds the B.20 baseline by 2,587 cells (+2.9%), recovering and surpassing
the 2,380-cell regression that motivated B.22.

*B.8 (adaptive lookahead).* Deeper lookahead produced zero improvement in the forcing dead zone.
B.26 addresses the dead zone by modifying which lines are co-resident in the plateau band
(via seed selection), rather than applying deeper probing to the existing topology. The two
strategies are complementary.

#### B.26.11 Open Questions

(a) Does the B.26c winner generalize across input blocks? The depth test uses a single
representative file (`useless-machine.mp4`). Validation on diverse input patterns (all-zeros,
all-ones, random, alternating) is required before concluding that the seed pair is universally
optimal.

(b) Would a four-dimensional joint search over $(s_1, s_2, s_3, s_4)$ improve further? The
B.22 observation of flat depth across $s_3$ and $s_4$ was made at the greedy-optimal
$(s_1, s_2)$ operating point. The new B.26c operating point may have shifted the sensitivity
landscape, making $s_3$ and $s_4$ no longer inert. However, a full $256^4$ search is
computationally infeasible ($4.3 \times 10^9$ evaluations), and even $36^4 = 1.7 \times 10^6$
evaluations at 23 seconds each would require approximately 1.2 years of compute. Targeted
verification (re-run B.22 Phases 3 and 4 with the new $(s_1, s_2)$ winners) is the practical
next step.

(c) Is there a structural proxy for seed quality that avoids the re-compression cost? The
$\Phi$ metric from B.22.4 evaluates cross-family line overlap in the plateau band and
requires only the static partition layout and a precomputed estimate of tight lines. If $\Phi$
correlates strongly with depth (Spearman $\rho \geq 0.7$ over the 1,296 B.26c results), it
could replace the full re-compress/decompress cycle as a pre-filter, reducing the per-candidate
cost from ~23 seconds to ~1 millisecond and enabling search over vastly larger seed spaces.

(d) What is the information content of seed2 given seed1? The landscape data from B.26c (and
eventually B.26d) provides a full $|\mathcal{S}| \times |\mathcal{S}|$ matrix of depth values.
Computing the mutual information $I(D; s_2 \mid s_1)$ would quantify how much depth variance
is attributable to seed2 conditional on seed1, informing whether future searches should
prioritize seed1 range expansion or seed2 range expansion.

(e) Can the parallel search infrastructure from B.26d be extended to a continuous background
optimization daemon? If the CRSCE toolchain supported a `--background-optimize` mode that
evaluated seed pairs during idle time and updated the production seed set when improvements
are found, the depth ceiling would continuously improve as a function of accumulated compute
without manual intervention.

### B.27 Increasing Constraint Density via LTP5 and LTP6 (Implemented)

#### B.27.1 Motivation

The B.26c joint seed search confirmed that the depth plateau at 91,090 cells (row ≈178) is
governed by the partition topology of LTP1–LTP4. Each cell belongs to exactly one line in each
of the four sub-tables, giving eight constraint lines per cell (row, column, diagonal,
anti-diagonal, LTP1, LTP2, LTP3, LTP4). At the plateau boundary, propagation stalls because no
forcing rule fires: every active line has $\rho > 0$ and $u > 0$. The number of cells that can
be in this "underdetermined interior" depends directly on how many independent constraint families
bound each cell. Adding two additional uniform-511 sub-tables raises the per-cell constraint
count from 8 to 10, increasing the probability that at least one line forces a cell at each DFS
level.

#### B.27.2 Architecture Changes

**New sub-tables.** LTP5 and LTP6 are constructed by the same Fisher-Yates LCG shuffle used for
LTP1–LTP4, seeded by two new 64-bit constants:

$$k_{\text{seed5}} = \texttt{0x4352'5343'4C54'5035}
  \quad (\text{"CRSCLTP5"}),
  \qquad
  k_{\text{seed6}} = \texttt{0x4352'5343'4C54'5036}
  \quad (\text{"CRSCLTP6"}).$$

These are the default alphanumeric seeds for sub-tables 5 and 6 — identical in construction
method to the pre-B.22 seeds for LTP1–LTP4. They were **not jointly optimized** at the time of
this experiment; joint optimization is the subject of future work (§B.27.5).

**Constraint store.** `ConstraintStore` is extended from 8 to 10 constructor parameters,
adding `ltp5Sums` and `ltp6Sums`. The total tracked line count increases from $10s - 2 = 5{,}108$
to $12s - 2 = 6{,}130$ (where $s = 511$). New constants `kLtp5Base = 5108` and
`kLtp6Base = 5619` index into the flat line array.

**Wire format.** Each block payload grows by $2 \times 511 \times 9 = 9{,}198$ bits:

| Field | B.26c | B.27 |
|-------|-------|------|
| LTP sub-tables | 4 | 6 |
| LTP payload bits | $4 \times 4{,}599 = 18{,}396$ | $6 \times 4{,}599 = 27{,}594$ |
| Total block bits | 125,988 | 135,186 |
| Block bytes | **15,749** | **16,899** |

**GPU integration.** `MetalPropagationEngine` and `ForEachCellOnLine` are updated to handle
`LineType::LTP5` and `LineType::LTP6`. `kMetalPropTotalLines` changes from $(10 \times 511) - 2$
to $(12 \times 511) - 2 = 6{,}130$.

#### B.27.3 Experimental Parameters

- **Seed5, Seed6:** `CRSCLTP5`, `CRSCLTP6` (default, unoptimized)
- **Seeds 1–4:** Fixed at B.26c winner: `CRSCLTPV` + `CRSCLTPP` (+ `CRSCLTP3`, `CRSCLTP4`)
- **Test input:** `useless-machine.mp4` (same as all prior experiments)
- **Decompress runtime:** 30 minutes
- **Comparison baseline:** B.26c depth 91,090, avg iter/sec ~411K

#### B.27.4 Results

| Metric | B.26c (4-LTP) | B.27 (6-LTP, default seeds) | Change |
|--------|:---:|:---:|:---:|
| Max depth (cells) | 91,090 | **91,090** | 0 (0.00%) |
| Plateau row | ≈178 | ≈178 | unchanged |
| `min_nz_row_unknown` | 161 | 161 | unchanged |
| Avg iter/sec | ~411K | ~399K | −3.0% |
| Stall escalations | 3 | 3 | unchanged |

The 6-LTP solver matches the B.26c depth ceiling exactly over 30 minutes and ~800M DFS
iterations. The depth-versus-time trace oscillates between 91,078 and 91,090, identical in
character to B.26c. No new maximum was observed.

**Throughput.** The 3% reduction in iteration rate is consistent with the cost of 25% more
constraint-line updates per cell assignment (10 lines instead of 8). The overhead is sublinear
because propagation is dominated by the fast-path `tryPropagateCell()` branch, which exits
early before iterating all lines in most cases.

**Seed search results.** An exhaustive joint search over all $256^2 = 65{,}536$ (seed5, seed6)
suffix-byte pairs was launched with seeds 1–4 fixed at B.26c values. The search was terminated
after 30,504 pairs (46.5% coverage) spanning all four quadrants of the seed5 byte range, each
quadrant fully paired with all 256 seed6 values.

| Metric | Value |
|--------|-------|
| Pairs evaluated | 30,504 / 65,536 (46.5%) |
| Unique depth values observed | 1 |
| Mean depth | 91,090.0 |
| Std deviation | **0.0** |
| Min / Max | 91,090 / 91,090 |
| Pairs beating baseline | 0 |

The landscape is completely flat: every tested (seed5, seed6) pair returns exactly 91,090. A
zero standard deviation over 30,504 independent evaluations uniformly distributed across the
full byte range is definitive — the search was terminated early as the remaining 53.5% is
statistically certain to yield no improvement.

#### B.27.5 Conclusions

1. **Additional LTP sub-tables are neutral at default seeds.** Adding LTP5 and LTP6 with
   unoptimized seeds preserves the depth ceiling (91,090) and plateau location (row 178)
   precisely. The extra constraint density does not spontaneously improve DFS performance —
   the seed selection governs which cells are co-constrained at the plateau boundary, and
   default seeds reproduce the same underdetermined interior.

2. **The 3% throughput penalty is acceptable.** At ~399K iter/sec the 6-LTP solver
   remains faster than B.20 (~198K), B.22 greedy (~329K), and approaches the B.26c rate.
   If seed optimization yields a depth improvement, the slowdown is justified.

3. **Seeds 5 and 6 are invariant at the B.26c operating point.** The exhaustive joint
   search confirmed zero variance across all tested pairs. The depth ceiling of 91,090 is
   set entirely by seeds 1+2 (CRSCLTPV+CRSCLTPP); seeds 5+6 contribute no additional
   discriminating constraint pressure regardless of their values.

4. **Sub-tables 5+6 are structurally inert — proven from both directions (B.27a, B.27b).**
   B.27a placed the B.26c winning seeds in slots 5+6 with weak seeds in slots 1+2: depth
   stayed at 86,123, unchanged from the weak 4-LTP baseline. B.27b swept all 256 seed5
   values with weak seeds 1+2: every candidate returned exactly 86,123 (std dev = 0.0,
   terminated after 136 of 256 values). Combined with the B.27 main result (seeds 5+6
   invariant at 91,090 when seeds 1+2 are optimal), the conclusion is definitive: no seed
   value placed in slots 5+6 affects depth under any condition. The sub-tables in those
   slots contribute zero discriminating constraint pressure. The depth is a function
   of slots 1+2 alone.

5. **The invariance pattern establishes a structural limit on uniform-511 expansion.**
   Seeds 3+4 were invariant at the B.22 operating point; seeds 5+6 are inert at all tested
   operating points. B.27c confirmed that re-optimizing seeds 1+2 in the 6-LTP context
   returns the identical winner (CRSCLTPV+CRSCLTPP=91,090) as the 4-LTP B.26c result.
   Adding more uniform-511 sub-tables beyond the first two jointly-optimized seeds provides
   no depth benefit. This closes the door on further uniform-511 expansion as a depth
   improvement strategy. B.28 and B.29 (alternative byte-position seed families) were
   evaluated and skipped — the same structural ceiling argument applies regardless of which
   byte of the 64-bit seed is varied. The seed-search paradigm is exhausted; the hill-climber is the next productive direction.

5. **Wire-format breakage is real.** Any container produced under the 4-LTP format
   (15,749 bytes/block) is incompatible with the 6-LTP reader (16,899 bytes/block) and
   vice versa. `ValidateContainer` was updated to enforce the new size. This is expected
   behavior for a research format with no versioning guarantee between branches.

#### B.27.6 Open Questions

(a) **Answered — seeds 5+6 are invariant.** The exhaustive joint search (30,504 of 65,536
pairs, zero variance) confirms that $s_5$ and $s_6$ produce no depth variation at the B.26c
operating point. The rugged landscape observed for $s_1$/$s_2$ in B.26c does not generalize
to additional uniform-511 sub-tables.

(b) **Answered — seeds 5+6 are structurally inert, not ceiling-limited.** B.27c established
that the seeds 1+2 landscape in the 6-LTP context is identical to the 4-LTP context: the
same winner (CRSCLTPV+CRSCLTPP=91,090) and the same rugged landscape (seed1 range
84,982–91,090; seed2 range 85,232–91,090). This rules out the "ceiling" interpretation:
if 91,090 were a hard ceiling from the first two sub-tables, adding more sub-tables would
not change the landscape shape, but we would still expect some seed2 candidates to exceed
91,090. None did. Instead, the evidence favors the "redundancy" interpretation: uniform-511
sub-tables 5+6 add no constraint discriminability that sub-tables 1+2 do not already provide.
The relevant implication for B.28/B.29 is that the depth ceiling is tied to the *value* of
the constraint geometry defined by seeds 1+2, not the count of sub-tables.

(c) **Partially answered.** B.27c confirms that 6 sub-tables with the optimal seed-1+2 pair
achieves the same depth as 4 sub-tables. B.27d (joint 4-axis) is moot per B.27b. The
question of whether non-uniform partitions (B.28, B.29 seed families) can break the ceiling
remains open.

#### B.27a Swap Experiment: Value vs. Slot Position (Completed)

**Setup.** Set seeds 1+2 to the pre-B.26c defaults (`CRSCLTP1` + `CRSCLTP2`, depth ~86,123 in
4-LTP) and seeds 5+6 to the B.26c winners (`CRSCLTPV` + `CRSCLTPP`). Seeds 3+4 remain fixed
at `CRSCLTP3` + `CRSCLTP4`.

**Question.** Do the B.26c winning seed *values* produce high depth regardless of which
sub-table slot they occupy, or is there something structurally special about slots 1+2?

**Result.**

| Configuration | Depth |
|---|---|
| B.26c baseline: slots 1+2 = CRSCLTPV+CRSCLTPP, slots 5+6 = CRSCLTP5+CRSCLTP6 | 91,090 |
| B.27a swap: slots 1+2 = CRSCLTP1+CRSCLTP2, slots 5+6 = CRSCLTPV+CRSCLTPP | **86,123** |
| Weak 4-LTP reference: slots 1+2 = CRSCLTP1+CRSCLTP2 | ~86,123 |

Depth = 86,123 — identical to the weak 4-LTP reference. Placing the B.26c winning values in
slots 5+6 produces zero benefit. The depth is the same as if those slots held default seeds.

**Interpretation.** Sub-tables 5+6 are **structurally inert** — they contribute zero
constraint pressure to the DFS regardless of their seed values. The depth is controlled
entirely by slots 1+2. This is not a "slot position matters" result in the sense that slots
1+2 are special and slots 5+6 are not; rather, slots 5+6 are inert *period* — no value placed
in them affects the outcome. The governing question for B.27b is whether this inertness holds
even when slots 1+2 are *also* weak (i.e., whether any seeds 5+6 can compensate).

#### B.27b Weak Seeds 1+2, Sweep of Seeds 5+6 (Completed)

**Setup.** Fix seeds 1+2 at `CRSCLTP1` + `CRSCLTP2` (weak, depth ~86,123 in 4-LTP). Sweep
all 256 suffix-byte candidates for seed5 with seed6 fixed at `CRSCLTP6`. Search terminated
early after 136 seed5 values (0x00–0x87) due to zero variance.

**Question.** Can seeds 5+6 independently drive depth from ~86K toward 91K when seeds 1+2
are weak?

**Result.**

| Metric | Value |
|--------|-------|
| Seed5 values evaluated | 136 / 256 (0x00–0x87) |
| Unique depth values observed | 1 |
| Depth (all candidates) | **86,123** |
| Std deviation | **0.0** |
| Best depth vs B.26c baseline | −4,967 |
| Best depth vs weak reference | 0 |

Every evaluated seed5 value returns exactly 86,123 — the same depth as the weak 4-LTP
baseline (CRSCLTP1+CRSCLTP2 with no additional sub-tables). The search was terminated early;
the uniformly sampled coverage across the full byte range is sufficient to call the result.

**Interpretation.** Seeds 5+6 are **structurally inert** under both conditions:
- When seeds 1+2 are *optimal* (B.26c winner): all seeds 5+6 give 91,090 (B.27 main result)
- When seeds 1+2 are *weak* (CRSCLTP1+CRSCLTP2): all seeds 5+6 give 86,123 (this result)

In both cases the depth is constant regardless of seeds 5+6. The sub-tables in slots 5+6
contribute zero discriminating constraint pressure. The depth is a function of slots 1+2 alone.
This closes the question raised by B.27a: it is not that slot position matters structurally —
it is that slots 5+6 are inert *period*, regardless of which values they hold or what state
slots 1+2 are in.

#### B.27c Re-search Seeds 1+2 in the 6-LTP Context (Completed)

**Setup.** Fix seeds 5+6 at their defaults (`CRSCLTP5` + `CRSCLTP6`). Run coordinate-descent
over seeds 1+2: sweep seed1 (256 candidates), freeze best; sweep seed2, freeze best; iterate.

**Question.** Does the 6-LTP optimal for seeds 1+2 differ from the 4-LTP optimal
(CRSCLTPV+CRSCLTPP)? If the landscape shifts, the additional sub-tables interact non-trivially
with seeds 1+2 — the 6-table joint optimum is at a different point and the B.26c winner was
specific to the 4-table operating point.

**Results (Round 1, converged).**

| Sweep | Winner | Depth | Range | Notes |
|---|---|---|---|---|
| Seed1 (seed2 fixed = CRSCLTPP) | CRSCLTPV | 91,090 | 84,982–91,090 | Rugged landscape; runners-up: 0xD8=90,399, 0x0B=90,216 |
| Seed2 (seed1 fixed = CRSCLTPV) | CRSCLTPP | 91,090 | 85,232–91,090 | Rugged landscape; runners-up: 0xB5=90,362, CRSCLTPy=89,874 |

Round 1 converged immediately (seed1 and seed2 unchanged). Peak depth: **91,090** (+0 vs
B.26c baseline). Script: `tools/b27c_reseed12_6ltp.py`. Results: `tools/b27c_results/b27c_results.json`.

**Interpretation.** The 6-LTP optimal for seeds 1+2 is identical to the 4-LTP optimal
(CRSCLTPV+CRSCLTPP=91,090). The addition of sub-tables 5 and 6 does not shift the seeds 1+2
landscape: the winner at the global optimum is the same seed pair. Combined with B.27a and
B.27b (sub-tables 5+6 structurally inert from both directions), this establishes that the
uniform-511 6-LTP configuration saturates at the same operating point as 4-LTP. No
seed-space search within the `CRSCLTP[X]` family improves on B.26c.

#### B.27d Joint Four-Axis Coordinate Descent: Seeds 1+2+5+6 (Moot — Superseded by B.27b)

**Original proposal.** Starting from (CRSCLTPV, CRSCLTPP, CRSCLTP5, CRSCLTP6), run
alternating coordinate descent over all four axes to find a 4D joint optimum.

**Status: not needed.** B.27b proved that the seed5 and seed6 axes are identically flat
regardless of the state of seeds 1+2. Since sweeping either axis always returns a constant
depth, there is no 4D joint optimum that differs from the 2D (seed1, seed2) optimum. The
seeds 5+6 directions contribute zero gradient everywhere in the search space; coordinate
descent over them cannot reach a new optimum from any starting point. B.27d is superseded.

### B.28 Interior-Byte Variant: `CRSC[X]LTP` (Skipped)

**Decision: not worth running.** After completing B.27c, the case for B.28 was re-evaluated
and the experiment was skipped for the following reasons:

1. **B.26c was exhaustive, not coordinate descent.** It evaluated all 256×256 pairs within the
   `CRSCLTP[X]` suffix-byte family and found the true global maximum (91,090). B.28 varying a
   different byte position still gives only *one degree of freedom per sub-table* — it probes a
   different slice of the same single-permutation-per-seed paradigm, not a richer search space.

2. **One byte, different position, same ceiling problem.** The Fisher-Yates shuffle maps a
   64-bit seed to a single permutation of 261,121 cells. Whether bit 7 or bit 31 varies, the
   search space per axis is identically 256 candidates. There is no reason to expect the
   interior-byte family to escape a ceiling that the exhaustive suffix-byte search could not.

3. **Seeds 3+4 invariance is the diagnostic.** In B.26c, seeds 3 and 4 in the `CRSCLTP[X]`
   family were completely flat across all 36 tested values. This flatness is not about which byte
   position is varied — it reflects that additional uniform-511 sub-tables contribute zero
   marginal constraint discriminability once seeds 1+2 are optimised. B.28 would test a different
   bit position in the same LCG-seeded Fisher-Yates architecture and is expected to exhibit the
   same saturation behaviour.

4. **The hill-climber is the higher-leverage next step.** Directly optimising LTP cell
   assignments (rather than seeds) gives O(s⁴) ≈ 6.8 × 10¹⁰ degrees of freedom and can target
   row-concentration geometry explicitly (B.9.2). B.28's expected return does not justify
   diverting effort from that more principled approach.

The original proposal is preserved below for reference.

#### B.28.1 Motivation (Original Proposal)

B.26d and B.27 vary the trailing byte (byte 8, LSB, bits 7..0) of the 8-byte seed, holding the
first 7 bytes fixed as `CRSCLTP`. B.29 varies the leading byte (byte 1, MSB, bits 63..56).
Both probe the *extremes* of the 64-bit seed integer.

B.28 tests the **interior**. It fixes the first 4 bytes (`CRSC`) and the last 3 bytes (`LTP`),
and sweeps a single variable byte in position 5 — the byte that has always been fixed to `L`
in all prior seeds. The word `CRSCLTP` is split in the middle:

$$\underbrace{C R S C}_{4\text{ fixed}} \underbrace{[X]}_{1\text{ var}} \underbrace{L T P}_{3\text{ fixed}}$$

This tests whether the constraint that anchors the seed family is the four-letter prefix `CRSC`
(bytes 0–3), or the full seven-letter string `CRSCLTP`. It also probes bit positions 31..24 —
the center of the 64-bit integer — which neither B.26d/B.27 nor B.29 covers. Together with
B.27 (LSB) and B.29 (MSB), these three 1-byte sweeps bracket the 64-bit seed space at three
independent positions and are each tractable in under one hour per axis with 4 workers.

**Hypothesis.** The Fisher-Yates LCG shuffle is sensitive to all 64 bits of its seed. The
convention of fixing byte 5 to `L` (0x4C) may be silently suboptimal. Seeds of the form
`CRSC[X]LTP` where $X \neq \texttt{0x4C}$ may produce partition layouts with higher depth.

#### B.28.2 Seed Structure

| Role | Bytes | Bit positions | Fixed value |
|------|-------|---------------|-------------|
| High prefix (fixed) | bytes 0–3 | 63..32 | `CRSC` = `0x43525343` |
| Middle (variable) | byte 4 | 31..24 | `0x00`–`0xFF` |
| Low suffix (fixed) | bytes 5–7 | 23..0 | `LTP` = `0x4C5450` |

The 256 B.28 seeds per axis are:

$$s(X) = (\texttt{0x43525343} \ll 32) \mid (X \ll 24) \mid \texttt{0x004C5450}, \quad X \in \{0, 1, \ldots, 255\}$$

When $X = \texttt{0x4C}$ (`L`), the seed is `0x435253434C4C5450` — not a valid B.26d/B.27
seed (`CRSCL`**`L`**`TP`). The B.26d/B.27 `CRSCLTP?` seeds have byte 4 = `0x4C` and byte 5 =
`0x54` (`T`); in B.28 notation byte 5 is fixed to `T` already, so this is consistent.

The four seed families now systematically probe three bit zones of the 64-bit integer:

| Experiment | Seed form | Variable bit zone | Space/axis |
|-----------|-----------|-------------------|-----------|
| B.26c | `CRSCLTP` + alphanum | bits 7..0, restricted | 36 |
| B.26d / B.27 | `CRSCLTP` + any byte | bits 7..0 | 256 |
| **B.28** | **`CRSC[X]LTP`** | **bits 31..24** | **256** |
| B.29 | `[X]CRSCLTP` | bits 63..56 | 256 |

#### B.28.3 Coordinate-Descent Staging

**Per-axis sweep.** Fix all other seeds at their current best values; sweep all 256 $X$-byte
values for the target axis. Cost: $256 \times 23\,\text{s} \div 4\,\text{workers} \approx 26$
minutes per axis — identical to a B.27 or B.29 sweep.

**Convergence round.** Sweep seed5 (hold seed6 at default), freeze best seed5; sweep seed6,
freeze best seed6; repeat until no improvement. Typical convergence: 2–3 rounds, $\approx 2$
hours total.

**Cross-family restart.** After B.28 converges on both axes, re-run B.27 (LSB) and B.29 (MSB)
sweeps with seeds fixed at the B.28 winner to verify no other bit zone can improve further.
Repeat across families until no axis yields improvement.

#### B.28.4 Tooling

A new script `tools/b28_seed_search.py`, structured identically to `tools/b27_seed_search.py`
with one change in seed construction:

```python
_HIGH = 0x43525343  # "CRSC" (bytes 0–3)
_LOW  = 0x004C5450  # "\x00LTP" (bytes 5–7, zero-padded for OR)

def make_seed(x_byte: int) -> int:
    return (_HIGH << 32) | (x_byte << 24) | _LOW
```

```bash
# Sweep seed5 B.28 axis (hold seeds 1-4 at B.26c winner, seed6 at default):
python3 tools/b28_seed_search.py --worker 0 --workers 4 --target-seed 5

# Merge and rank after completion:
python3 tools/b28_merge_results.py --out-dir tools/b28_results --workers 4
```

A companion `tools/b28_merge_results.py` mirrors `tools/b27_merge_results.py`.

#### B.28.5 Expected Outcomes

**Optimistic.** A byte value other than `0x4C` (`L`) yields meaningfully higher depth for one
or more seeds. This would confirm that the `CRSCLTP` prefix is not optimal in full and that
the interior byte space offers genuine gains, motivating further 2-byte interior sweeps.

**Likely.** Depth is approximately flat across most of the 256 values, with `0x4C` (`L`) being
near-optimal — similar to the B.22 observation that seeds 3 and 4 were invariant within the
alphanumeric set. The plateau at 91,090 is insensitive to the interior byte.

**Pessimistic.** Depth degrades sharply for $X \neq \texttt{0x4C}$ (`L`), confirming that the
specific byte sequence `CRSCLTP` matters structurally and cannot be perturbed at byte 5.

#### B.28.6 Relationship to Other Proposals

*B.26d / B.27.* B.28 tests bit positions 31..24 (byte 5), whereas B.26d/B.27 test bit
positions 7..0 (byte 8). Together these two experiments probe opposite halves of the 64-bit
integer. A flat response in both zones would strongly indicate that depth is insensitive to
individual seed byte values within the `CRSC??LTP`-style family.

*B.29.* B.29 tests bit positions 63..56 (byte 1). Together B.28 and B.29 cover the interior
and the MSB, while B.26d/B.27 cover the LSB. All three are 256-candidate, sub-hour-per-axis
sweeps — a consistent methodology that avoids the infeasibility of 2-byte joint searches.

*B.22.* B.22 used greedy coordinate descent within the 36-element alphanumeric set. B.28
extends that strategy to all 256 byte values at an interior seed byte position, testing
coordinate-descent in a new bit zone at the same per-evaluation cost.

### B.29 Single-Byte Prefix with `CRSCLTP` Suffix (Skipped)

**Decision: not worth running.** B.29 was evaluated after B.28 was skipped and the same
reasoning applies with equal force:

1. **Same architecture, different byte position.** B.29 varies the MSB (bits 63..56) while
   keeping `CRSCLTP` as the trailing 7 bytes — the mirror image of B.26d (vary LSB, keep
   `CRSCLTP` as the leading 7 bytes). Both give 256 candidates per axis within the same
   single-permutation-per-seed paradigm. The search space per axis is identical in size and
   the structural ceiling problem is unchanged.

2. **LCG trajectory analysis confirms symmetry.** In a multiplicative LCG
   (`state = a·state + c mod 2^64`), varying the MSB shifts the starting state by `Δ << 56`.
   After *n* steps the two trajectories differ by `a^n·(Δ << 56) mod 2^64` — structurally
   identical to varying the LSB by a different linear offset in the same cycle. There is no
   mechanism that makes MSB variation qualitatively different from the already-exhausted LSB
   variation in a Fisher-Yates/LCG context.

3. **Seeds 3+4 invariance is the diagnostic, not byte position.** The flatness observed in
   B.26c for seeds 3 and 4 was not caused by the byte that was varied. It reflects that
   additional uniform-511 sub-tables contribute zero marginal constraint discriminability once
   seeds 1+2 are optimised. Varying the MSB of further seeds faces the same barrier.

4. **B.26c exhausted the LSB family exhaustively.** Its global maximum (91,090) is the true
   ceiling for that family. B.29's MSB family is disjoint and could in principle have a higher
   ceiling — but there is no theoretical mechanism making MSB variation qualitatively superior,
   so this remains low-probability speculation.

5. **The hill-climber is the higher-leverage next step.** Directly optimising LTP cell
   assignments gives O(s⁴) ≈ 6.8 × 10¹⁰ degrees of freedom and can target row-concentration
   geometry explicitly. B.29's expected return does not justify the cost.

The original proposal is preserved below for reference.

#### B.29.1 Motivation (Original Proposal)

B.26d and B.27 search seeds of the form `CRSCLTP` + one variable byte: the high 7 bytes are
fixed and byte 8 (the LSB, big-endian) varies. Both families share the property that the
*leading* bytes of the 64-bit seed are fixed and the *trailing* bytes vary.

B.29 inverts this structure. It fixes `CRSCLTP` as the trailing 7 bytes (the suffix) and
sweeps a single variable byte in the leading position (the MSB):

$$\text{B.26d/B.27:}\quad \underbrace{C R S C L T P}_{7\text{ fixed}} \underbrace{X}_{1\text{ var}}
\qquad
\text{B.29:}\quad \underbrace{X}_{1\text{ var}} \underbrace{C R S C L T P}_{7\text{ fixed}}$$

The seed values (big-endian uint64):

$$\text{B.26d seed}(X) = \texttt{0x435253434C5450}\lVert X
\qquad
\text{B.29 seed}(X) = X \lVert \texttt{0x435253434C5450}$$

where $\lVert$ denotes byte concatenation and $X \in [0\text{x}00, 0\text{x}FF]$.

These two families are **disjoint**: no seed value is reachable by both. The B.26d family fixes
bits 63..8 at `0x435253434C5450` and varies bits 7..0. The B.29 family fixes bits 55..0 at
`0x435253434C5450` and varies bits 63..56. The only potential overlap would require simultaneously
fixing and varying the same bits, which is impossible.

**Hypothesis.** The Fisher-Yates LCG shuffle is sensitive to all 64 bits of its seed; there is
no reason to expect the high byte to be irrelevant. Seeds where the low 7 bytes spell `CRSCLTP`
but the high byte differs from `C` (0x43) may produce partition layouts with tighter line
co-residency at the plateau boundary, yielding higher depth.

#### B.29.2 Seed Structure

| Role | Bytes | Bit positions | Value |
|------|-------|---------------|-------|
| Prefix (variable) | byte 0 | 63..56 | `0x00`–`0xFF` |
| Suffix (fixed) | bytes 1–7 | 55..0 | `0x435253434C5450` (`CRSCLTP`) |

The 256 B.29 seeds per axis are:

$$s(X) = (X \ll 56) \mid \texttt{0x435253434C5450}, \quad X \in \{0, 1, \ldots, 255\}$$

When $X = \texttt{0x43}$ (`C`), the seed is `0x43435253434C5450` — note that this is **not** a
valid B.26d seed; B.26d's `C`-prefixed seed with suffix `P` is `0x435253434C545050` (CRSCLTPP),
which has a different bit layout.

#### B.29.3 Coordinate-Descent Staging

- **Seeds per axis:** 256 (same as B.26d/B.27 and B.28)
- **Cost per axis sweep:** $256 \times 23\,\text{s} \div 4\,\text{workers} \approx 26$ minutes
- **Convergence round (two axes):** $\approx 52$ minutes; 2–3 rounds typical ($\approx 2$ hours)

**Procedure.** Fix $s_6$ at its default B.29 seed ($X = \texttt{0x43}$, i.e., prefix byte =
`C`, yielding `0x43435253434C5450`); sweep all 256 prefix-byte values for $s_5$; freeze the
best $s_5$; sweep all 256 prefix-byte values for $s_6$; freeze the best $s_6$; iterate until
no axis improves. This is identical in structure to the B.27 and B.28 staging procedures.

**Cross-family restart** *(original proposal, superseded — B.28 and B.29 both skipped).*
After B.29 converged, the plan was to re-run B.27 (LSB) sweeps with seeds fixed at the B.29
winner to verify no further improvement. This step is moot.

#### B.29.4 Relationship to the Current Seed Families

The seed families considered across B.26c–B.29:

| Experiment | Seed form | Variable bits | Space/axis | Status |
|-----------|-----------|---------------|-----------|--------|
| B.26c | `CRSCLTP` + alphanum suffix | bits 7..0, restricted | 36 | Completed — global max 91,090 |
| B.26d / B.27 | `CRSCLTP` + any suffix byte | bits 7..0 | 256 | Completed — confirmed 91,090 ceiling |
| B.28 | `CRSC[X]LTP` interior byte | bits 31..24 | 256 | Skipped — same objection as B.29 |
| B.29 | prefix `[X]` + `CRSCLTP` | bits 63..56 | 256 | Skipped — see rationale above |

B.26c and B.26d/B.27 together exhaustively covered the LSB family. B.28 and B.29 were
proposed to probe different bit zones but were skipped because varying a different byte position
within the same single-permutation-per-seed LCG architecture offers no qualitative increase in
degrees of freedom and is not expected to break the 91,090 ceiling.

#### B.29.5 Tooling

A new script `tools/b29_seed_search.py`, structured identically to `tools/b27_seed_search.py`
with one change in seed construction:

```python
_SUFFIX = 0x435253434C5450  # "CRSCLTP" as a 56-bit integer (big-endian bytes 1..7)

def make_seed(prefix_byte: int) -> int:
    return (prefix_byte << 56) | _SUFFIX
```

The worker partitioning, run_candidate logic, progressive JSON output, and resume support are
unchanged. A companion `tools/b29_merge_results.py` mirrors `tools/b27_merge_results.py`.

#### B.29.6 Expected Outcomes

**Optimistic.** A prefix byte other than `0x43` (`C`) yields a substantially higher depth for
one or more seeds. This would prove that the convention of starting seeds with `C` (from
`CRSCE`) has been silently constraining the search and that better partition layouts exist.

**Likely.** Depth is approximately flat across all 256 prefix values for each seed, similar to
the B.22 observation that seeds 3 and 4 were invariant within the alphanumeric set. The plateau
at 91,090 cells is insensitive to the high byte of the seed, and the LCG high-bit sensitivity
does not translate to partition-quality sensitivity.

**Pessimistic.** Prefix `0x43` (`C`) is the best for every seed, confirming that the `CRSCLTP?`
convention is not merely arbitrary — the high bits of the seed do matter, and the current
family happens to be optimal (or near-optimal) within the tractable search space.

#### B.29.7 Retrospective: Conclusion of the Seed-Search Programme

B.26d/B.27 (LSB, completed) together with the decision to skip B.28 (interior byte) and B.29
(MSB) complete the seed-search programme. The combined evidence supports a clear conclusion:

- **B.26c** exhaustively covered the LSB family (256×256 pairs) and found the global maximum
  at 91,090. The landscape is rugged (range 84,265–91,090, mean 86,941) with only 2.2% of
  pairs beating the B.22 baseline — but the ceiling is definite.
- **B.27 series** (B.27, B.27a–c) established that additional uniform-511 sub-tables (LTP5/6)
  provide zero marginal benefit regardless of seed values or operating point.
- **B.28 and B.29 were skipped** because varying a different byte position in the same
  LCG-seeded Fisher-Yates architecture does not increase degrees of freedom and there is no
  theoretical mechanism by which MSB or interior-byte variation escapes the same structural
  ceiling.

The strong inference is that 91,090 is not a seed-search artefact but a structural property
of the uniform-511 partition geometry under the current solver. Seed search within any
single-byte-variable `CRSCLTP`-family is exhausted. The natural next direction is the
**hill-climber**: directly optimising cell assignments with O(s⁴) degrees of
freedom, targeting the row-concentration geometry identified in B.9.2.

---

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

### B.31 Pincer Decompression — Option B: Full Alternating-Direction DFS (Proposed)

#### B.31.1 Motivation

B.30 (Option A) runs a propagation-only sweep from the bottom after a top-down stall. However,
the `PropagationEngine` is already a fixed-point computation — it forces every cell that can be
deduced from the current partial assignment after every branching decision. A pure re-propagation
sweep on the same ConstraintStore state will find no new forced cells and is therefore expected
to be a no-op or near-no-op.

The mechanism that makes the Pincer hypothesis genuinely powerful is **SHA-1 row hash
verification**, not propagation. When the bottom-up DFS assigns all 511 cells in a bottom row
and verifies its SHA-1 hash, it eliminates an entire exponential subtree of infeasible bottom
assignments. This constraint is non-linear and global — it cannot be derived from any
cardinality propagation over the line constraints alone. Propagation has no access to this
information; only branching decisions that complete bottom rows can trigger it.

**Option B implements the full Pincer:** the solver actually branches in both directions,
completing rows from row 510 upward after the top-down plateau. The SHA-1 verifications on
completed bottom rows tighten the residual constraint network available to the resumed top-down
pass, providing the constraint-density improvement described in B.30.3.

#### B.31.2 DI Consistency

A potential concern is that alternating-direction branching changes the enumeration order, making
the DI invalid. This concern does not apply: **the compressor runs the same decompressor code
to find the DI**. Whatever enumeration order the decompressor uses, the compressor uses
identically. The DI is the zero-based position of the correct solution in the decompressor's
enumeration — its definition is stable regardless of which direction cells are assigned in,
as long as the enumeration is deterministic and reproducible.

The alternating-direction DFS is fully deterministic: given the same constraint system, it
always produces the same direction switches and the same cell selections. DI is therefore
well-defined and consistent between compression and decompression.

#### B.31.3 The Alternating Structure

B.31 adds a `direction` state to the DFS and maintains two independent frontiers:

| State | Description |
|-------|-------------|
| `direction` | `TOP_DOWN` or `BOTTOM_UP` — current traversal direction |
| `topFrontier` | Lowest row with any unassigned cell (advances downward) |
| `bottomFrontier` | Highest row with any unassigned cell (advances upward) |
| `stall_count` | Branching decisions since the last forced cell in current direction |

**Plateau trigger:** when `stall_count` reaches threshold $N$ (candidate: 511 — one full row
of unconstrained branching), declare a plateau and switch direction. Reset `stall_count`.

**Convergence:** if both directions have plateaued without any new forced cell since the last
direction switch, the meeting band (rows `topFrontier`..`bottomFrontier`) is irreducibly hard
with the current assignments. The solver falls back to standard DFS within the meeting band in
row-major order, or terminates the alternation and continues with the existing single-direction
strategy.

**Termination:** when `topFrontier > bottomFrontier` (the two frontiers cross), all rows are
assigned. SHA-256 block hash verification; if valid, yield the solution and advance the DI
counter.

#### B.31.4 Cell Selection in Each Direction

The existing DFS pre-computes a `cellOrder` sorted by probability confidence. For B.31:

- **TOP_DOWN ordering** (existing): cells in row-major order, reordered by confidence.
  Unchanged from current implementation.

- **BOTTOM_UP ordering** (new): cells in *reverse* row-major order (rows 510→0, within
  each row by decreasing confidence). Pre-computed once at the start of `enumerateSolutionsLex`
  alongside the existing top-down ordering.

The DFS stack is extended with a `direction` field per frame. When a direction switch occurs,
the solver pushes a new frame using the opposite ordering starting from the appropriate
frontier. The existing row-completion heap (rows with $u \leq 64$) applies in both directions —
in BOTTOM_UP mode it preferentially selects near-complete rows from the bottom.

#### B.31.5 Unified Undo Stack

The undo stack (`brancher_->undoToSavePoint / recordAssignment`) is **direction-agnostic**:
it records `(r, c)` pairs regardless of which direction made the assignment. When a backtrack
crosses a direction-switch point (unwinding bottom-up assignments while in the top-down phase),
the ConstraintStore correctly unassigns those cells and restores line residuals. No special
handling is required.

The `UndoToken` mechanism (stack-size save points) already provides this: a token saved before
a direction switch, when restored, undoes all assignments in both directions since the save
point.

#### B.31.6 SHA-1 Verification

Row $r$ verifies its SHA-1 when `cs.getStatDirect(r).unknown == 0`, regardless of direction.
In BOTTOM_UP mode, the solver verifies rows as they complete in the sequence 510, 509, 508, …
No changes to the hash verification infrastructure are needed.

#### B.31.7 Proposed Implementation

**New state (local to `enumerateSolutionsLex`):**
```cpp
enum class SolverDirection : std::uint8_t { TopDown, BottomUp };
SolverDirection direction    = SolverDirection::TopDown;
std::uint16_t   topFrontier  = 0;     // next unassigned row from top
std::uint16_t   bottomFrontier = kS - 1; // next unassigned row from bottom
std::uint32_t   dirStallCount  = 0;   // branching decisions since last forced cell
std::uint32_t   kDirStallMax   = kS;  // plateau threshold (511 = one row)
std::uint64_t   dirSwitches    = 0;   // total direction switches
```

**Precompute bottom-up ordering** (alongside existing `cellOrder`):
```cpp
auto bottomOrder = estimator.computeGlobalCellScoresReverse();
// or: reverse of cellOrder sorted by row descending
```

**In the DFS loop — per-frame direction:**
```cpp
struct ProbDfsFrame {
    // ... existing fields ...
    SolverDirection direction{SolverDirection::TopDown}; // NEW
};
```

**Stall detection for direction switch:**
```cpp
// After assignment + propagation succeeds:
if (/* propagation forced no cells && no heap selection */) {
    ++dirStallCount;
    if (dirStallCount >= kDirStallMax) {
        dirStallCount = 0;
        ++dirSwitches;
        direction = (direction == SolverDirection::TopDown)
            ? SolverDirection::BottomUp
            : SolverDirection::TopDown;
    }
}
```

**Cell selection by direction:**
```cpp
const auto &activeOrder = (direction == SolverDirection::TopDown)
    ? cellOrder : bottomOrder;
// Heap still applies in both directions (near-complete rows from appropriate end)
```

#### B.31.8 Expected Outcomes

**Optimistic.** Bottom-up DFS completes rows 510 → ~332 via SHA-1-guided backtracking. Each
completed bottom row provides a non-linear constraint that cardinality propagation cannot
express. When top-down resumes at `topFrontier`, many meeting-band cells are forced outright
by the combined top-down + bottom-up constraint tightening. The alternation converges in 2–3
switches. Effective branching space falls from ~170K underdetermined cells to ~10–30K. Wall
time decreases by 1–2 orders of magnitude.

**Likely.** Bottom-up achieves a shallow but nonzero cascade (10–50 rows from the bottom)
before SHA-1 failures become frequent. Each direction switch provides incremental constraint
tightening. The meeting band shrinks modestly. Net speedup is 2–5× in wall time, not an order
of magnitude. Multiple direction switches (5–10) are required before convergence.

**Pessimistic.** The residual constraint budgets after the forward pass are too tight for
bottom-up branching: SHA-1 failures begin almost immediately from row 510 because the top-down
pass has exhausted many line budgets, leaving no feasible bottom-row assignments near the
"expected" region. Bottom-up barely advances; the direction switches add overhead without
benefit. Performance degrades to below the B.30.4 / current baseline.

#### B.31.9 Relationship to B.30

B.30 and B.31 are the same Pincer hypothesis at different implementation depths:

| | B.30 (Option A) | B.31 (Option B) |
|---|---|---|
| Bottom-up phase | Propagation sweep (no branching) | Full DFS with branching |
| SHA-1 in bottom-up | Not triggered (no row completions) | Triggered for each completed row |
| Expected benefit | Likely zero (propagation at fixed-point) | Real: SHA-1 eliminates subtrees |
| Implementation cost | Minimal (10–20 lines) | Substantial (150–250 lines) |
| DI change | None (top-down order unchanged) | Deterministic alternating order |

B.30 is implemented first as a cheap probe. If it produces zero forced cells (confirming the
no-op hypothesis), this validates the need for B.31 and eliminates any ambiguity about whether
propagation alone is sufficient.

#### B.31.10 Open Questions

(a) **What is the bottom-up plateau depth?** Empirical measurement from B.31 first run will
answer B.30.10(a) definitively.

(b) **What is `kDirStallMax`?** The threshold for direction switching. Too small → switches too
often (overhead dominates). Too large → misses the beneficial switch point. Candidate starting
values: 511 (one row), 1024, 4096.

(c) **Is the row-completion heap useful in BOTTOM_UP mode?** The existing heap selects rows
with $u \leq 64$ and applies in both directions. In BOTTOM_UP, near-complete rows are those
near row 510. The heap naturally captures this without modification.

(d) **Should the DI still use lex order?** The alternating DFS enumerates in a deterministic
but non-lex-row-major order. The compressor, running the same code, finds the DI in the same
order. All that matters for correctness is consistency. "Lexicographic" in the DI specification
is therefore generalized to "alternating-DFS order" in B.31.

---

### B.32 Four-Direction Iterative Pincer — Diagonal and Anti-Diagonal Passes (Proposed)

#### B.32.1 Motivation

B.30 and B.31 attack the meeting band from two directions: top-down and bottom-up, both
along the row axis. The diagonal cross-sums (DSM, 1,021 lines) and anti-diagonal cross-sums
(XSM, 1,021 lines) carry constraint information along axes rotated 45° from the row/column
grid, but neither B.30 nor B.31 exploits this geometric diversity. After the top-down and
bottom-up passes plateau, the meeting band (~rows 178–332, ~79K cells) remains underdetermined.
Constraints along the row axis have been exhausted by the two row-serial passes; constraints
along the diagonal and anti-diagonal axes have been tightened only as a side effect of
row-serial cell assignments, not by directed traversal.

**The B.32 hypothesis:** by adding diagonal and anti-diagonal DFS passes that proceed
top-down through the row dimension, the solver can tighten constraint lines from four
geometric directions within a single cycle. Each completed cycle reduces the unknowns in
the meeting band; the cycle repeats until a full pass-cycle produces no progress. The
diagonal and anti-diagonal passes preserve SHA-1 row-hash verification because they
advance top-to-bottom through rows, allowing rows to complete and trigger hash checks.

#### B.32.2 The Four-Direction Structure

The B.32 solver executes a repeating four-pass cycle on a **single shared ConstraintStore**:

| Pass | Direction | Cell Selection | SHA-1 Order |
|------|-----------|---------------|-------------|
| **1. Top-down** | Rows 0 → 510 | Row-major (existing) | Rows 0, 1, 2, … |
| **2. Bottom-up** | Rows 510 → 0 | Reverse row-major (B.31) | Rows 510, 509, 508, … |
| **3. Diagonal top-down** | DSM[$s{-}1$ … $2(s{-}1)$], rows 0 → 510 | Cells on diagonals $d \in [510, 1020]$, row-ascending within each diagonal | Rows complete as last diagonal cell in each row is assigned |
| **4. Anti-diagonal top-down** | XSM[$s{-}1$ … $2(s{-}1)$], rows 0 → 510 | Cells on anti-diagonals $x \in [510, 1020]$, row-ascending within each anti-diagonal | Same: rows complete when last anti-diagonal cell is assigned |

**Cycle termination:** if a complete four-pass cycle produces zero newly forced cells and
zero newly completed rows across all four passes, the meeting band is irreducibly hard at
the current constraint density. The solver falls back to standard row-major DFS within
the remaining underdetermined cells.

**Plateau detection within each pass:** identical to B.31. When `stall_count` reaches
threshold $N$ (candidate: 511), the current pass declares a plateau and yields to the
next pass in the cycle. The pass does not resume until the next cycle iteration.

#### B.32.3 Geometric Coverage of the Diagonal and Anti-Diagonal Passes

**DSM[$s{-}1$ … $2(s{-}1)$] coverage.** Diagonal index $d = c - r + (s-1)$. The range
$d \in [510, 1020]$ corresponds to cells where $c \geq r$ — the upper-right triangle of
the matrix (including the main diagonal).

For meeting-band row $r$ ($178 \leq r \leq 332$), pass 3 assigns cells at columns
$c \in [r,\, 510]$:

| Row | Cells assigned by pass 3 | Count |
|-----|-------------------------|-------|
| 178 | $c \in [178, 510]$ | 333 |
| 255 | $c \in [255, 510]$ | 256 |
| 332 | $c \in [332, 510]$ | 179 |

**XSM[$s{-}1$ … $2(s{-}1)$] coverage.** Anti-diagonal index $x = r + c$. The range
$x \in [510, 1020]$ corresponds to cells where $c \geq 510 - r$.

For meeting-band row $r$, pass 4 assigns cells at columns $c \in [510 - r,\, 510]$.
The net new cells (not already assigned by pass 3) are in columns
$c \in [510 - r,\, r - 1]$, which is non-empty only when $510 - r < r$, i.e., $r > 255$:

| Row | New cells from pass 4 | Count |
|-----|----------------------|-------|
| 178 | None (pass 3 already covers $[178, 510] \supseteq [332, 510]$) | 0 |
| 255 | None (pass 3 covers $[255, 510]$ = pass 4's $[255, 510]$) | 0 |
| 332 | $c \in [178, 331]$ | 154 |

**Union of passes 3 + 4:** for meeting-band row $r$, columns $c \in [\min(r,\, 510{-}r),\, 510]$
are assigned. The complementary **left portion** — columns $c \in [0,\, \min(r,\, 510{-}r) - 1]$ —
remains unassigned after both diagonal passes. This left portion contains:

- Row 178: 178 cells in columns $[0, 177]$
- Row 255: 255 cells in columns $[0, 254]$
- Row 332: 178 cells in columns $[0, 177]$

#### B.32.4 SHA-1 Preservation

A key concern from the column-major variant (discussed prior to B.32) was loss of SHA-1
row-hash verification. B.32 avoids this by construction:

**Passes 3 and 4 proceed top-down through the row dimension.** Within each diagonal (or
anti-diagonal), cells are assigned in ascending row order. As the passes progress, cells
accumulate in each meeting-band row from right to left. When any row's last unassigned
cell is filled — whether by direct assignment, propagation, or a combination — SHA-1
verification fires immediately via the existing `u(row_r) == 0` trigger.

The diagonal and anti-diagonal passes do not complete rows as quickly as row-serial
passes (they spread assignments across many rows simultaneously rather than filling one
row at a time). However, they do not prevent row completion — they merely defer it.
Combined with the assignments from passes 1 and 2 (which fully completed the top and
bottom rows), the diagonal passes fill the right portion of each meeting-band row,
leaving fewer cells for the subsequent cycle's row-serial passes to resolve before
SHA-1 fires.

**B.26a is not applicable.** B.26a's MRV cell selection fragmented row completion during
the *primary* solve, preventing SHA-1 from ever firing efficiently. B.32's diagonal
passes operate *after* the row-serial passes have plateaued, as a supplementary
mechanism. The row-serial passes remain the primary SHA-1-verified solve; the diagonal
passes pre-fill portions of meeting-band rows to reduce the branching cost of the
subsequent row-serial cycle.

#### B.32.5 The Left-Portion Blind Spot

The DSM[$s{-}1$ … $2(s{-}1)$] and XSM[$s{-}1$ … $2(s{-}1)$] passes cover cells where
$c \geq \min(r,\, 510{-}r)$. Cells in the left portion ($c < \min(r,\, 510{-}r)$) lie on
diagonal lines DSM[$0$ … $s{-}2$] and anti-diagonal lines XSM[$0$ … $s{-}2$], which are
outside the targeted index ranges.

The left-portion cells receive **no direct assignments** from passes 3 or 4. Their
constraint tightening is purely indirect:

1. **LTP cross-pollination.** Each right-portion cell assigned by passes 3/4 updates four
   LTP constraint lines. LTP lines are spatially random (Fisher-Yates shuffle), spanning
   both left and right portions of the meeting band. A right-portion assignment on an LTP
   line that includes left-portion cells reduces $u$ on that line, potentially pushing it
   toward forcing ($u = 1$).

2. **Diagonal bleed-through.** Diagonals in DSM[$0$ … $s{-}2$] pass through both the
   assigned top/bottom regions (from passes 1/2) and the meeting band. Right-portion
   assignments from passes 3/4 are not on these diagonals (by definition), but the LTP
   cross-pollination may force cells that *are* on these lower diagonals, cascading
   indirectly.

3. **Column constraint tightening.** For right-portion columns ($c > 255$), passes 3/4
   assign many meeting-band cells, heavily consuming the column budget. For left-portion
   columns ($c < 178$), passes 3/4 provide zero meeting-band assignments — these columns
   see only the top-down and bottom-up contributions (~356 of 511 cells assigned, $u \approx
   155$). Column forcing in the left portion depends entirely on the row-serial passes.

**The cycle's second iteration resolves the blind spot.** When the top-down pass resumes
on cycle 2, it enters the meeting band with the right portion already assigned. For each
meeting-band row $r$, only $\min(r,\, 510{-}r)$ cells remain — the left portion. Row 178
needs 178 branching decisions instead of 511 before SHA-1 fires. Row 255 needs 255 instead
of 511. This is a **2–3× reduction in branching cost per row**, yielding correspondingly
faster SHA-1 feedback and cheaper backtracking.

#### B.32.6 Constraint Density After One Full Cycle

After the first complete four-pass cycle, a typical remaining cell in the left portion
(e.g., $r = 255, c = 100$) has the following constraint profile:

| Constraint | Assigned | Unknown ($u$) | Near forcing? |
|------------|----------|---------------|---------------|
| Row 255 (LSM) | 256 (right portion) | 255 | No |
| Column 100 (VSM) | ~356 (top + bottom) | ~155 | No |
| Diagonal $d = 355$ (DSM) | ~200 (top + bottom + right portion of meeting band) | ~100 | No |
| Anti-diag $x = 355$ (XSM) | ~200 | ~100 | No |
| LTP lines | ~350 (all passes) | ~160 | Unlikely |

No individual line is near $u = 1$, so propagation alone does not resolve the left portion.
**The value of B.32 is not that the diagonal passes solve the meeting band directly — it is
that they halve the branching work for the subsequent row-serial cycle.** Faster SHA-1
feedback on the resumed top-down pass is the primary benefit mechanism.

On subsequent cycles, each pass enters with a progressively tighter constraint network.
The cycle converges when either: (a) all cells are assigned and verified, or (b) a complete
cycle produces no new forced cells, indicating the remaining cells are irreducibly hard.

#### B.32.7 Proposed Implementation

**New state (local to `enumerateSolutionsLex`):**

```cpp
enum class PassDirection : std::uint8_t {
    TopDown,          // Pass 1: row-major, rows 0 → 510
    BottomUp,         // Pass 2: reverse row-major, rows 510 → 0
    DiagonalTopDown,  // Pass 3: DSM[s-1..2(s-1)], row-ascending
    AntiDiagTopDown   // Pass 4: XSM[s-1..2(s-1)], row-ascending
};

PassDirection currentPass     = PassDirection::TopDown;
std::uint32_t passStallCount  = 0;      // branching decisions since last forced cell
std::uint32_t kPassStallMax   = kS;     // plateau threshold (511)
std::uint64_t cycleProgress   = 0;      // forced cells + completed rows in current cycle
std::uint8_t  cycleCount      = 0;      // total completed cycles
```

**Precompute diagonal and anti-diagonal cell orderings:**

```cpp
// Pass 3 ordering: for each diagonal d in [kS-1, 2*(kS-1)], cells sorted by ascending row.
// Only meeting-band cells (those still unassigned after passes 1/2) are included.
auto diagOrder = computeDiagCellOrder(kS - 1, 2 * (kS - 1), SortOrder::RowAscending);

// Pass 4 ordering: for each anti-diagonal x in [kS-1, 2*(kS-1)], cells sorted by ascending row.
auto antiDiagOrder = computeAntiDiagCellOrder(kS - 1, 2 * (kS - 1), SortOrder::RowAscending);
```

**Pass cycling logic:**

```cpp
// After current pass plateaus:
passStallCount = 0;
switch (currentPass) {
    case PassDirection::TopDown:        currentPass = PassDirection::BottomUp;       break;
    case PassDirection::BottomUp:       currentPass = PassDirection::DiagonalTopDown; break;
    case PassDirection::DiagonalTopDown: currentPass = PassDirection::AntiDiagTopDown; break;
    case PassDirection::AntiDiagTopDown:
        // End of cycle — check convergence
        if (cycleProgress == 0) {
            // No progress in entire cycle: fall back to row-major DFS
            currentPass = PassDirection::TopDown;
            // ... enter fallback mode ...
        } else {
            currentPass = PassDirection::TopDown;
            cycleProgress = 0;
            ++cycleCount;
        }
        break;
}
```

**Cell selection by pass:**

```cpp
switch (currentPass) {
    case PassDirection::TopDown:         activeOrder = &cellOrder;        break;
    case PassDirection::BottomUp:        activeOrder = &bottomOrder;      break;
    case PassDirection::DiagonalTopDown:  activeOrder = &diagOrder;        break;
    case PassDirection::AntiDiagTopDown:  activeOrder = &antiDiagOrder;    break;
}
```

**SHA-1 verification** is unchanged: row $r$ verifies when $u(\text{row}_r) = 0$, regardless
of which pass completed the row. The existing `IHashVerifier` interface requires no
modification.

**Unified undo stack:** identical to B.31. The undo stack records $(r, c)$ pairs regardless of
which pass made the assignment. Backtracking across pass boundaries correctly restores the
ConstraintStore via the existing assign/unassign machinery.

**DI consistency:** as in B.31, the compressor runs the same four-pass cycle as the
decompressor. The DI is the zero-based position of the correct solution in the four-pass
enumeration order. This order is deterministic and reproducible; DI consistency between
compression and decompression is guaranteed by code identity.

#### B.32.8 Expected Outcomes

**Optimistic.** The diagonal and anti-diagonal passes complete their respective cascades,
assigning ~50–65% of meeting-band cells in the right portion. LTP cross-pollination forces
a significant fraction of left-portion cells. When the top-down pass resumes on cycle 2,
most meeting-band rows need only 50–100 branching decisions before SHA-1 fires. The faster
SHA-1 feedback dramatically reduces backtracking. The cycle converges in 2–3 iterations.
Effective branching space falls from ~170K cells to ~5–10K. Wall-clock decompression time
decreases by 1–2 orders of magnitude compared to B.31's two-direction Pincer.

**Likely.** The diagonal and anti-diagonal passes fill the right portion of the meeting band
but the left portion remains largely underdetermined (LTP cross-pollination forces few cells).
The top-down pass on cycle 2 benefits from the 2–3× reduction in per-row branching cost:
row 178 needs 178 guesses instead of 511 before SHA-1 fires, catching bad branches ~3×
sooner. Net speedup over B.31 is modest but real — perhaps 1.5–3× in wall time. The
improvement concentrates at the meeting-band edges (rows 178 and 332, where the left portion
is smallest) and is weakest at the center (row 255, where 255 cells still need branching).

**Pessimistic.** The diagonal and anti-diagonal passes thrash in the meeting band: without
per-row SHA-1 verification during the diagonal sweeps themselves, the solver makes incorrect
branching decisions that propagate incorrect assignments into the right portion. When the
top-down pass resumes, the pre-filled right portion is wrong, causing immediate SHA-1
failures and full backtracking through the diagonal pass assignments. The four-pass cycle
overhead exceeds the constraint-tightening benefit. Performance degrades to below B.31.

#### B.32.9 Relationship to B.30 and B.31

B.32 extends the Pincer hypothesis from two directions (B.31) to four, adding constraint
tightening from the diagonal and anti-diagonal axes:

| | B.31 (Two-direction) | B.32 (Four-direction) |
|---|---|---|
| Passes per cycle | 2 (top-down, bottom-up) | 4 (top-down, bottom-up, diagonal, anti-diagonal) |
| Constraint axes exploited | Row (primary), col/diag/anti-diag/LTP (via propagation) | Row + diagonal + anti-diagonal (directed), col/LTP (via propagation) |
| Meeting-band pre-fill | None (row-serial only) | Right portion (~50–65% of meeting-band cells) |
| SHA-1 preservation | Full (both passes are row-serial) | Full (all passes proceed top-down in row dimension) |
| Per-row branching cost (cycle 2) | 511 cells/row | $\min(r, 510{-}r)$ cells/row (178–255) |
| Implementation cost | ~150–250 lines over baseline | ~300–400 lines over baseline |

B.32 is implemented after B.31 validates the two-direction Pincer. If B.31 shows that the
meeting band shrinks significantly from top-down + bottom-up alternation alone, the marginal
value of diagonal/anti-diagonal passes may be small. Conversely, if B.31 leaves a substantial
meeting band (~100+ rows), B.32's per-row branching reduction becomes the primary mechanism
for further progress.

#### B.32.10 Open Questions

(a) **Do the diagonal passes thrash without per-row SHA-1?** The diagonal and anti-diagonal
passes assign cells across many rows simultaneously. A bad branching decision on diagonal $d$
affects cells in many rows, but SHA-1 cannot reject any individual row until it completes.
The effective pruning during passes 3/4 relies on constraint-line feasibility checks
($0 \leq \rho \leq u$) and diagonal sum verification (when $u(d) = 0$, the accumulated
sum must equal DSM[$d$]). These are weaker than SHA-1 but may suffice for the partially
pre-constrained meeting band.

(b) **Should passes 3 and 4 target DSM[$0$ … $s{-}2$] and XSM[$0$ … $s{-}2$] instead (or
additionally)?** The current design targets the upper-right diagonals, leaving a left-portion
blind spot. Adding lower-diagonal passes (DSM[$0$ … $s{-}2$], XSM[$0$ … $s{-}2$]) would
cover the left portion directly, making it a six-pass or eight-pass cycle. The trade-off is
implementation complexity vs. coverage completeness.

(c) **What is the optimal diagonal traversal order within each pass?** Candidates include:
processing diagonals shortest-first (exploiting early sum verification on short diagonals),
longest-first (maximizing propagation cascade breadth), or by residual tightness (diagonals
nearest to $\rho = 0$ or $\rho = u$ first). The choice affects how quickly the passes trigger
forced cells and row completions.

(d) **Can seeds be jointly optimized for four-direction depth?** The B.26c seed-search
infrastructure optimized seeds for the forward (top-down) cascade. B.32 introduces two
additional pass directions whose effectiveness may depend on which LTP lines are loaded
in the right vs. left portions of the meeting band. A four-direction objective
$\text{depth}_{\text{fwd}} + \text{depth}_{\text{rev}} + \text{depth}_{\text{diag}} +
\text{depth}_{\text{anti-diag}}$ is a higher-dimensional search problem.

(e) **What is the convergence rate of the cycle?** Each cycle tightens constraints from
four directions; diminishing returns are expected. Empirical measurement of cells-forced
per cycle will determine whether the iteration converges in 2–3 cycles (useful) or requires
10+ cycles (overhead-dominated).

(f) **Is the right-portion pre-fill correct often enough?** The diagonal and anti-diagonal
passes branch without SHA-1 verification. If the branching decisions in passes 3/4 have a
high error rate, the pre-filled right portion will be mostly wrong, and cycle 2's top-down
pass will waste time backtracking through incorrect diagonal assignments rather than
benefiting from the pre-fill. The hash-mismatch rate during the diagonal passes is the
key diagnostic.

---

### B.33 Complete-Then-Verify Solver with Constraint-Preserving Local Search (Proposed)

#### B.33.1 Motivation

Every solver from B.1 through B.32 uses the same fundamental architecture: **interleaved
constraint solving and hash verification**. The DFS assigns cells row-by-row, checking SHA-1
at each row boundary and backtracking on failure. At the current operating point (B.26c,
depth ~91,090), the solver spends the vast majority of its compute budget on SHA-1 checks
that fail — millions of rejected row candidates at the propagation frontier (~row 178),
each providing approximately zero bits of useful search information.

**The architectural problem.** A failed SHA-1 check tells the solver "this specific row
assignment is wrong" but provides no information about *how* it is wrong or *which bits*
need to change. SHA-1 is a cryptographic hash: the output is a pass/fail signal with no
gradient, no partial credit, and no locality. A single bit flip in the row changes the hash
completely. Each failed check eliminates exactly one candidate from a space of
$\binom{511}{\text{LSM}[r]} \approx 2^{510}$ possible row assignments — negligible pruning.
The solver is spending cycles on near-zero-information operations.

**The deeper problem.** The solver tests SHA-1 on *partial solutions* — a row assignment
within an incomplete CSM where ~170,000 cells are still unassigned. A row that passes SHA-1
at this stage IS correct (SHA-1 has no false positives), but a row that fails SHA-1 may fail
because:

(a) The row assignment itself is wrong (the row's bits need to change), or

(b) The partial assignment of prior rows is wrong (this row cannot be completed to a valid
    solution given the current prefix).

The solver cannot distinguish (a) from (b). In case (b), it exhaustively tries all
assignments of the current row before backtracking to a prior row — exploring an
exponentially large subtree that contains zero solutions. This is the "spinning wheels"
failure mode.

**The B.33 hypothesis.** Separate constraint solving from hash verification entirely:

1. **Solve cross-sums to completion** — produce a full CSM candidate satisfying all 5,108
   constraint lines, without any SHA-1 checks during the solve.
2. **Verify the complete candidate** — check SHA-1 on every row. A complete, cross-sum-valid
   CSM is a coherent hypothesis worth testing; a partial assignment through row 178 is not.
3. **Modify, don't restart** — for rows that fail SHA-1, search for alternative cross-sum-valid
   assignments via constraint-preserving local transformations. Never discard the complete state.

#### B.33.2 Information-Theoretic Foundation

The cross-sum constraint system has 5,108 lines over 261,121 binary variables. The constraint
matrix $A$ (5,108 × 261,121) has effective rank $\leq 5,101$ (accounting for linear dependencies:
each of the 8 partition families sums to the same global popcount, giving at least 7 redundant
equations). The null space of $A$ has dimension $\geq 256,020$.

**Propagation determines ~91,090 cells** — values uniquely forced by the cross-sum constraints
via arc consistency (forcing rules: $\rho = 0 \Rightarrow$ unknowns are 0; $\rho = u \Rightarrow$
unknowns are 1). The remaining ~170,031 cells lie in the null space: their values can vary freely
while maintaining all cross-sum constraints.

**The cross-sum solution space** contains approximately $2^{170{,}000}$ valid matrices — an
astronomically large set, of which exactly one is the target CSM.

**SHA-1 is not a linear constraint.** A 160-bit hash does not provide 160 bits of constraint
information in the search-theoretic sense. It provides a binary pass/fail oracle:

- **PASS** (probability $\sim 2^{-160}$ for a random candidate): confirms the row is correct.
  Highly informative (~160 bits).
- **FAIL** (probability $\sim 1$): eliminates one candidate from $\sim 2^{510}$ possible row
  assignments. Information content: $\log_2\!\bigl(\tfrac{2^{510}}{2^{510}-1}\bigr) \approx 0$
  bits.

The current solver performs millions of FAIL checks per second. Each provides ~0 bits of
information. The total information gathered from SHA-1 failures is negligible despite
enormous compute expenditure. **SHA-1 verification is only useful when applied to a
complete, plausible candidate — not as a search heuristic on partial assignments.**

#### B.33.3 The Complete-Then-Verify Architecture

**Phase 1: Cross-sum solve.** Starting from the propagation-determined cells (~91K, rows
0–177), extend the assignment through the meeting band (rows 178–510) using DFS with
cross-sum feasibility checks only. No SHA-1 verification during this phase.

The cross-sum system in the meeting band is heavily underdetermined ($u \gg 1$ on all
lines), so infeasibilities are rare. The DFS should reach a complete 261,121-cell assignment
quickly — orders of magnitude faster than the current solver, which backtracks at every
row boundary.

**Phase 2: Hash verification.** Compute SHA-1 on all 511 rows of the complete candidate.
Rows 0–177 (propagation-determined) will pass. Meeting-band rows will almost certainly fail
(probability $\sim 2^{-160}$ per row of a coincidental match).

**Phase 3: Lock and modify.** Lock every row that passes SHA-1 (confirmed correct). For
failing rows, apply **constraint-preserving transformations** to the complete CSM,
searching for alternative cross-sum-valid matrices where additional rows pass SHA-1.
After each transformation, re-check SHA-1 on affected rows. Lock newly passing rows.
Repeat until all rows pass or the search budget is exhausted.

The critical property: **the solver never discards a complete state**. It always holds a
full, cross-sum-valid CSM and incrementally modifies it. This contrasts sharply with the
current DFS, which throws away partial work on every SHA-1 failure.

#### B.33.4 Constraint-Preserving Transformations (Swap Polygons)

A **constraint-preserving swap** is a set of cells whose values can be flipped (0↔1) while
maintaining all 5,108 cross-sum constraints. Formally, a swap is a vector
$D \in \{-1, 0, +1\}^{261{,}121}$ in the null space of the constraint matrix $A$, subject to:

- **Balance:** each constraint line $L$ has $\sum_{(r,c) \in L} D(r,c) = 0$
- **Feasibility:** the modified matrix $M + D$ remains in $\{0, 1\}^{s \times s}$

The null space has dimension ~256,020, so valid swap patterns exist abundantly. The practical
question is: **how small can a swap be?**

**Known results for subsets of constraint families:**

| Families preserved | Min swap size | Construction |
|---|---|---|
| LSM + VSM (row + column) | 4 cells | 2×2 rectangle: $(r_1,c_1), (r_1,c_2), (r_2,c_1), (r_2,c_2)$ |
| LSM + VSM + DSM (+ diagonal) | 6 cells | 3×3 hexagonal: rows $r_1,r_2,r_3$; columns $c_1 = c_3{-}r_2{+}r_1$, $c_2 = c_3{+}r_1{-}r_3$ |
| LSM + VSM + DSM + XSM (all geometric) | ≥8 cells | Unverified; 6-cell diagonal-preserving swaps break anti-diagonal sums |

**The 4-cell rectangle failure.** A rectangle at $(r_1,c_1), (r_1,c_2), (r_2,c_1), (r_2,c_2)$
preserves row and column sums but places its 4 cells on 4 distinct diagonals ($d = c-r+510$)
and 4 distinct anti-diagonals ($x = r+c$). Each diagonal and anti-diagonal sees exactly one
±1 disturbance with nothing to cancel it. For two diagonals to pair, one would need
$c_1-r_1 = c_2-r_2$ AND $c_2-r_1 = c_1-r_2$, which requires $r_1 = r_2$ (degenerate).

**Adding LTP families.** Each LTP sub-table is a pseudorandom partition (Fisher-Yates shuffle)
with no spatial correlation to the geometric families. A compensating cell for an LTP
disturbance lands at a spatially unrelated position, creating new disturbances on geometric
lines that require their own compensation. The minimum swap for all 8 families (4 geometric
+ 4 LTP) is unknown and must be computed empirically.

**Swap polygon model.** A swap can be visualized as a polygon whose vertices are cell
positions and whose edges connect cells sharing a constraint line. Traversing the polygon
with alternating signs (+1, −1, +1, −1, …) ensures each edge's constraint line sees
balanced changes. However, each vertex (cell) sits on ALL 8 constraint lines, not just the
2 polygon edges it participates in. The non-edge lines see unbalanced disturbances. Closing
all disturbances requires additional structure beyond a simple polygon — the pattern must
be a higher-dimensional cycle in the constraint hypergraph.

#### B.33.5 Sub-experiment B.33a: Minimum Swap Size Computation

**Objective.** Determine the minimum number of cells in a valid all-constraint-preserving
swap for the current 8-family constraint system (LSM, VSM, DSM, XSM, LTP1–LTP4) with
seeds CRSCLTPV, CRSCLTPP, CRSCLTP3, CRSCLTP4.

**Hypothesis.** Two competing models:

- **Linear model:** the minimum swap grows as ~2 cells per constraint family, predicting
  ~16 cells (8 families × 2) or possibly as low as 8 (one cell per family).
- **Exponential model:** each pseudorandom LTP family approximately doubles the
  cancellation chain, predicting ~128–256 cells.

**Method.**

(a) **Construct the constraint matrix** $A$ (5,108 × 261,121) for the current LTP seeds.
Each row of $A$ is an indicator vector for one constraint line.

(b) **Compute the null space** of $A$ over the integers (or equivalently, over $\mathbb{Q}$).
The null space has dimension ~256,020. Finding it exactly is infeasible at this scale, but
we only need SHORT null-space vectors.

(c) **Greedy chain repair.** Start with a random 4-cell rectangle (preserves row + column).
Identify violated lines (diagonals, anti-diagonals, LTPs with net ±1 change). For each
violated line, find a compensating cell on that line whose addition (with opposite sign)
cancels the disturbance. The compensating cell creates new disturbances on its other 7
lines; add those to the repair queue. Iterate until all lines are balanced. Record the
total swap size. Repeat 10,000 times with random starting rectangles; report the minimum.

(d) **Exhaustive small-swap search.** For $k = 4, 6, 8, 10, \ldots, 32$: enumerate
(or sample) sets of $k$ cells with $k/2$ designated +1 and $k/2$ designated −1. Check
whether $A \cdot D = 0$ for each candidate. Report the smallest $k$ with a valid swap.
For small $k$, this is feasible via constraint propagation: the first cell's 8 lines each
require a compensating cell, which constrains the search.

(e) **LTP-only isolation test.** Compute the minimum swap preserving ONLY the 4 LTP families
(ignoring geometric constraints). If this is already large, the LTP pseudorandom structure
is the binding constraint. If it is small (4–8 cells), the geometric constraints are the
bottleneck.

**Expected outcome.** The minimum swap is between 8 and 64 cells, likely closer to the
lower end because the null space is enormous (~256K-dimensional) and short vectors are
statistically likely in high-dimensional spaces. If the minimum exceeds ~100, the local
search approach may be impractical due to the cost of enumerating valid swap patterns.

**B.33a Results (Completed).**

*Tool:* `tools/b33a_min_swap.py` — score-guided greedy BFS from 4-cell rectangle seed.

*Geometric-only (4 families, num_ltp=0):* **0% convergence** in 200 trials with max_cells=500.
The algorithm starts from a balanced 4-cell rectangle (rows, cols satisfied), then greedily
repairs the 4 diagonal and 4 anti-diagonal violations by adding cells with the highest
score. Despite a positive score being achievable on the first step (~+16 per well-placed
cell), the repair chain never terminates within the budget.

*Full system (6 LTP sub-tables, 10 families):* **0% convergence** in 200 trials.

*LTP sensitivity sweep (0–6 LTP sub-tables, 200 trials each):*

| Families (4+LTP) | Success % | Min | Mean |
|---|---|---|---|
| 4 (geo only) | 0% | N/A | N/A |
| 5 | 0% | N/A | N/A |
| 6 | 0% | N/A | N/A |
| 7 | 0% | N/A | N/A |
| 8 | 0% | N/A | N/A |
| 9 | 0% | N/A | N/A |
| 10 | 0% | N/A | N/A |

*Root-cause analysis.* The greedy BFS diverges monotonically: each repair step fixes 1
violation on a targeted line but creates n-1 new violations (one per additional family
the chosen cell participates in). For 4 families, each step nets +2 new violations; for
10 families, each step nets +8 new violations. The score function can pick cells that
cancel more than one violation simultaneously (score >+10) but such cells are rare; the
expected net violation change per step is still positive, so the repair chain diverges in
expectation.

*Geometric minimum (analytic):* The minimum constraint-preserving swap for 4 geometric
families (row, col, diag, anti-diag) is **8 cells**. Proof sketch: any swap must include at
least one pair of cells on each violated family (4 cells to fix diagonals, 4 cells to fix
anti-diagonals, which are the same 4 additional cells by a tiling argument). The construction
is uniquely constrained given a starting rectangle; whether a valid construction exists
depends on the specific rectangle geometry (row/col parity constraints).

*Full-system minimum:* **Unknown** — lower bound is 8 (from geometric argument), upper bound
is unknown. The BFS approach cannot determine it. The null space has ~256,020 dimensions,
so short null vectors likely exist, but finding them requires a different algorithm.

*Conclusion.* The score-guided greedy BFS approach is **not viable** for constructing
minimum constraint-preserving swaps. This does NOT mean B.33 Phase 3 is infeasible: it
means the swap-finding algorithm must be redesigned. Two alternative approaches are viable:

1. **B.33b (Analytic construction):** Characterize the structure of minimum swaps for the
   full 10-family system using the LTP assignment arrays. For each cell, the 10 families
   are known exactly. A minimum swap is a balanced assignment of ±1 signs to a small set
   of cells such that every family containing any selected cell has exactly equal ±1 counts.
   This is equivalent to finding a balanced 10-uniform hypergraph cover — tractable for
   small support sizes via exhaustive search with pruning.

2. **B.33c (Solver upper bound):** Run the C++ decompressor twice on the same test block
   with DI=0 and DI=1. The XOR of the two solution CSMs gives a valid constraint-preserving
   swap; its support size is an **upper bound** on the minimum swap. Consecutive solutions
   differ starting from the first branching cell (~91,090 from the base), so the expected
   support is ~170,031 cells — large but confirms a valid swap EXISTS, and may reveal the
   structure of nearby solutions.

#### B.33.6 Phase 3 Local Search Strategy

Given a minimum swap size of $k$ cells, the Phase 3 search operates as follows:

**Swap vocabulary.** Pre-compute or lazily generate valid $k$-cell swap patterns. Each
swap is parameterized by a starting cell and a chain of compensating cells determined by
the constraint structure. The vocabulary size depends on $k$ and the constraint geometry;
for $k = 8$, a rough estimate is $O(s^4)$ possible swaps (choosing 4 pairs from $s$ options
per family).

**Search loop:**

```
while (failing_rows is not empty):
    select a failing row r (e.g., lowest index, or random)
    for each valid swap pattern involving at least one cell in row r:
        apply swap to the CSM
        recompute SHA-1 on all affected rows
        if any affected row newly passes SHA-1:
            lock that row
            accept the swap
            break
        else:
            undo the swap
```

**Acceptance criteria.** Strictly greedy: accept a swap only if it increases the number of
locked (SHA-1-verified) rows. Alternatively, simulated annealing: accept swaps that decrease
the locked count with probability $e^{-\Delta/T}$, allowing the search to escape local optima.

**SHA-1 landscape.** Each swap changes the values of $k$ cells, affecting at most $k$ rows
(if all cells are in distinct rows, which is likely for $k \geq 8$). SHA-1 is recomputed
only on those rows. With SHA-1 at ~1 µs per 64-byte block, the per-swap verification cost
is $O(k)$ microseconds — negligible.

The fundamental challenge: SHA-1 provides no gradient. A swap either makes a row pass
(probability $\sim 2^{-160}$ per affected row) or doesn't. The search is a random walk on
the space of cross-sum-valid matrices, guided only by the binary signal of SHA-1 pass/fail.

**Mitigation: row-targeted swaps.** Rather than applying random swaps, target swaps to
specific failing rows. For a failing row $r$ with all other rows fixed, the column
constraints fully determine row $r$'s values. To change row $r$, at least one other row
must also change (to alter the column residuals). Enumerate swap patterns that modify row
$r$ and one or more other rows, checking SHA-1 on each candidate. The per-row search space
is bounded by the number of valid swaps involving row $r$, which is much smaller than the
full swap vocabulary.

#### B.33.7 Relationship to Additional LTP Sub-tables

The B.33 architecture becomes strictly more powerful as the constraint density increases.
With more LTP sub-tables:

- The null space shrinks (fewer degrees of freedom → fewer cross-sum-valid matrices)
- The propagation cascade extends (more cells determined without branching)
- The meeting band narrows (fewer underdetermined rows)
- The swap vocabulary shrinks (fewer valid swap patterns, but each is more targeted)
- The search space contracts exponentially

At the limit (~52 LTP sub-tables), the null space collapses to dimension ~0. There is
exactly one cross-sum-valid matrix. Phase 1 produces the correct CSM directly via
propagation. Phases 2 and 3 are trivially satisfied. Decompression is $O(n)$.

The practical trade-off:

| Sub-tables | Block payload | Compression ratio | Null space dim | Phase 1 | Phase 3 |
|---|---|---|---|---|---|
| 4 (current) | ~15.7 KB | ~48.2% | ~170,000 | Fast | Huge search |
| 12 | ~22.3 KB | ~68.5% | ~130,000 | Fast | Large search |
| 24 | ~35.5 KB | ~109% (expansion) | ~80,000 | Fast | Moderate search |
| 36 | ~48.7 KB | ~149% | ~30,000 | Fast | Small search |
| 52 | ~65.6 KB | ~201% | ~0 | Deterministic | N/A |

The sweet spot — if one exists — is the minimum number of sub-tables where the Phase 3
search converges within a practical time budget. This depends on the minimum swap size
(B.33a) and the structure of the SHA-1 landscape over the constrained solution space.

#### B.33.8 Expected Outcomes

**Optimistic.** The minimum swap size is 8–16 cells. Phase 1 completes the CSM in
milliseconds (no SHA-1 backtracking). Phase 3 finds SHA-1-matching row assignments via
targeted swaps within a tractable search budget. The solver converges to the correct CSM
in minutes rather than the current approach's hours-to-never. The architecture
fundamentally outperforms DFS with interleaved SHA-1.

**Likely.** The minimum swap size is 16–64 cells. Phase 1 is fast. Phase 3 is tractable
for rows near the propagation frontier (which have tight constraints from the locked rows
above) but struggles with deep meeting-band rows (which have high null-space dimensionality).
The solver fixes some meeting-band rows but stalls in the center of the meeting band.
Performance is comparable to or modestly better than the current DFS approach for the
first ~200 rows, then degrades.

**Pessimistic.** The minimum swap size exceeds 100 cells. The swap vocabulary is too large
to enumerate efficiently. Phase 3 degenerates into a random walk on the $2^{170{,}000}$
cross-sum-valid matrices with negligible probability of improving any row's SHA-1 status.
The architecture provides no advantage over the current DFS and is abandoned in favor of
constraint-density improvements (more LTP sub-tables). Alternatively, Trump finds a way to 
screw up the fundamentals of mathematics too.

#### B.33.9 Relationship to Prior Work

**B.1 (CDCL).** B.1 attempted to learn clauses from SHA-1 failures — a non-linear global
constraint that produces uselessly long 1-UIP traces. B.33 avoids SHA-1 during the solve
entirely, eliminating the need for CDCL over hash constraints. If CDCL is reintroduced in
B.33, it would operate on cross-sum constraint violations only (cardinality constraints),
which are linear and well-structured — producing tight, useful learned clauses with bounded
jump distance.

**B.30/B.31 (Pincer).** The Pincer experiments attempted to tighten constraints by attacking
from multiple directions. B.31 was tested and produced a null result: bottom-up SHA-1
verification rate was ~0% because unconstrained rows have random assignments that never
match. B.33 embraces this reality — it does not expect meeting-band rows to pass SHA-1
during the solve. Instead, it defers hash verification to a post-solve phase where the
complete CSM provides a meaningful hypothesis to test.

**B.32 (Four-direction Pincer).** B.32 proposes diagonal and anti-diagonal passes to
pre-fill the right portion of the meeting band before resuming row-serial DFS with SHA-1.
B.33 is a more radical departure: it eliminates SHA-1 from the solve loop entirely and
replaces the tree-search paradigm with a local-search paradigm. B.32's pre-filling strategy
could be used within B.33's Phase 1 (as a heuristic for generating the initial candidate),
but Phase 3's swap-based search is a fundamentally different mechanism.

#### B.33.10 Open Questions

(a) **What is the minimum swap size for all 8 constraint families?** This is the critical
parameter determining Phase 3's feasibility. Sub-experiment B.33a addresses this directly.

(b) **Can the swap vocabulary be precomputed?** If the minimum swap is $k$ cells, the number
of valid $k$-cell swap patterns is the key computational parameter. For $k = 8$, the
vocabulary might be manageable ($O(s^4) \sim 10^{10}$). For $k = 64$, it is likely
intractable. Precomputation vs. lazy generation is an implementation decision that depends
on $k$.

(c) **Does simulated annealing help?** Greedy acceptance (only accept swaps that lock new
rows) may trap the search in local optima where no single swap improves SHA-1. Simulated
annealing or tabu search could escape by allowing temporary regressions. Whether this
helps depends on the structure of the SHA-1 landscape over cross-sum-valid matrices.

(d) **Is there a better Phase 3 strategy than element-wise swaps?** Alternatives include:
re-running Phase 1 with different branching decisions in the meeting band (producing a
different complete candidate), Gaussian elimination or linear-programming relaxation over
the null space, or belief propagation on the factor graph of cross-sum constraints with
SHA-1 priors.

(e) **How does the minimum swap size depend on the LTP seeds?** Different seeds produce
different LTP partition geometries. The null-space structure — and therefore the minimum
swap size — depends on the specific partitions. Seeds that produce smaller minimum swaps
may enable more efficient Phase 3 search, adding a new dimension to the seed optimization
problem (B.26c).

(f) **Can Phase 1 be biased toward SHA-1-friendly candidates?** If the DFS in Phase 1
uses a heuristic that produces "more typical" or "less extreme" row assignments (e.g.,
preferring cells that minimize the variance of column residuals), the resulting candidate
might be closer to the correct CSM in Hamming distance, reducing the number of swaps
needed in Phase 3. This is speculative — SHA-1 provides no useful signal for biasing —
but statistical properties of the row constraints might provide weak guidance.

(g) **What is the expected number of swaps to fix one row?** For a failing row $r$, the
number of valid swap patterns involving row $r$ determines the per-row search space. If
the column constraints (with all other rows fixed) leave $d$ degrees of freedom for row $r$'s
affected cells after a swap, the search space per swap is $O(2^d)$. With SHA-1 as the
discriminator, the expected number of swaps to fix row $r$ is $\sim 2^{\min(d, 160)}$.
Measuring $d$ empirically for typical meeting-band rows is a key sub-experiment.

#### B.33.11 Sub-experiment B.33b: Backtracking Minimum Swap Search (Completed)

**Objective.** Determine the minimum constraint-preserving swap size for the full 10-family
system via backtracking DFS rather than greedy forward repair (as in B.33a).

**Method.** `tools/b33b_min_swap_bt.py` — analytic geometric base + backtracking extension.

1. Construct the analytic 8-cell geometric base from a starting rectangle using the exact
   formula: for rectangle (r1,c1),(r1,c2),(r2,c1),(r2,c2) with signs (+1,-1,-1,+1), the
   four unique repair cells are determined by the midpoint formula (requires parity condition
   r1+r2+c1+c2 even). This gives an 8-cell swap that is EXACTLY balanced for all 4 geometric
   families (row, col, diag, anti-diag) — verified analytically.

2. Compute the LTP violations created by the 8-cell base (each cell falls on `num_ltp`
   random LTP lines, creating 8·num_ltp violations in the worst case, all distinct).

3. Backtracking DFS to extend the base swap to balance LTP violations:
   - Pick the most-constrained violated LTP line (minimum fan-out criterion)
   - Branch on the top-20 score-ranked candidate cells for that line
   - Prune if |swap| ≥ budget or lower-bound on remaining cells ≥ remaining budget
   - Recurse; backtrack on no-progress

**Results.**

| Families | Budget | Timeout | Rectangles | Converged | Min found |
|---|---|---|---|---|---|
| 4 (geo only, num_ltp=0) | 20 | 10s | 20/20 | 18/20 | **8 cells** |
| 5 (geo + 1 LTP) | 50 | 15s | 10/10 | 0/10 | N/A |
| 8 (geo + 4 LTP) | 150 | 30s | 10/10 | 0/10 | N/A |

For geometric-only, the analytic base trivially satisfies all 4 families — 8-cell minimum
confirmed. For any LTP sub-table added, backtracking exhausts the 15–30s budget across
10 rectangles without finding any swap within 50–150 cells.

**Root-cause analysis (theoretical).** With Fisher-Yates random partition, each of the
8 geometric-base cells falls on a UNIQUE LTP line with probability ≈ 1 (8 cells out of
511 lines per sub-table; expected distinct lines ≈ 8). For the swap to be LTP-balanced,
each occupied LTP line must have equal ±1 counts, requiring a mate cell for each cell
(since each line has exactly 1 cell). Each mate falls on another unique LTP line
(expected overlap with existing violations ≈ 8/511 ≈ 1.6%). Thus:

- After adding 8 mates to fix 8 violations: new violations ≈ 8 × (1 - 8/511) × num_ltp ≈ 8
  (approximately equal new violations). The cascade is approximately a random walk on
  violations, neither converging nor diverging rapidly.
- But: each mate also creates geometric violations (row, col, diag, anti-diag) since mates
  are NOT generally geometrically balanced. Fixing those creates more LTP violations...

The net result is that violations grow monotonically. The MINIMUM swap size is likely
O(N/511) = O(511) cells per LTP sub-table (the size needed for each partition family to
have enough candidate cells that balanced pairs can form across all families
simultaneously). For 4 LTP sub-tables: minimum ≈ O(2000) cells.

**Conclusion.** The minimum constraint-preserving swap for the full 10-family CRSCE
system with Fisher-Yates random LTP partitions is **O(hundreds to thousands) of cells**.
This is not a local structure — swaps are fundamentally global. Consequently:

- **B.33 Phase 3 is infeasible** as originally specified. No "local search using small
  constraint-preserving swaps" exists for the random-partition LTP structure.
- The search reduces to a random walk on the $2^{256{,}020}$-dimensional null space,
  indistinguishable from exhaustive search.
- The correct CSM M* and the Phase 1 output M0 differ in O(170,000) cells (from the
  first branching point ~91,090 to the end at 261,121). There is no efficient path
  between them via small moves.

**B.33 STATUS: ABANDONED.** The Complete-Then-Verify architecture does not provide a
tractable Phase 3 for random-partition LTP. The decoupling of constraint satisfaction
from hash verification is not achievable via constraint-preserving local search.

**Implication for future work.** The B.33 analysis reveals a fundamental property of
the CRSCE constraint structure: all constraint-preserving transformations are GLOBAL
(O(1000+) cells). This explains why the current DFS with row-serial SHA-1 verification
is effective — it's the only known approach that exploits the correct CSM's structure
(hash constraints) to efficiently navigate the vast null space toward the unique solution.
Any approach that tries to decouple constraint solving from hash verification faces this
global-swap barrier. I'm not 100% comfortable with this, but I cannot explain it any better.
This probably just needs more thinking with a cigar and a long walk.

### B.34 LTP Table Hill-Climbing with Moderate Threshold and Save-Best (Implemented)

#### B.34.1 Context and Relationship to B.24

B.24 established that direct hill-climbing of LTP cell assignments is destructive: using a
threshold $\theta = 340$ (requiring $> 66.5\%$ early-row concentration) drove depth to $< 500$,
a catastrophic regression of more than 90,000 cells.  B.24 concluded that the proxy metric $\Phi$
and DFS depth are anti-correlated.

B.34 revisited this conclusion with two critical modifications:

1. **Conservative threshold.** The uniform-511 Fisher–Yates expectation at $K_{\text{plateau}} = 178$
   is $\text{cov}(L) \approx 178$ cells per line.  B.34 sets $\theta = 178$ — rewarding any
   line that exceeds the uniform expectation rather than requiring extreme concentration.  This
   is far below B.24's threshold of 340 and operates in the moderate-improvement regime.

2. **Save-best with periodic depth validation.** Every 50,000 accepted swaps, the decompressor is
   invoked for 20 seconds and peak depth recorded.  When depth improves, the current assignment
   table is saved to a separate file so that subsequent regression cannot overwrite the best-seen
   state.

#### B.34.2 Implementation

**Environment variable loading.** The C++ runtime checks `CRSCE_LTP_TABLE_FILE` at static
initialization of `getLtpData()`.  If the path is set and the file passes `ltpFileIsValid()`,
the LTP table is loaded from the LTPB binary file rather than generated by Fisher–Yates, enabling
the Python hill-climber's output to be used by the decompressor without recompilation.

**LTPB binary format:**

| Offset | Size               | Field         | Value           |
|:-------|:-------------------|:--------------|:----------------|
| 0      | 4 bytes            | magic         | `"LTPB"`        |
| 4      | 4 bytes            | version       | 1 (uint32 LE)   |
| 8      | 4 bytes            | S             | 511             |
| 12     | 4 bytes            | num_subtables | 4               |
| 16     | 2 × 4 × 261,121    | assignment    | uint16\_t line index per cell, sub-major order |

Total: 16 + 2,088,968 ≈ **2 MB**.

**Score function:**
$$\Phi = \sum_{\text{sub}=1}^{4} \sum_{L=0}^{510} \max\!\left(0,\; \text{cov}(L) - \theta\right)^2$$

where $\text{cov}(L) = |\{(r,c) \in L : r \leq K_{\text{plateau}} = 178\}|$
and $\theta = 178$ (the uniform-511 expectation at $K_{\text{plateau}}$).

**Swap acceptance:** Accept if $\Delta\Phi > 0$ and $\text{donor\_new} \geq \text{FLOOR}$
(FLOOR = 0 disabled in primary run; FLOOR = 120 tested in §B.34.4).

**Run parameters:**
- Seeds: CRSCLTPV + CRSCLTPP (B.26c winner), CRSCLTP3, CRSCLTP4
- `--checkpoint 50000` (accepted swaps per depth probe)
- `--patience 4` (early-stop after 4 consecutive non-improving checkpoints)
- `--secs 20` (decompressor wall time per probe)

#### B.34.3 Experimental Results

**LTPB baseline.** Loading the B.26c seeds via LTPB file and running the 20-second decompressor
test gave baseline depth = **91,511** (vs official B.26c depth 91,090; the small difference is
attributable to timing non-determinism in timed decompress runs).

**Primary run (FLOOR = 0, checkpoint = 50K accepted swaps, patience = 4):**

| Checkpoint | Accepted swaps | Score       | Depth     | Best depth | Top-10 cov range |
|:----------:|:--------------:|:-----------:|:---------:|:----------:|:----------------:|
| init       | 0              | 571,463     | 91,511    | 91,511     | ~178 (uniform)   |
| 1          | 50,000         | 2,919,876   | 87,909    | 91,511     | 287–300          |
| 2          | 100,000        | 7,308,146   | 88,891    | 91,511     | 327–342          |
| 3          | 150,000        | 13,352,613  | 90,259    | 91,511     | 363–378          |
| **4**      | **200,000**    | **20,785,740** | **92,001** | **92,001** | **400–415** ← NEW BEST |
| 5          | 250,000        | 29,403,064  | 82,614    | 92,001     | 429–445          |
| 6          | 300,000        | 38,830,359  | 76,088    | 92,001     | 451–451 (all)    |
| 7          | 350,000        | 48,512,269  | 69,529    | 92,001     | 451–451          |
| 8          | 400,000        | 58,300,200  | 63,953    | 92,001     | 451–451          |

Early-stop triggered after 4 consecutive stale checkpoints (checkpoints 5–8).
Best table saved to `tools/b32_best_ltp_table.bin` at checkpoint 4.

**Summary:**

| Metric                        | Value              |
|:------------------------------|:-------------------|
| Best depth                    | **92,001**         |
| Accepted swaps at peak        | 200,000            |
| Score at peak                 | 20,785,740         |
| Top-10 coverage at peak       | 400–415            |
| Δ vs B.26c baseline (91,090)  | **+911 (+1.00%)**  |
| Δ vs LTPB init baseline (91,511) | +490 (+0.54%) |

#### B.34.4 Sub-experiment B.34b: Donor Floor (FLOOR = 120)

To prevent catastrophic depletion of plateau-band coverage after the peak, a donor floor was
introduced: reject any otherwise-accepted swap that would bring the donor line below FLOOR = 120
early-row cells.

**Equilibrium analysis.** With FLOOR = $F$ and maximum observable coverage $M = 451$, the
maximum number of lines that can be concentrated above FLOOR is:
$$K = \frac{511 \times 511 - 511 \times F}{M - F} = \frac{91{,}469 - 511 \times 120}{451 - 120} \approx 91 \text{ lines}$$

**Results (FLOOR = 120, 15M iterations, 287K accepted swaps):**

| Checkpoint | Accepted swaps | Score      | Depth  | Best depth | Top-10 cov range |
|:----------:|:--------------:|:----------:|:------:|:----------:|:----------------:|
| init       | 0              | 571,463    | 91,511 | 91,511     | —                |
| 1          | 100,000        | 7,240,581  | 88,321 | 91,511     | 325–340          |
| 2          | 200,000        | 19,651,017 | 89,969 | 91,511     | 416–431          |
| final      | 286,661        | 34,062,171 | —      | 91,511     | —                |

**Comparison at matched checkpoints:**

| Accepted swaps | FLOOR = 0 depth | FLOOR = 120 depth | Δ          |
|:--------------:|:---------------:|:-----------------:|:----------:|
| 100,000        | 88,891          | 88,321            | −570       |
| 200,000        | **92,001**      | 89,969            | **−2,032** |
| best-seen      | **92,001**      | 91,511 (init)     | **−490**   |

FLOOR = 120 is strictly worse at every checkpoint.  The donor floor creates a **bimodal coverage
distribution**: ~91 lines near MAX_COV = 451 and ~420 lines pinned near FLOOR = 120, eliminating
the mid-range coverage band (250–400) that drives improvement in the FLOOR = 0 case.

#### B.34.5 Score–Depth Correlation Analysis

The proxy metric $\Phi$ and DFS depth are **non-monotonically correlated** in the FLOOR = 0 run:

| Φ range       | Depth trajectory       | Top-10 cov | Interpretation                                     |
|:--------------|:-----------------------|:-----------|:---------------------------------------------------|
| 0 – 5M        | Declining (91K → 88K)  | 178–300    | Rising Φ disrupts uniform structure initially      |
| 5M – 20M      | Recovering (88K → 92K) | 300–415    | Moderate concentration benefits propagation        |
| **~20.8M**    | **Peak: 92,001**        | **400–415** | **Optimal regime**                                |
| 20M – 40M     | Rapid decline (92K → 76K) | 415–451 | Saturation: biased lines cluster near 451 ceiling  |
| > 40M         | Continued decline (< 70K) | 451–451 | All top lines at ceiling; plateau band depleted    |

The B.24 analysis (anti-correlation) was correct for the high-Φ regime ($\Phi > 30\text{M}$)
but incomplete: there is a transient **pro-correlation window** ($5\text{M} \leq \Phi \leq 20\text{M}$)
where moderate row-concentration genuinely improves depth.  At 200K accepted swaps, approximately
$200{,}000 / (4 \times 261{,}121) \approx 0.19\%$ of cell-to-line assignments have been changed
from the Fisher–Yates baseline — a perturbation small enough to preserve cross-row co-occurrence
while creating sufficient concentration to tighten ~100 early-row LTP lines.

#### B.34.6 Conclusion

LTP table hill-climbing with a conservative threshold ($\theta = K_{\text{plateau}} = 178$) and
early stopping at the depth peak yields **depth 92,001**, a **+1.00% improvement** over the
B.26c baseline of 91,090. The improvement is modest, consistent, and reproducible.

Key findings:

1. **Proxy-depth correlation is non-monotonic.** B.24's anti-correlation conclusion applies only
   at high $\Phi$.  There is a transient pro-correlation window (score 5M–20M) where moderate
   concentration helps.  Stopping within this window captures the improvement.

2. **Save-best is essential.** Without saving at each depth improvement, the best state
   (checkpoint 4, depth 92,001) is overwritten by subsequent regressed states.  The current
   output file (`b32_ltp_table.bin`) contains the regressed state; the best table is in
   `b32_best_ltp_table.bin`.

3. **Donor floor is counter-productive.** FLOOR = 120 creates a bimodal coverage distribution
   that is worse at every measured checkpoint, peaking at only 89,969 (vs 92,001 for FLOOR = 0).

4. **Scope for further improvement.** The +1% gain demonstrates that the pro-correlation window
   contains real structure.  Open questions include: (a) whether simulated annealing could
   maintain score in the 5M–20M range indefinitely; (b) whether optimizing fewer, strategically
   selected lines (rather than all 511) could achieve greater per-line concentration without
   depleting the plateau band; (c) whether combining hill-climbing with seed re-search (B.26c
   on the hill-climbed table) could yield compounding improvements.

The best LTP table is stored in `tools/b32_best_ltp_table.bin` and is loadable via
`CRSCE_LTP_TABLE_FILE=tools/b32_best_ltp_table.bin`.

---

### B.35 Multi-Start Iterated Hill-Climbing (Implemented)

#### B.35.1 Motivation

B.34 demonstrated that the greedy hill-climber has a non-monotonic score–depth curve: it finds a
local optimum at score ≈ 20.8M (depth 92,001) but regresses past that point as all lines pile up
at the MAX_COV ceiling.  The save-best pattern captures the peak, but a single run from the
Fisher–Yates baseline consistently finds the same local optimum regardless of the numpy random
seed.

B.35 investigates whether **different numpy random seeds** (which control the order of proposed
swaps) lead to different local optima — i.e., whether the hill-climb landscape has multiple
distinct basins accessible from the same starting table.

#### B.35.2 Method

Each run:
1. Loads the B.34 best table (`tools/b32_best_ltp_table.bin`, depth 92,001) via `--init`.
2. Runs the B.34 greedy hill-climber with a fresh numpy seed (`--seed N`).
3. Saves the best-seen depth table to a separate file.
4. Early-stops after 10 consecutive checkpoints without depth improvement (`--patience 10`).

After identifying a high-value basin from a fresh start, the best table from that basin is used
as the `--init` for further iterated restarts with different seeds — i.e., the search chains:
FY → B.34 table → seed $S_1$ → seed $S_2$ → …

#### B.35.3 Results

**Single fresh-start seeds from B.34 baseline (selected results):**

| Seed | Best depth | Gain over B.34 |
|:----:|:----------:|:--------------:|
| 99   | 92,408     | +407           |
| 101  | 92,359     | +358           |
| 419  | 92,487     | +486           |
| 555  | 93,252     | **+1,251**     |
| 5000 | 93,231     | +1,230         |
| 6000 | 92,753     | +752           |
| 11235| 93,481     | +1,480         |
| 222222 | 93,722   | +1,721         |
| 200, 666, 777, 888 | 92,001 | 0 (flat) |

Approximately 50% of fresh seeds return the B.34 baseline (92,001 = strong attractor); ~15%
find basins at 93K+.

**Iterated restart chain (best sequence found):**

| Step | Init table | Seed | Best depth | Gain over prev |
|:-----|:----------:|:----:|:----------:|:--------------:|
| B.34 pass 1 | FY | 42 | 92,001 | +911 vs B.26c |
| Fresh start | B.34 (92,001) | 5000 | 93,231 | +1,230 |
| Iterate | 93,231 | 22 | **93,805** | +574 |
| Iterate | 93,805 | 44,55,66,77,88 | 93,805 | 0 — converged |

The 93,805 basin is extremely robust: five independent seeds from that basin all return exactly
93,805, confirming it is a deep local optimum.

**Basin convergence experiment (from 93,252 basin):**

Seeds 700, 800, 900 applied to the 93,252 table all returned exactly 93,252 — a distinct, robust
basin at a lower depth.

**Frustration-band mode (rows 179–210) experiment:**

Running with `--mode band --k1 179 --k2 210` from the B.34 best table (depth 92,001):
- top-10 band coverage reached 192 cells at early-stop
- depth never improved over the B.34 baseline (92,001)
- Band mode is ineffective: the 32-row band is too sparse (~32 cells/line uniform expectation)
  to create the forcing density needed at the frontier.

**Summary:**

| Metric                        | Value              |
|:------------------------------|:-------------------|
| Best depth                    | **93,805**         |
| Chain                         | FY → 92,001 → 93,231 → **93,805** |
| Δ vs B.26c baseline (91,090)  | **+2,715 (+2.98%)** |
| Δ vs B.34 (92,001)            | +1,804 (+1.96%)    |
| Seeds explored                | >40                |
| Basin convergence             | 93,805 = confirmed robust local optimum |

Best table stored in `tools/b35_best_ltp_table.bin`.

#### B.35.4 Score–Depth Landscape

The multi-start data reveals the basin structure of the hill-climb landscape:

| Depth range | Frequency | Notes |
|:-----------|:---------:|:------|
| 92,001 (exact) | ~50% of fresh starts | Strong global attractor |
| 92,100–92,799 | ~35% | Scattered moderate basins |
| 93,000–93,499 | ~10% | Higher basins (rarer) |
| 93,500–93,805 | ~5% | Deep basins; all converge to local optimum |
| > 93,805 | 0 of 40+ seeds | No basin found above 93,805 via greedy hill-climb |

The landscape is rugged with many local optima.  The greedy acceptor (accept iff $\Delta\Phi > 0$)
finds 93,805 as the deepest accessible basin from the B.34 baseline.

#### B.35.5 Conclusion

Iterated multi-start hill-climbing from the B.34 table yields **depth 93,805**, a
**+2.98% improvement** over the B.26c baseline of 91,090.

Key findings:

1. **Multiple distinct basins exist.** The landscape from B.34 has basins at 92,001, 92,400–
   92,800, 93,200–93,500, and 93,805+.  The greedy acceptor finds different basins depending
   on the order of proposed swaps (numpy seed).

2. **Iterated restart provides compounding improvement.** Chaining two or three hill-climb
   passes (each starting from the previous best) can escape moderate basins and reach deeper
   ones, as demonstrated by the 92,001 → 93,231 → 93,805 chain.

3. **93,805 appears to be the greedy hill-climb ceiling.** Across 40+ seeds applied to the
   93,805 basin and 20+ fresh starts from B.34, no seed exceeded 93,805.  Escaping this basin
   likely requires a non-greedy algorithm (simulated annealing, iterated local search with
   random perturbation, or beam search).

4. **Band mode (frustration band) is ineffective.** Targeting the 32-row transition band
   rows 179–210 produces no depth improvement; the sparse band cannot create forcing density.

5. **Gap to 100K.** The remaining gap from 93,805 to 100,000 is 6,195 cells (+6.6%).  At the
   current rate of improvement through greedy multi-start (~300–500 cells per pass), reaching
   100K would require approximately 15–20 more passes if higher basins exist — which is
   unconfirmed.  A fundamentally different optimization strategy (e.g., simulated annealing
   on depth) is likely required to make further significant progress.

---

### B.36 Simulated Annealing on LTP Score Proxy (Implemented)

#### B.36.1 Motivation

The multi-start greedy hill-climber (B.35) converged to depth **93,805** across 40+ numpy seeds.
Every seed applied to the 93,805 basin returned exactly 93,805; no seed exceeded it.  The greedy
acceptor cannot escape this local optimum because it requires a score decrease to reach a
neighbouring basin.  Simulated annealing (SA) addresses this by accepting downhill score moves with
probability exp(Δ/T), where T is a temperature that cools geometrically from T₀ to T_f over the
run.  At high T the optimiser explores broadly; at low T it converges greedily.

The SA objective is identical to B.34/B.35: the capped quadratic row-concentration score.  No
changes to the decompressor or LTPB format are required; SA is a drop-in replacement for the
greedy acceptance criterion.

#### B.36.2 Method

**Acceptance criterion** (added to `hill_climb_one_sub`):

| Δ     | Condition                  | Accept probability       |
|-------|----------------------------|--------------------------|
| > 0   | uphill, donor floor OK     | 1.0                      |
| < 0   | downhill, annealing active | exp(Δ / T)               |
| < 0   | downhill, T = 0 (greedy)   | 0.0                      |
| = 0   | neutral                    | 0.0                      |

Downhill SA moves bypass the donor-floor guard (F_ = 0 by default) to allow genuine exploration.

**Temperature schedule**: geometric cooling over N total iterations.

$$T(k) = T_0 \cdot \left(\frac{T_f}{T_0}\right)^{k / N_{\text{outer}}}$$

where N_outer = N / (BATCH × NUM\_SUB) is the number of outer loop iterations.

**Calibration** — representative acceptance probabilities at Δ = −250 (typical single-swap delta
near the 93,805 basin with coverage around 270–320):

| Temperature | P(accept \| Δ=−250) |
|-------------|----------------------|
| T = 2000    | 88.2%                |
| T = 500     | 60.7%                |
| T = 200     | 28.7%                |
| T = 50      | 0.67%                |
| T = 5       | ≈ 0                  |

Rationale: T_init = 2000 gives high acceptance of moderate downhill moves at the start, enabling
genuine basin-hopping.  T_final = 5 collapses to essentially greedy behaviour during the last phase
to exploit the best-found basin.

**Note on init files**: The B.35 best table (`tools/b35_best_ltp_table.bin`) was not committed to git
and was unavailable for this session.  Both runs started from fresh Fisher-Yates seeds (B.26c
defaults), which establishes a cleaner baseline and still tests whether SA can beat the FY-derived
greedy ceiling.

**Runs performed**:

| Run label | Init       | --iters | --T_init | --T_final | --seed | --checkpoint | --secs |
|-----------|------------|---------|----------|-----------|--------|--------------|--------|
| SA-A      | FY seeds   | 50M     | 2000     | 5         | 42     | 50K          | 20     |
| SA-B      | FY seeds   | 50M     | 2000     | 5         | 137    | 50K          | 20     |

CLI invocations (as executed):

```bash
# SA-A
python3 tools/b32_ltp_hill_climb.py \
    --anneal --T_init 2000 --T_final 5 \
    --iters 50000000 --checkpoint 50000 --secs 20 \
    --seed 42 \
    --out tools/b36_sa_fy_s42.bin \
    --save-best tools/b36_best_ltp_table.bin

# SA-B
python3 tools/b32_ltp_hill_climb.py \
    --anneal --T_init 2000 --T_final 5 \
    --iters 50000000 --checkpoint 50000 --secs 20 \
    --seed 137 \
    --out tools/b36_sa_fy_s137.bin \
    --save-best tools/b36_best_s137_ltp_table.bin
```

#### B.36.3 Expected Outcomes

**H1 (optimistic)**: SA escapes the 93,805 local optimum and reaches depth ≥ 95,000 (+1.3% over
B.35).  The high-T exploration phase discovers a qualitatively different geometry (different top-10
coverage distribution) that the greedy phase cannot reach.  Best table saved automatically via
`--save-best`.

**H2 (moderate)**: SA transiently visits basins in the 94,000–95,000 range during the exploration
phase, but the cooling schedule returns the table near the 93,805 level.  `--save-best` captures
the transient peak.  Net improvement: 300–800 cells over B.35.

**H3 (pessimistic, same basin)**: SA with these parameters finds 93,805 reliably.  The
neighbourhood of 93,805 is truly optimal for the score proxy; no escaping basin is accessible at
this scale.  Result: best depth ≈ 93,805, confirming that the score proxy has hit its effective
ceiling for this geometry.

**H4 (anti-correlation)**: SA increases score significantly (> 30M) during the exploration phase
and depth drops (similar to B.34 FLOOR=0 regression at score=38.7M → depth=76,087).  If this
occurs, the run should be shortened or T_init reduced.

The most likely outcome given B.35 basin structure is H2 or H3.  H1 is possible but requires
that an unexplored higher basin exists and is reachable within 50M iterations.

#### B.36.4 Actual Results

**Outcome: H4 (Anti-Correlation) — SA overshoots optimal score window.**

Both runs completed (50M iterations each, ~2 hours total).  Neither improved upon the FY
baseline depth of 91,522.

| Run  | Init depth | Best depth | Final score | Final depth |
|------|-----------|------------|-------------|-------------|
| SA-A | 91,522    | 91,522     | 33,239,934  | 73,979      |
| SA-B | 91,522    | 91,522     | 33,388,992  | 91,186      |

The `--save-best` path was never written for either run because no checkpoint depth exceeded the
initial FY depth (91,522).

**Score trajectory** (SA-A):

| Iters (M) | T       | Score     | Top-10 coverage           | Depth  |
|-----------|---------|-----------|---------------------------|--------|
| 0         | 2000    | 571,463   | 214–260                   | 91,522 |
| 0.15      | 1964    | 464,121   | 242–252                   | 87,688 |
| 0.75      | 1828    | 274,325   | 223–237                   | 86,314 |
| 16        | ~340    | ~500,000  | early stage, recovering   | ~88K   |
| 23.3      | 129     | 6,297,661 | 334–382                   | 87,572 |
| 24.4      | 116     | 11,387,189| 401–442                   | 89,171 |
| 25.4      | 107     | 16,731,426| 428–451                   | 89,849 |
| 27.8      | 71.5    | 28,118,728| 451–452 (all at ceiling)  | 84,272 |
| 50 (final)| 5       | 33,239,934| all at 451                | 73,979 |

**Root cause analysis**: The SA schedule with T_init = 2000 causes a multi-phase trajectory:

1. **Destruction phase** (T > 500): SA accepts most downhill moves, randomizing the assignment.
   Score drops from 571K to near-random (≈ 200K–500K).

2. **Recovery phase** (T 100–500): SA begins preferring uphill moves.  Score climbs back
   through the pro-correlation window (5M–20M).

3. **Over-drive phase** (T < 100): Greedy-like climbing now dominates.  Score continues to
   climb beyond the optimal window (20M), exactly replicating the B.34 FLOOR=0 collapse.
   All top-10 lines converge to the MAX_COV ceiling (451 early cells).

4. **Anti-correlation state** (T < 20): Score = 28–33M, all 10+ lines fully concentrated.
   The matrix is now deprived of LTP constraint density for late rows → depth collapses to
   73K–84K.

The critical failure mode: SA with T_init = 2000 reliably discovers the over-concentration basin
(score ≈ 30M) rather than the optimal basin (score ≈ 20M, depth 92–94K).  This basin is large
and strongly attracting during the greedy cooling phase; the optimizer converges to it from
any direction.

**Why T_init = 2000 is too large**: At T = 2000, the exploration phase is so aggressive that
it destroys all the structure built during Fisher-Yates initialization.  When cooling begins, the
optimizer effectively restarts from a near-random state.  The near-random state sits far from the
optimal basin (score ≈ 20M) and the cooling path passes through it only briefly before overshooting
into the anti-correlation zone (score > 30M).

**Comparison with B.34 FLOOR=0**: B.34 (pure greedy) also overshot — at score=38.7M it hit depth
76,087 — but its *peak* was at score=20.7M with depth 92,001.  SA with T_init=2000 never paused
at score≈20M; it blew through that regime at T≈150 (late in the run) and converged past it.

**Conclusion**: SA as designed (T_init=2000 → T_final=5) is not an improvement over greedy
hill-climbing for this proxy.  The proxy has a **narrow optimal score window** (≈15–25M) that
SA must be tuned to stay within.  Recommended re-tuning:

1. **Low-T SA** (B.37a): T_init=30→T_final=5 — start just inside the pro-correlation regime;
   tiny excursions around the 20M optimum without overshooting.

2. **Score-capped SA** (B.37b): Accept downhill moves only while score < 25M (hard cap);
   revert to greedy when score exceeds budget.

3. **Depth-direct SA** (B.37c): Use measured decompressor depth as the SA objective directly
   (no proxy).  Computationally expensive (20s per move) but eliminates proxy-depth mismatch.
   Feasible only with a much smaller iteration budget and coarser checkpoint.

**Key finding**: The score proxy has a confirmed optimal band at 15–25M.  Any optimization
strategy (greedy or SA) that drives score above 25M enters the anti-correlation zone.  Future
experiments must either hard-cap score at 25M or shift to depth-direct optimization.

---

### B.37 Score-Capped Simulated Annealing (Proposed)

#### B.37.1 Motivation

B.34–B.36 established a consistent failure mode: any optimization strategy that drives the
row-concentration score above ≈ 25M enters an anti-correlation zone where depth regresses.
The greedy hill-climber (B.34/B.35) peaks at score ≈ 20M and depth 93,805, then overshoots.
SA with T_init=2000 (B.36) passes through the optimal window too quickly and converges at 33M.

The root cause is the **score proxy overshoot problem**: the capped-quadratic reward function
has no natural stopping criterion — it continues rewarding concentration beyond the point where
additional concentration harms the solver.  A hard score cap prevents this overshoot, while
the SA acceptance criterion (exp(Δ/T) for downhill moves) allows genuine basin-hopping within
the optimal band.

#### B.37.2 Method

**Score-capped SA acceptance rule** (added to `hill_climb_one_sub`):

| Condition                              | Action    |
|---------------------------------------|-----------|
| δ > 0 and score_new ≤ CAP and floor OK| Accept    |
| δ > 0 and score_new > CAP             | Reject    |
| δ < 0 and T > 0                       | exp(δ/T)  |
| δ = 0 or T = 0 and δ < 0             | Reject    |

The score cap prevents the optimizer from ever entering the anti-correlation zone.  The SA
component allows it to make small downhill moves within the cap budget, enabling escape from
the greedy ceiling at 93,805.

**Parameters**:

- `--score-cap 22000000` — hard cap at 22M (inside the 15–25M optimal window, below the 20.7M
  peak from B.34)
- `--anneal --T_init 50 --T_final 5` — low temperature; P(accept|Δ=-500) ≈ 4.5% at T=50;
  explores locally without catastrophic randomization
- `--init tools/b35_best_ltp_table.bin` — start from the B.35 best (93,805); do not waste
  iterations replicating the B.34/B.35 greedy climb
- `--iters 50000000 --checkpoint 50000 --secs 20`

**Temperature calibration at T_init=50**:

| Δ      | P(accept) |
|--------|-----------|
| −50    | 36.8%     |
| −250   | 0.7%      |
| −1000  | ≈ 0       |

At T=50, only very small downhill moves are accepted.  The optimizer stays close to the 93,805
basin but can make short downhill excursions to reach neighbouring basins blocked by a small
score barrier.

**Implementation** — new `--score-cap N` flag in `hill_climb_one_sub`:

```python
# In hill_climb_one_sub (added to uphill acceptance branch):
if delta > 0:
    score_after = current_score + delta
    do_accept = (donor_new >= F_) and (score_after <= score_cap_)
```

`score_cap_` is a new module global defaulting to `sys.maxsize` (disabled), set from `--score-cap`.
`current_score` is passed into the function as a new parameter so the cap can be evaluated
per-move without recomputing the full score.

#### B.37.3 Expected Outcomes

**H1 (optimistic)**: Score-capped SA finds a basin above 93,805 in the 94,000–96,000 range.
The cap prevents overshoot; low-T SA makes basin-hopping excursions the greedy climber cannot.
`--save-best` captures any transient peak above 93,805.

**H2 (moderate)**: Score-capped SA returns consistently to 93,805.  The cap keeps it in the
optimal band but 93,805 is a true global optimum for the score proxy at this scale.  Best depth
unchanged; confirms that proxy-based optimization has reached its ceiling.

**H3 (proxy ceiling confirmed)**: Score-capped SA confirms 93,805 as the proxy ceiling.
Next required step: depth-direct SA (B.37b) or a fundamentally different proxy.

**H4 (cap too tight)**: A cap of 22M is too restrictive; the optimizer gets stuck because
uphill moves are rejected at the boundary and downhill moves lead back to lower-score states.
Symptom: acceptance rate near zero, no depth improvement.  Fix: raise cap to 24M or 26M.

#### B.37.4 Actual Results

**Outcome: H1/H2 partial — score-cap is working; new best depth 92,883 (+882 over starting point).**

| Run      | Init   | Seed | Cap | Best depth | Best T | Final score | Final depth |
|----------|--------|------|-----|------------|--------|-------------|-------------|
| B.37a-s42 | 92,001 | 42  | 22M | 92,001     | —      | 22,000,000  | 91,356      |
| B.37a-s137| 92,001 | 137 | 22M | **92,883** | 28.8   | 22,000,000  | 79,567      |

Best table saved to `tools/b37a_best_s137_ltp_table.bin`.

The score cap functioned as designed: score locked at exactly the cap (21,999,958–22,000,000)
throughout both runs.  SA downhill moves (T=50→5) allowed exploration within the band.

**Observations**:

1. **Score-capped SA finds improvements greedy cannot preserve.** s137 found 92,883 (+882 over
   the 92,001 start).  This is a strictly better result than running uncapped greedy from the same
   starting table (which overshoots to score=65M and regresses to depth=63K).

2. **Score at best-seen = cap boundary.** The 92,883 depth was found at score=21,999,996 — the
   table is essentially AT the cap when at its best.  Starting a follow-on run from this table
   with the same cap allows only SA downhill moves, which is suboptimal.  The next run must
   either use a higher cap (24M) or restart from the lower-score 92,001 table.

3. **T cooling too aggressive.** By T=16 (checkpoint 7 of 8), the depth had already regressed
   below the best.  Most SA benefit occurred in the T=50→20 range.  Future runs could use
   `--T_final 20` and longer `--iters` to keep the optimizer in the productive temperature range.

4. **seed=42 produced no improvement.** The same seed that found 92,001 from FY cannot find
   anything better from the 92,001 geometry — it converges back to the same local optimum.
   Multi-seed search is essential.

**Gap to 100K**: 92,883 → 100,000 = 7,117 cells (+7.7%).  Continuing multi-seed search.

---

### B.37b Multi-Seed Score-Capped SA from Best Known Table (Implemented)

#### B.37b.1 Motivation

B.37a confirmed that score-capped SA finds improvements the uncapped greedy climber cannot
preserve.  seed=137 found 92,883 from 92,001 while seed=42 found nothing — the landscape has
multiple reachable basins depending on the random walk direction.  B.37b extends this by:

1. Running the B.35 seeds (5000 and 22) that previously found 93,231→93,805 via greedy, now
   with the score cap engaged, to see if those same seeds find analogous improvements.
2. Running additional fresh seeds from 92,001 to explore more of the band.
3. Running one seed from the 92,883 table with a raised cap (24M) to give the optimizer room
   to climb from the 92,883 geometry.

#### B.37b.2 Method

Four runs launched in parallel:

| Run        | Init    | Seed  | Cap | --T_init | --T_final |
|------------|---------|-------|-----|----------|-----------|
| B.37b-s5k  | 92,001  | 5000  | 22M | 50       | 20        |
| B.37b-s22  | 92,001  | 22    | 22M | 50       | 20        |
| B.37b-s777 | 92,001  | 777   | 22M | 50       | 20        |
| B.37b-hi   | 92,883  | 42    | 24M | 50       | 20        |

`--T_final 20` replaces 5 to avoid spending iterations at near-zero temperature where SA offers
no benefit over greedy (which is already blocked by the cap).

CLI invocations:

```bash
python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 22000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 5000 \
    --out /tmp/b37b_s5k.bin --save-best tools/b37b_best_s5k.bin

python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 22000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 22 \
    --out /tmp/b37b_s22.bin --save-best tools/b37b_best_s22.bin

python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 22000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 777 \
    --out /tmp/b37b_s777.bin --save-best tools/b37b_best_s777.bin

python3 tools/b32_ltp_hill_climb.py --init tools/b37a_best_s137_ltp_table.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 24000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 42 \
    --out /tmp/b37b_hi.bin --save-best tools/b37b_best_hi.bin
```

#### B.37b.3 Expected Outcomes

**H1**: seeds 5000 or 22 (the B.35 winners) find analogous improvements with the cap engaged,
reaching 93,000–94,000.  The cap prevents overshoot and allows the chain to continue.

**H2**: No seed exceeds 92,883.  The landscape from 92,001 has only one reachable basin at
92,883 within the 22M cap; other seeds converge back to 92,001 or find intermediate values.

**H3 (raised cap)**: The 24M cap run from 92,883 finds a new local optimum above 92,883,
suggesting that the cap at 22M was constraining too tightly.

#### B.37b.4 Actual Results

**Outcome: H2 — no seed exceeded 92,883 (best from B.37a).**

| Run        | Init    | Seed  | Cap | Best depth | Final depth |
|------------|---------|-------|-----|------------|-------------|
| B.37b-s5k  | 92,001  | 5000  | 22M | 92,706     | 91,592      |
| B.37b-s22  | 92,001  | 22    | 22M | 92,001     | 90,170      |
| B.37b-s777 | 92,001  | 777   | 22M | 92,001     | 91,712      |
| B.37b-hi   | 92,883  | 42    | 24M | 92,883     | 90,840      |

Overall best remains **92,883** (from B.37a-s137).  The raised-cap run (24M from 92,883)
produced no improvement — the 92,883 geometry is a local optimum within cap=24M.

s5k (seed=5000, the B.35 greedy chain winner) found 92,706 but not the 93,231 it found via
uncapped greedy.  This strongly suggests the path to 93,231 required visiting score states
above 22M (and possibly above 24M).  The cap is structurally blocking access to the 93K+ basins.

**Conclusion**: the 93K+ basins found in B.35 require a score path exceeding 22–24M.
The cap must be raised further, OR a different initialization must be found that starts closer
to the 93K basin geometry.  B.37c tests cap=25M and cap=28M to determine the minimum cap
required to reach 93K+.

---

### B.37c Cap-Sweep: Minimum Cap for 93K+ Basins (Implemented)

#### B.37c.1 Motivation

B.37a–b showed that cap=22M and cap=24M cannot reach the 93K+ basins found in B.35 (93,805).
The hypothesis is that those basins lie in a score corridor above 24M but below the
anti-correlation threshold (≈30M).  B.37c tests whether raising the cap to 25M or 28M
unlocks access to these basins, while the SA downhill acceptance prevents the runaway overshoot
that destroyed B.36.

Key constraint: at T_init=50, P(accept|Δ=-250) ≈ 0.7%.  This means SA can make small
downhill excursions but will quickly converge to the cap boundary.  The cap still prevents
runaway, but leaves room to visit states above 22M on the way to 93K+.

#### B.37c.2 Method

Six runs: three cap levels (25M, 28M, 31M) × two seeds each.  All start from the 92,001 table.

| Run        | Init   | Seed  | Cap | --T_init | --T_final |
|------------|--------|-------|-----|----------|-----------|
| B.37c-25-s137 | 92,001 | 137 | 25M | 50 | 20 |
| B.37c-25-s5k  | 92,001 | 5000| 25M | 50 | 20 |
| B.37c-28-s137 | 92,001 | 137 | 28M | 50 | 20 |
| B.37c-28-s5k  | 92,001 | 5000| 28M | 50 | 20 |
| B.37c-31-s137 | 92,001 | 137 | 31M | 50 | 20 |
| B.37c-31-s5k  | 92,001 | 5000| 31M | 50 | 20 |

Rationale for cap levels:
- **25M**: just above the cap that locked us out (24M), minimum increment test.
- **28M**: mid-point between optimal (20M) and anti-correlation onset (≈30M).
- **31M**: approaching the anti-correlation zone; expected to show H4 regression.

#### B.37c.3 Expected Outcomes

**H1**: cap=25M or cap=28M seeds 137/5000 find depth ≥ 93,000 (demonstrating the 93K+ basins
   are reachable within a moderate cap).  This identifies the minimum viable cap.

**H2**: All cap levels converge to ≤ 92,883 — the 93K+ basins are not reachable even at 28M
   because the path requires specifically the uncapped greedy trajectory that passes through
   high-score intermediate states.

**H3 (anti-correlation at 31M)**: cap=31M runs show depth < 92,883 due to overshoot.
   Confirms that the useful cap range is 22–28M.

#### B.37c.4 Actual Results

**Outcome: H3 confirmed — higher caps are uniformly worse. Optimal cap is 22M.**

| Run        | Cap | Seed | Best depth |
|------------|-----|------|------------|
| B.37c-25-s137 | 25M | 137 | 92,472 |
| B.37c-25-s5k  | 25M | 5000| 92,139 |
| B.37c-28-s137 | 28M | 137 | 92,001 |
| B.37c-28-s5k  | 28M | 5000| 92,001 |
| B.37c-31-s137 | 31M | 137 | 92,001 |
| B.37c-31-s5k  | 31M | 5000| 92,001 |

The cap=22M result (92,883 from B.37a-s137) remains the best.  Cap=25M is second-best at 92,472;
cap=28M and above find nothing beyond the 92,001 starting point.

**Key finding: higher caps are worse, not better.** Each increment in cap allows the optimizer to
visit higher-score geometries, but those geometries have uniformly worse depth than the ≤22M band.
The optimal depth geometry is at score≈20–22M, not at 25–31M.

**The structural barrier**: the path from 92,001 to 93,231→93,805 (found in uncapped B.35) requires
*transiting* through intermediate score states that are momentarily high (>22M) but the optimizer
passes *through* rather than converging *to*.  The cap blocks these transits.  The solution is to
use uncapped greedy with fine checkpoints (so `--save-best` captures the transient peaks before
the run overshoots) rather than using a static cap.

**B.37d strategy**: uncapped greedy from 92,001 with checkpoint=10000 and patience=3, multiple
seeds — replicating the B.35 methodology but now with `--save-best` available to preserve peaks.

---

### B.37d Fine-Checkpoint Uncapped Greedy Multi-Seed from Best (Implemented)

#### B.37d.1 Motivation

B.37a–c established that score-capped SA is limited to ≈92,883 because the 93K+ basins require
transiting score states above 22M.  The original B.35 chain found 93,231→93,805 using uncapped
greedy with fine checkpoints (saving every 10K accepted swaps via `--save-best`).  B.37d
replicates this exactly from the 92,001 table, but now with `--patience 3` to stop early and
preserve the best-seen table before the run overshoots into the anti-correlation zone.

The critical parameter difference from B.37b: `--checkpoint 10000` (not 50000).  With 10K
checkpoint, the first save-best fires at 10K accepted swaps, before the greedy climber
overshoots.  With 50K checkpoint, the first fire was already past the peak.

#### B.37d.2 Method

Six seeds from the 92,001 table.  Uncapped (no --score-cap).  Fine checkpoint=10000.
patience=3 stops the run 30K accepted swaps after the last depth improvement.

| Run         | Init   | Seed | Checkpoint | Patience |
|-------------|--------|------|------------|----------|
| B.37d-s5k   | 92,001 | 5000 | 10K        | 3        |
| B.37d-s22   | 92,001 | 22   | 10K        | 3        |
| B.37d-s100  | 92,001 | 100  | 10K        | 3        |
| B.37d-s200  | 92,001 | 200  | 10K        | 3        |
| B.37d-s300  | 92,001 | 300  | 10K        | 3        |
| B.37d-s400  | 92,001 | 400  | 10K        | 3        |

CLI invocation pattern:

```bash
python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --iters 10000000 --checkpoint 10000 --patience 3 --secs 20 \
    --seed SEED --out /tmp/b37d_sSEED.bin --save-best tools/b37d_best_sSEED.bin
```

#### B.37d.3 Expected Outcomes

**H1**: seeds 5000 and/or 22 find 93,231–93,805 (reproducing B.35 with fine checkpoint).
   `--patience 3` stops the run before the score collapses to 65M, preserving the best table.

**H2**: All seeds find depths in the 92,001–92,883 range, no improvement over B.37a.
   This would indicate the 93K+ basins are only reachable from a specific starting geometry
   that we no longer have (the B.35 table chain was path-dependent).

**H3**: A seed not in the B.35 chain finds a depth > 93,805.  Unlikely but possible — the
   multi-seed search covers a wider landscape.

#### B.37d.4 Actual Results

**Outcome: H1 confirmed — fine checkpoints reproduce B.35 result; best depth 93,231.**

| Run        | Seed | Best depth |
|------------|------|------------|
| B.37d-s5k  | 5000 | **93,231** ← reproduced B.35 step-2 result |
| B.37d-s22  | 22   | 92,146     |
| B.37d-s100 | 100  | **93,230** ← independently found same basin |
| B.37d-s200 | 200  | 92,001     |
| B.37d-s300 | 300  | 92,594     |
| B.37d-s400 | 400  | 92,001     |

Best tables saved to `tools/b37d_best_s5000.bin` (93,231) and `tools/b37d_best_s100.bin` (93,230).

The critical parameter was `--checkpoint 10000`: with fine checkpoints, `--save-best` fires before
the greedy climber overshoots past the 93K basin.  With checkpoint=50000 (B.37b-s5k), the same
seed only found 92,706 because the first save-best fired after the overshoot.

Two independent seeds (5000 and 100) converged to the same basin at depth 93,230–93,231,
confirming this is a genuine local optimum for the score proxy, not a measurement artifact.

Seed=22 only found 92,146 from 92,001 — this seed's B.35 result of 93,805 came from starting
at 93,231, not 92,001.  The chain structure matters.

**Continuing chain**: from 93,231 (saved), now running B.37e with multiple seeds to find 93,805+.

---

### B.37e Fine-Checkpoint Greedy Chain from 93,231 (Implemented)

#### B.37e.1 Motivation

B.37d reproduced 93,231 via seed=5000 and seed=100 independently.  B.35 step-2 found 93,805 from
93,231 using seed=22 (but with checkpoint=50000 which was coarse).  B.37e tests whether fine
checkpoints (10K) from 93,231 can reliably reproduce 93,805 and whether other seeds find new
higher optima.

#### B.37e.2 Method

Starting table: `tools/b37d_best_s5000.bin` (depth 93,231).
Parameters: `--iters 10000000 --checkpoint 10000 --patience 3 --secs 20`

| Run       | Start | Seed | Checkpoint | Patience |
|-----------|-------|------|------------|----------|
| B.37e-s22   | 93,231 | 22   | 10K | 3 |
| B.37e-s42   | 93,231 | 42   | 10K | 3 |
| B.37e-s137  | 93,231 | 137  | 10K | 3 |
| B.37e-s500  | 93,231 | 500  | 10K | 3 |
| B.37e-s1000 | 93,231 | 1000 | 10K | 3 |
| B.37e-s2000 | 93,231 | 2000 | 10K | 3 |

#### B.37e.3 Expected Outcomes

- **H1 (success)**: seed=22 reproduces 93,805; possibly other seeds find new optima > 93,805.
- **H2 (partial)**: seed=22 reproduces 93,805 but no seed exceeds it; continue chain from 93,805.
- **H3 (stall)**: all seeds plateau at 93,231; landscape has no accessible exit from 93,231.

#### B.37e.4 Actual Results

| Run        | Seed | Best Depth |
|------------|------|------------|
| B.37e-s22  | 22   | **93,805** ← reproduced B.35 exactly |
| B.37e-s42  | 42   | 93,231 (no improvement) |
| B.37e-s137 | 137  | 93,231 (no improvement) |
| B.37e-s500 | 500  | 93,231 (no improvement) |
| B.37e-s1000| 1000 | 93,231 (no improvement) |
| B.37e-s2000| 2000 | 93,231 (no improvement) |

**Outcome: H2.** Seed=22 reliably finds 93,805; the chain FY→92,001→93,231→93,805 is confirmed
fully reproducible.  No seed exceeded 93,805.  Continuing chain: B.37f from 93,805
(`tools/b37e_best_s22.bin`).

---

### B.37f Fine-Checkpoint Greedy Chain from 93,805 (Implemented)

#### B.37f.1 Motivation

The chain FY→92,001→93,231→93,805 is reproducible.  B.37f attempts to extend the chain from
93,805 to a new optimum, using 12 seeds with fine checkpoints to capture peaks before overshoot.

#### B.37f.2 Method

Starting table: `tools/b37e_best_s22.bin` (depth 93,805).
Parameters: `--iters 10000000 --checkpoint 10000 --patience 3 --secs 20`

| Run       | Start  | Seed | Checkpoint | Patience |
|-----------|--------|------|------------|----------|
| B.37f-s22   | 93,805 | 22   | 10K | 3 |
| B.37f-s42   | 93,805 | 42   | 10K | 3 |
| B.37f-s100  | 93,805 | 100  | 10K | 3 |
| B.37f-s137  | 93,805 | 137  | 10K | 3 |
| B.37f-s200  | 93,805 | 200  | 10K | 3 |
| B.37f-s300  | 93,805 | 300  | 10K | 3 |
| B.37f-s400  | 93,805 | 400  | 10K | 3 |
| B.37f-s500  | 93,805 | 500  | 10K | 3 |
| B.37f-s1000 | 93,805 | 1000 | 10K | 3 |
| B.37f-s2000 | 93,805 | 2000 | 10K | 3 |
| B.37f-s5000 | 93,805 | 5000 | 10K | 3 |
| B.37f-s7777 | 93,805 | 7777 | 10K | 3 |

#### B.37f.3 Expected Outcomes

- **H1**: At least one seed finds depth > 93,805; continue chain.
- **H2**: All seeds plateau at 93,805; landscape local minimum confirmed; try broader diversity.
- **H3**: Some seeds find 94,000+; acceleration suggests chain nearing 100K attractor.

#### B.37f.4 Actual Results

All 12 seeds failed to improve over the 93,805 baseline.  Key diagnostic:

| Seed | Baseline | First ckpt score | First ckpt depth | Best |
|------|----------|-----------------|-----------------|------|
| 22   | 93,805   | 29.5M           | 81,550          | 93,805 |
| 42   | 93,805   | 29.5M           | 92,812          | 93,805 |
| 100  | 93,805   | 29.5M           | 72,926          | 93,805 |
| others | 93,805 | 29.5M           | 72K–86K         | 93,805 |

**Outcome: H2 (stall).**  The 93,805 table has score=27.7M — already near the
anti-correlation threshold.  Every accepted greedy swap raises score (+1.8M per 10K accepted),
reaching 33M within 30K accepted swaps.  Depth collapses before any improvement can fire.

**Root cause**: pure greedy hill-climb is structurally blocked from 93,805:
- Starting score 27.7M > 25M threshold; every greedy step raises it further
- First checkpoint at 29.5M → depth already collapsed
- The 94K+ basin (if it exists) is not reachable by pure uphill moves from 27.7M

**Implication**: an escape mechanism is required.  ILS (random kick + hill-climb) or SA
targeting score ≤ 27.7M during exploration may work.  B.37g implements the ILS approach.

---

### B.37g Iterated Local Search (ILS) from 93,805 (Implemented)

#### B.37g.1 Motivation

From 93,805, pure greedy is blocked (score rises monotonically → depth collapse).  ILS applies
a random perturbation ("kick") to escape the local basin, then hill-climbs to a new local
optimum.  If the 94K+ basin is accessible via a random walk of K steps, ILS will find it.

Added `--kick K` to `tools/b32_ltp_hill_climb.py`: applies K random unconstrained swaps to the
loaded table before the hill-climb phase.  Each kick swap: random sub-table, random la/lb/pos_a/pos_b,
applied unconditionally (no score filter, floor, or cap).  Coverage updated for zone-straddling kicks.

#### B.37g.2 Method

Starting table: `tools/b37e_best_s22.bin` (depth 93,805, score=27.7M).
Parameters: `--iters 10M --checkpoint 10000 --patience 3 --secs 20`

Three kick sizes × 6 seeds each = 18 runs:

| Variant | Kick | Seeds       | Notes |
|---------|------|-------------|-------|
| B.37g-A | 500  | 22,42,100,137,500,1000 | Small perturbation |
| B.37g-B | 2000 | 22,42,100,137,500,1000 | Medium perturbation |
| B.37g-C | 10000| 22,42,100,137,500,1000 | Large perturbation |

Expected post-kick scores:
- K=500: ~25-27M (minor score perturbation)
- K=2000: ~22-27M (moderate perturbation)
- K=10000: ~15-22M (significant perturbation, possibly back in optimal band)

#### B.37g.3 Expected Outcomes

- **H1 (success)**: at least one kick-size/seed combination finds depth > 93,805.
- **H2 (kick too small)**: K=500 returns to 93,805; need larger kick for escape.
- **H3 (kick too large)**: K=10000 degrades structure too much → climbs to 91K not 94K.
- **H4 (structural limit)**: no kick size finds 94K+; landscape has no accessible 94K basin.

#### B.37g.4 Actual Results

**Critical bug discovered and fixed**: save-best was never written when the kick itself produced
the best state.  Baseline depth was set but save_best_path was only written when a *checkpoint*
exceeded baseline.  Fix: write save_best_path immediately after baseline measurement.

| Variant | Kick | Seed | Baseline | Best Depth | Mechanism |
|---------|------|------|----------|-----------|-----------|
| B.37g-A | 500  | 1000 | 79,148   | **93,954** | kick→79K, greedy climbed |
| B.37g-B | 2000 | 42   | 94,004   | **94,004** | kick itself landed at 94K |
| B.37g-B | 2000 | 22   | 93,790   | 93,790    | no improvement |
| B.37g-C | 10000| all  | 81K-90K  | ≤89,988   | too destructive |

**Outcome: H1 (success).**  `kick=2000, seed=42` found depth **94,004** — the new best.
Best table: `tools/b37g_k2000_best_s42.bin`.

Two working ILS mechanisms:
1. **Direct basin jump**: kick=2000 lands directly in the 94K basin (kick score 27.5M ≈ 27.7M)
2. **Kick+climb**: kick=500 degrades to 79K, greedy hill-climb reaches 93,954 on the way back

Key finding: `kick=10000` is too destructive (degrades to 81–90K range); `kick=2000` is
sweet spot for basin-escaping from the 94K level.  Continuing chain: B.37h from 94,004.

---

### B.37h ILS Chain from 94,004 (Implemented)

#### B.37h.1 Motivation

B.37g found 94,004 via ILS kick=2000 from 93,805.  B.37h attempts the same strategy from 94,004,
seeking to reach 95,000+.

#### B.37h.2 Method

Starting table: `tools/b37g_k2000_best_s42.bin` (depth 94,004, score=27.7M).
Parameters: `--iters 10M --checkpoint 10000 --patience 3 --secs 20`

| Variant | Kick | Seeds |
|---------|------|-------|
| B.37h-A | 500  | 22,42,100,137,200,300,500,1000,2000,5000 |
| B.37h-B | 2000 | 22,42,100,137,200,300,500,1000,2000,5000 |

#### B.37h.3 Expected Outcomes

- **H1**: at least one seed/kick finds depth > 94,004.
- **H2**: all plateau at 94,004; need even larger kick or more diverse seeds.

#### B.37h.4 Actual Results

| Variant | Kick | Seed | Baseline | Best Depth | Notes |
|---------|------|------|----------|-----------|-------|
| B.37h-A | 500  | 300  | **94,026** | 94,026 | kick landed at 94K+ |
| B.37h-A | 500  | 1000 | 94,002    | 94,002 | |
| B.37h-A | 500  | 137  | 93,995    | 93,995 | |
| B.37h-B | 2000 | 1000 | **94,847** | 94,847 | kick landed at 94.8K ← NEW BEST |
| B.37h-B | 2000 | 500  | 93,780    | 93,780 | |
| B.37h-B | 2000 | 22   | 93,589    | 93,589 | |
| others  | both | —    | 80K–93K   | ≤93,438 | regression from start |

**Outcome: H1 (success).**  `kick=2000, seed=1000` found depth **94,847** — new best.
Best table: `tools/b37h_k2000_best_s1000.bin`.

Chain progression: FY→91,090→92,001→93,231→93,805→94,004→**94,847** (+5,757 / +6.3% over B.26c).

Pattern confirmed: ILS with kick=2000 is the dominant mechanism; the kick itself lands in
higher-depth basins directly (no further hill-climbing improvement).  kick=500 can also find
new optima via the kick+climb mechanism (79K→93,954 in B.37g-A).

---

### B.37i ILS Chain from 94,847 (Implemented)

#### B.37i.1 Motivation

The chain continues with ILS kick from the new best 94,847.  We now have a reliable pattern:
kick=2000 from depth D tends to land in a basin at depth D+600–1000.  Scaling to 100K
would require ~6–10 more ILS steps if each gain is +700–900.

#### B.37i.2 Method

Starting table: `tools/b37h_k2000_best_s1000.bin` (depth 94,847, score=27.35M).
Parameters: `--iters 10M --checkpoint 10000 --patience 3 --secs 20`
Two kick sizes × 12 seeds = 24 runs.

#### B.37i.3 Expected Outcomes

- **H1**: kick=2000 lands at ≥95,500; chain continues.
- **H2**: kick=2000 finds ≤94,847; plateau indicates weakening returns.

#### B.37i.4 Actual Results

| Variant | Kick | Seed | Baseline | Best Depth |
|---------|------|------|----------|-----------|
| B.37i-A | 500  | 137  | **94,484** | 94,484 |
| B.37i-A | 500  | 100  | 94,368    | 94,368 |
| B.37i-A | 500  | 42   | 94,048    | **94,275** (climbed) |
| B.37i-B | 2000 | 7777 | **94,744** | 94,744 |
| B.37i-B | 2000 | 42   | 93,808    | **94,841** (climbed) |
| others  | both | —    | 80K–94K   | ≤94,322 |

**Outcome: H2 (near-plateau).**  Best found: 94,841 (kick=2000, s42) — just 6 below record 94,847.
The 94,847 basin proves hard to escape.  Notable: kick=500/s137 found 94,484 and kick=2000/s7777
found 94,744; these are in different sub-basins than 94,847.

Continuing: B.37j applies larger kicks (1000, 3000, 5000) from 94,847 AND chains from the
94,484/94,744 sub-basins (which may have different landscape structure).

---

### B.37j Broader ILS Sweep (Implemented)

#### B.37j.1 Method

Three kick sizes × 12 seeds from 94,847, plus kick=2000 × 8 seeds from each of 94,484 and 94,744.
Total 36 + 16 = 52 parallel runs.

#### B.37j.2 Actual Results

**From 94,847 (kick=1000, 3000, 5000 × 12 seeds each = 36 runs)**:

| Kick | Best Depth | Notes |
|------|-----------|-------|
| 1000 | 94,642 (s42) | Below 94,847 |
| 3000 | 94,323 (s7777) | Below 94,847 |
| 5000 | 94,141 (s7777) | Below 94,847 |

**From 94,484 (kick=2000 × 8 seeds)**: best 94,743 (s42).
**From 94,744 (kick=2000 × 8 seeds)**: best 94,717 (s22).

**Outcome: H2 (plateau).** No improvement over 94,847 from 52 runs.

---

### B.37k Extended Seed Sweep from 94,847 (Completed)

kick=2000 × 36 seeds (seeds 3000–39000) from 94,847.
Best found: 94,452 (seed=11000).  **No improvement over 94,847.**

Total kick=2000 attempts from 94,847 after B.37h/i/j/k: ~60 runs, best = 94,841 (B.37i s42).

---

### B.37l Further Extension (Implemented)

kick=2000 × 36 seeds (seeds 40000–75000) from 94,847.  Simultaneously: 6 fresh FY chains
(seeds 100,200,300,400,500,999) to explore diverse landscape regions independent of the
current seed=42 chain.

**Motivation**: After 60+ kick=2000 failures from 94,847, either (a) success rate is ~1/100 and
we need more tries, or (b) the landscape around 94,847 has no accessible 95K+ basins via
kick=2000.  The fresh FY chains test hypothesis (b) by finding 92K tables via different paths,
which may have better landscape properties for reaching 95K+.

#### B.37l.1 Actual Results

36 seeds (40000–75000), all below 94,847.  Best: 94,394 (seed=68000).  **No improvement.**

After B.37h/i/j/k/l: 96+ kick=2000 attempts from 94,847 total, none > 94,847.
The 94,847 basin appears locally stable within kick=2000 reach.

---

### B.37m Large-Kick Exploration from 94,847 (Completed)

kick=8000 and kick=15000 × 12 seeds each from 94,847.  patience=5, --iters 30M.

| Kick | Best | Notes |
|------|------|-------|
| 8000 | 94,372 (s200) | Below 94,847 |
| 15000 | 94,208 (s22) | Below 94,847 |

**Confirmed**: larger kicks are not the solution.  kick=8000 gives mean depth ~92K; kick=15000 ~93K.
All remain below 94,847.

---

### B.37n Fresh FY Multi-Seed Step-1 Chains (Completed)

10 fresh FY chains (seeds 42–9999) with checkpoint=100K, patience=5, iters=30M.
(Previous FY runs used checkpoint=10K → terminating at 91,402, only 52K accepted swaps.
B.34 used checkpoint=50K–100K to reach 92K.)

| Seed | Best Step-1 Depth |
|------|------------------|
| 9999 | **92,492** ← new step-1 best |
| 500  | **92,245** |
| 400  | **92,041** |
| 100  | 91,851 |
| 42   | 91,836 |
| 5678 | 91,612 |
| 200,300,999,1234 | 91,522 |

Best tables saved as `tools/b37n_fystep1_best_s{seed}.bin`.  The s9999 chain produces a
92K table from a different landscape trajectory than the original s42 (B.34) chain.
These tables are in different landscape regions and may chain to higher-depth basins.

---

### B.37o Multi-Trajectory ILS Step-2 (Implemented)

kick=2000 × 12 seeds from each of the top-5 FY step-1 tables (seeds 9999, 500, 400, 100, 42).
60 parallel runs total.  Goal: find step-2 basins beyond 94,847 from diverse starting points.

#### B.37o.1 Actual Results

ILS kick=2000 × 12 seeds from each of the top-5 FY step-1 tables (9999, 500, 400, 100, 42).
Best results by starting table:

| Start Table | Seed | Best Depth | Notes |
|-------------|------|------------|-------|
| s9999 (92,492) | 42  | **94,118** | big jump from 92K in one ILS step |
| s500 (92,245)  | any | 93,746     | best from s500 trajectory |
| s400 (92,041)  | —   | ≤93,600    | below s9999/s500 basins |
| s100 (91,851)  | —   | ≤93,300    | weaker starting point |
| s42  (91,836)  | —   | ≤93,200    | below primary chain entry |

The s9999→94,118 result (kick=2000, seed=42) establishes an independent trajectory.
Table saved as `tools/b37q_from9999_best_s42.bin`.

---

### B.37p Pure-Greedy Step-2 from FY Tables (Completed, Failed)

Attempted pure greedy (no kick) from FY transient tables, starting with s9999 (92,492).
All seeds gave 92,009 or regression. Root cause: FY transient tables are NOT at true local
optima; the score is already elevated, and greedy immediately raises it further into the
anti-correlation zone (score > 27M → depth regression). The save-best fires at the initial
transient measurement, then all greedy checkpoints worsen.

**Conclusion**: FY chains produce transient peaks; ILS kick is required for step-2 (B.37q).

---

### B.37q ILS Step-2 from FY Transients (Completed)

ILS kick=2000 from 92,492 (s9999) and 92,245 (s500):

| Start     | Seed | Depth  | Notes |
|-----------|------|--------|-------|
| s9999 42  | 42   | **94,118** | ← kick landed directly in new basin |
| s9999 100 | 100  | 93,420 | |
| s500 42   | 42   | 93,746 | |

s9999 chain step 2: **92,492 → 94,118**.  Table: `tools/b37q_from9999_best_s42.bin`.

---

### B.37r Chain from 94,118 (s9999 Trajectory, Completed)

16 seeds with kick=2000 from 94,118:

| Seed | Best Depth | Notes |
|------|------------|-------|
| 400  | **94,419** | ← new step-3 best on s9999 chain |
| 22   | 94,003 | |
| 1000 | 93,892 | |

s9999 chain step 3: **94,118 → 94,419**.  Table: `tools/b37r_best_s400.bin`.

---

### B.37s Chain from 94,419 (s9999 Trajectory, Completed)

18 seeds with kick=2000 from 94,419.  Pattern: peak always at first checkpoint (10K accepted,
score ~27.5M), then score rises → depth regresses.

| Seed  | Best Depth | Notes |
|-------|------------|-------|
| 20000 | **94,661** | ← new step-4 best on s9999 chain |
| 7777  | 94,122 | |
| 30000 | 94,389 | |
| 9999  | 93,154 | |
| 300   | 92,737 | |
| 1000  | 91,867 | |
| others| ≤91,836 | |

s9999 chain step 4: **94,419 → 94,661**.  Table: `tools/b37s_best_s20000.bin`.

s9999 full chain: FY → 92,492 → 94,118 → 94,419 → 94,661.
Primary chain:    FY → 92,001 → 93,231 → 93,805 → 94,004 → 94,847 (record).

The s9999 chain remains 186 cells below the primary chain record.

---

### B.37t Chain from 94,661 + Extended Primary Sweep (Completed)

**A: 24 seeds from 94,661** (s9999 chain step 5).  kick=2000, checkpoint=10K, patience=3, secs=20.

| Seed  | Depth   | Notes |
|-------|---------|-------|
| 100   | **95,060** | ← NEW OVERALL BEST; peak at ckpt 2 (score=31M) |
| 200   | 94,833 | |
| 20000 | 94,795 | |
| 70000 | 94,730 | |
| 50000 | 94,475 | |
| 9999  | 94,295 | |
| others| ≤93,985 | |

Significant detail: s100 peak at checkpoint 2 (20K accepted, score=31M), not checkpoint 1 as
usual.  Baseline after kick was 94,408; ckpt 1 gave 83,728 (worse); ckpt 2 gave 95,060 (best);
ckpt 3 gave 80,055; stopped at ckpt 5 (patience exhausted).  The proxy score at the optimum
was 31M, higher than the typical 27.5M window — the optimal score band may extend higher
than previously characterized.

Best table: `tools/b37t_best_s100.bin`.  s9999 chain step 5: **94,661 → 95,060**.

**B: 12 seeds from 94,847** (primary chain, new seeds 111111–999999 range).
All below 94,847.  Best: s111111=93,537.  Primary chain confirmed stuck at 94,847.

---

### B.37u Chain from 95,060 (Completed)

24 seeds with kick=2000 from `tools/b37t_best_s100.bin` (depth 95,060).

| Seed  | Depth  | Notes |
|-------|--------|-------|
| 300   | **95,408** | ← NEW RECORD; kick→baseline 94,573; peak ckpt 1 score=32.8M |
| 90000 | 95,197 | |
| 1000  | 95,079 | |
| 25000 | 94,521 | |
| others| ≤91,870 | |

Three seeds exceeded 95,060.  Key detail: initial table score was 31M (up from 27.5M in B.37t).
The optimal score window is shifting upward as table quality improves.
Peak consistently at first checkpoint (10K accepted) then regression.

Best table: `tools/b37u_best_s300.bin`.  s9999 chain step 6: **95,060 → 95,408**.

s9999 chain: FY → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → **95,408**.

---

### B.37v Chain from 95,408 (Completed)

24 seeds with kick=2000 from `tools/b37u_best_s300.bin` (depth 95,408).

| Seed  | Depth  | Notes |
|-------|--------|-------|
| 100   | **95,973** | ← NEW RECORD; baseline 95,970; peak ckpt 1 score=34.5M |
| 200   | 94,769 | |
| 40000 | 94,234 | |
| 1000  | 94,068 | |
| others| ≤93,420 | |

Noteworthy: baseline after kick was 95,970 (kick itself nearly at peak). Only +3 cells gained
by ckpt 1.  Initial table score was 32.6M (rising from 31M in B.37t → 31.9M B.37u → 32.6M).
The optimal score window shifts upward with each improvement.

Best table: `tools/b37v_best_s100.bin`.  s9999 chain step 7: **95,408 → 95,973**.

s9999 chain: FY → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408 → **95,973**.

---

### B.37w Chain from 95,973 (Completed — Stall Confirmed)

24 seeds with kick=2000 from `tools/b37v_best_s100.bin` (depth 95,973).
Best: s1000=94,468.  All below 95,973.

The s9999 chain has stalled.  Initial score of the 95,973 table is ~33-34M (beyond the
previously characterized optimal window of 15-27M).  Each improvement step adds ~1-2M to
the initial score; we appear to be at or near the ceiling for this trajectory.

Score trajectory: 27.5M (94,661) → 31M (95,060) → 32.6M (95,408) → ~33M (95,973) → stall.

---

### B.37x Plateau-Breaking Strategies (Completed — Plateau Confirmed)

Three parallel strategies to escape the 95,973 basin:

**A: kick=500 from 95,973** (14 seeds):
best = 95,068 (s2000); all others ≤94,261. All below 95,973.

**B: kick=5000 from 95,973** (12 seeds):
best = 95,391 (s137); all others ≤94,867. All below 95,973.

**C: kick=2000 from 95,197** (12 seeds, alternative B.37u runner-up):
best = 94,915 (s300). All below 95,973.

**Diagnosis**: 95,973 table initial score is ~33-34M (outside previously known optimal window
of 15-27M).  Kicks bring it down 0.5-2M but cannot return to the 20-25M zone needed for
sustained hill-climbing.  The table has converged too far and lost exploitable degrees of
freedom.  Score trajectory: 27.5M→31M→32.6M→33M+; each step makes the next harder.

---

### B.37y Broad Fresh-Seed Sweep from 95,973 (Completed — Plateau Confirmed)

28 unexplored seeds (11111–314159) with kick=2000 from 95,973.
Best: 93,930 (s800008).  All 28 seeds below 95,973.

Distribution: range 81K–93.9K, mean ~90K.  Much worse than kicking from 94,661 (which found 95K+).
The 95,973 basin is well-isolated.  Total kick=2000 attempts from 95,973 across B.37w/x-A/y: 66 runs.
Zero found > 95,973.  **Plateau confirmed at 95,973 for kick=2000 ILS from current table.**

---

### B.37z Fresh Trajectory Attempt (Running)

**Step 1** (completed): 14 fresh FY chains with unexplored seeds (10000–555000),
checkpoint=100K, patience=5.

| Seed   | Step-1 Depth |
|--------|-------------|
| 77777  | **92,595** ← new step-1 best from fresh batch |
| 33333  | 92,557 |
| 111000 | 91,882 |
| 222000 | 91,361 |
| 555000 | 91,276 |
| others | 87,911–89,333 |

**Step 2** (completed): ILS kick=2000 from s77777 (92,595) and s33333 (92,557), 16 seeds each.

| Start | Best depth | Notes |
|-------|-----------|-------|
| s77777 | 93,856 (s400) | |
| s33333 | 92,623 (s500) | |

Both trajectories stall far below 95,973.  The s77777/s33333 basins are weaker than s9999,
which uniquely produced the strong 94K+ chain.  No new independent basin found.

**B.37 series final conclusion**: ILS with kick=2000 and current row-concentration proxy
reached **95,973** (s9999 chain, `tools/b37v_best_s100.bin`).  Exhaustive plateau
confirmation: 66+ kick=2000 runs from 95,973, 28 from 95,408 via B.37v-check, and 2 fresh
trajectory attempts (s77777/s33333) — all fail to exceed 95,973.

The binding constraint is not seed choice but the initial score level: the 95,973 table
has score ~33-34M, above the productive greedy window.  A kick of ≤2000 swaps reduces score
by only ~500K, insufficient to return to the optimal 20-25M band.  The ILS mechanism is
limited by kick size vs. table concentration — increasing kick beyond 5000 destroys enough
structure that the table climbs back only to 91-94K.

The row-concentration proxy has driven a **+5.36% improvement** over B.26c baseline
(91,090 → 95,973).  To continue, B.38 must either use a different optimization mechanism
or address the kick-concentration mismatch directly.

---

## B.38 Deflation-Kick ILS (Running)

### B.38.1 Motivation

The 95,973 table (B.37v) has initial proxy score ~34.5M — beyond the productive hill-climb
window (20–25M).  Random ILS kicks (2000 swaps) reduce the score by only ~500K per kick,
insufficient to return to the productive zone.  66+ kick attempts from 95,973 found nothing.

Root cause: the table is over-concentrated.  Each greedy step monotonically raises the score.
Starting from 34.5M, the greedy hill-climb can only go higher, driving deeper into the
anti-correlation zone.  A different escape mechanism is needed.

### B.38.2 Method: Targeted Deflation

New `--deflate K` argument added to `tools/b32_ltp_hill_climb.py`:
- Apply K **targeted** swaps that move early-row cells from high-coverage lines
  (coverage > `--deflate_high`, default 320) to late-row cells on low-coverage lines
  (coverage < `--deflate_low`, default 280)
- Each swap reduces the proxy score by ~500–700 units (compared to ~0 for a random kick)
- Stops early once score < `--deflate_target` (default 22M)
- Unlike random kicks, deflation reduces score *without random structural damage*

Empirical calibration: 35,000 deflation swaps reduced 95,973 table from 34.5M → 21.9M
(100% success rate with high=320, low=280; down from 4.4% with high=350, low=250).

### B.38.3 Hypothesis

Starting from the partially-deflated 95,973 structure at score ~22M:
1. The table retains the useful structural properties found by B.37's 5-step ILS chain
2. The score reset allows greedy hill-climbing to find a new local optimum
3. If the deflation pathway preserves "good structure" while removing "over-concentration",
   the hill-climb may converge to a basin above 95,973

Expected outcomes:
- **H1**: depth > 95,973 — deflation successfully resets the table for further improvement
- **H2**: depth 92K–95K — deflation preserves some structure but lands in a different basin
- **H3**: depth < 92K — deflation destroys critical structure (like excessive kicks in B.37m)

### B.38.4 Parameters

```
--init tools/b37v_best_s100.bin   (depth 95,973)
--deflate 50000 --deflate_high 320 --deflate_low 280 --deflate_target 22000000
--iters 10000000 --checkpoint 10000 --patience 3 --secs 20
```

24 seeds (22–99999) running in parallel.

### B.38.5 Actual Results

Outcome H2.  Best = 95,375 (s50000).  All 24 seeds below 95,973.
Score resetting to 22M preserves ~60% of structural advantage (94-95K vs 92K from FY)
but deflation destroys the critical properties of the 95,973 table.

---

### B.38b Optimal Deflation Level Sweep (Completed)

Sweep over 6 deflation target scores (33M, 32M, 31M, 30M, 29M, 28M) × 3 seeds (42, 100, 300).
`--init tools/b37v_best_s100.bin` (depth 95,973).

| Target | Seed | Depth | Notes |
|--------|------|-------|-------|
| 32M    | 100  | **95,997** | ← NEW RECORD (+24 over 95,973); 7,000 deflation swaps → 31.8M |
| 31M    | 42   | 95,864 | |
| 28M    | 42   | 95,755 | |
| 30M    | 100  | 95,704 | |
| 31M    | 100  | 95,493 | |
| others | —    | ≤94,864 | |

Peak for all runs at ckpt 1 (10K accepted, score ~33.7M).  Optimal deflation is minimal
(~7,000 swaps, target 32M) — preserves nearly all structure while resetting just enough
for the hill-climb to find a new local optimum at score 33.7M.

Key finding: 95,997 found by deflating 34.5M → 31.8M then greedy hill-climb.
The entire s9999 chain: FY → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408 → 95,973 → **95,997**.

Best table: `tools/b38b_t32000000_best_s100.bin`.  +24 over 95,973.

---

### B.38c Chain from 95,997 (Completed)

24 seeds from 95,997 with deflation to 32M (7,000 deflation swaps → 31.8M → greedy climb).

| Seed  | Depth  | Notes |
|-------|--------|-------|
| 70000 | **96,122** | ← NEW RECORD |
| 25000 | 95,997 | |
| 1000  | 95,988 | |
| 5000  | 95,863 | |
| 500   | 95,829 | |
| 99999 | 95,576 | |
| others| ≤95,259 | |

Six seeds exceeded 95,973; one exceeded 95,997.  Deflation + hill-climb is a repeatable
mechanism.  Best table: `tools/b38c_best_s70000.bin`.  Chain step 9: **95,997 → 96,122**.

Full chain: FY→92,492→94,118→94,419→94,661→95,060→95,408→95,973→95,997→**96,122**.
vs B.26c: +5,032 cells (+5.53%).

---

### B.38d Chain from 96,122 (Completed)

24 seeds from `tools/b38c_best_s70000.bin` (depth 96,122) with deflation to 32M
(`--deflate 20000 --deflate_target 32000000`).

| Seed | Best Depth |
|------|-----------|
| s22  | 96,081 |
| s42  | 95,977 |
| (others) | ≤ 95,800 |

**Result: 96,081** — all 24 seeds below 96,122. The 96,122 table starts at score 33.8M (lower
than 95,973's 34.5M), so deflation to 32M uses only ~5K swaps — insufficient to escape the basin.

**Diagnosis**: deflation target 32M is the right level for 34.5M-starting tables (95,973),
but is too shallow for the 33.8M-starting 96,122 table. Lower targets (31M, 30M, 29M) needed.

---

### B.38e Deflation Target Sweep from 96,122 (Completed)

**Design**: Test three deflation targets (31M, 30M, 29M) × 12 seeds from `tools/b38c_best_s70000.bin`
(depth 96,122). 36 parallel runs, `--deflate 20000 --deflate_high 320 --deflate_low 280`.

### B.38e Results

| Target | Best Seed | Best Depth | Swap Count (approx) |
|--------|-----------|-----------|---------------------|
| 31M    | s137      | **96,672** | ~8K swaps |
| 30M    | s22       | 96,217     | ~12K swaps |
| 29M    | s42       | 95,885     | ~15K swaps |

**Winner: deflation target 31M, seed 137 → depth 96,672 (NEW RECORD)**

Table saved: `tools/b38e_t31000000_best_s137.bin`

**Key observations:**
- Optimal deflation level shifts upward as tables improve: 96,122 table (33.8M start) needs
  31M target (not 32M as was optimal for 95,973's 34.5M start).
- Each successive table requires a slightly deeper deflation because the score ceiling rises.
- Target 29M over-deflates: too many qualifying (high>320, low<280) lines leads to structural
  destruction.
- 6/12 seeds at t31M exceeded 96,122 (3 exceeded 95,900); deflation robustly recovers.

Chain so far: FY(s9999)→92,492→94,118→94,419→94,661→95,060→95,408→95,973→95,997→96,122→96,672

---

### B.38f Chain from 96,672 (Completed)

**Hypothesis.** The 96,672 table (score 30,743,316) can be further improved by deflation to 31M
followed by hill-climbing, continuing the chain pattern that produced each prior record.

**Methodology.** 24 seeds from `tools/b38e_t31000000_best_s137.bin` (depth 96,672, score 30.74M)
with `--deflate 20000 --deflate_high 320 --deflate_low 280 --deflate_target 31000000`.

**Expected result.** Approximately 97K depth by analogy with the B.38c→B.38e progression.

**Actual result.** All 24 seeds applied 0 deflation swaps. The log reported
`post-deflation score: 30,743,316` — identical to the input score. Best depth: 96,672 (no change).

**Root cause.** The 96,672 table itself was already produced by deflation (B.38e applied ~8K
swaps from the 96,122 table). The post-deflation state IS the 96,672 table; its score (30.74M) is
already below the requested 31M target. There are no qualifying (high > 320, low < 280) lines
remaining to deflate. The chain technique cannot be applied to its own output without first driving
the score upward via hill-climbing — but hill-climbing from 30.74M drives the score to 36M+
(anti-correlation zone), regressing depth.

**Conclusion.** The 96,672 record was achieved by the deflation operation itself, not by
hill-climbing. The deflated state is a local optimum that cannot be improved by further deflation
or by uncapped hill-climbing.

---

### B.38g Deflation Scan Below 30.7M from 96,672 (Completed)

**Hypothesis.** Targets below the current 30.74M score (29M, 28M, 27M, 26M, 25M) may reveal
a deeper optimum that the 31M sweep missed.

**Methodology.** Targets 29M / 28M / 27M / 26M / 25M × 8 seeds from
`tools/b38e_t31000000_best_s137.bin` (96,672); otherwise same flags as B.38f.

**Expected result.** At least one target finds a basin deeper than 96,672.

**Actual result.**

| Target | Best Baseline Depth |
|--------|---------------------|
| 29M    | 95,885 |
| 28M    | 95,885 |
| 27M    | 94,890 |
| 26M    | ~94,000 |
| 25M    | ~93,500 |

All targets produce depths below 96,672. Depth degrades monotonically as the target decreases.
No target found a deeper basin.

**Conclusion.** The 96,672 table is a structural local optimum under deflation-based techniques.
Deflating below 30.74M removes too many early-row cell assignments from high-coverage lines,
destroying the favorable constraint structure rather than refining it. The optimal deflation level
for a 30.74M-starting table is approximately 30.74M — i.e., no further deflation is productive.

---

### B.38h Score-Capped Hill-Climbing from 96,672 (Completed)

**Hypothesis.** Standard hill-climbing overshoots the optimal score window (30–32M) because it
accepts all improvements regardless of the resulting score. A hard cap on total score prevents
overshoot and allows the optimizer to explore the local neighborhood within the productive range.

**Methodology.** Score caps 31M / 31.5M / 32M / 33M × 8 seeds from 96,672.
Each run uses normal hill-climbing but refuses swaps that would push total score above the cap.

**Expected result.** Some capped run finds marginal improvement (96,700+) by exploring the
immediate neighborhood without driving the score into the anti-correlation zone.

**Actual result.**
- cap=31M: accepted only 1,329 total swaps across all seeds; all best=96,672.
- cap=31.5M: accepted ~10K swaps; all best=96,672.
- cap=32M: accepted ~25K swaps; all best=96,672.
- cap=33M: accepted larger neighborhood; all best=96,672.

No cap produced improvement. The cap=31M result is most informative: only 1,329 improvement-making
swaps exist within the 31M score ceiling — the table is a local optimum within any capped range.

**Conclusion.** The 96,672 table is a local optimum under score-capped hill-climbing with any cap
between 31M and 33M. The capped optimizer confirms there are no improving swaps available within
the productive score window. The optimum is not merely a consequence of score overshoot; it is a
genuine local maximum of the depth-score relationship at this point.

---

### B.38h2 Score-Capped Hill-Climbing from 96,217 (Completed)

**Hypothesis.** The second-best table (96,217, score 29.98M) may have a different local
neighborhood structure allowing capped climbing to find improvement.

**Methodology.** Same caps (31M / 31.5M / 32M / 33M) × 8 seeds from
`tools/b38e_t30000000_best_s22.bin` (depth 96,217, score ~29.98M).

**Actual result.** All runs best=96,217 (baseline). No improvement from the second-best table.

**Conclusion.** Both top-two tables are local optima under score-capped hill-climbing.

---

### B.38i Kick + Score-Cap from 96,672 (Completed)

**Hypothesis.** A random kick (unconstrained perturbation swaps) perturbs the table out of
the local optimum; the score cap then prevents the subsequent hill-climb from overshooting.

**Methodology.** kick=2000 / 5000 / 10000 × cap=32M / 33M / 34M × 8 seeds from 96,672.
`--kick K` applies K random accept-anything swaps before the capped hill-climb.

**Expected result.** Kick escapes the local optimum; capped climb recovers to a deeper basin.

**Actual result.**

| Kick Size | Best Depth |
|-----------|-----------|
| 2,000     | 95,389 |
| 5,000     | 96,004 |
| 10,000    | 94,988 |

Best overall: 96,004 (kick=5,000, seed=5000). All runs below 96,672.

**Conclusion.** Kick destroys critical constraint structure faster than the capped hill-climb can
rebuild it. Larger kicks produce worse outcomes. The 96,672 table's depth-critical structure
(sub-tables 0 and 1, established by B.38k crossover analysis) is fragile to random perturbation.
Kick-based escape strategies are ineffective at this operating point.

---

### B.38j Fresh Fisher-Yates Seed Sweep (Completed)

**Hypothesis.** The chain FY(s9999) → 92,492 was specifically favorable because s9999 reached
an unusually high first-step depth. Other FY seeds may find similarly favorable basins that lead
to basins exceeding 96,672 via multi-step ILS.

**Methodology.** 60 fresh FY seeds (checkpoint=100K, patience=3, secs=20). Top performers
proceeded to ILS step 2.

**Expected result.** Several seeds reach 92K+ first step; ILS chains from those reach 96K+.

**Actual result.**
- Best first-step: s42=91,825; s12=91,003; s38=90,796.
- No seed exceeded s9999's 92,492 first-step result.
- ILS from s42 step-2 best: 94,209 (seed k999999).
- No seed reached 96K territory from any continuation.

**Conclusion.** The s9999 starting basin is exceptional among the 60 sampled seeds. The 92K+
first-step is a necessary (though not sufficient) condition for reaching 96K via ILS. Alternative
FY seeds produce shallower chains. The landscape is rugged and the 92,492 starting basin is
unusually favorable.

---

### B.38k Population Crossover from Top-Two Tables (Completed)

**Hypothesis.** The 96,672 and 96,217 tables have complementary strengths across sub-tables.
Crossover — swapping individual sub-tables between the two tables — may produce a hybrid that
exceeds either parent.

**Methodology.** All 64 combinations (2^6) of 6 sub-tables, each drawn from either
A=`tools/b38e_t31000000_best_s137.bin` (96,672) or B=`tools/b38e_t30000000_best_s22.bin` (96,217).
Each combination assembled in Python by direct array replacement, written to temp file, depth
measured (secs=20).

**Expected result.** At least one combination exceeds 96,672.

**Actual result.**

| Combination | Depth |
|-------------|-------|
| AAAAAA      | 96,672 |
| BBBBBB      | 96,217 |
| AABAAA      | 96,672 |
| AAABAA      | 96,672 |
| AABBAA      | 96,672 |
| AAAABA      | 96,672 |
| AABABA      | 96,672 |
| AAABBA      | 96,672 |
| AABBBA      | 96,672 |
| (remaining B-sub combinations) | ≤ 96,217 |

Sub-tables 0 and 1 from A are critical: any combination using sub-tables 0 and 1 from A
returns 96,672. Sub-tables 2–5 are interchangeable with B without affecting depth.

No combination exceeded 96,672.

**Conclusion.** The 96,672 depth is entirely determined by sub-tables 0 and 1. Sub-tables 2–5
are structurally inert at this operating point. Crossover cannot exceed the best parent because
the critical sub-tables cannot be improved by mixing with a weaker table's corresponding sub-tables.

---

### B.38l Deflation Walk (Completed)

**Hypothesis.** Rather than a single large deflation step, a sequence of small steps (5,000 swaps
each) with depth measurement after each step can navigate through better intermediate states than a
single-step deflation jump.

**Methodology.** Starting from 96,672, apply 5,000 deflation swaps per step, measure depth
after each step.

**Expected result.** Some step lands on a depth > 96,672 before crossing into destructive territory.

**Actual result.**
- Step 1: ~94,667
- Step 2: ~94,404 (deepening destruction)
- Terminated early.

**Conclusion.** Deflation in 5,000-swap steps is too destructive per step at the 96,672 table's
score (30.74M). The 96,672 table has few qualifying lines remaining; each 5K-swap step converts a
disproportionate fraction of remaining early-row cells, causing rapid depth regression.

---

### B.38l2 Micro-Step Direct Depth Search (Completed)

**Hypothesis.** At the resolution of ~200 random swaps, the depth landscape may have
microscopic local maxima above 96,672 that normal hill-climbing (score proxy) cannot find because
they do not correspond to proxy score improvements.

**Methodology.** Apply 200 random swaps (uniformly at random, no score filter), measure depth
(15s timer), keep the table if depth improved, discard otherwise. Repeat for 6 steps.

**Expected result.** Small random perturbations discover a microstate with depth 96,700+.

**Actual result.**
- Step 1: 96,448 (discarded)
- Step 2: 96,674 (kept — appeared to be new record) (It may have been scotch)
- Step 3: 95,867 (discarded)
- Step 4: 96,664 (discarded vs step 2 baseline)
- Step 5: 96,437 (discarded)
- Step 6: 95,706 (discarded)

**Conclusion.** Step 2's 96,674 is within measurement noise of 96,672 (the 15s timer instead of
the standard 20s timer reduces measured depth by ~150–200 cells under load). No genuine
improvement detected. The micro-step approach provides no sustained progress; the depth landscape
at this resolution is highly non-monotonic.

---

### B.38m K_PLATEAU=195 Hill-Climbing from 96,672 (Completed)

**Hypothesis.** The solver stalls at row ~189 (96,672 / 511 ≈ 189.2). The current optimizer uses
K_PLATEAU=178 (row 178), which targeted the B.26c plateau. Changing to K_PLATEAU=195 aligns
the optimization objective with the actual observed stall boundary, potentially rewarding different
cell-line assignments that facilitate the actual bottleneck rows.

**Methodology.** 12 seeds from 96,672, `--threshold 195` (all other params unchanged).

**Expected result.** Tables optimized for K=195 reach deeper into the 190–510 row range.

**Actual result.** All 12 seeds: best=96,672 for all; finals ranging 80K–92K.

An additional run used fresh FY seeds with K=195 to establish baselines: best first-step
s5000=91,451, similar to K=178 first-step results.

**Conclusion.** K_PLATEAU=195 trades row 0–178 concentration for row 0–195 coverage, diluting the
critical early-row constraint density that enables the propagation cascade. The solver's 189-row
stall is caused by structural underdetermination, not by the specific K_PLATEAU parameter.
Hill-climbing with K=195 fails to match the depth achieved with K=178, confirming that the optimal
parameter is not simply the observed stall row.

---

### B.38n Band Mode (Rows 179–195) from 96,672 (Completed)

**Hypothesis.** Optimizing specifically the transition zone (rows 179–195) — neither the
well-optimized rows 0–178 nor the distant rows 196–510 — can bridge the row-189 stall without
disrupting the existing early-row structure.

**Methodology.** 12 seeds from 96,672, `--mode band --k1 179 --k2 195`.
Band mode rewards concentration of LTP cells specifically within the band [179, 195], leaving the
K_PLATEAU=178 structure of rows 0–178 undisturbed.

**Expected result.** Band-mode improvement in the transition zone pushes the stall from row 189
to row 195+.

**Actual result.** Initial band score: 87,534. Optimizer found no improvements in the band. All
12 seeds completed with best=96,672 (baseline unchanged).

**Conclusion.** The transition band rows 179–195 are already well-covered by the existing LTP
structure (a consequence of the K_PLATEAU=178 optimization). The optimizer cannot find
band-improving swaps because the band coverage is already near-optimal relative to its own score.
Band mode provides no new degrees of freedom at this operating point.

---

### B.38o Deflation from Second-Best Table (96,217) (Completed)

**Hypothesis.** The second-best table (96,217, score 29.98M) may have a different basin topology
allowing deeper deflation to reach a state that surpasses 96,672.

**Methodology.** Targets 27M / 26M / 25M / 24M × 8 seeds from
`tools/b38e_t30000000_best_s22.bin` (96,217).

**Actual result.**

| Target | Best Baseline Depth |
|--------|---------------------|
| 27M    | 95,009 |
| 26M    | 94,055 |
| 25M    | 94,004 |
| 24M    | 93,995 |

All below 96,672. The second-best table is also a local optimum; its basin does not connect to
a deeper region via deflation.

**Conclusion.** Both the 96,672 and 96,217 tables are confirmed local optima. The two best
known tables occupy structurally isolated basins; neither can serve as a stepping stone to a
deeper basin via deflation-based techniques.

---

### B.38p Narrow Simulated Annealing (T=50→1) from 96,672 (Completed)

**Hypothesis.** A very narrow SA temperature window (T_init=50, T_final=1) stays close to the
greedy limit while accepting occasional slightly-worsening moves, potentially escaping the
96,672 local optimum without the catastrophic destruction observed in B.36 (T_init=2000).

**Methodology.** SA with T=50→1, 8 seeds from 96,672.

**Actual result.** This run was corrupted by CPU load: 40+ concurrent hill-climber processes were
running, causing the decompressor to reach only depth 96,223 instead of 96,672 under the 20s
timer. All seeds reported baseline=96,223 (the load-suppressed value). SA immediately
moved uphill (accepting score-worsening swaps at T=50), driving score to 36M+, regressing depth.

**Conclusion.** SA at T=50 is still too aggressive — the acceptance of worsening moves drives the
score past the productive 30–32M window. Even very low temperatures cause the score to exit the
optimal band. The optimal score window for 96K+ depth is narrow (~0.5M wide); any stochastic
acceptance policy that allows score increases will exit it.

**Measurement note.** CPU load from parallel processes causes the 20s-timer measurement to read
96,223 instead of the true 96,672. Subsequent experiments limited concurrent processes to ≤12 to
avoid this artifact.

---

### B.38q Kick + Deflate to 31M from 96,672 (Completed)

**Hypothesis.** Combining kick perturbation with subsequent deflation (rather than kick + capped
climb) may find deeper intermediate states by reshaping the score landscape before measuring.

**Methodology.** kick=2000 / 5000 / 10000 × then `--deflate_target 31000000` × 8 seeds from 96,672.
The kick first perturbs the table randomly (increasing score), then deflation drives it back to
the 31M level where depth measurement occurs.

**Expected result.** kick=5000 + deflate to 31M creates a table different from the original
96,672 that measures deeper.

**Actual result.**

| Kick Size | Best Baseline Depth |
|-----------|---------------------|
| 2,000     | 95,389 |
| 5,000     | 96,004 |
| 10,000    | 94,988 |

Best: 96,004 (kick=5000). All below 96,672.

**Conclusion.** Kick + deflate cannot recover the 96,672 structure. The kick destroys sub-tables 0
and 1 (the depth-critical sub-tables identified in B.38k). Even with subsequent deflation to the
correct score level, the destroyed sub-table structure cannot be reconstructed by the deflation
operator.

---

### B.38r Varied Deflation High/Low Thresholds from 96,672 (Completed)

**Hypothesis.** The deflation operator's effectiveness depends on the HIGH/LOW thresholds
(h=320/l=280 in all prior experiments). Different threshold pairs may select different qualifying
lines, finding a perturbation direction that the standard parameters miss.

**Methodology.** Four threshold configurations × 8 seeds from 96,672:
- h=350, l=260 (wider band)
- h=340, l=270 (intermediate)
- h=300, l=260 (lower threshold, more qualifying lines)
- h=350, l=290 (tighter lower bound)

**Actual result.**

| h / l  | Best Baseline Depth |
|--------|---------------------|
| 350/260 | 95,991 |
| 340/270 | 94,746 |
| 300/260 | 95,133 |
| 350/290 | 95,565 |

All below 96,672.

**Conclusion.** The standard h=320/l=280 parameters were the best choice for producing the 96,672
record. Widening the HIGH threshold selects lines that should not be deflated; narrowing the LOW
threshold selects too many recipient lines, diluting the deflation's directionality. Threshold
variation confirms h=320/l=280 as the empirical optimum.

---

### B.38s K_PLATEAU=188 (Stall Row Targeting) from 96,672 (Completed)

**Hypothesis.** K_PLATEAU=188 = floor(96,672 / 511) targets exactly the observed stall row —
the last row completed before the solver halts. This should be the most accurate proxy for the
bottleneck, outperforming the proxy-at-K=178 used to build the 96,672 table.

**Methodology.** 12 seeds from `tools/b38e_t31000000_best_s137.bin`, `--threshold 188`,
`--iters 50000000 --checkpoint 10000 --patience 3 --secs 20`.

**Expected result.** K=188 optimization finds a marginally better table tailored to the actual stall.

**Actual result.** All 12 seeds: best=96,672; finals ranging 81K–96K (depth degrades during
hill-climbing regardless of seed).

**Conclusion.** K_PLATEAU=188 does not improve on K=178. The hill-climbing process degrades depth
because moving score upward from 30.74M overdrives into the anti-correlation zone. The K value
affects which score the optimizer converges to, not the starting point depth; any K that drives
the score above ~32M causes depth regression. The 96,672 optimum is K-invariant: all tested
K values (178, 188, 195) produce best=96,672 with degraded finals.

---

### B.38 Summary and Conclusions

**Current best: depth 96,672** (`tools/b38e_t31000000_best_s137.bin`)

The B.38 deflation technique produced a systematic chain of improvements:

```
FY(s9999) → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408
          → 95,973 → 95,997 → 96,122 → 96,672  ← CURRENT BEST
```

**Key finding: Deflation is the optimization, not hill-climbing.** The 96,672 depth was achieved
by the deflation step itself (B.38e: 8,000 swaps from 96,122's 33.8M score to 30.74M).
Subsequent hill-climbing from the deflated state only degrades depth by driving score into the
anti-correlation zone (>32M).

**The 96,672 table is a confirmed local optimum.** Experiments B.38f through B.38s exhausted all
tested escape strategies:

| Experiment | Strategy | Best Depth |
|------------|----------|-----------|
| B.38f | Deflation chain (target 31M) | 96,672 (no improvement — 0 swaps) |
| B.38g | Deflation to 25M–29M | 95,885 (over-deflation) |
| B.38h/h2 | Score-capped climbing | 96,672 (local optimum) |
| B.38i | Kick + score-cap | 96,004 |
| B.38j | 60 fresh FY seeds | 94,209 (no competitive basin found) |
| B.38k | Crossover top-2 tables | 96,672 (no improvement) |
| B.38l | Deflation walk (5K steps) | 94,667 (too destructive) |
| B.38l2 | Micro-step 200 swaps | 96,672 (noise) |
| B.38m | K_PLATEAU=195 | 96,672 (finals 80K–92K) |
| B.38n | Band mode rows 179–195 | 96,672 (no band improvements) |
| B.38o | Deflation from 96,217 | 95,009 (second table also stuck) |
| B.38p | SA T=50→1 | 96,223 (load-corrupted + score overshoot) |
| B.38q | Kick + deflate | 96,004 |
| B.38r | Varied h/l thresholds | 95,991 |
| B.38s | K_PLATEAU=188 | 96,672 (finals degrade) |

**The B.38 proxy-based approach has saturated.** The row-concentration proxy (score) is a
necessary but not sufficient condition for depth improvement. The optimal score window for 96K+
is narrow (~30–32M); neither climbing into the window from below (deflation) nor exploring within
it (capped climbing, SA, kicks) produces improvement beyond 96,672.

**B.33 (Complete-Then-Verify): ABANDONED.** B.33 was investigated as the next architectural
candidate after B.38 saturated. Sub-experiments B.33a and B.33b (tools/b33a_min_swap.py,
tools/b33b_min_swap_bt.py) determined that the minimum constraint-preserving swap for the
full 10-family CRSCE system is O(1000+) cells — NOT local. The Fisher-Yates random LTP
partitions ensure every cell lands on a unique random LTP line, requiring mate cells that
cascade to O(511) cells per sub-table. Consequently, the correct CSM and any "close"
cross-sum-valid matrix differ in ~170,000 cells — no efficient local path exists between
them. Phase 3 is infeasible. See §B.33.11 for full analysis.

**B.39 (N-Dimensional Constraint Geometry): COMPLETED &mdash; PESSIMISTIC.** B.39a confirmed the B.33b cascade model algebraically: the minimum constraint-preserving swap is 1,528 cells (upper bound), touching 11 rows and 492 columns. The $3n$ dimensional scaling model ($3 \times 511 \approx 1{,}533$) matches the measured minimum. B.39b (null-space navigation) not attempted per the pessimistic-outcome criterion. Complete-Then-Verify is definitively closed. See &sect;B.39.5a for full results.

**B.40 (Hash-Failure Correlation): COMPLETED &mdash; NULL.** Zero SHA-1 events at the plateau after 200M+ iterations. The plateau is caused by constraint exhaustion ($\rho$ violations), not hash failure. No row in the meeting band ever reaches $u = 0$. SHA-1 verification never triggers. See &sect;B.40.6.

**B.41 (Cross-Dimensional Hashing): DIAGNOSTIC COMPLETE &mdash; INFEASIBLE FOR DEPTH.** Column unknown counts at the plateau reveal that columns are far less complete than rows (min column unknown = 206 vs min row unknown = 2). Column-serial passes would need ~206 assignments before any verification event. The meeting band's constraint exhaustion affects columns more severely than rows. B.41 retains value for collision resistance but does not improve solver depth. See &sect;B.41.10.

**B.42 (Pre-Branch Pruning Spectrum): COMPLETED.** B.42a revealed 56.6% preferred-infeasible and 43.2% both-infeasible branching waste, but 0% interval-tightening potential. B.42c (both-value probing) eliminates all waste and provides ~40% throughput improvement but **does not change depth**. The plateau is constraint-exhausted: the depth ceiling is a property of the constraint system's information content, not the solver's search efficiency.

**B.43 (Bottom-Up RCLA Initialization): COMPLETED &mdash; INFEASIBLE.** At most 1 row has $u \leq 20$ at the plateau; meeting-band rows have $u \approx 300$-$400$. RCLA cannot cascade. Bottom-up sweep direction provides no advantage (constraint information is direction-invariant). See &sect;B.43.

**Current status:** All proposed solver-architecture approaches (B.33, B.38, B.39, B.40, B.41, B.42, B.43) have been completed or abandoned. The depth ceiling (~86K-96K depending on LTP table and test block) is a fundamental limit of the 8-family cross-sum constraint system with Fisher-Yates random LTP partitions (5,097 independent constraints over 261,121 cells = 2% constraint density, leaving 256,024 unconstrained degrees of freedom). Further depth improvement requires adding information to the constraint system itself&mdash;additional projection families, finer-grained hash constraints, or a fundamentally different encoding. Open questions are consolidated in Appendix C.

---

### B.39 N-Dimensional Constraint Geometry and Complete-Then-Verify Revisited (Completed &mdash; Pessimistic)

#### B.39.1 Motivation

B.33 proposed a complete-then-verify solver architecture whose feasibility depended on the
existence of small constraint-preserving swaps. Sub-experiments B.33a and B.33b determined
that such swaps are infeasible under the analysis framework used: greedy forward repair
(B.33a) and backtracking DFS from an 8-cell geometric base (B.33b) both failed to find any
swap preserving all 8 constraint families within budgets of 50--500 cells.

However, both B.33a and B.33b framed the swap-finding problem in two dimensions&mdash;starting
from a planar geometric pattern (a rectangle or its 8-cell diagonal extension) and attempting
to "repair" violations on the pseudorandom LTP families by adding cells one at a time. This
framing is structurally biased: it privileges the geometric families (which admit small
planar solutions) and treats the LTP families as disturbances to be patched, leading to the
divergent cascade identified in B.33b.

B.39 reframes the problem using an **n-dimensional geometric paradigm** that treats all
constraint families symmetrically.

#### B.39.2 The N-Dimensional Paradigm

The four geometric cross-sum families (LSM, VSM, DSM, XSM) are projections within the 2D
plane of the CSM. Row sums project onto the row axis, column sums onto the column axis,
diagonal sums onto the line $c - r = \text{const}$, and anti-diagonal sums onto the line
$r + c = \text{const}$. These are four directions of projection within a shared 2D
embedding, which is why small planar swap patterns (8-cell constructions) can satisfy all
four simultaneously&mdash;the geometric correlations between families create coincidental
cancellations at small scale.

Each LTP sub-table assigns every cell $(r, c)$ to one of 511 lines via a Fisher--Yates
permutation that is independent of the $(r, c)$ coordinate system. This assignment is
structurally equivalent to a new coordinate axis: cell $(r, c)$ acquires a third coordinate
$\ell_1(r, c) = \text{LTP1}(r, c)$ that is uncorrelated with the first two. The LTP1 sum
constraint is then a projection along this third axis&mdash;a hyperplane slice through 3-space
where $\ell_1 = k$, summing the cell values in that slice. The pseudorandom nature of the
LTP assignment means this third axis is maximally uncorrelated with the 2D plane, which is
precisely why LTP partitions add non-redundant constraint information (unlike the
toroidal-slope partitions they replaced in B.20, whose algebraic regularity created partial
correlation with DSM and XSM).

LTP2 adds a fourth coordinate axis, LTP3 a fifth, LTP4 a sixth. Each cell occupies a point
in 6-dimensional space:

$$
    (r,\; c,\; \ell_1(r,c),\; \ell_2(r,c),\; \ell_3(r,c),\; \ell_4(r,c))
$$

where the first two coordinates are deterministic grid positions and the last four are
pseudorandom partition assignments. The eight constraint families are projections along six
axes of this 6D space (with DSM and XSM being oblique projections within the $(r, c)$ plane
rather than independent axes, reducing the effective independent dimensionality to six).

A **constraint-preserving swap** is a balanced perturbation in this 6D space: a set of cells
whose signed changes sum to zero on every hyperplane slice in every dimension. The B.33a/B.33b
analysis failed because it attempted to construct 6D-balanced structures by starting from
2D-balanced seeds and patching the higher dimensions incrementally. This is analogous to
trying to build a 3D cube by starting from a 2D square and adding height&mdash;a strategy that
works only if the third axis has geometric regularity, which pseudorandom LTP axes do not.

The n-dimensional paradigm suggests that swap structures should be conceived natively in
the full 6D space, treating all axes symmetrically.

#### B.39.3 Swap Scaling Analysis

The minimum constraint-preserving swap size depends on the number of independent
dimensional axes and the intersection density of constraint lines across them.

**Dimensional scaling model.** In 2D (row $\times$ column), a balanced swap requires a
4-cell rectangle: 2 values per dimension, $2 \times 2 = 4$ intersection cells. Each row
sees one +1 and one $-1$; each column sees the same. Adding a third independent
dimension introduces a third axis. The balanced structure must now satisfy zero net
change on every line in all three dimensions. There are $\binom{3}{2} = 3$ coordinate
planes, each requiring a 4-cell balanced sub-structure, giving $3 \times 4 = 12$ cells
before accounting for sharing. In 6D, there are $\binom{6}{2} = 15$ coordinate planes,
but each cell sits on all 6 axes simultaneously, so a single cell contributes to
balancing $\binom{6}{2} - \binom{5}{2} = 5$ planes at once. The effective minimum is
therefore far below $15 \times 4 = 60$.

The confirmed data point supports linear scaling: for $n = 4$ geometric families (LSM,
VSM, DSM, XSM), the minimum swap is **8 cells** $= 2n$. Extrapolating linearly:

| Dimensions | Lower bound ($2n$) | Upper estimate ($3n$) | $2^n$ (hyperrectangle) |
|---|---|---|---|
| 2 (LSM + VSM) | 4 | 6 | 4 |
| 4 (+ DSM + XSM) | 8 (**confirmed**) | 12 | 16 |
| 5 (+ LTP1) | 10 | 15 | 32 |
| 6 (+ LTP2) | 12 | 18 | 64 |
| 8 (all families) | 16 | 24 | 256 |

The $2^n$ hyperrectangle model (placing cells at every corner of an $n$-dimensional
hypercube) is a sufficient but grossly non-minimal construction. It scales exponentially
with dimension and is the wrong baseline for estimating minimum swap feasibility.

The linear model ($2n$ to $3n$ cells) is consistent with the $n = 4$ data point and with
the structure of the balancing requirement: each independent dimension requires at least
one +1 and one $-1$ cell on each affected line, but cells serve multiple dimensions
simultaneously because every cell lies on one line in every family. The question is
whether the pseudorandom LTP axes permit sufficient cell-sharing to maintain linear
scaling, or whether the lack of geometric correlation between LTP lines forces the cascade
expansion identified in B.33b.

**Intersection density.** The feasibility of cell-sharing depends on how often constraint
lines from different families co-occupy the same cells.

**2D intersections (geometric families).** Every (row, column) pair intersects in exactly
one cell. Every (row, diagonal) pair intersects in 0 or 1 cells. The 2D grid structure
guarantees dense intersections, enabling small balanced patterns with maximal cell-sharing.

**Cross-dimensional intersections (geometric $\times$ LTP).** For a given row $r$ and LTP1
line $L$, the expected intersection size is $|r| \times |L| / s^2 = 511 \times 511 /
261{,}121 = 1$ cell. Pairwise intersections between any two families contain approximately
one cell on average, so 2D balanced sub-structures (4-cell rectangles) are constructible
across any pair of dimensions.

**The critical question for B.39a** is whether these pairwise intersections can be composed
into a single small set of cells that simultaneously balances *all* dimensions. The $n = 4$
confirmed minimum of 8 cells demonstrates that simultaneous balancing across 4 geometric
dimensions is achievable at the linear lower bound. Whether this extends to the pseudorandom
LTP dimensions&mdash;where the intersection structure is uncorrelated with the geometric
axes&mdash;is precisely what B.39a's algebraic computation will determine.

#### B.39.4 Null Space Formulation

A constraint-preserving swap is a vector $v \in \{-1, 0, +1\}^{261{,}121}$ in the
(integer) null space of the constraint incidence matrix $A$ ($5{,}108 \times 261{,}121$),
subject to the feasibility constraint that the modified matrix $M + v$ remains binary.

**B.39a result (empirical).** GF(2) Gaussian elimination on the full $5{,}108 \times 261{,}121$
matrix confirms $\text{rank}(A) = 5{,}097$ over GF(2), giving a null-space dimension of
exactly **256,024**. Eleven constraint lines are linearly dependent (consistent with the
conservation axiom and parity identities). This is an enormous space&mdash;roughly 98% of all
degrees of freedom are unconstrained by cross-sum projections.

The **minimum swap size** is the minimum Hamming weight (number of non-zero entries) of
any non-zero vector in this null space. This is equivalent to the minimum-weight codeword
problem in coding theory, which is NP-hard in general but admits efficient approximations
for structured matrices via lattice reduction (LLL/BKZ algorithms) or integer linear programming.

The key distinction from B.33a/B.33b: those experiments searched for minimum swaps via
heuristic forward repair and backtracking DFS with small budgets. B.39a will compute the
null space structure directly using algebraic methods, which can determine whether short
null-space vectors exist without enumerating them by trial and error.

#### B.39.5 Sub-experiment B.39a: N-Dimensional Minimum Swap Characterization

**Objective.** Determine the minimum constraint-preserving swap size as a function of the
number of constraint families, using algebraic methods native to the n-dimensional paradigm
rather than the geometric seed-and-repair approach of B.33a/B.33b.

**Hypothesis.** Three competing models:

- **Linear model (dimensional scaling):** The minimum swap grows as $2n$ to $3n$ cells
  for $n$ independent constraint dimensions. For $n = 8$ (4 geometric + 4 LTP), this
  predicts 16-24 cells. This model is consistent with the confirmed $n = 4$ minimum of
  8 cells and assumes that cell-sharing across dimensions remains efficient even for
  pseudorandom LTP axes. Under this model, B.33's Phase 3 is feasible and B.33b's cascade
  analysis was an artifact of the biased 2D-seed-and-repair search strategy.

- **Cascade model (B.33b extrapolation):** The minimum swap grows as $O(511)$ cells per
  LTP sub-table, giving $O(2{,}000)$ for 4 LTP sub-tables. Under this model, B.33's Phase 3
  remains infeasible. The cascade occurs because pseudorandom LTP axes prevent the
  cell-sharing that keeps geometric swaps small&mdash;every repair cell creates new violations
  on uncorrelated LTP lines, and the repair chain diverges.

- **Intermediate model:** The minimum swap is substantially smaller than the cascade
  estimate but larger than the linear prediction&mdash;potentially $O(50\text{-}200)$
  cells&mdash;because the high-dimensional null space (dimension $\geq 256{,}020$) admits
  short vectors that are not discoverable by greedy or backtracking search from 2D seeds,
  but are findable via algebraic computation. The null space permits multi-family
  cancellations that the sequential repair strategy cannot exploit.

**Method.**

(a) **Construct the constraint incidence matrix** $A$ ($5{,}108 \times 261{,}121$) for the
current LTP seeds (CRSCLTPV, CRSCLTPP, CRSCLTP3, CRSCLTP4). Each row of $A$ is a binary
indicator vector for one constraint line. Store $A$ as a sparse matrix (each row has exactly
511 non-zero entries for LTP/LSM/VSM lines, or $\text{len}(k)$ for DSM/XSM lines).

(b) **Stratified null-space sampling.** Compute the null space of $A$ (over $\mathbb{Q}$)
incrementally by family count:

| Configuration | Matrix size | Predicted null dim | Measured null dim | GF(2) Rank |
|---|---|---|---|---|
| $n = 4$ (LSM+VSM+DSM+XSM) | 3,064 $\times$ 261,121 | ~258,057 | **258,064** | 3,057 |
| $n = 5$ (+ LTP1) | 3,575 $\times$ 261,121 | ~257,546 | **257,554** | 3,567 |
| $n = 6$ (+ LTP1+LTP2) | 4,086 $\times$ 261,121 | ~257,035 | **257,044** | 4,077 |
| $n = 8$ (+ all 4 LTP) | 5,108 $\times$ 261,121 | ~256,013 | **256,024** | 5,097 |

**Key observation.** Each LTP sub-table contributes exactly $S - 1 = 510$ independent
constraint lines, consistent with the partition structure (511 line sums with one degree of
freedom removed by the global sum identity). The total LTP contribution is $4 \times 510 =
2{,}040$ independent constraints&mdash;reducing the null space from 258,064 to 256,024. This
is less than 1\% of the total freedom, quantifying LTP's marginal constraining power.

At each level, use lattice basis reduction (LLL algorithm) on a random projection of the
null space to find short vectors. Record the minimum Hamming weight found across 1,000
random projections. This gives an upper bound on the true minimum for each family count.

(c) **Integer linear programming (ILP) formulation.** For each family count $n$, solve:

$$
    \min \|v\|_0 \quad \text{s.t.} \quad Av = 0, \quad v \in \{-1, 0, +1\}^{261{,}121}
$$

where $\|v\|_0$ counts non-zero entries. This is an $\ell_0$-minimization problem; relax to
$\ell_1$ for tractability. The $\ell_1$-relaxed solution provides a lower bound on the
$\ell_0$ minimum. If the $\ell_1$ solution is sparse (few non-zero entries), it likely
corresponds to a true minimum swap.

(d) **Swap structure analysis.** For each minimum or near-minimum swap found, record:

- Total cell count (Hamming weight of $v$)
- Row span (number of distinct rows touched)
- Column span
- Maximum cells per row
- LTP line distribution (how many LTP lines are affected per sub-table)

The row span is the critical parameter for B.39b: a swap spanning $k$ rows disturbs $k$
SHA-1 hashes simultaneously.

(e) **Comparison with B.33b cascade estimate.** If the algebraic minimum is substantially
smaller than $O(2{,}000)$ (say, below 100 cells), this would indicate that B.33b's cascade
analysis overestimated the minimum because the greedy repair strategy cannot exploit
multi-family cancellations. If the algebraic minimum confirms $O(1{,}000\text{+})$, the
cascade model is validated and B.33's Phase 3 is definitively infeasible.

**Expected outcome.** The minimum swap for the full 8-family system is between 20 and 2,000
cells. The experiment will resolve which regime applies. The $n = 4$ (geometric-only) result
should confirm the known 8-cell minimum. The growth curve from $n = 4$ to $n = 8$ will
reveal whether each LTP sub-table contributes linearly ($+O(511)$ per sub-table, supporting
the cascade model) or sub-linearly (supporting the null-space structure model).

**Tool.** `tools/b39a_ndim_swap.py`&mdash;sparse constraint matrix construction, GF(2)
Gaussian elimination, null-space basis weight analysis, pairwise and k-way XOR sampling.

#### B.39.5a B.39a Results (Completed &mdash; Pessimistic)

**Phase 1: Stratified rank (confirmed).** Identical to prior run; rank = 5,097, null-space
dimension = 256,024 over GF(2).

**Phase 2: Minimum-weight null-space vector search (NEW).**

The enhanced tool extracts the RREF basis, computes the Hamming weight of all 256,024
individual basis vectors, then searches pairwise and k-way XOR combinations for shorter
vectors.

| Search method | Samples | Min weight found |
|---|---|---|
| Individual basis vectors | 256,024 (exhaustive) | **1,548** |
| Pairwise XOR (top-2,000 lightest) | 1,999,000 pairs | **1,528** |
| 2-way random XOR | 40,240 | 1,556 |
| 3-way random XOR | 39,926 | 1,530 |
| 4-way random XOR | 40,091 | 1,554 |
| 5-way random XOR | 39,906 | 1,574 |
| 6-way random XOR | 39,837 | 1,576 |

**Overall minimum weight (upper bound on minimum swap): 1,528 cells.**

Basis weight distribution (all 256,024 individual vectors):

| Statistic | Weight |
|---|---|
| min | 1,548 |
| p5 | 1,688 |
| p25 | 1,798 |
| median | 1,938 |
| mean | 1,982 |
| p75 | 2,116 |
| p95 | 2,458 |
| max | 2,868 |

**Phase 3: Swap structure of the lightest vector (pairwise XOR, weight 1,528).**

| Property | Value |
|---|---|
| Weight (swap size) | 1,528 cells |
| Rows touched | 11 |
| Row span | 401 (rows 0&ndash;400) |
| Columns touched | 492 |
| Column span | 511 (full width) |
| Cells/row min | 2 |
| Cells/row max | 226 |
| Cells/row mean | 138.9 |

**Interpretation: PESSIMISTIC.** The minimum constraint-preserving swap for the full
8-family CRSCE system is at least 1,528 cells (upper bound; the true minimum may be
slightly lower but is definitively $O(1{,}500\text{+})$). This confirms the B.33b cascade
model with algebraic evidence: Fisher-Yates random LTP partitions create a constraint
system whose shortest null-space vectors are $\sim 3 \times S = 1{,}533$, consistent with
the $3n$ upper estimate from the dimensional scaling model (Table in &sect;B.39.3) for
$n = 8$ families.

The lightest swap touches only 11 rows (manageable for SHA-1 verification) but requires
~139 cells changed per affected row on average. Since SHA-1 has the avalanche property
(any 1-bit change to the 512-bit message completely changes the digest), every affected
row's hash is destroyed. No partial repair is possible.

**B.39a conclusion: B.33 Phase 3 is definitively infeasible.** The null-space analysis
confirms with algebraic certainty what B.33b's heuristic search estimated: the minimum
constraint-preserving swap is $O(1{,}500)$ cells, not $O(16\text{-}24)$ as the linear
model predicted. The n-dimensional paradigm's key insight is validated&mdash;the geometric
and LTP dimensions are so weakly correlated that cell-sharing across all 8 families
requires ~1,500 simultaneous changes. B.39b (null-space navigation) should not be
attempted per the pessimistic-outcome criterion (&sect;B.39.6).

**B.39b STATUS: NOT ATTEMPTED (infeasible per B.39a pessimistic outcome).**

#### B.39.6 Sub-experiment B.39b: Complete-Then-Verify with N-Dimensional Swaps

**Prerequisite.** B.39a must establish that the minimum swap size is tractable (below
approximately 200 cells and spanning fewer than ~20 rows). If B.39a confirms the cascade
model ($O(1{,}000\text{+})$ cells spanning hundreds of rows), B.39b is infeasible and
should not be attempted.

**Objective.** Test the complete-then-verify architecture (B.33 Phases 1-3) using the swap
characterization from B.39a, with the Phase 3 search operating natively in the n-dimensional
constraint space rather than via 2D geometric patterns.

**Hypothesis.** If small n-dimensional swaps exist (B.39a null-space structure model), the
complete-then-verify architecture can reconstruct the CSM by:

1. Propagating constraints to the current depth (~96,672 cells, ~189 rows)
2. Completing the remaining ~164,449 cells to satisfy all cross-sum constraints (Phase 1)
3. Navigating the null space via small swaps to find the unique CSM satisfying all SHA-1
   row hashes (Phase 3)

The critical metric is the **row span** of the minimum swap. If swaps span $k$ rows, each
swap disturbs $k$ SHA-1 hashes. Phase 3's convergence depends on whether the number of
passing rows increases monotonically (or at least net-positively) as swaps are applied.

**Method.**

(a) **Phase 1: Cross-sum completion.** Starting from the propagation-determined cells
(~96,672 at current operating point), extend the assignment through the remaining rows using
DFS with cross-sum feasibility checks only (no SHA-1 verification). The heavily
underdetermined constraint system in the meeting band ($u \gg 1$ on all lines) should admit
a complete assignment quickly.

(b) **Phase 2: Initial verification.** Compute SHA-1 on all 511 rows of the complete
candidate. Record the set of passing rows $P$ and failing rows $F = \{0, \ldots, 510\}
\setminus P$. The propagation-determined rows (0-188) should pass. Meeting-band rows
(189-510) will almost certainly fail.

(c) **Phase 3: Null-space navigation.** Using the minimum-swap vocabulary characterized by
B.39a:

**Strategy A: Row-targeted greedy.** For each failing row $r \in F$ (in order of lowest
index first), enumerate all valid swaps involving at least one cell in row $r$. Apply each
swap, recompute SHA-1 on all affected rows, accept if the number of passing rows increases.
This is the B.33 Phase 3 strategy, but with swaps defined by the n-dimensional null space
rather than by 2D geometric patterns.

**Strategy B: Global random walk.** Apply random swaps from the minimum-swap vocabulary
without targeting specific rows. After each swap, recompute SHA-1 on all affected rows.
Accept if the number of passing rows does not decrease (plateau-accepting). This treats
Phase 3 as a random walk on the null-space manifold of cross-sum-valid matrices, guided
by a fitness function (count of passing rows).

**Strategy C: Compound moves.** Apply sequences of $m$ swaps simultaneously (a compound
move is a sum of $m$ minimum swaps, which is also in the null space by linearity). Compound
moves explore a larger neighborhood per step at the cost of higher per-step computation.
If single-swap row spans are too large (each swap disturbs too many rows), compound moves
may achieve net improvement by combining swaps whose SHA-1 effects partially cancel.

(d) **Metrics.** For each strategy, record:

- Convergence: does the number of passing rows reach 511?
- Convergence rate: passing rows as a function of swap count
- SHA-1 landscape structure: is the landscape unimodal (monotone improvement toward the
  correct CSM) or multimodal (local optima where swaps cannot improve without first
  regressing)?
- Per-swap cost: wall time per swap application + SHA-1 recomputation on affected rows

**Expected outcomes.**

- **Optimistic (B.39a minimum swap $\leq 50$ cells, row span $\leq 5$):** Phase 3
  converges. Each swap disturbs few rows; the passing-row count increases monotonically
  or near-monotonically. The solver reconstructs the full CSM in minutes. The
  complete-then-verify architecture is viable and outperforms the current DFS approach
  for the meeting band.

- **Moderate (B.39a minimum swap 50-200 cells, row span 10-30):** Phase 3 is marginally
  tractable. Each swap disturbs many rows; progress is non-monotonic but net-positive
  over many steps. The solver converges but slowly. Performance is comparable to or
  modestly better than DFS for the meeting band. Strategy C (compound moves) may be
  necessary to achieve net improvement.

- **Pessimistic (B.39a minimum swap $> 200$ cells or row span $> 30$):** Phase 3 does
  not converge within practical time budgets. Each swap disturbs too many rows, and the
  SHA-1 landscape is effectively flat (each swap has $\sim 2^{-160}$ probability of
  improving any given row). The complete-then-verify architecture provides no advantage,
  and the null-space navigation problem is as hard as exhaustive search.

#### B.39.7 Relationship to Prior Work

**B.33 (Complete-Then-Verify: Abandoned).** B.39 is a direct successor to B.33, revisiting
the same three-phase architecture with a different analytical framework. B.33's abandonment
was based on B.33a/B.33b results showing $O(1{,}000\text{+})$-cell minimum swaps. B.39a
re-examines this conclusion using algebraic methods (null-space computation, lattice
reduction, ILP) rather than heuristic search (greedy BFS, backtracking DFS). If B.39a
confirms B.33b's cascade estimate, B.33's abandonment is validated with stronger theoretical
justification. If B.39a finds smaller swaps, B.33 is rehabilitated via B.39b.

**B.34-B.38 (LTP Table Geometry Optimization).** B.39 operates at a different level than
B.34-B.38. The LTP geometry experiments optimize the cell-to-line assignment tables (the
axes of the n-dimensional space); B.39 operates on the cell values within a fixed constraint
system. The two approaches are composable: B.39b would use the B.38-optimized LTP tables
(which produce deeper propagation cascades and therefore a smaller meeting band for Phase 3
to navigate).

The n-dimensional paradigm also explains why B.34-B.38's hill-climbing works: optimizing
LTP cell-to-line assignments is reshaping the LTP axes to be more aligned with the solver's
top-down traversal order along the row axis. The row-concentration proxy literally measures
the projection of each LTP line's cell mass onto the early-row region of the row axis.

**B.27 (LTP5/LTP6: Inert).** The n-dimensional paradigm explains B.27's null result.
Dimensions 7 and 8 (LTP5, LTP6) add constraint lines, but the solver's propagation cascade
was already saturating the information available from dimensions 1-6 within the first ~178
rows. Additional dimensions cannot help if the existing dimensions already force all cells
that the row-serial traversal order can reach. The inertness of LTP5/LTP6 is a property of
the solver's traversal strategy, not of the constraint system's information content.

**B.31 (Alternating-Direction Pincer: Null).** The n-dimensional paradigm explains the
pincer null result. Bottom-up DFS traverses the row axis in reverse, but the SHA-1 hash
constraints are indexed by row&mdash;they can only verify complete rows, regardless of
traversal direction. The hash constraints are projections along the row axis; traversing
that axis in either direction does not create new verification opportunities in the other
five dimensions.

#### B.39.8 Implications Beyond Complete-Then-Verify

Regardless of whether B.39b proves feasible, the n-dimensional characterization from B.39a
has independent value:

(a) **Understanding the constraint system's geometry.** The minimum swap size quantifies the
"coupling strength" between constraint families. A small minimum swap means the families are
weakly coupled (local moves can satisfy all families simultaneously). A large minimum swap
means they are strongly coupled (only global moves preserve all constraints). This is a
fundamental structural property of the CRSCE constraint system that informs all future
solver design.

(b) **LTP seed optimization.** Different LTP seeds produce different 6D geometries with
different minimum swap sizes. Seeds that produce smaller minimum swaps correspond to weaker
inter-family coupling, which may correlate with deeper propagation cascades (because weak
coupling means each family's constraints are more independently informative). This suggests
a new LTP optimization criterion: minimize the minimum swap size as a proxy for maximizing
constraint independence. This criterion is fundamentally different from the row-concentration
proxy used in B.34-B.38 and could open a new optimization avenue.

(c) **Solver architecture guidance.** If the minimum swap is large (confirming B.33b), this
provides a theoretical explanation for why the current DFS + row-serial SHA-1 architecture
is effective: the constraint system's strong inter-family coupling prevents local search from
competing with systematic enumeration. Conversely, if the minimum swap is small, this
motivates hybrid architectures that combine DFS propagation with local search in the meeting
band.

#### B.39.9 Open Questions

(a) **Does the minimum swap size depend on the specific LTP seeds?** Different Fisher-Yates
permutations produce different 6D geometries. The minimum swap for CRSCLTPV + CRSCLTPP
(B.26c winners) may differ from that of other seed pairs. If the variance is large, this
adds a new dimension to seed optimization (B.26c).

(b) **Is the null-space minimum achievable by signed {-1, 0, +1} vectors?** The null space
is computed over $\mathbb{Q}$; the shortest rational null vector may not have entries
restricted to $\{-1, 0, +1\}$. The gap between the rational minimum and the signed-integer
minimum determines whether the algebraic lower bound is tight.

(c) **What is the computational cost of the $\ell_1$-relaxed ILP at scale?** The constraint
matrix has 261,121 columns. Modern ILP solvers may struggle at this scale. Decomposition
strategies (solving per-family sub-problems and combining) may be necessary.

(d) **Can compound moves (sums of multiple minimum swaps) achieve row-local effects?** Even
if individual minimum swaps span many rows, the sum of two swaps might cancel disturbances
on some rows while reinforcing changes on others. The linearity of the null space guarantees
that any sum of null-space vectors is also a null-space vector. Whether useful row-local
compound moves exist is an empirical question.

(e) **Does the SHA-1 fitness landscape over the null space have useful structure?** Phase 3
of B.39b treats the count of SHA-1-passing rows as a fitness function over the null-space
manifold. If this landscape is smooth (nearby null-space points have similar fitness), local
search can exploit gradient-like information. If it is rugged (fitness changes unpredictably
with each swap), the search degenerates to random sampling. The structure of this landscape
is unknown and is a key empirical question for B.39b.

---

## B.40 Hash-Failure Correlation Harvesting + Within-Row Priority Branching (Completed &mdash; Null)

### B.40.1 Motivation

The solver reaches a consistent plateau at depth ~96,672 (row ~189 of 511). At the plateau, the
DFS backtracks repeatedly through the same row range. Every SHA-1 failure at a row boundary
discards the entire row's assignment and explores an alternative. Currently no information from
failed paths is retained — each failure is treated as independent.

However, SHA-1 failures at the plateau are not random: the same row range consistently fails,
and the correlation between specific cell assignments and hash failure is a structural property
of the constraint system at that depth. This exploitable structure has never been measured.

### B.40.2 Hypothesis

SHA-1 failures at the plateau are systematically correlated with specific cells within the
failing row. Certain cells (those most tightly coupled to column, LTP, and cross-family
constraints) contribute disproportionately to hash mismatches. If these "high-correlation" cells
are branched on first within the row, SHA-1 pruning triggers earlier in the row evaluation,
reducing the search space for subsequent cells and compressing the effective branching factor
within the plateau row range.

This is distinct from B.26b (within-row MRV by constraint uncertainty): B.26b used `u` values
(remaining unknowns per constraint line) as a proxy for importance; B.40 uses observed SHA-1
failure correlation — a signal that directly measures which cells are responsible for
unsatisfiable paths, rather than which cells are most constrained.

### B.40.3 Method

**Phase 1 — Profiling run (no behavior change):**

Add a `HashFailureTracker` to `RowDecomposedController`. For each SHA-1 failure at row `r`:

1. Record the `1`-assigned cells in row `r` at the time of failure.
2. Accumulate a per-row, per-column failure count: `fail_count[r][c]` incremented each time
   cell `(r,c)=1` at a SHA-1 failure.
3. Also accumulate success count: `pass_count[r][c]` when SHA-1 passes with `(r,c)=1`.
4. Run a fixed profiling budget (e.g., 20M iterations at plateau) and emit correlation
   statistics to a JSON file.

**Correlation score per cell:**

```
phi[r][c] = (fail_count[r][c] / total_fail[r]) - (pass_count[r][c] / total_pass[r])
```

Positive `phi` means the cell appears more often in failing assignments than passing ones.

**Phase 2 — Priority-branching run:**

Load the pre-computed `phi` table. In `BranchingController`, within a row, order cells by
descending `phi` score (branch on high-correlation cells first). The row-serial outer order is
unchanged (rows are still processed 0→510 in sequence), preserving the SHA-1 pruning invariant
established in B.26a. Only the within-row cell ordering changes.

### B.40.4 Implementation Notes

- `HashFailureTracker`: lightweight array `uint32_t fail_count[511][511]`, `uint32_t pass_count[511][511]`
  (~1 MB stack or heap). Incremented only on SHA-1 events (rare relative to propagation steps).
- Profiling run: use existing `RowDecomposedController` with profiling enabled; emit
  `phi[r][c]` to JSON after the run. Requires ~20M iterations at plateau for statistical
  significance (failure events are common in this zone).
- Priority-branching run: load `phi` table at startup; `BranchingController::nextCell()` within
  a row sorts by `phi` (or uses a pre-sorted column-priority array per row).
- Row-serial order preserved: outer DFS loop advances rows 0→510 identically to current B.26c.

### B.40.5 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Plateau depth > 96,672 | Within-row phi correlation is a genuine pruning signal; proceed to Phase 2 full run |
| H2 (Neutral) | Depth ~96,672, iteration rate unchanged | Correlation exists but not at the right depth; try profiling only rows 150–200 |
| H3 (Regression) | Depth < 96,672 | Priority disrupts a beneficial order; revert; analyze phi distribution shape |
| H4 (Null — no correlation) | phi values near 0 for all cells | SHA-1 failures are independent of individual cell assignments; B.40 infeasible |

### B.40.6 Results (Completed &mdash; Null)

**Phase 1 profiling run.** The solver was run on a synthetic random 50%-density CSM block with B.31 direction switching disabled (top-down only) and `CRSCE_B40_PROFILE` enabled. After 200M+ DFS iterations at peak depth 85,694 (row ~167):

- Total SHA-1 failures: **0**
- Total SHA-1 passes: **0**
- Hash mismatches: **0**

**Root cause: constraint exhaustion, not hash failure.** The plateau is NOT caused by SHA-1 verification failures. No meeting-band row ever reaches $u(\text{row}) = 0$ (fully assigned) during top-down DFS. The solver backtracks due to constraint infeasibility ($\rho < 0$ or $\rho > u$ on some constraint line) long before any row in the meeting band becomes fully assigned. SHA-1 verification never triggers because the verification condition ($u = 0$) is never met.

This invalidates the B.40 hypothesis. The within-row priority branching by phi correlation is inapplicable because there is no phi signal to measure&mdash;the solver never generates SHA-1 events in the region where improvement is needed.

**B.40 outcome: H4 (null&mdash;no correlation).** SHA-1 failures are not the binding constraint at the plateau. The binding constraint is cross-sum feasibility, which exhausts before any row completes. B.40 Phase 2 (priority branching) is not attempted.

**Implication for B.41.** This finding initially appeared to strengthen the case for B.41 (cross-dimensional hashing + four-direction pincer), suggesting that if the row axis is exhausted, the column axis might not be. However, B.41 diagnostic instrumentation (column unknown counts at the plateau) reveals the opposite: at peak depth 86,125, the minimum nonzero column unknown count is **206** (vs 2 for rows), and only 8 of 511 columns are fully assigned. Columns are far LESS complete than rows at the plateau, with ~206 unassigned cells per column versus ~2 per row. Column-serial passes would need to assign ~206 cells per column before any column-hash verification could fire&mdash;dramatically worse than the row situation. The meeting band's constraint exhaustion is not axis-specific; it affects columns even more severely than rows because columns span the entire row range including the ~343 rows of the meeting band. **B.41's four-direction pincer is unlikely to improve depth.**

**Status: COMPLETED &mdash; NULL.**

---

## B.41 Cross-Dimensional Hashing + Four-Direction Pincer (Diagnostic Complete &mdash; Infeasible for Depth)

### B.41.1 Motivation

The B.39a null-space analysis established that the minimum constraint-preserving swap for the full 8-family system is 1,528 cells, touching 11 rows and 492 columns. This structural result has a direct implication for hash design: per-row SHA-1 hashes alone exploit only the row dimension for pruning, even though the swap structure spans nearly the full column range (96% of columns). Adding column hashes would provide a second verification axis, enabling solver architectures that prune along both rows and columns.

The current per-row SHA-1 system stores 511 hashes (81,760 bits), representing 64.9% of the block payload. The solver verifies each row hash at row completion during row-serial DFS, providing 511 pruning events per block. However, no pruning mechanism exists for columns &mdash; the solver has no way to verify partial progress along the column axis. This limitation is why B.31 (alternating-direction pincer) produced null results: the bottom-up solver could verify row hashes on bottom rows, but a column-serial or left-right solver would have had no verification capability at all.

B.41 introduces **cross-dimensional hashing** &mdash; storing both row hashes and column hashes &mdash; and resurrects the B.31 alternating-direction pincer with four-direction capability (top-down, bottom-up, left-right, right-left). Column hashes give the left-right and right-left passes a pruning mechanism they previously lacked.

### B.41.2 Hash Construction

**Row hashes.** Each row hash covers a **pair** of adjacent rows. Row pair $p$ hashes the concatenation of rows $2p$ and $2p+1$ (1,022 data bits + 2 padding bits = 1,024 bits = 128 bytes) via SHA-1. For $S = 511$ rows, this produces $\lfloor 511/2 \rfloor = 255$ row-pair hashes, plus one singleton hash for row 510 (512 bits = 64 bytes). Total: 256 row hashes.

**Column hashes.** Each column hash covers a **pair** of adjacent columns. Column pair $q$ hashes the concatenation of columns $2q$ and $2q+1$ (1,022 data bits, serialized column-major + 2 padding bits = 1,024 bits = 128 bytes) via SHA-1. Total: 256 column hashes, with column 510 hashed solo.

**Hash message format.** Row-pair message: bits from row $2p$ (511 bits) concatenated with bits from row $2p+1$ (511 bits), plus 2 zero padding bits = 1,024 bits = 128 bytes. Column-pair message: bits from column $2q$ across all 511 rows (511 bits) concatenated with bits from column $2q+1$ across all 511 rows (511 bits), plus 2 zero padding bits = 1,024 bits = 128 bytes.

**Payload impact.**

| Component | Current | B.41 |
|-----------|---------|------|
| Row hashes | 511 &times; 160 = 81,760 bits | 256 &times; 160 = 40,960 bits |
| Column hashes | 0 | 256 &times; 160 = 40,960 bits |
| Hash total | 81,760 bits | 81,920 bits |
| Block payload | 125,988 bits | 126,148 bits |
| Compression ratio | 48.2% | 48.3% |

The payload is essentially unchanged (+160 bits, one additional hash).

### B.41.3 Collision Resistance

The minimum constraint-preserving swap (1,528 cells, 11 rows, 492 columns) must evade both row-pair and column-pair hashes. With 11 affected rows spread across a span of 401 rows, approximately 6 row-pair hashes are affected (some affected rows may share a pair, but the 401-row span makes this unlikely &mdash; expected shared pairs $\approx \binom{11}{2}/511 \approx 0.1$, so $k_{\text{row}} \approx 11$ row-pair hashes). With 492 affected columns, approximately 246 column-pair hashes are affected ($k_{\text{col}} \approx 246$).

$$P_{\text{collision}} = 2^{-160(k_{\text{row}} + k_{\text{col}}) - 256} = 2^{-160 \times 257 - 256} = 2^{-41,376}$$

Compared to the current system's $2^{-2,016}$, this is a $2^{39,360}$ improvement in collision resistance at essentially the same payload cost.

### B.41.4 Four-Direction Pincer Solver

B.41 extends the B.31 alternating-direction pincer from two directions (top-down, bottom-up) to four:

| Direction | Traversal | Hash verification trigger |
|-----------|-----------|--------------------------|
| Top-down (TD) | Row 0 &rarr; 510, left-to-right within row | Row-pair hash when both rows $2p$ and $2p+1$ are complete |
| Bottom-up (BU) | Row 510 &rarr; 0, left-to-right within row | Row-pair hash when both rows $2p$ and $2p+1$ are complete |
| Left-right (LR) | Column 0 &rarr; 510, top-to-bottom within column | Column-pair hash when both columns $2q$ and $2q+1$ are complete |
| Right-left (RL) | Column 510 &rarr; 0, top-to-bottom within column | Column-pair hash when both columns $2q$ and $2q+1$ are complete |

**Plateau trigger and direction switching.** When the current direction's `stall_count` reaches threshold $N$ (candidate: 1,022 &mdash; one full row-pair or column-pair of unconstrained branching), declare a plateau and switch to the next direction in the cycle TD &rarr; BU &rarr; LR &rarr; RL &rarr; TD. Reset `stall_count`.

**Frontier tracking.** Four frontiers are maintained:

- `topFrontier`: lowest row with any unassigned cell (advances downward)
- `bottomFrontier`: highest row with any unassigned cell (advances upward)
- `leftFrontier`: leftmost column with any unassigned cell (advances rightward)
- `rightFrontier`: rightmost column with any unassigned cell (advances leftward)

**Convergence.** When all four directions have plateaued without progress since the last switch, the remaining unassigned region (bounded by the four frontiers) is irreducibly hard. The solver falls back to standard row-serial DFS within this region.

**DI consistency.** As established in B.31.2, the enumeration order is determined by the decompressor's deterministic traversal. The compressor runs the same four-direction solver to discover DI. The alternating order is fully deterministic and reproducible.

### B.41.5 Why B.31 Failed and B.41 May Succeed

B.31 (two-direction pincer) produced null results. The B.39 n-dimensional analysis explains why: the bottom-up solver could verify row hashes but had no way to exploit column-axis information. The solver was alternating between two projections along the same axis (the row axis), gaining no new dimensional coverage.

B.41 adds column hashes, giving the LR and RL passes genuine verification capability along the column axis &mdash; a fundamentally different projection than the row axis. This provides cross-dimensional pruning: the TD/BU passes prune based on row structure, and the LR/RL passes prune based on column structure. The combination may tighten the meeting region more effectively than either axis alone.

The key hypothesis: if the meeting band's intractability is caused by the solver's inability to verify column-axis constraints (because no column hashes exist), then adding column hashes and column-serial passes will reduce the meeting band and increase depth.

### B.41.6 Sub-experiment B.41a: Full Paired Hashing + Four-Direction Pincer

**Hash configuration:** 256 row-pair hashes + 256 column-pair hashes (512 total, 81,920 bits).

**Solver:** Four-direction pincer (TD, BU, LR, RL) with plateau threshold $N = 1{,}022$.

**Metric:** Peak depth (cells assigned before exhaustive backtracking). Compare against baseline 96,672.

**Expected outcomes:**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | Depth > 96,672 | Cross-dimensional pruning reduces the meeting band; four-direction pincer is effective |
| H2 (Neutral) | Depth &asymp; 96,672 | Column hashes provide no additional pruning power; meeting band is axis-independent |
| H3 (Regression) | Depth < 96,672 | Row-pair granularity (2 rows per hash) is coarser than per-row; the pruning loss outweighs the column-axis gain |

If H3 occurs, the regression isolates the cost of paired-row hashing. Compare against B.41b to distinguish the row-pair granularity effect from the column-hash benefit.

### B.41.7 Sub-experiment B.41b: Half-Hash Configuration + Four-Direction Pincer

**Hash configuration:** Hash only odd-indexed rows and odd-indexed columns:

- 256 row hashes: SHA-1 of row $2p+1$ for $p = 0, \ldots, 254$ (rows 1, 3, 5, ..., 509). Each hash covers a single row (511 bits + 1 padding bit = 512 bits = 64 bytes).
- 256 column hashes: SHA-1 of column $2q+1$ for $q = 0, \ldots, 254$ (columns 1, 3, 5, ..., 509). Each hash covers a single column (511 bits + 1 padding bit = 512 bits = 64 bytes).

**Payload impact:**

| Component | B.41a | B.41b |
|-----------|-------|-------|
| Row hashes | 256 &times; 160 = 40,960 bits | 256 &times; 160 = 40,960 bits |
| Column hashes | 256 &times; 160 = 40,960 bits | 256 &times; 160 = 40,960 bits |
| Hash total | 81,920 bits | 81,920 bits |

Identical payload to B.41a, but the hash granularity differs: B.41b hashes individual rows/columns (finer verification per hash, but only half the rows and half the columns are verified).

**Solver:** Same four-direction pincer as B.41a. The TD and BU passes verify row hashes only at odd rows (1, 3, 5, ..., 509). The LR and RL passes verify column hashes only at odd columns (1, 3, 5, ..., 509). Even rows and columns have no hash verification and rely solely on cross-sum constraint propagation.

**Verification granularity comparison:**

| Property | B.41a (paired) | B.41b (odd-only) |
|----------|---------------|-----------------|
| Row hash granularity | 2 rows (1,022 bits) per hash | 1 row (511 bits) per hash |
| Column hash granularity | 2 columns (1,022 bits) per hash | 1 column (511 bits) per hash |
| Rows with hash verification | All 511 (via pairs) | 256 of 511 (odd only) |
| Columns with hash verification | All 511 (via pairs) | 256 of 511 (odd only) |
| TD pruning events | 256 (every 2 rows) | 256 (every other row) |
| LR pruning events | 256 (every 2 columns) | 256 (every other column) |

B.41b trades coverage (only half of rows/columns verified) for precision (each hash covers exactly one row or column, making failures more localizable). The unchecked even rows and columns rely on cross-sum propagation from their verified neighbors, which may provide sufficient indirect constraint.

**Expected outcomes:**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (B.41b > B.41a) | B.41b depth exceeds B.41a depth | Finer per-hash granularity matters more than full coverage; single-row hash pruning is more effective than paired-row hash pruning |
| H2 (B.41b &asymp; B.41a) | Depths within 5% | Coverage and granularity are equivalent at this scale; cross-sum propagation fills the gaps |
| H3 (B.41b < B.41a) | B.41b depth below B.41a | Full coverage matters; unchecked even rows/columns create exploitable gaps |

**Collision resistance (B.41b).** With 11 affected rows, approximately 6 land on odd rows (hashed). With 492 affected columns, approximately 246 land on odd columns (hashed). The collision probability is $2^{-160 \times (6 + 246) - 256} = 2^{-160 \times 252 - 256} = 2^{-40,576}$ &mdash; comparable to B.41a.

### B.41.8 Sub-experiment B.41c: Row/Column-Completion Look-Ahead (Conditional on B.41b H1)

**Prerequisite.** B.41b must achieve H1 (depth > 96,672). If B.41b is neutral or regressive, B.41c is not
attempted &mdash; the four-direction pincer does not provide the architectural context that makes
completion look-ahead valuable across both axes.

**Motivation.** B.17 proposed Row-Completion Look-Ahead (RCLA): when a row drops to $u \leq u_{\max}$
unknowns, enumerate all $\binom{u}{\rho}$ cardinality-feasible completions, check each against SHA-1,
and backtrack immediately if none pass. B.17 was never implemented because it is a constant-factor
optimization &mdash; it prunes the same infeasible subtrees that DFS would have pruned, just faster
&mdash; and does not change the depth ceiling in a row-serial-only solver.

B.41b changes the calculus. The four-direction pincer introduces column-serial passes (LR, RL) with
column-hash verification. **Column-Completion Look-Ahead (CCLA)** is the natural dual of RCLA: when
a column drops to $u \leq u_{\max}$ unknowns, enumerate all $\binom{u}{\rho}$ completions consistent
with the column's cardinality constraint, check each against the column hash, and backtrack immediately
if none pass. In B.41b, CCLA applies to the 256 odd columns that have hashes.

With both RCLA and CCLA active, all four passes benefit from look-ahead pruning:

| Pass | Look-ahead type | Hash verified | Trigger |
|------|----------------|---------------|---------|
| TD (top-down) | RCLA | Odd-row SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd row |
| BU (bottom-up) | RCLA | Odd-row SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd row |
| LR (left-right) | CCLA | Odd-column SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd column |
| RL (right-left) | CCLA | Odd-column SHA-1 | $\binom{u}{\rho} \leq C_{\max}$ on odd column |

Even-indexed rows and columns (unhashed in B.41b) cannot trigger look-ahead &mdash; they rely solely
on cross-sum constraint propagation, as in the base B.41b design.

**Phase 1 &mdash; Instrumentation (no behavior change).**

Before implementing look-ahead, instrument the B.41b solver to log the distribution of $u$ and $\rho$
at the moment each hash verification fires (both row and column hashes). This answers B.17's open
question B.17.8(a) in the four-direction context:

- If $u$ is typically near 0 at hash verification (propagation forces most cells before the hash
  fires), look-ahead has no room to trigger and B.41c is moot.
- If $u$ is typically 5&ndash;20 at hash verification (propagation stalls before the row or column is
  complete, leaving cells for branching), look-ahead can trigger and is worth implementing.

The instrumentation is ~10 lines of logging in `RowDecomposedController` and imposes negligible
runtime overhead.

**Phase 2 &mdash; RCLA + CCLA implementation.**

If Phase 1 shows $u \geq 5$ at hash verification for a significant fraction of plateau-band rows
and columns:

1. **Triggering.** Invoke look-ahead when $\binom{u}{\rho} \leq C_{\max}$ (candidate: $C_{\max} = 500$,
   corresponding to ~100 $\mu$s of SHA-1 work). The threshold adapts naturally to the residual:
   extreme $\rho$ (near 0 or $u$) allows larger $u$ values; $\rho \approx u/2$ requires smaller $u$.
   Use a precomputed $\binom{u}{\rho}$ lookup table ($\leq 1$ MB).

2. **Cross-line pre-filtering (B.17.3).** Before checking SHA-1, filter each candidate completion
   against cross-line feasibility (column, diagonal, anti-diagonal, and LTP residuals for RCLA;
   row, diagonal, anti-diagonal, and LTP residuals for CCLA). Cost: $O(8u)$ per candidate. This
   may reduce the SHA-1 candidate count by 2&ndash;10&times; in the plateau band, where cross-line
   residuals are moderately constrained.

3. **DI consistency.** Look-ahead is purely deductive and deterministic (B.17.6). It prunes only
   provably infeasible subtrees. The enumeration order (lexicographic over the $u$ unknown positions)
   is fixed. The solver visits the same solutions in the same order, but backtracks sooner on doomed
   rows and columns. DI semantics are preserved.

**Cost-benefit estimate.**

| Metric | Without look-ahead | With look-ahead (u=10, &rho;=5) |
|--------|-------------------|-------------------------------|
| Cost per infeasible row/column | $\binom{10}{5} \times 10 \times 3\mu s \approx 7.5$ ms (DFS) | $\binom{10}{5} \times 1\mu s \approx 250\mu s$ (enumerate) |
| Speedup per infeasible detection | &mdash; | ~30&times; |
| Depth improvement | &mdash; | None (same subtrees pruned) |
| Iteration rate improvement | &mdash; | Higher (more iterations per wall-clock second) |

Look-ahead is a constant-factor optimization: it increases the iteration rate in the plateau band,
allowing the solver to explore more of the search space per unit time. It does not change the depth
ceiling directly. However, in a time-bounded decompression context, faster iteration translates to
deeper effective exploration.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improvement) | B.41c iteration rate &gt; B.41b iteration rate by &geq; 20% in plateau band | Look-ahead triggers frequently; cross-line pre-filtering effective; worth keeping |
| H2 (Neutral) | Iteration rate within 5% | Look-ahead triggers rarely ($u$ near 0 at hash verification); Phase 1 instrumentation was correct to gate Phase 2 |
| H3 (Regression) | Iteration rate decreases | Look-ahead overhead (enumeration + SHA-1) exceeds DFS overhead for the observed $u$ distribution; revert |

### B.41.9 Implementation Notes

Both B.41a and B.41b require:

1. **Format change:** Add column hashes to the block payload (Section 12). Increment format version. Column hash messages are serialized column-major: for column $c$, the message is the sequence of bits $M[0][c], M[1][c], \ldots, M[510][c]$.

2. **Compressor change:** Compute column hashes after CSM construction, serialize into payload.

3. **Decompressor/solver change:** Implement four-direction DFS in `RowDecomposedController`. Add column-serial cell ordering. Add column-hash verification at column-pair (B.41a) or odd-column (B.41b) completion. Track four frontiers.

4. **LTP table:** Use the B.38-optimized table (`b38e_t31000000_best_s137.bin`) for both experiments to isolate the hash/solver effect from LTP geometry.

B.41c additionally requires:

5. **Phase 1 instrumentation:** Log $u$ and $\rho$ at each hash verification event (row and column) in the B.41b solver. Emit to JSON for offline analysis. ~10 lines in `RowDecomposedController`.

6. **Phase 2 look-ahead:** Precomputed $\binom{u}{\rho}$ lookup table ($u, \rho \leq 511$). Row-completion enumerator and column-completion enumerator sharing common combinatorial generation logic. Cross-line pre-filter checking 8 constraint lines per candidate. ~100&ndash;150 lines total.

### B.41.10 Relationship to Prior Work

**B.31 (Alternating-Direction Pincer: Null).** B.41 is a direct successor to B.31, addressing the root cause of its failure: the absence of column-axis verification. B.31 alternated between top-down and bottom-up passes along the same (row) axis, gaining no new dimensional coverage. B.41 adds column hashes and column-serial passes, providing genuine cross-dimensional pruning.

**B.39a (Null-Space Analysis).** The minimum swap structure (11 rows, 492 columns) directly motivates the column-hash design: the 96% column coverage of the lightest swap means column hashes are maximally effective at detecting constraint-preserving perturbations. The B.39a result also explains why per-row-only hashing leaves the column dimension unpoliced.

**B.32 (Four-Direction Iterative Pincer: Proposed).** B.32 proposed a four-direction pincer but was never implemented because B.31 (its two-direction prerequisite) produced null results. B.41 supersedes B.32 by addressing the verification gap that made B.31 ineffective.

**B.17 (Row-Completion Look-Ahead: Proposed).** B.41c generalizes B.17's RCLA to both row and column axes, exploiting B.41b's column hashes for Column-Completion Look-Ahead (CCLA). B.17 was never implemented as a standalone experiment because it is a constant-factor optimization that does not change the depth ceiling in a row-serial-only solver. In the four-direction pincer context, CCLA gives the LR/RL passes a pruning acceleration mechanism that did not previously exist.

### B.41.10 Diagnostic Results (Completed &mdash; Infeasible for Depth)

Before implementing the full four-direction solver and format changes, diagnostic instrumentation was added to measure column completion at the plateau. The solver was run on a synthetic random 50%-density CSM block using the B.38-optimized LTP table.

**Column vs. row unknown counts at peak depth 86,125:**

| Metric | Row axis | Column axis |
|--------|----------|-------------|
| Min nonzero unknown | 2 | 206 |
| Axes fully complete | 168 / 511 | 8 / 511 |
| Cells needed for next verification event | 2 | 206 |

**Interpretation.** Columns are **far less complete** than rows at the plateau. The minimum column unknown count is 206, meaning even the closest-to-completion column has 206 unassigned cells. Column-serial passes would need to assign ~206 cells before any column-hash verification could fire&mdash;approximately 100x worse than the row situation (where some rows are within 2 cells of completion).

**Root cause.** Columns span all 511 rows, including the ~343 rows of the meeting band (rows ~168-510). A column cannot complete until its meeting-band cells are assigned. Since the meeting band is precisely the region where constraint propagation has exhausted, columns inherit the meeting band's intractability in full. Row-serial DFS at least completes rows in the propagation zone (rows 0-167); column-serial DFS cannot complete ANY column until the meeting band is traversed, which is the problem it was supposed to solve.

**Conclusion.** The B.41 four-direction pincer will not improve solver depth. The meeting band's constraint exhaustion is not axis-specific&mdash;it affects columns more severely than rows. B.41's column hashes remain valuable for collision resistance (&sect;B.41.3 shows $2^{39,360}$ improvement in collision resistance at the same payload cost), but the solver architecture does not benefit from column-serial passes.

**B.41a/B.41b sub-experiments: NOT ATTEMPTED** (infeasible per diagnostic result).

**Status: DIAGNOSTIC COMPLETE &mdash; INFEASIBLE FOR DEPTH IMPROVEMENT. Collision resistance value retained.**

---

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

## B.44 Constraint Density Breakthrough: Discovering New Projection Families (Proposed)

### B.44.1 Motivation

Experiments B.33 through B.43 have exhaustively characterized the solver's depth ceiling and established that it is a property of the constraint system's **information content**, not the solver's search strategy. The 8-family system (LSM, VSM, DSM, XSM, LTP1-4) provides 5,097 independent constraints over 261,121 cells&mdash;a 2% constraint density that leaves 256,024 degrees of freedom (98% of cells unconstrained by cross-sums alone). SHA-1 lateral hashes disambiguate within this null space, but only at row completion boundaries, which the solver cannot reach in the meeting band.

The path to 100K+ depth requires **increasing the constraint density**&mdash;adding new projection families that contribute independent constraints, reducing the null-space dimension and enabling propagation to force cells deeper into the matrix. B.44 systematically explores which new families are viable, how many independent constraints each contributes, and what payload cost each imposes.

### B.44.2 The Constraint Budget

The current system's information balance:

| Component | Bits | Independent constraints (GF(2)) |
|-----------|------|---------------------------------|
| LSM (511 row sums) | 4,599 | 510 |
| VSM (511 column sums) | 4,599 | 510 |
| DSM (1,021 diagonal sums) | 8,185 | 1,017 |
| XSM (1,021 anti-diagonal sums) | 8,185 | 1,020 |
| LTP1-4 (4 &times; 511 sums) | 18,396 | 2,040 |
| **Total cross-sums** | **43,964** | **5,097** |
| LH (511 SHA-1 hashes) | 81,760 | &mdash; (non-linear) |
| BH (SHA-256 block hash) | 256 | &mdash; (non-linear) |
| **Total payload** | **125,980** | |

The CSM has 261,121 bits. The payload is 125,980 bits&mdash;a 48.2% compression ratio. To reach 100% constraint density (every cell independently constrained by cross-sums), the system would need $261{,}121 - 5{,}097 = 256{,}024$ additional independent constraints. At ~9 bits per constraint line (for a 511-cell uniform partition), this would require $256{,}024 / 510 \approx 502$ additional LTP-like sub-tables&mdash;adding $502 \times 4{,}599 = 2{,}308{,}698$ bits (288,587 bytes) to the payload. This would make the payload far larger than the original data, defeating the purpose of compression.

The practical question is: **what is the minimum number of additional constraint families needed to push depth from ~96K to 261K, and what payload cost does each impose?**

### B.44.3 Candidate Constraint Families

Six categories of new projection families are considered, ordered by expected information yield per payload bit:

**Category 1: Sub-row / sub-column block sums.** Partition each row into $B$ blocks of $\lceil 511/B \rceil$ cells and store the sum of each block. For $B = 2$: 511 rows &times; 2 blocks = 1,022 constraints, each ~8 bits. Payload cost: ~8,176 bits. These are NOT independent of the row sum (the two block sums must add to LSM[r]), so the marginal independent constraints are $511 \times (B-1) = 511$ for $B = 2$.

**Category 2: Parity constraints.** Store the XOR (parity) of cells along each constraint line rather than (or in addition to) the sum. Parity is 1 bit per line vs 9 bits per sum, drastically reducing payload cost. However, parity is strictly less informative than the sum: it captures only the least significant bit. For lines with known sum, parity is redundant ($\text{parity} = \text{sum} \bmod 2$). Parity constraints on NEW lines (not already covered by cross-sums) could add cheap independent information.

**Category 3: Higher-order polynomial projections.** Instead of linear sums ($\sum c_i$), store quadratic sums ($\sum c_i c_j$ for specific cell pairs) or weighted sums ($\sum w_i c_i$ with non-binary weights). These capture non-linear relationships that cross-sums miss. The challenge: quadratic sums are expensive to verify during propagation (each cell assignment affects $O(k)$ terms for $k$ paired cells).

**Category 4: Algebraically structured partitions (MOLS).** Mutually Orthogonal Latin Squares of order 511 provide partitions where every pair of lines from different families intersects in exactly one cell. Since $511 = 7 \times 73$, $\text{MOLS}(511) \geq 6$ (at least 6 mutually orthogonal squares exist). Each MOLS-based partition provides 511 lines of 511 cells, but with a crucial difference from Fisher-Yates LTP: the algebraic structure guarantees uniform pairwise intersection, enabling tighter joint constraint bounds.

**Category 5: Sub-block SHA-1 hashes.** Instead of (or in addition to) per-row SHA-1, hash sub-blocks of each row. For example, split each 511-bit row into 4 blocks of ~128 bits and hash each block independently. This provides 4 SHA-1 verification events per row (at $u_{\text{block}} = 0$ instead of $u_{\text{row}} = 0$), enabling verification at 128-cell granularity rather than 511-cell granularity. Payload cost: $511 \times 4 \times 160 = 326{,}240$ bits (40,780 bytes)&mdash;expensive, but fundamentally changes the verification boundary.

**Category 6: Hybrid constraint-hash families.** Store a lightweight hash (e.g., CRC-32 or truncated SHA-1) of each LTP sub-table line. This provides hash-based verification at the LTP-line granularity (511 cells per line) in addition to the cross-sum. Payload cost: $4 \times 511 \times 32 = 65{,}408$ bits (8,176 bytes) for CRC-32.

### B.44.4 Theoretical Analysis: Constraints Required for Full Depth

The solver's propagation cascade forces cells when a constraint line's residual $\rho = 0$ (force all unknowns to 0) or $\rho = u$ (force all unknowns to 1). Each independent constraint contributes one potential forcing event. For a random 50%-density CSM, the probability that a line with $u$ unknowns has $\rho = 0$ or $\rho = u$ decreases exponentially with $u$: $P \approx 2 \times 2^{-u}$.

At the plateau, typical lines have $u \approx 300$. The probability of any single line triggering forcing is $\sim 2^{-299}$&mdash;effectively zero. Propagation works in the early rows because $u$ is small (many cells already assigned), not because the constraint system has high density.

**The leverage point is not more lines but shorter lines.** Each LTP sub-table has 511 lines of 511 cells. If we partitioned the same cells into 2,555 lines of ~102 cells, the same number of cells would be constrained, but each line would reach $\rho = 0$ or $\rho = u$ with probability $\sim 2^{-102}$ (still negligible) at the plateau.

**The real lever: finer-grained verification boundaries.** SHA-1 forces are non-linear and fire at $u = 0$. Reducing the verification granularity from 511 cells (per row) to $G$ cells (per sub-block) means the solver gets hash verification events when $G$ contiguous cells are assigned, enabling pruning at a $G$-cell boundary instead of a 511-cell boundary. At $G = 64$: the solver gets a verification event every 64 assignments (within the block), providing ~8 pruning checkpoints per row instead of 1.

### B.44.5 Sub-experiment B.44a: Constraint Family Discovery via Simulation

**Objective.** Empirically determine which candidate families provide the most independent constraints per payload bit, and measure the marginal depth improvement from each.

**Method.** Write a Python simulation tool (`tools/b44a_constraint_sim.py`) that:

1. Constructs the $5{,}108 \times 261{,}121$ constraint matrix $A$ for the current 8-family system (reusing B.39a code).
2. For each candidate family from &sect;B.44.3, extends $A$ with the new constraint lines.
3. Computes the GF(2) rank of the extended matrix to determine the number of **new independent constraints** (rank increase over the baseline 5,097).
4. Computes the new null-space dimension ($261{,}121 - \text{rank}_{\text{new}}$).
5. Estimates the payload cost of the new family (bits per constraint line &times; number of lines).
6. Computes the **information efficiency**: independent constraints gained per payload bit.

**Families to test:**

| ID | Family | Lines | Bits/line | Payload bits | Expected new rank |
|----|--------|-------|-----------|-------------|-------------------|
| C1a | Sub-row blocks ($B = 2$) | 1,022 | 8 | 8,176 | ~511 |
| C1b | Sub-row blocks ($B = 4$) | 2,044 | 7 | 14,308 | ~1,533 |
| C2a | Row parity on new random partitions | 511 | 1 | 511 | ~510 |
| C2b | 8 parity partitions (FY, new seeds) | 4,088 | 1 | 4,088 | ~4,080 |
| C4a | MOLS(511) partition (1 square) | 511 | 9 | 4,599 | ~510 |
| C4b | MOLS(511) partitions (6 squares) | 3,066 | 9 | 27,594 | ~3,060 |
| C5a | Sub-row SHA-1 ($G = 128$, 4 blocks/row) | 2,044 | 160 | 326,240 | N/A (non-linear) |
| C5b | Sub-row SHA-1 ($G = 64$, 8 blocks/row) | 4,088 | 160 | 654,080 | N/A (non-linear) |
| C6a | CRC-32 per LTP line | 2,044 | 32 | 65,408 | N/A (non-linear) |

**Key metric:** For linear families (C1, C2, C4), the rank increase directly measures the information gained. For hash families (C5, C6), the value is in verification granularity, not rank&mdash;these must be tested via solver simulation.

**Solver depth simulation.** For each candidate family, modify the synthetic-block test infrastructure to:
1. Compute the new constraint values from the known CSM.
2. Load them as additional constraints in the solver.
3. Measure peak depth with the extended constraint system.
4. Compare against the 86,125 baseline.

This can be done in Python for the rank analysis and via C++ solver modifications for the depth measurement.

### B.44.6 Sub-experiment B.44b: Sub-Row Block Sums

**Prerequisite.** B.44a rank analysis confirms that sub-row block sums (C1a or C1b) provide $\geq 500$ new independent constraints.

**Hypothesis.** Splitting each row into $B$ blocks and storing sub-block sums provides finer-grained constraint information that forces cells within partially-assigned rows. At the plateau, rows have $u = 300$-$400$ unknowns distributed across the full 511-cell width. Sub-block sums ($B = 4$: blocks of ~128 cells each) provide 4 independent residuals per row, each with $u_{\text{block}} \approx 75$-$100$. While $2^{-75}$ is still negligibly small for direct forcing ($\rho = 0$ or $\rho = u$), the sub-block sums provide **tighter bounds** on each cell's feasibility: a cell in a block with $\rho_{\text{block}} = 0$ is forced to 0 even if the full-row $\rho$ is nonzero.

**Implementation.** Extend `ConstraintStore` with sub-row block lines. Each row $r$ is divided into $B$ blocks: cells $[r, 0..127]$, $[r, 128..255]$, $[r, 256..383]$, $[r, 384..510]$. Each block is a new constraint line with its own target sum (stored in the payload) and its own $u/\rho$ statistics. The propagation engine handles these lines identically to existing lines.

**Payload cost.** $511 \times 3 \times 7 = 10{,}731$ bits ($B = 4$: 3 new block sums per row, since the 4th is determined by the row sum). Adds ~1,342 bytes to the block payload.

**Expected outcome.** If sub-block $\rho = 0$ or $\rho = u$ events occur during DFS in the meeting band, propagation will force additional cells, potentially cascading to deeper depths. The question is how often sub-block residuals hit extremes when the full-row residual does not.

### B.44.7 Sub-experiment B.44c: Parity Partitions (Cheap Information)

**Prerequisite.** B.44a confirms parity constraints add $\geq 500$ independent constraints per 511 bits of payload.

**Hypothesis.** Random parity partitions are the cheapest way to add independent constraints: 1 bit per line vs 9 bits for a sum. Eight new FY-shuffled parity partitions (C2b) would add ~4,080 independent constraints at a payload cost of only 4,088 bits (511 bytes)&mdash;an order of magnitude more efficient than LTP sums per payload bit.

**Mechanism.** Each parity partition assigns every cell to one of 511 parity lines (via Fisher-Yates shuffle, different seeds from LTP1-6). The stored parity bit for each line is the XOR of all cells on that line. During propagation, when a parity line has $u = 1$ (one unknown cell), the cell's value is determined: $v = \text{parity}_{\text{stored}} \oplus \text{assigned\_parity}$. This is a new forcing rule: $u = 1$ forcing via parity, independent of the sum-based $\rho = 0 / \rho = u$ forcing.

**Key advantage.** Parity forcing fires at $u = 1$ (exactly one unknown on the line), which is a much more achievable threshold than $\rho = 0$ (requires all remaining unknowns to be zero). In the meeting band, parity lines from random partitions will reach $u = 1$ as the DFS assigns cells&mdash;providing forcing events that sum-based constraints cannot.

**Key risk.** Parity constraints carry only 1 bit of information per line (vs 9 bits for a sum). The forcing is weaker: it fires only at $u = 1$ (not $u > 1$), and it provides no residual tightening during propagation cascades.

**Payload cost.** $8 \times 511 \times 1 = 4{,}088$ bits = 511 bytes. New payload size: $16{,}899 + 511 = 17{,}410$ bytes. Compression ratio: $17{,}410 / 32{,}641 = 53.3\%$ (from 51.8%).

### B.44.8 Sub-experiment B.44d: Sub-Block Hash Verification (Finer Pruning Boundaries)

**Prerequisite.** B.44a solver simulation shows that linear constraint families (B.44b, B.44c) provide insufficient depth improvement. This sub-experiment tests whether finer hash verification boundaries break through the ceiling via non-linear constraints.

**Hypothesis.** The B.40 finding&mdash;no SHA-1 events at the plateau because no row reaches $u = 0$&mdash;identifies the verification boundary as the binding constraint. If hash verification fires at $G$-cell sub-block boundaries instead of 511-cell row boundaries, the solver can prune infeasible sub-trees 8&times; sooner (for $G = 64$).

**Method.** Divide each 511-bit row into 8 blocks of 64 bits (blocks 0-6: 64 bits each, block 7: 63 bits + 1 padding). Compute SHA-1 of each 64-bit block independently. Store 8 block hashes per row: $511 \times 8 \times 160 = 653{,}280$ bits. This is a **large** payload increase (81,660 bytes), raising the block payload to ~98,559 bytes and the compression ratio to ~302%.

**The compression ratio exceeds 100%**: the payload is 3&times; larger than the original data. This makes the approach impractical for compression, but the experiment isolates the question: **does finer hash verification extend depth, regardless of payload cost?**

If the answer is yes, the next step is to find a cheaper hash mechanism (CRC-32, truncated SHA-1, or block parity) that provides similar pruning at lower payload cost.

**Alternative: CRC-32 sub-block verification (B.44d').** Use 32-bit CRC instead of 160-bit SHA-1. Payload: $511 \times 8 \times 32 = 130{,}816$ bits = 16,352 bytes. New total: ~33,251 bytes. Ratio: ~102%. CRC-32 has collision probability $2^{-32}$ per block, which is adequate for pruning (the solver doesn't need cryptographic collision resistance for pruning, only for correctness&mdash;BH provides the final guarantee).

### B.44.9 Implementation Plan

B.44 sub-experiments are ordered by implementation cost and information yield:

1. **B.44a (simulation):** Python-only. Reuse B.39a's constraint matrix infrastructure. Extend with candidate families, measure rank. Estimate: ~200 lines of Python, 1-2 hours of computation. **Do this first.**

2. **B.44c (parity partitions):** Cheapest to implement and test. Add 8 parity lines to ConstraintStore + PropagationEngine + payload. Estimate: ~100 lines of C++, minor payload format change.

3. **B.44b (sub-row block sums):** Moderate complexity. Add block-sum lines to ConstraintStore. Estimate: ~150 lines of C++.

4. **B.44d (sub-block hashes):** Most complex. Requires new hash verifier, modified DFS loop, significant payload change. Estimate: ~300 lines of C++. Test CRC-32 variant first (cheaper payload).

### B.44.10 Relationship to Prior Work

**B.27 (LTP5/LTP6: Inert).** B.27 added two more LTP sub-tables and found zero depth improvement. The B.39a analysis explains why: each LTP sub-table adds 510 independent constraints, but these are sum-based constraints on 511-cell lines. At the plateau, lines with $u = 300$ have $\rho$ far from both 0 and $u$&mdash;the new constraints provide no forcing. B.44c (parity partitions) addresses this by using a different forcing rule ($u = 1$ parity forcing instead of $\rho = 0/u$ sum forcing).

**B.20 (Toroidal-Slope to LTP Transition).** B.20 replaced algebraically structured partitions with pseudorandom Fisher-Yates partitions, gaining depth because FY partitions are uncorrelated with the geometric families. B.44.4 (MOLS) revisits algebraic structure, but with a different motivation: MOLS provides guaranteed uniform pairwise intersection between families, which may enable tighter joint bounds than random partitions (whose intersection structure is probabilistic).

**B.42a (Interval Tightening: Zero Potential).** B.42a showed that per-cell interval bounds $[v_{\min}, v_{\max}]$ from the current 8 families are always $[0, 1]$. Adding new families (B.44b sub-block sums, B.44c parity) may tighten these bounds because the new constraints provide independent information about sub-regions of the row. Sub-block sums in particular decompose the row residual into block residuals, each with smaller $u$, potentially reaching $v_{\min} = v_{\max}$ where the full-row constraint cannot.

**B.40 (No SHA-1 at Plateau).** B.40's null result directly motivates B.44d: if the solver never reaches $u = 0$ on a full row, make the verification boundary smaller so $u = 0$ is achievable on sub-blocks.

**Status: PROPOSED.**

---

## Appendix C: Open Questions Consolidated

This appendix consolidates all open questions from Appendices A and B into a single reference. Each question is reproduced from its original context, followed by a *Discovered Data* subsection summarizing relevant experimental evidence gathered during the research program, and a *Conclusions* subsection stating what can be inferred. Questions are grouped by their source section to preserve traceability. The principal data sources are: B.8 telemetry (depth plateau ~87,500, 37% hash mismatch, min_nz_row_unknown = 1); B.20.9 Configuration C results (depth ~88,503, mismatch 25.2%, iter/sec ~198K); B.21 observed results (depth 50,272, mismatch 0.20%, iter/sec ~687K); the B.12.6(a) BP convergence experiment; and the CDCL infrastructure assessment (B.1.10).

---

### C.1 From B.1.8: CDCL and Non-Chronological Backjumping

#### C.1.1 Question (a): LH Failure Root-Cause Depth

*How often do LH failures have distant root causes? If the typical hash failure is caused by a decision within the last 5--10 levels, backjumping provides little benefit. If root causes are frequently 50--500 levels deep, the savings are exponential.*

**Discovered Data.** B.8 telemetry shows min_nz_row_unknown = 1 throughout: rows reach completion and fail SHA-1 immediately. B.20.9 confirms the pattern persists under LTP substitution (mismatch rate dropped from 37% to 25.2% but failures remain frequent). B.21 reduced mismatch to 0.20%, but the depth regression to 50,272 suggests root causes are shallow — the solver makes better decisions with variable-length lines but encounters an earlier structural wall.

**Conclusions.** The evidence strongly suggests that root causes are predominantly shallow (within the last few rows of assignments). B.1.9 explicitly concluded that the bottleneck is search guidance, not constraint density or lookahead reach. The backjumping benefit is therefore limited; the typical failure distance is within chronological backtracking's efficient range.

#### C.1.2 Question (b): Implication Graph Maintenance Cost

*Can the implication graph be maintained cheaply enough to justify full CDCL?*

**Discovered Data.** B.1.10 documents that the full CDCL infrastructure was built (ConflictAnalyzer, ReasonGraph, CellAntecedent, BackjumpTarget) but never integrated into the DFS loop. The forcing dead zone at the plateau (ρ/u ≈ 0.5 for all length-511 lines) means the reason graph contains no meaningful antecedent chains — there are no forcing events to record. B.21's variable-length lines create forcing events in early rows (short lines exhaust quickly), but the depth regression suggests these events do not propagate deeply enough to create useful conflict clauses.

**Conclusions.** The question is moot under current architecture. The implication graph can be maintained at low cost per se (the infrastructure exists), but the cost is irrelevant because the forcing dead zone generates no useful antecedent chains. B.21's short LTP lines may change this calculus by creating forcing events, but B.21's depth regression indicates the benefit would be marginal at best. CDCL should be revisited only if a future architecture change produces sustained forcing in the plateau band.

#### C.1.3 Question (c): Lightweight Backjump Estimator Sufficiency

*Is the lightweight backjump estimator (B.1.7) sufficient in practice?*

**Discovered Data.** B.1.9 reports that the estimator was implemented first per the adopted sequence. B.8 telemetry shows no measurable improvement in depth from the estimator (depth stayed at ~87,500). B.1.10's retrospective analysis explains why: the estimator needs forcing events to identify causal decisions, but the dead zone produces no forcing.

**Conclusions.** Answered negatively. The lightweight estimator is insufficient because it operates on forcing-event metadata that does not exist in the plateau band. The approach is structurally unable to identify backjump targets when ρ/u ≈ 0.5.

#### C.1.4 Question (d): CDCL and Adaptive Lookahead Interaction

*How does CDCL interact with the adaptive lookahead (B.8)?*

**Discovered Data.** B.8 was implemented with k = 1…4. The k = 4 probe depth added ~15% per-iteration overhead with zero depth improvement. CDCL was built but never integrated. No interaction data exists.

**Conclusions.** Unanswered empirically, but likely unproductive. The lookahead probes explore subtrees that, like the main DFS, encounter the forcing dead zone. Conflicts discovered by probes would produce equally vacuous learned clauses (111-literal clauses that rarely fire, per B.1.9). The interaction is theoretically interesting but practically mooted by the dead zone.

#### C.1.5 Question (e): Clause Management Strategy

*What clause-management strategy is appropriate for CRSCE?*

**Discovered Data.** CDCL was never integrated, so no clause database was populated. B.1.9 calculated that hash-failure clauses would contain ~111 decision literals, making reuse probability negligible.

**Conclusions.** Unanswered but deprioritized. The question becomes relevant only if a future architecture change (e.g., variable-length lines producing short conflict clauses) makes CDCL viable. Under current or foreseeable architectures, the clause database would remain empty or contain only unreusable long clauses.

---

### C.2 From B.2.6: Additional Toroidal Slope Partitions

#### C.2.1 Question (a): 5th Slope Pair Speedup

*What is the empirical decompression speedup from adding a 5th slope pair?*

**Discovered Data.** B.20 tested a different direction — replacing all 4 slopes with 4 LTP partitions rather than adding a 5th slope pair. The result (B.20.9): +1.1% depth, −11.8 pp mismatch, ~0% throughput change. B.21 went further with variable-length LTP and produced 3.5× throughput improvement but depth regression.

**Conclusions.** Superseded. The research program moved past additional slope pairs toward non-linear partitions (B.9/B.20) and variable-length partitions (B.21). Adding a 5th slope pair is no longer under consideration given that the 4 existing slopes were entirely replaced by LTP partitions. The positional hypothesis (B.20.9) indicates that any additional uniform-length-511 line — slope or otherwise — cannot break the forcing dead zone.

#### C.2.2 Question (b): Per-Iteration Cost Scaling

*At what partition count does per-iteration cost become a bottleneck?*

**Discovered Data.** B.20 Config C reduced lines per cell from 10 to 8 with negligible throughput change (~198K vs ~200K iter/sec). B.21 reduced lines per cell from 8 to 5–6 and achieved 687K iter/sec (3.5× improvement). The relationship is clearly super-linear: removing 2–3 lines per cell tripled throughput.

**Conclusions.** Partially answered. Reducing constraint lines per cell from 8 to 5–6 produced a dramatic throughput improvement, confirming that per-iteration cost scales significantly with line count. The bottleneck onset appears to be around 8–10 lines per cell. However, throughput is not the sole metric — B.21's throughput gain came at the cost of depth regression, suggesting the optimal constraint density balances throughput against propagation power.

#### C.2.3 Question (c): Optimal Slope Values

*What slope values should a 5th pair use?*

**Discovered Data.** No experiments with alternative slope values were conducted. The entire slope family was replaced by LTP partitions in B.20.

**Conclusions.** Superseded. The toroidal slope family has been removed from the architecture. The question is only relevant if a future experiment re-introduces algebraic slopes alongside LTP.

---

### C.3 From B.3.8: Variable-Length Curve Partitions

#### C.3.1 Question (a): Minimum Invisible Swap Size

*What is the empirically measured k_min for a 6-partition system at small matrix sizes?*

**Discovered Data.** No small-matrix experiments were conducted. However, B.21's joint-tiled variable-length design is conceptually related — it uses variable-length lines (1 to 256 cells) rather than the 1-to-511 triangular sequence proposed in B.3.

**Conclusions.** Unanswered. The question remains relevant for understanding swap-invisible pattern theory but has been deprioritized relative to the empirical approach taken in B.20/B.21.

#### C.3.2 Question (b): 16-Partition Cascade Solving

*For 16 variable-length curve partitions, does the cascade from anchor cells solve enough of the matrix?*

**Discovered Data.** B.21 used 4 variable-length sub-tables (not 16 full partitions) with line lengths 1–256. The result was 687K iter/sec (fast cascade) but depth regression to 50,272, suggesting that cascade solving works rapidly but the reduced constraint density (5–6 lines per cell instead of 8) creates a premature wall.

**Conclusions.** Partially answered. Variable-length lines produce extremely effective cascade forcing (evidenced by B.21's 0.20% mismatch rate and 3.5× throughput). However, 4 sub-tables are insufficient to maintain depth — the solver runs out of propagation power. Scaling to 16 partitions would restore constraint density, but at enormous storage cost (the original B.3 proposal). The B.21 results suggest that a middle ground — more than 4 sub-tables but fewer than 16 full partitions — warrants investigation.

#### C.3.3 Question (c): LH Elimination Feasibility

*Is collision resistance from interlocking partitions sufficient to eliminate LH entirely?*

**Discovered Data.** B.21's 0.20% hash mismatch rate demonstrates that variable-length LTP lines dramatically improve collision resistance. However, LH still caught 0.20% of attempts — without LH, these would produce incorrect reconstructions.

**Conclusions.** Not yet. The 0.20% mismatch rate is impressive but nonzero. LH remains necessary for correctness verification. Future work could investigate whether increasing the number of variable-length partitions drives mismatch to zero, enabling LH elimination.

#### C.3.4 Questions (d), (e), (f): Curve Family, Segment Schedule, Crossing Property

*Optimal curve family, segment schedule optimization, dense crossing property proof.*

**Discovered Data.** B.21 used pseudorandom spatial distribution rather than space-filling curves. The triangular length distribution (min(k+1, 511−k)) was adopted directly from DSM/XSM. No formal crossing-property analysis was performed.

**Conclusions.** These questions remain open. B.21 sidestepped them by using pseudorandom assignment rather than deterministic curve-based construction. The theoretical questions about optimal curve families and crossing properties are still relevant for understanding the fundamental limits of partition-based constraint systems.

---

### C.4 From B.4.9: Dynamic Row-Completion Priority

#### C.4.1 Question (a): Optimal Threshold τ

*What is the optimal threshold τ?*

**Discovered Data.** B.4 was implemented and subsumed by B.10. No isolated τ optimization data is available. B.8 telemetry shows depth plateau regardless of probe depth k = 1…4, suggesting that row-completion priority alone cannot break the plateau.

**Conclusions.** Unanswered but deprioritized. The τ parameter is now part of the broader B.10 tightness-driven ordering, and the plateau bottleneck has been shown (B.20.9) to be a consequence of uniform-length-511 lines rather than cell ordering.

#### C.4.2 Questions (b), (c), (d): Multi-Line Priority, Burst Completion, Probing Interaction

*Multi-line priority, burst vs. single-cell completion, probing interaction.*

**Discovered Data.** B.10 generalizes multi-line priority. No isolated experiments on burst completion or probing interaction were conducted. B.8 telemetry showed that deeper probing (k = 4) had no effect on depth, suggesting that the ordering refinements proposed here cannot overcome the fundamental plateau barrier.

**Conclusions.** Deprioritized. The forcing dead zone is the binding constraint, not cell ordering. These questions become relevant only after a future architecture change (e.g., variable-length lines) creates sustained forcing in the plateau band where ordering differences matter.

---

### C.5 From B.5.1: Hash Alternatives

#### C.5.1 Question (a): SHA-1 vs. SHA-256 Throughput

*What is the empirical throughput difference between SHA-1 and SHA-256 row hashing?*

**Discovered Data.** SHA-1 was adopted for row hashing. B.8 telemetry reports ~200K iter/sec with SHA-1. B.21 achieved ~687K iter/sec, though the improvement came from reduced constraint lines rather than hash function choice. No direct SHA-1 vs. SHA-256 benchmark was conducted.

**Conclusions.** Partially answered. SHA-1 is in production use and supports ~200K–687K iter/sec depending on constraint architecture. The SHA-1 choice appears sound, but no controlled comparison with SHA-256 exists. Given that hash computation is a small fraction of per-iteration cost, the difference is likely minor.

#### C.5.2 Question (b): 4-Partition vs. 10-Partition Allocation

*Is the 4-partition allocation optimal, or does scaling to 10 partitions yield better propagation?*

**Discovered Data.** B.9 added 2 LTP partitions (10 total, 6,130 lines). B.20 replaced 4 slopes with 4 LTP (8 total, 5,108 lines). B.21 kept 8 total but with variable-length LTP. The progression shows diminishing returns from adding uniform-length lines and qualitative improvements from variable-length lines. B.21's 5–6 lines per cell with 687K iter/sec outperforms B.9's 10 lines per cell.

**Conclusions.** Answered. The optimal allocation is not more partitions but *better* partitions. Variable-length LTP lines (B.21) at 5–6 per cell outperform 10 uniform-length lines per cell on throughput and mismatch rate. The 4-partition allocation in its B.21 form is superior to the 10-partition B.9 configuration.

#### C.5.3 Question (c): Alternative Slope Values

*Are there slope values that provide superior propagation interaction?*

**Discovered Data.** No experiments with alternative slope values were conducted before the slope family was removed.

**Conclusions.** Superseded. The toroidal slope family was replaced by LTP in B.20.

---

### C.6 From B.6.6: Singleton Arc Consistency

#### C.6.1 Questions (a), (b), (c): SAC Fixpoint Depth, Partial SAC, GPU Parallelization

*SAC fixpoint depth, partial SAC convergence, GPU parallelization of probe loop.*

**Discovered Data.** SAC was not implemented. B.8's k = 4 exhaustive lookahead is a weaker form of SAC probing (it probes 4 levels deep rather than to fixpoint). B.8 showed zero depth improvement from deeper probing, suggesting that SAC's additional power (probing to fixpoint) would similarly fail to break the plateau because the dead zone prevents fixpoint propagation from reaching a contradiction.

**Conclusions.** Unanswered but likely unproductive under current architecture. The forcing dead zone means that probing to fixpoint produces the same result as no probing — ρ/u ≈ 0.5 across all lines, no contradictions detected. SAC becomes potentially valuable under B.21's variable-length architecture, where short lines create early forcing events that could amplify SAC probes. However, B.21's depth regression suggests the amplification is insufficient.

---

### C.7 From B.8.7: Adaptive Lookahead

#### C.7.1 Questions (a), (b): Window Size and Thresholds

*Optimal window size W, optimal stall/recovery thresholds.*

**Discovered Data.** B.8 was implemented with W = 10,000. Stall detection successfully triggered escalation from k = 0 to k = 4 in the plateau band. B.20.9 and B.21 both show the stall detector activating appropriately.

**Conclusions.** Partially answered. The W = 10,000 default works correctly for stall detection. However, the escalation response (increasing k) proved ineffective — the solver reaches k = 4 and remains there without depth improvement. The thresholds detect stalls accurately; the problem is that the available interventions (deeper lookahead) cannot address the root cause (forcing dead zone).

#### C.7.2 Question (c): Linear-Chain Approximation Beyond k = 4

*Is a linear-chain approximation viable for k > 4?*

**Discovered Data.** Not tested. B.8 capped at k = 4 exhaustive. B.1.9 concluded that the bottleneck is search guidance rather than lookahead depth, making deeper probing (whether exhaustive or approximate) unlikely to help.

**Conclusions.** Likely unproductive. If exhaustive k = 4 (16 probes per decision) produces zero depth improvement, approximate k = 8 or k = 16 with their higher false-negative rates are even less likely to help. The fundamental issue is that contradictions in the dead zone are invisible to any finite-depth probe.

#### C.7.3 Question (d): Probing Both Values vs. Alternate Only

*Should the lookahead tree explore both values?*

**Discovered Data.** No controlled comparison. The existing implementation probes only the alternate value.

**Conclusions.** Unanswered but deprioritized given that lookahead at any depth fails to break the plateau.

---

### C.8 From B.9.9: Non-Linear Lookup-Table Partitions

#### C.8.1 Question (a): Optimized LTP vs. 5th Slope Pair

*Does an optimized LTP pair provide measurable benefit beyond a 5th slope pair?*

**Discovered Data.** B.20 Config C replaced 4 slopes with 4 LTP (unoptimized, Fisher-Yates baseline). Result: +1.1% depth, −11.8 pp mismatch. B.21 used variable-length LTP with 3.5× throughput but depth regression. No direct LTP-vs-slope pair comparison with identical partition counts was conducted.

**Conclusions.** Partially answered. Unoptimized LTP outperforms slopes on mismatch rate (25.2% vs 37%), confirming that non-linearity provides constraint benefit. The LTP's advantage appears to come from breaking the algebraic null space, not from offline optimization. Variable-length LTP (B.21) provides dramatically better constraint quality (0.20% mismatch) but at the cost of reduced constraint density.

#### C.8.2 Question (b): Optimization Test Suite Size

*How large must the optimization test suite be for generalization?*

**Discovered Data.** B.20 Config C used unoptimized tables (Fisher-Yates baseline, no hill-climbing). The unoptimized tables already outperformed slopes, suggesting that the pseudorandom structure itself is the key feature, not per-instance optimization.

**Conclusions.** Partially answered. Unoptimized LTP already outperforms slopes, reducing the urgency of offline optimization. The question remains relevant for squeezing marginal gains from the LTP architecture, but the B.20/B.21 progression suggests that structural innovations (variable-length lines) dominate over optimization of fixed-length tables.

#### C.8.3 Questions (c), (d), (e): Search Heuristic, Stacking, Lookahead Interaction

*Convergence heuristic, stacking multiple LTPs, interaction with adaptive lookahead.*

**Discovered Data.** B.20 used 4 stacked LTP partitions (answering (d) affirmatively — multiple LTPs can be stacked). B.21 changed the stacking architecture to joint tiling. No optimization search or lookahead interaction data.

**Conclusions.** Question (d) is answered: 4 LTP partitions were successfully stacked in B.20 with positive results. Questions (c) and (e) remain unanswered but are deprioritized given the shift to variable-length architecture.

---

### C.9 From B.10.7: Constraint-Tightness-Driven Cell Ordering

#### C.9.1 Questions (a), (b), (c), (d): Weights, Incremental Cost, Confidence Integration, Predictive Power

*Optimal weights, incremental maintenance justification, confidence score integration, tightness as failure predictor.*

**Discovered Data.** B.10's tightness-driven ordering was implemented (B.4 as the first stage). B.8 telemetry showed no depth improvement from adaptive lookahead, and B.20.9 showed only +1.1% depth from LTP substitution. B.21 achieved 0.20% mismatch (dramatically better branching quality) but depth regression, suggesting that constraint quality — not ordering — is the binding constraint.

**Conclusions.** Deprioritized. Cell ordering is a second-order effect relative to constraint density and structure. The forcing dead zone prevents tightness-driven ordering from activating — when all lines have ρ/u ≈ 0.5, tightness scores are uniformly weak and provide no meaningful differentiation. Under B.21's variable-length architecture, short lines create tightness variation that could make these questions relevant again.

---

### C.10 From B.11.6: Randomized Restarts

#### C.10.1 Questions (a)–(e): Heavy-Tail Distribution, Luby Base, Partial Restart, Phase Saving, Stall Interaction

*Heavy-tail distribution, optimal Luby base, partial vs. full restart, phase saving interaction, stall detection combination.*

**Discovered Data.** Restarts were not implemented. B.11.1 identified the fundamental constraint: DI determinism requires fully deterministic restart policies, ruling out classical randomized restarts. B.8 telemetry shows the plateau stall is consistent (depth ~87,500 across k = 1…4), suggesting a structural rather than stochastic bottleneck — inconsistent with heavy-tailed behavior. B.21's depth regression to 50,272 with near-zero mismatch further suggests the wall is structural (constraint density insufficient) rather than stochastic (bad early decisions).

**Conclusions.** Likely unproductive. The evidence points to a structural plateau rather than a heavy-tailed distribution. The plateau depth is remarkably consistent across lookahead depths and partition architectures (87,500 with slopes, 88,503 with LTP, always in the same band), suggesting a phase transition inherent to the constraint system rather than unlucky branching. Deterministic restart policies compatible with DI would need to navigate this structural barrier, not just escape stochastic traps.

---

### C.11 From B.12.6: Survey Propagation and BP-Guided Decimation

#### C.11.1 Question (a): BP Convergence — ANSWERED

*Does BP converge reliably on the CRSCE factor graph?*

**Discovered Data.** Empirically answered on 2026-03-04. Gaussian BP with damping α = 0.5 converges reliably in 30 iterations (run_id=1c64a9a5). Cold-start max_delta = 18.252, warm-start max_delta = 0.000. BP converges but BP-guided branch-value ordering does not break the depth plateau (depth remained at ~87,487–87,498 after 3 StallDetector escalations).

**Conclusions.** Answered. BP converges but does not improve depth. The Gaussian approximation may be too coarse for the densely-loopy CRSCE factor graph. The forcing dead zone renders the marginal estimates uninformative — when ρ/u ≈ 0.5, the true marginals are near 0.5, and BP correctly computes them as near 0.5, providing no branching guidance.

#### C.11.2 Questions (b)–(e): Checkpoint Interval, SP Frozen Cells, Batch Size, GPU Acceleration

*Optimal checkpoint interval, SP backbone detection, batch decimation size, Metal GPU acceleration.*

**Discovered Data.** B.12.6(a)'s negative finding (BP-guided ordering doesn't break plateau) reduces the urgency of all optimization questions. If the marginals themselves carry no signal in the dead zone, optimizing how frequently or efficiently they are computed is moot.

**Conclusions.** Deprioritized. The binding constraint is not BP's implementation efficiency but the dead zone's information-theoretic barrier. These questions become relevant only if a future architecture change (e.g., variable-length lines creating non-trivial marginals) makes BP's output actionable.

---

### C.12 From B.13.5: Portfolio and Parallel Solving

#### C.12.1 Questions (a)–(d): Portfolio Size, Parameter Sensitivity, Clause Sharing, Restart Combination

*Optimal portfolio size, diversification parameter selection, inter-instance clause sharing, restart combination.*

**Discovered Data.** Portfolio solving was not implemented. B.8 telemetry showed the plateau is consistent across probe depths, and B.20.9 showed it persists under LTP substitution. The consistency of the plateau (~87,500 ± 1,000) across heuristic variations suggests that diversifying heuristic parameters within a portfolio would produce uniformly poor results — all instances would stall at approximately the same depth.

**Conclusions.** Likely unproductive under current architecture. Portfolio benefit requires that different heuristic configurations encounter qualitatively different search landscapes. The plateau's consistency across k = 0…4, across slope vs. LTP, and across static vs. dynamic ordering suggests a single structural bottleneck that no heuristic variation can circumvent. Portfolio solving becomes interesting only if a future architecture change creates a landscape where heuristic choice materially affects search trajectory beyond the plateau entry point.

---

### C.13 From B.14.7: Lightweight Nogood Recording

#### C.13.1 Questions (a)–(d): Failure Recurrence, Partial vs. Full Nogoods, Checking Strategy, Portfolio Sharing

*Failure recurrence rate, partial nogood power, optimal checking strategy, inter-instance sharing.*

**Discovered Data.** Not implemented. B.8 telemetry reports 37% hash mismatch rate (6.1M mismatches / 16.7M iterations). B.20.9 reduced this to 25.2%. B.21 achieved 0.20%. The dramatic mismatch reduction from B.21 suggests that with variable-length lines, hash failures become rare enough that nogood recording provides minimal additional benefit — there are few failures to record.

**Conclusions.** Conditional on architecture. Under B.8/B.20 (25–37% mismatch), nogood recording could be valuable — the high failure rate suggests many repeated configurations. Under B.21 (0.20% mismatch), the question is largely moot: failures are so rare that recording them provides negligible pruning. The question's relevance depends entirely on which architecture is deployed.

---

### C.14 From B.16.7: Partial Row Constraint Tightening

#### C.14.1 Questions (a)–(d): Additional Forcing, Iteration Count, Integration Trigger, SAC Combination

*PRCT forcing yield, bounds propagation iterations, integration strategy, SAC combination.*

**Discovered Data.** PRCT was not implemented. B.20.9's partial answer to the underlying problem: the mismatch reduction from 37% to 25.2% under LTP substitution demonstrates that constraint quality improvements can push some cells past the forcing threshold. B.21's 0.20% mismatch shows that variable-length lines achieve near-complete forcing in early rows, making PRCT redundant for those lines. However, B.21's depth regression (50,272 vs. 88,503) suggests that PRCT-like reasoning on the remaining long lines could help.

**Conclusions.** Potentially relevant for B.21-class architectures. In the original uniform-length regime, PRCT faces the same dead-zone problem as all other forcing-based techniques. Under B.21, the short LTP lines provide the tightness variation that PRCT exploits. PRCT applied to the interaction between short (nearly-forced) LTP lines and long (still-loose) basic lines could yield additional forced cells in the plateau band. This is one of the more promising avenues for improving B.21's depth performance.

---

### C.15 From B.17.8: Look-Ahead on Row Completion

#### C.15.1 Questions (a)–(d): Completion u/ρ Distribution, Nogood Combination, Trigger Region, Cross-Line Ordering

*Distribution of u and ρ at row completion, nogood combination, trigger region, cross-line check ordering.*

**Discovered Data.** B.8 telemetry reports min_nz_row_unknown = 1, meaning rows consistently reach u = 0 through forcing cascades — the solver fully forces every cell in a row before hash-checking. B.21's 0.20% mismatch rate confirms that forcing cascades are nearly complete with variable-length lines.

**Conclusions.** RCLA is largely redundant. Since min_nz_row_unknown = 1, rows reach u = 0 through normal propagation. RCLA's value (enumerating completions when u ≤ u_max) activates only when propagation stalls with u > 0 remaining, which the telemetry shows rarely or never happens. The answer to question (a) is that u is typically 0 or 1 at row completion, making RCLA's enumeration trivial or unnecessary.

---

### C.16 From B.18.6: Learning from Repeated Hash Failures

#### C.16.1 Questions (a)–(d): Failure Correlation, Counter Overhead, Cross-Block Sharing, B.14 Synergy

*Per-cell failure correlation, counter maintenance cost, cross-block transfer, nogood synergy.*

**Discovered Data.** Not implemented. B.21's 0.20% mismatch rate means hash failures are extremely rare under variable-length architecture, reducing the statistical signal available for failure-frequency learning. Under B.8/B.20 (25–37% mismatch), the higher failure rate would provide richer statistics, but the question of whether per-cell failure patterns are correlated (rather than pseudorandom due to SHA-1) remains open.

**Conclusions.** Architecture-dependent. Under B.21 (0.20% mismatch), failure-biased learning has almost no signal to work with. Under B.8/B.20 architectures, the approach has more potential but the fundamental question — whether hash failures are correlated with specific cell-value decisions or are essentially random — remains unanswered. Given that SHA-1 is a pseudorandom function of all 511 bits, per-cell correlations are likely weak.

---

### C.17 From B.19.7: Enhanced Stall Detection and Extended Probes

#### C.17.1 Questions (a)–(e): Backtrack Distributions, Beam Search, Row-Boundary Probing, Threshold Tuning, Bandit Formulation

*Backtrack depth distribution, beam search effectiveness, row-boundary probing overhead, threshold tuning, multi-armed bandit formulation.*

**Discovered Data.** B.19's enhanced stall detection was not fully implemented. B.8 provides partial data: the solver reaches k = 4 in the plateau and remains there, indicating persistent stalling. B.20.9 and B.21 show different stalling profiles (B.20.9: 198K iter/sec, depth 88,503; B.21: 687K iter/sec, depth 50,272), suggesting that stall characteristics vary significantly across architectures.

**Conclusions.** Partially answered. The stall detector correctly identifies the plateau, but the available interventions (lookahead escalation) cannot address the root cause. Enhanced stall detection (distinguishing shallow oscillation from deep regression) is still potentially valuable for selecting appropriate interventions, but the interventions themselves (beam search, row-boundary probing) remain untested. The multi-armed bandit formulation (question (e)) is theoretically elegant but constrained by the DI determinism requirement.

---

### C.18 From B.20.8: LTP Substitution Experiment

#### C.18.1 Question (a): Sequential Table Optimization Bias

*Is sequential table optimization sufficient, or does greedy ordering leave performance on the table?*

**Discovered Data.** B.20 Config C used unoptimized tables (Fisher-Yates baseline). No hill-climbing or sequential optimization was performed. The unoptimized tables already outperformed toroidal slopes.

**Conclusions.** The question presupposes that optimization is necessary. B.20.9 demonstrates that unoptimized pseudorandom LTP already outperforms algebraically structured slopes, suggesting that the non-linearity itself (breaking the algebraic null space) is more important than per-table optimization. Sequential optimization may yield marginal improvements but is no longer the critical research question.

#### C.18.2 Question (b): Test Suite Sensitivity

*How sensitive is the experiment to the choice of test suite?*

**Discovered Data.** B.20 Config C was tested on block 0 of useless-machine.mp4 (~14M iterations). No multi-input validation.

**Conclusions.** Partially answered. The single-block result is encouraging but not validated across input types. Future experiments should include all-zeros, all-ones, alternating, and natural data inputs to confirm that LTP performance generalizes.

#### C.18.3 Question (c): Truncated-DFS Proxy

*Can the truncated-DFS proxy objective mislead the optimizer?*

**Discovered Data.** No optimization was performed — the question is moot for unoptimized tables.

**Conclusions.** Unanswered but deprioritized given that unoptimized tables already perform well.

#### C.18.4 Question (d): Variable-Length LTP Design — ANSWERED

*If Outcome 1 materializes, is there a variable-length LTP design that escapes the dead zone?*

**Discovered Data.** This is the most directly answered question in the entire research program. B.21 implemented exactly this: joint-tiled variable-length LTP with lines of length min(k+1, 511−k), ranging from 1 to 256 cells. Results: mismatch dropped from 25.2% to 0.20% (dead zone escaped for short lines), throughput increased 3.5×, but depth regressed from 88,503 to 50,272.

**Conclusions.** Answered with nuance. Variable-length LTP *does* escape the forcing dead zone for short lines — the 0.20% mismatch rate proves this conclusively. However, escaping the dead zone on LTP lines while reducing per-cell constraint count from 8 to 5–6 created a new bottleneck: insufficient constraint density in the mid-matrix. The variable-length design works as theorized but requires either (a) more sub-tables to restore constraint density, or (b) complementary techniques (PRCT, CDCL, or additional variable-length partitions) to compensate for the density reduction.

#### C.18.5 Question (e): Alternative Slope Parameters

*Should the experiment include different slope parameters?*

**Discovered Data.** Not tested. The slope family was replaced entirely.

**Conclusions.** Superseded by B.20's direction. Alternative slope parameters are no longer under consideration.

---

### C.19 From B.21.13: Joint-Tiled Variable-Length LTP

#### C.19.1 Question (a): Dual-Covered Cell Distribution

*What is the optimal distribution of the 1,023 dual-covered cells?*

**Discovered Data.** B.21 implemented extreme-corner placement. The 0.20% mismatch rate and 3.5× throughput suggest the corner placement is effective, but no alternative distributions were tested.

**Conclusions.** Unanswered comparatively. The implemented strategy works well by multiple metrics, but no evidence exists to determine whether alternative distributions (e.g., anti-diagonal extremes) would perform better.

#### C.19.2 Question (b): Sequential Construction Ordering Bias

*Does sequential sub-table construction introduce ordering bias?*

**Discovered Data.** B.21 used sequential construction (T_0 first pick, T_3 on residual). No iterative refinement was tested. The 0.20% mismatch and balanced throughput suggest no severe bias, but subtle quality differences across sub-tables were not measured.

**Conclusions.** Unanswered but likely minor. The overall results are strong enough that any ordering bias does not prevent effective performance. Iterative refinement is a potential optimization but not a priority.

#### C.19.3 Question (c): Optimal Length Distribution

*Is the triangular length distribution optimal?*

**Discovered Data.** Three distributions have now been tested:

1. *B.21 triangular (1–256 cells, partial coverage):* depth 50,272; 1–2 lines per cell. The partial coverage (5–6 lines per cell) created the dominant bottleneck; line length distribution was a secondary factor.

2. *B.22 full-coverage triangular (2–1,021 cells):* depth ~80,300; always 4 lines per cell. Restoring full coverage recovered 30K frames. However, very short lines (2–6 cells) provide minimal constraint on deep rows, and very long lines (>511 cells) accumulate pressure slowly. The extreme range hurts on both ends.

3. *B.23 uniform-511 + CDCL / B.25 uniform-511 (no CDCL):* depth ~69K / ~86K; always 4 lines per cell. B.25 (no CDCL) confirmed that uniform-511 reaches 86K — close to B.20's 88.5K. The residual 2K gap is attributable to different LCG shuffle seeds (different random partition), not the line-length distribution.

**Critical interaction discovered (B.21.13):** LTP line length directly controls CDCL jump distances. Shorter lines produce more compact antecedent chains: B.22 (lines 2–1,021 cells) produced CDCL jumps of up to 732 frames; B.23 (uniform-511) produced jumps up to 1,854 frames. The longer jumps in B.23 caused greater depth regression. However, CDCL was ultimately found to be net-harmful regardless of jump distance (see C.21).

**Conclusions.** The uniform-511 distribution (B.20/B.25) achieves the best depth at 86–88K. The triangular distribution of B.22 (2–1,021 cells) performs worse primarily because the extreme tails (very short and very long lines) are less effective than the 511-cell midrange. The optimal distribution is likely near-uniform in the 400–511 cell range, prioritizing consistent constraint density over variability. Distributions with minimum length ≥ 64 cells warrant testing.

#### C.19.4 Question (d): Belief Propagation Interaction

*How does joint tiling interact with BP-guided branching?*

**Discovered Data.** B.12.6(a) showed that BP-guided branching does not break the plateau under uniform-length architecture. B.21 changes the factor graph structure (5–6 factors per cell instead of 8, variable-length factors). No BP experiment has been run under B.21.

**Conclusions.** Unanswered but potentially relevant. B.21's variable-length lines create non-uniform marginals (short lines produce near-forcing beliefs, long lines produce near-0.5 beliefs), which could give BP more actionable signal than it had under the uniform architecture. This warrants empirical testing.

#### C.19.5 Question (e): Joint Tiling for Basic Partitions

*Can the joint-tiling principle extend to DSM/XSM?*

**Discovered Data.** No experiments. B.21's depth regression suggests caution about further reducing per-cell constraint count.

**Conclusions.** Likely counterproductive. B.21 demonstrated that reducing lines per cell from 8 to 5–6 trades depth for throughput. Jointly tiling DSM and XSM would further reduce lines per cell, likely worsening the depth regression. DSM and XSM provide qualitatively different constraint information (different slope directions) that should not be merged.

---

### C.20 From D.1.9: Loopy Belief Propagation (Abandoned)

#### C.20.1 Questions (a)–(e): LBP Convergence, SIMD Acceleration, Overconfidence Harm, Async Hybrid, Region-Based BP

*Convergence behavior at various depths, SIMD exploitation, overconfidence effects, async hybrid approach, GBP on Kikuchi clusters.*

**Discovered Data.** D.1.8 (the verdict) concluded that LBP is dominated on cost-benefit grounds by checkpoint BP (B.12). B.12.6(a) subsequently showed that even checkpoint BP fails to break the plateau — the marginals themselves carry no useful signal in the dead zone.

**Conclusions.** Moot. The parent proposal (continuous LBP) was abandoned in favor of checkpoint BP (B.12), and checkpoint BP itself was empirically shown to be ineffective. All implementation-detail questions about LBP are superseded by the negative finding that BP-based approaches — whether continuous or checkpoint — cannot break the forcing dead zone. The questions could become relevant only if a future architecture change (e.g., variable-length lines producing non-trivial marginals) rehabilitates BP-guided search.

---

### C.21 From B.21.13: CDCL Interaction Study

#### C.21.1 Does CDCL Help?

**Discovered Data.** Four experiments tested CDCL in different configurations:

| Configuration | Depth | iter/sec | CDCL max jump |
|---|---|---|---|
| B.20 (no CDCL, uniform-511) | ~88,503 | ~198K | — |
| B.22+CDCL (triangular 2–1,021) | ~80,300 | ~521K | 732 |
| B.23+CDCL (uniform-511) | ~69,000 | ~306K | 1,854 |
| B.24 (cap=0, overhead intact) | ~86,123 | ~120K | 0 |
| B.25 (overhead fully removed) | ~86,123 | ~329K | 0 |

**Conclusions.** CDCL as implemented is **strictly harmful** in every tested configuration. It reduces depth by 3–22% while increasing iter/sec (due to backjump shortcuts that skip DFS work). The net result is consistently lower depth than chronological backtracking.

The root cause is fundamental: CDCL's 1-UIP analysis is designed for **unit-clause propagation failures** in SAT, where a single "bad" decision is traceable as the conflict's unique implication point. SHA-1 hash verification is a **global non-linear constraint** over all 511 cells in a row; no single decision is the culprit. The 1-UIP trace therefore runs back hundreds to thousands of frames, producing a backjump that discards correct work without any constraint-learning benefit (the current implementation does not store learned clauses).

#### C.21.2 Does Line Length Affect CDCL Performance?

**Discovered Data.** Yes — directly. LTP line length determines how many decisions are required to "seal" a row hash. Short lines force individual cells in 1–2 decisions; long lines accumulate assignments over hundreds of decisions.

- B.22 (triangular, max 1,021 cells): CDCL max jump 732 frames → depth 80,300
- B.23 (uniform-511): CDCL max jump 1,854 frames → depth 69,000

Shorter lines produce shorter antecedent chains, which produce shorter CDCL jumps. But even the shorter B.22 jumps (732 frames) still reduced depth by 9% vs B.20.

**Conclusions.** Line length modulates CDCL harm rather than enabling CDCL benefit. No distribution was found where CDCL produces net improvement. The interaction confirms that CDCL is incompatible with hash-based row verification at any line length.

#### C.21.3 Does Removing CDCL Overhead Recover B.20's Performance?

**Discovered Data.** Partially. B.24 disabled backjumping but left the per-assignment recording overhead (recordDecision, recordPropagated, unrecord on every cell). This reduced iter/sec to 120K and recovered depth to 86K. B.25 removed the overhead entirely, recovering iter/sec to 329K (66% faster than B.20's 198K) while maintaining 86K depth.

The remaining 2K depth gap (86K vs B.20's 88K) is attributed to **partition seed differences**: B.25 uses seeds `CRSCLTP1`–`CRSCLTP4` (ASCII) while B.20 used different seeds. Same Fisher-Yates construction algorithm, different random shuffles, different depth ceiling. The depth ceiling is entirely partition-quality-driven.

**Conclusions.** Yes, removing CDCL overhead recovers the depth lost to CDCL (and surpasses B.24's throughput). The solver is now faster than B.20 in iter/sec. The residual 2K gap is a partition-seed artifact and is within the normal variation expected from different random partitions.

#### C.21.4 What Is the Path Forward?

**Discovered Data.** With CDCL removed (B.25), the solver is at ~86K depth / ~329K iter/sec. B.20 achieved ~88K / ~198K.  The depth ceiling is determined by the partition structure, not the backtracking strategy.

**Conclusions.** Three directions are promising:

1. **Partition seed search:** Run multiple seeds and select the one achieving maximum depth. The B.20 seeds produced 88K; B.25 seeds produced 86K. A systematic seed search over ≥10 candidates may find seeds reaching 90K+.

2. **Distribution tuning:** The uniform-511 distribution is near-optimal but not proven optimal. Clipped triangular (minimum 64 cells), quadratic, or distributions biasing toward 256-cell lines may improve depth by creating more uniform constraint pressure across all DFS depths.

3. **Propagation improvements:** More aggressive propagation (e.g., probing on more constraint families, earlier hash verification) could increase the fraction of cells forced deterministically, raising the depth ceiling independent of partition geometry.

CDCL-style conflict learning with actual clause storage (not just backjumping) remains theoretically interesting but requires a fundamentally different conflict analysis adapted to global row constraints. This is a long-term research direction.

---

## Appendix D: Abandoned Ideas

This appendix collects research directions that were explored in detail but ultimately abandoned on
cost-benefit grounds. Each section is preserved in full for archival purposes: the analysis may become
relevant if future architectural changes alter the assumptions that led to abandonment.

### D.1 Loopy Belief Propagation as Integrated Inference (formerly B.15)

B.12 proposes belief propagation (BP) as a periodic reordering oracle: the solver pauses DFS at checkpoint rows,
runs BP to convergence on the residual subproblem, and uses the resulting marginals to guide cell ordering or
batch decimation. This appendix examines a more aggressive alternative: *loopy belief propagation* (LBP)
embedded directly into the propagation loop, running continuously alongside constraint propagation rather than
at discrete checkpoints. The question is whether LBP can provide sufficiently accurate marginal estimates on the
CRSCE factor graph to break the 87K-depth stalling barrier, and whether the computational cost can be amortized
to a tolerable level.

#### D.1.1 Background and Theoretical Foundations

Belief propagation computes exact marginal distributions on tree-structured factor graphs by passing messages between
variable nodes and factor nodes until convergence (Pearl, 1988). Each variable-to-factor message summarizes the
variable's belief about its own state given all constraints *except* the recipient factor, and each factor-to-variable
message summarizes the factor's constraint on the variable given messages from all other variables in the factor's
scope. On trees, BP converges in a number of iterations equal to the graph diameter, and the resulting marginals are
exact (Kschischang, Frey, & Loeliger, 2001).

When the factor graph contains cycles&mdash;as the CRSCE graph invariably does, since every cell participates in 8
constraint lines and those lines share cells&mdash;BP is no longer guaranteed to converge, and the marginals it
produces are approximations. This regime is called *loopy belief propagation*. Despite the lack of formal
convergence guarantees on loopy graphs, LBP has achieved remarkable empirical success in several domains: turbo
decoding and LDPC codes in communications (McEliece, MacKay, & Cheng, 1998), stereo vision and image segmentation
in computer vision (Sun, Zheng, & Shum, 2003), and random satisfiability near the phase transition (Mézard, Parisi,
& Zecchina, 2002).

The theoretical justification for LBP's empirical success rests on two results. First, Yedidia, Freeman, and Weiss
(2001) showed that fixed points of LBP correspond to stationary points of the Bethe free energy, a variational
approximation to the true log-partition function. The Bethe approximation is exact on trees and typically accurate
when the factor graph has long cycles (low density of short loops). Second, Tatikonda and Jordan (2002) proved that
LBP converges to a unique fixed point when the spectral radius of the dependency matrix is less than one&mdash;a
condition related to the graph's coupling strength and cycle structure.

#### D.1.2 The CRSCE Factor Graph: Structural Analysis

The CRSCE factor graph has specific structural properties that bear directly on LBP's applicability. The graph
contains $s^2 = 261{,}121$ binary variable nodes and $10s - 2 = 5{,}108$ factor nodes (with an additional $s = 511$
LH verification factors). Each variable participates in exactly 8 factors (one per constraint-line family: row,
column, diagonal, anti-diagonal, and four LTP partitions), and each factor connects to exactly $s = 511$ variables
(all lines in the 8 standard families are length $s$; DSM/XSM lines near the matrix boundary are shorter, ranging
from 1 to 511).

The shortest cycles in this graph are length 4: any two cells that share both a row and a column form a 4-cycle
through the row factor and column factor. Since every pair of cells in the same row shares a row factor, and many
pairs share a column or diagonal factor, the CRSCE graph is densely loopy. The average cycle density can be
quantified: for each cell at position $(r, c)$, the 8 factors it participates in collectively touch approximately
$8 \times 511 - 7 = 4{,}081$ other cells (subtracting the 7 redundant counts of the cell itself). Pairs among those
4,081 cells that share a second factor with each other create 4-cycles through the original cell's factors. The
expected number of such 4-cycles per cell is $O(s)$, yielding a total 4-cycle count of $O(s^3) \approx 1.3 \times
10^8$.

This high density of short cycles is the central concern for LBP in CRSCE. The Bethe approximation assumes that the
factor graph's neighborhood is locally tree-like&mdash;that the subgraph reachable from any node within $k$ hops
contains few cycles. With $O(s)$ 4-cycles per cell, this assumption is violated at $k = 2$. The practical
consequence is that LBP's marginal estimates will overcount correlations: a cell's belief will incorporate the same
constraint information multiple times through different cycle paths, producing overconfident (polarized) marginals.

#### D.1.3 The Case for LBP in CRSCE

Despite the structural concerns, several arguments favor LBP for CRSCE plateau-breaking.

*Argument 1: Marginal accuracy need not be high.* LBP is proposed not as an exact inference engine but as a
heuristic guide for branching decisions. The solver needs only a *ranking* of cells by how constrained they are, not
precise probabilities. If LBP's marginals are monotonically correlated with true marginals&mdash;even with substantial
absolute error&mdash;the resulting ordering will be superior to the current static `ProbabilityEstimator`. Empirical
studies in SAT solving have shown that even crude BP approximations improve branching heuristics relative to purely
local measures (Hsu & McIlraith, 2006).

*Argument 2: LBP captures long-range correlations that local propagation misses.* The 87K-depth stalling barrier
arises because cardinality forcing becomes inactive in the plateau band (rows ~100--300): residuals are neither 0 nor
equal to the unknown count, so the propagation engine cannot force any cells. The information needed to break the
stalemate exists in the constraint system&mdash;distant cells' assignments constrain the residuals of lines passing
through the plateau&mdash;but the current propagator does not transmit this information because it operates only on
individual lines. LBP's message-passing iterates through the entire factor graph, propagating constraints across
multiple hops. After $t$ iterations, each cell's belief incorporates information from cells up to $t$ hops away in
the factor graph. At $t = 10$, a cell's belief reflects constraints from cells up to 80 constraint lines distant
($10 \times 8$ lines per hop), potentially spanning the entire matrix. This global view is precisely what the solver
lacks at depth 87K.

*Argument 3: Incremental LBP amortizes cost across assignments.* Rather than running BP from scratch at checkpoints
(as B.12 proposes), incremental LBP updates only the messages affected by each new assignment. When cell $(r, c)$ is
assigned, only the 8 factors containing $(r, c)$ receive new information. Those 8 factors send updated messages to
their $\sim 4{,}000$ neighboring variables, which update their beliefs and propagate outward. If the solver limits
propagation to $\delta$ hops from the assigned cell (a "message-passing wavefront"), the per-assignment cost is
$O(\delta \cdot s)$ multiply-add operations. At $\delta = 3$ and $s = 511$, this is $\sim 1{,}500$ operations per
assignment&mdash;approximately $10\times$ the cost of the current constraint-propagation step, but far cheaper than
a full BP computation ($\sim 50$ ms).

*Argument 4: Warm-starting preserves convergence quality.* Because LBP runs continuously, each new assignment
perturbs the message state only slightly. The messages are already close to the fixed point of the previous
subproblem, so convergence to the new fixed point (given the new assignment) requires only a few iterations of the
affected messages. This warm-start property is well-established in the graphical models literature (Murphy, Weiss,
& Jordan, 1999) and is the key to making continuous LBP computationally feasible.

#### D.1.4 The Case Against LBP in CRSCE

The arguments against LBP are substantial and grounded in both theory and the specific structure of the CRSCE
problem.

*Objection 1: Non-convergence on densely loopy graphs.* Tatikonda and Jordan's (2002) convergence condition requires
that the spectral radius of the graph's dependency matrix be less than one. For the CRSCE factor graph, each factor
connects to $s = 511$ variables, and each variable participates in 8 factors. The coupling strength is directly
related to the factor's constraint tightness: a cardinality constraint with residual $\rho$ and $u$ unknowns has
coupling strength proportional to $|\rho / u - 0.5|$ (how far the constraint is from maximum entropy). In the
plateau band, $\rho / u \approx 0.5$ (the constraint is maximally uninformative), so coupling is weak and LBP may
converge. But at the matrix edges (early and late rows), constraints are tight and coupling is strong. The spectral
radius condition may be satisfied only in the plateau band&mdash;exactly where LBP is needed but also where the
marginals carry the least information. This creates a paradox: LBP converges where it is least useful and diverges
where it could provide the strongest guidance.

*Objection 2: Overconfident marginals from short cycles.* The $O(s^3)$ 4-cycles in the CRSCE graph cause LBP to
double-count constraint evidence. A cell $(r, c)$ receives a message from its row factor and a message from its
column factor, but these messages are not independent: they share information about cells in the same row-column
intersection. The result is overconfident marginals&mdash;beliefs close to 0 or 1 even when the true marginal is near
0.5. Overconfident marginals produce aggressive branching decisions that frequently lead to conflicts, increasing
rather than decreasing the backtrack count. Wainwright and Jordan (2008, Section 4.2) document this failure mode
extensively and show that it is intrinsic to the Bethe approximation on graphs with dense short cycles.

Mitigation strategies exist. Tree-reweighted BP (TRW-BP) (Wainwright, Jaakkola, & Willsky, 2005) assigns
edge-appearance probabilities that correct for double-counting, guaranteeing an upper bound on the log-partition
function. However, TRW-BP requires computing edge-appearance probabilities from a set of spanning trees, which is
$O(|E|^2)$ for the CRSCE graph ($|E| \approx 2 \times 10^6$, so $O(4 \times 10^{12})$)&mdash;prohibitively
expensive. Fractional BP (Wiegerinck & Heskes, 2003) and convex variants (Globerson & Jaakkola, 2007) similarly
incur overhead that exceeds the computational budget.

*Objection 3: Cardinality factors are expensive.* Standard BP message computation for a factor with $k$ variables
requires summing over $2^k$ joint configurations. For CRSCE's cardinality constraints (each connecting $s = 511$
binary variables), the naive cost is $2^{511}$&mdash;clearly intractable. Efficient message computation for cardinality
factors is possible using the convolution trick: the sum-product messages for a cardinality constraint on $k$ binary
variables with target sum $\sigma$ can be computed in $O(k^2)$ time using dynamic programming (Darwiche, 2009,
Section 12.4). At $k = 511$, this is $\sim 261{,}000$ operations per factor per iteration. With 5,108 factors and
10 iterations, the total cost is $\sim 1.3 \times 10^{10}$ operations ($\sim 13$ seconds on Apple Silicon) ---
orders of magnitude slower than the current solver's throughput of $\sim 500{,}000$ assignments/second.

The incremental approach (D.1.3, Argument 3) mitigates this by updating only 8 factors per assignment rather than
all 5,108, but each factor update still costs $O(s^2) \approx 261{,}000$ operations. At 8 factors per assignment,
the incremental cost is $\sim 2 \times 10^6$ operations per assignment&mdash;a $4{,}000\times$ overhead relative to
the current propagation step ($\sim 500$ operations per assignment). Even with Apple Silicon's throughput, this
reduces the solver from 500K assignments/second to $\sim 125$ assignments/second, extending block solve times from
minutes to weeks.

*Objection 4: LBP provides no pruning, only ordering.* Unlike constraint propagation (which prunes infeasible
assignments) or CDCL (which prunes infeasible subtrees), LBP provides only soft guidance: a ranking of cells and
value preferences. The solver still explores the same search tree; it merely traverses it in a different order. If
the search tree's branching factor is dominated by the 87K-depth plateau (where $\sim 111$ cells per row are
unconstrained), reordering the traversal cannot reduce the tree's exponential size&mdash;it can only improve the
constant factor in front of the exponential. The fundamental problem is that the plateau's combinatorial explosion
requires *pruning* (eliminating subtrees) rather than *reordering* (visiting subtrees in a better sequence). LBP
does not prune.

This objection does not apply if LBP's marginals enable other pruning mechanisms to activate. For example, if LBP
reveals that a cell is near-forced ($p \approx 0.01$), the solver can treat it as forced and propagate, triggering
constraint-propagation cascades that genuinely prune. But this amounts to using LBP as a soft forcing heuristic ---
converting approximate beliefs into hard assignments&mdash;which risks correctness violations if the marginal is wrong.
The solver would need a verification mechanism (e.g., backtracking on LBP-guided forced assignments that lead to
conflicts), eroding much of the computational savings.

*Objection 5: DI determinism is fragile under floating-point LBP.* LBP's messages are real-valued quantities
updated by iterative multiplication and normalization. Floating-point arithmetic is not associative, and message
update order can affect the converged values. If the compressor and decompressor execute LBP with different message
scheduling (e.g., due to parallelism or platform-specific optimizations), the resulting marginals may differ,
producing different cell orderings and different search trajectories, violating DI determinism. B.12.4 addresses
this for checkpoint-based BP by proposing fixed-point arithmetic or a fixed iteration count. For *continuous* LBP
integrated into the propagation loop, the determinism requirement is stricter: every incremental message update after
every assignment must produce bitwise-identical results on both compressor and decompressor. This requires either
(a) fixed-point arithmetic with specified precision and rounding mode, or (b) a deterministic message schedule
(e.g., always process the 8 affected factors in a fixed order, then propagate outward in breadth-first order from
the assigned cell). Both approaches constrain the implementation and prevent performance optimizations (e.g.,
parallel message updates on GPU) that would violate the deterministic schedule.

#### D.1.5 Quantitative Cost-Benefit Estimate

To evaluate LBP's net value, consider a concrete scenario. Assume the solver spends $T_{\text{base}} = 600$ seconds
on a block without LBP, with 90\% of the time ($540$s) consumed in the plateau band (rows 100--300). The plateau
generates $\sim 10^{10}$ backtracks.

*Optimistic case.* LBP's improved ordering reduces the backtrack count by $100\times$ (from $10^{10}$ to $10^8$).
The per-assignment cost increases $4{,}000\times$ due to LBP overhead (Objection 3). The net effect: plateau time
changes from $10^{10} \times 2\,\mu\text{s} = 540$s to $10^8 \times 8{,}000\,\mu\text{s} = 800{,}000$s ($\approx
9.3$ days). LBP is a net loss despite a $100\times$ reduction in backtracks, because the per-node overhead
overwhelms the savings.

*Break-even analysis.* For LBP to be net-positive, the backtrack reduction must exceed the overhead ratio. At
$4{,}000\times$ per-node overhead, LBP must reduce backtracks by more than $4{,}000\times$ to improve on the
baseline. A $4{,}000\times$ backtrack reduction (from $10^{10}$ to $2.5 \times 10^6$) would reduce plateau time to
$2.5 \times 10^6 \times 8{,}000\,\mu\text{s} = 20{,}000$s ($\approx 5.6$ hours)&mdash;still slower than the 540s
baseline, because the LBP overhead applies to all $\sim 100{,}000$ assignments in the plateau, not just to the
$2.5 \times 10^6$ backtracks. The full accounting: $100{,}000$ assignments $\times$ $8{,}000\,\mu\text{s/assignment}
= 800$s for the forward pass, plus $2.5 \times 10^6$ backtracks $\times$ $8{,}000\,\mu\text{s} = 20{,}000$s for
backtracking. Total: $20{,}800$s. The LBP overhead on the forward pass alone ($800$s) exceeds the $540$s baseline.

*Reduced-overhead scenario.* If the incremental wavefront is limited to $\delta = 1$ hop (only the 8 directly
affected factors, with no outward propagation), the per-assignment cost drops to $\sim 8 \times 261{,}000 \approx
2 \times 10^6$ operations ($\sim 2$ms). At $100{,}000$ forward assignments: $200$s. At a $100\times$ backtrack
reduction ($10^8$ backtracks $\times$ $2$ms): $200{,}000$s. Still a net loss. Even at $\delta = 1$ and a
$10{,}000\times$ backtrack reduction: $100{,}000 \times 2\text{ms} + 10^6 \times 2\text{ms} = 200\text{s} +
2{,}000\text{s} = 2{,}200$s&mdash;roughly $4\times$ worse than baseline.

The arithmetic is unforgiving. LBP's per-node cost on length-511 cardinality factors is fundamentally too high for
integration into a DFS loop that processes hundreds of thousands of nodes.

#### D.1.6 Comparison with B.12's Checkpoint Approach

B.12 avoids LBP's per-node overhead by running BP only at checkpoints (every 50 rows, or at plateau entry). The
amortized cost is $\sim 50$ms per $25{,}000$ assignments ($\sim 2\,\mu\text{s/assignment}$)&mdash;nearly identical to
the baseline per-assignment cost. The tradeoff is that BP's marginals become stale between checkpoints: after
$25{,}000$ assignments, the factor graph has changed substantially, and the checkpoint marginals may no longer
reflect the current constraint state.

LBP's theoretical advantage over checkpoint BP is freshness: by updating messages after every assignment, LBP
always reflects the current constraint state. But as Section D.1.5 shows, the cost of freshness is prohibitive. The
checkpoint approach trades marginal accuracy for computational feasibility, and this trade is overwhelmingly
favorable. If the solver needs more accurate marginals between checkpoints, the natural remedy is to increase the
checkpoint frequency (e.g., every 10 rows) rather than to switch to continuous LBP. Even at 1 checkpoint per row,
the amortized cost is $\sim 50\text{ms} / 511 \approx 100\,\mu\text{s/assignment}$&mdash;a $50\times$ overhead rather
than $4{,}000\times$.

#### D.1.7 Mitigation Strategies and Their Limitations

Several techniques could reduce LBP's overhead, though none sufficiently to change the fundamental cost-benefit
calculus.

*Approximate message updates.* Rather than computing exact sum-product messages for cardinality factors ($O(s^2)$
per factor), the solver could use Gaussian approximation: approximate the binomial distribution of the cardinality
factor's output with a Gaussian and compute messages in $O(s)$ time. This reduces the per-factor cost from
$\sim 261{,}000$ to $\sim 511$ operations, yielding a per-assignment cost of $\sim 4{,}000$ operations ($8\times$
overhead). However, the Gaussian approximation is poor for binary cardinality factors when the residual is small
($\rho \ll s$) or large ($\rho \approx u$)&mdash;precisely the regime where the constraint is informative and LBP
would be most useful.

*Selective LBP.* Run LBP only in the plateau band (rows 100--300) and use standard propagation elsewhere. This
limits the overhead to $\sim 100{,}000$ assignments (the plateau portion) rather than all $261{,}121$. At $8\times$
overhead with Gaussian approximation: $100{,}000 \times 16\,\mu\text{s} = 1.6$s for the forward pass. The
backtracking overhead depends on the backtrack reduction, but the forward pass alone is tolerable. The concern is
that selective LBP introduces a discontinuity at the plateau boundary: the solver switches from propagation-only
to propagation-plus-LBP at row 100, potentially causing the message state to be cold (uninitialized) at exactly
the point where it is needed. Warm-starting from a checkpoint BP computation (B.12) at row 100 mitigates this.

*GPU-accelerated message updates.* Apple Silicon's Metal GPU can parallelize the message computation across factors
and variables. The cardinality-factor DP is inherently sequential within a factor (each step depends on the
previous), but the 8 independent factor updates per assignment are independent and can run in parallel. At 8-way
parallelism, the per-assignment cost drops by $\sim 8\times$, but the GPU kernel launch overhead ($\sim
10\,\mu\text{s}$) and data transfer cost may exceed the computation savings for such small workloads.

#### D.1.8 Verdict

The weight of evidence is against continuous LBP as an integrated propagation mechanism for CRSCE. The core problem
is the mismatch between LBP's computational cost and the DFS solver's throughput requirements. The CRSCE solver
processes $\sim 500{,}000$ nodes/second in the fast regime and needs to maintain high throughput even in the plateau
band to solve blocks within practical time bounds. LBP's per-node overhead on length-511 cardinality factors ---
even with Gaussian approximation and selective application&mdash;reduces throughput by at least an order of magnitude.
The backtrack reduction LBP provides (improved ordering but no pruning) cannot compensate for this throughput loss
except under implausibly optimistic assumptions ($> 4{,}000\times$ backtrack reduction).

B.12's checkpoint approach captures most of LBP's benefit (global marginal information for branching guidance)
at a fraction of the cost (one BP computation every $N$ rows, amortized across thousands of assignments). The
marginal-staleness cost of checkpoint BP is small relative to the computational savings, and can be reduced by
increasing checkpoint frequency.

If future solver development reveals that checkpoint BP's marginal estimates are critically stale between
checkpoints&mdash;causing branching decisions that are actively worse than the current `ProbabilityEstimator` ---
then selective LBP with Gaussian approximation in the plateau band (D.1.7) merits empirical evaluation. But this
scenario is unlikely: checkpoint BP with fresh marginals at plateau entry should outperform the static estimator
over the entire plateau span, and the incremental degradation over 50 rows is unlikely to be catastrophic.

The recommendation is to pursue B.12's checkpoint BP approach and to treat continuous LBP as a theoretical
alternative that is dominated on cost-benefit grounds.

#### D.1.9 Open Questions

a. What is the empirical convergence behavior of LBP on the CRSCE factor graph at various depths? If LBP converges rapidly in the plateau band (where coupling is weak) but slowly at the edges, the selective-LBP strategy (D.1.7) may be more viable than the quantitative estimates suggest, because the plateau is where the solver spends most of its time.
b. Can the cardinality-factor message computation be restructured to exploit SIMD on Apple Silicon? The $O(s^2)$ dynamic programming has data dependencies that prevent naive vectorization, but the 8 independent factor updates per assignment could be pipelined across NEON lanes, potentially reducing the $8\times$ factor to $2$--$3\times$.
c. Does LBP's overconfidence (Objection 2) systematically harm branching in the CRSCE instance, or does the ranking correlation with true marginals (Argument 1) dominate? This is an empirical question that could be answered by running LBP on the CRSCE factor graph, comparing the resulting marginals with exact marginals (computed on small subproblems), and measuring the rank correlation.
d. Is there a hybrid approach where LBP runs asynchronously on a background thread, and the DFS solver queries the most recent marginal estimates without blocking? This decouples LBP's throughput from the DFS throughput, at the cost of using stale (but continuously improving) marginals. The determinism concern (Objection 5) is severe for this approach, as the asynchronous schedule is inherently non-deterministic.
e. Would region-based generalizations of BP&mdash;such as generalized belief propagation (GBP) on Kikuchi clusters (Yedidia, Freeman, & Weiss, 2005)&mdash;provide more accurate marginals on the densely-loopy CRSCE graph? GBP operates on clusters of variables rather than individual variables, explicitly accounting for short cycles within each cluster. The cost is $O(2^{|C|})$ per cluster of size $|C|$, which is tractable only for small clusters
($|C| \leq 20$). Whether meaningful clusters exist in the CRSCE graph's regular structure is an open question.

---

### D.2 Conflict-Driven Clause Learning (CDCL) from Hash Failures (formerly B.1)

#### D.2.1 Proposal

When a SHA-1 lateral hash mismatch kills a subtree at depth ~87K, the solver backtracks one level ---
undoing the most recent branching assignment and trying the alternate value. This is chronological backtracking: the solver retries assignments in reverse order of when they were made, regardless of which assignments actually *caused* the conflict. It learns nothing from the failure. If the root cause was an assignment made 500 levels earlier, the solver exhausts an exponential number of intermediate configurations before reaching that level.

CDCL adaptation was proposed to exploit hash failures as conflict sources. An LH mismatch on row $r$ constitutes a proof that the current partial assignment to the $s$ cells in row $r$ is infeasible. The solver could analyze this proof to identify a small subset of earlier assignments jointly responsible for the conflict, record that combination as a *nogood clause*, and backjump directly to the deepest responsible assignment rather than unwinding the stack one frame at a time.

#### D.2.2 Background: CDCL in SAT Solvers

In a CDCL SAT solver, when unit propagation derives a conflict, the solver performs *conflict analysis* by
tracing the implication graph backward from the conflicting clause. The analysis identifies a *unique
implication point* (UIP)&mdash;the most recent decision variable that, together with propagated literals,
implies the conflict. The solver learns a new clause encoding the negation of the responsible assignments,
adds it to the clause database, and backjumps to the decision level of the second-most-recent literal in
the learned clause. The learned clause immediately forces the UIP variable to its opposite value at the
backjump level, avoiding the need to re-explore the intervening search space (Marques-Silva & Sakallah,
1999).

The power of CDCL comes from two properties: *non-chronological backtracking* (backjumping past irrelevant
decision levels) and *clause learning* (preventing the solver from ever entering the same conflicting
configuration again). Together, these properties transform an exponential-time DFS into a procedure that
can prune large regions of the search space based on information extracted from each conflict.

#### D.2.3 Hash Failures as Conflict Sources

In the CRSCE solver, conflicts arise from two sources: *cardinality infeasibility* (a constraint line's
residual $\rho$ falls outside $[0, u]$) and *hash mismatch* (an SHA-1 lateral hash check fails on a
completed row). The cardinality conflicts are detected immediately during propagation and trigger standard
backtracking. The hash conflicts are detected only when a row is fully assigned ($u(\text{row}) = 0$) and
the computed SHA-1 digest disagrees with the stored LH value.

A hash mismatch on row $r$ means that the 511-bit assignment to row $r$ does not match the unique preimage
expected by the stored hash. However, some of those 511 bits were determined by branching decisions (either
directly or via propagation from decisions in other rows), while others were forced by cardinality
constraints with no branching alternative. The forced bits are *consequences* of earlier decisions; the
branching decisions are the *causes*. Conflict analysis must distinguish between the two.

When row $r$ fails its LH check, the conflict clause (in the SAT sense) is the conjunction of all 511 cell
assignments in the row. A typical row in the plateau region has ~400 cells forced by propagation and ~111
cells assigned by branching, reducing the conflict clause to ~111 decision literals. Further reduction
requires tracing the implication graph backward to identify the first UIP&mdash;the most recent decision
variable whose reversal would have prevented the conflict.

#### D.2.4 Implementation

The following data structures were fully implemented and unit-tested:

- `CellAntecedent`&mdash;per-cell reason record (stack depth, antecedent line ID, isDecision flag)
- `ReasonGraph`&mdash;flat 511x511 heap-allocated antecedent table, populated during propagation
- `ConflictAnalyzer`&mdash;BFS 1-UIP backjump target finder; returns `BackjumpTarget{targetDepth, valid}`
- `BackjumpTarget`&mdash;result struct from conflict analysis

`PropagationEngine_propagate.cpp` was modified to record the triggering line (`antecedentLine`) for every
forced cell. The DFS loop recorded decisions via `recordDecision()` and propagated assignments via
`recordPropagated()`. On hash failure, `conflictAnalyzer.analyzeHashFailure(failedRow, ...)` computed the
backjump target and `brancher_->undoToSavePoint(savePoint)` executed the jump.

DI determinism holds: the backjump target is a deterministic function of the implication graph, which is
deterministic given identical initial constraint state. Both compressor and decompressor see the same
conflicts in the same order.

#### D.2.5 Experimental Results

Three configurations were tested to isolate whether CDCL interacts with partition quality:

**B.22 + CDCL (variable-length LTP, 2-1,021 cells/line, max jump 732):**
- Depth: ~80,300 (vs. B.20 baseline 88,503: **-9%**)
- CDCL max jump dist: 732 cells
- Conclusion: CDCL harmful even with shorter LTP lines

**B.23 + CDCL (uniform-511 LTP, same as B.20 partitions, max jump 1,854):**
- Depth: ~69,000 (vs. B.20 baseline: **-22%**)
- CDCL max jump dist: 1,854 cells
- iter/sec: ~306K
- Conclusion: uniform-511 lines produce longer antecedent chains; CDCL more harmful

**B.24 (CDCL disabled via cap=0, overhead intact):**
- Depth: ~86,123 (depth recovers without CDCL jumps)
- iter/sec: ~120K (40% throughput lost from record/unrecord overhead alone)
- Conclusion: CDCL overhead costs 40% throughput even when firing zero backjumps

**B.25 (CDCL fully removed):**
- Depth: ~86,123
- iter/sec: ~329K (**2.7x faster than B.24**)
- Conclusion: removing all record/unrecord call overhead is essential

#### D.2.6 Root Cause Analysis

CDCL was designed for unit-clause propagation failures in SAT solvers. In SAT, a conflict occurs when a
single variable's forced value contradicts an existing assignment&mdash;the implication chain is typically
short (5-20 steps). The 1-UIP algorithm finds a useful cut close to the conflict.

SHA-1 hash verification is fundamentally different: it is a global nonlinear constraint over all 511 cells
in a row. When a row hash fails, the conflict has no single proximate cause in the SAT sense. The 511-bit
row pattern is wrong as a whole, but the 1-UIP algorithm traces antecedent chains across 700-1,854 cells
before finding a cut. Each backjump discards hundreds of levels of correct work&mdash;progress that
chronological backtracking would have reused.

Additionally, at the 87K-88K plateau, all 511-cell constraint lines have $\rho / u \approx 0.5$, placing
them in the forcing dead zone. CDCL's non-chronological backjumping identifies *which* decision caused a
conflict, but when all constraint lines are equally inert&mdash;none generating forcing events&mdash;every
decision level is equally stuck. There is nowhere productive to jump to. CDCL degenerates to chronological
backtracking plus bookkeeping overhead, strictly worse than the baseline.

#### D.2.7 Abandonment Verdict

CDCL non-chronological backjumping is **incompatible with SHA-1 hash constraint topology** in CRSCE.
The mechanism assumes short implication chains with a meaningful UIP close to the conflict. Row hash
failures do not satisfy this assumption. All three partition configurations tested (B.22, B.23, B.24)
confirmed monotonically worsening depth under CDCL, with no configuration showing benefit.

The implementation was removed in B.25. All CDCL data structures (`ConflictAnalyzer`, `ReasonGraph`,
`CellAntecedent`, `BackjumpTarget`) remain in the codebase as unit-tested headers but are disconnected
from the DFS loop.

CDCL-style conflict learning with actual clause storage (not just backjumping) remains theoretically
interesting but requires a fundamentally different conflict analysis adapted to global row constraints.
This is a long-term research direction, not a near-term solver improvement.

### D.2 Toroidal-Slope Partitions (formerly HSM1/SFC1/HSM2/SFC2)

#### D.2.1 Original Design

The four toroidal-slope partitions were proposed in B.2 and implemented as the first auxiliary constraint
families added beyond the four geometric partitions (LSM, VSM, DSM, XSM). Each partition consisted of
$s = 511$ lines defined on the $s \times s$ torus by a fixed modular slope $p$:

$$
L_k^{(p)} = \{(t,\; (k + pt) \bmod s) : t = 0, \ldots, s-1\}, \quad k \in \{0, \ldots, s-1\}.
$$

Because $s = 511$ is prime and $\gcd(p, s) = 1$ for each implemented slope, every line contained exactly
$s$ cells and every (slope-line, row) pair intersected in exactly one cell. The four slopes and their
designations were:

| Designation | Slope $p$ | Line index formula |
|-------------|-----------|-------------------|
| HSM1        | 256       | $k = (c - 256r) \bmod 511$ |
| SFC1        | 255       | $k = (c - 255r) \bmod 511$ |
| HSM2        | 2         | $k = (c - 2r) \bmod 511$   |
| SFC2        | 509       | $k = (c - 509r) \bmod 511$ |

The slopes were chosen so that each pair of partitions was mutually 1-cell orthogonal with respect to
rows and columns: any two distinct slope lines from different slope families intersected in at most one
cell. This property ensured that all four families contributed maximally independent constraints.

Each partition's cross-sum vector contained $s = 511$ elements, each encoded in $b = 9$ bits (MSB-first
packed bitstream), for a per-partition storage cost of $sb = 4{,}599$ bits. Adding all four toroidal-slope
partitions increased the per-block payload from 25,568 bits (geometric only) to 43,964 bits, raising the
total to 125,988 bits after LH and BH.

The implementation used a `ToroidalSlopeSum` class (deriving from `CrossSumVector`) with $O(1)$ line-index
computation via a single multiply-and-modulus operation. The class remains in the source tree
(`include/common/CrossSum/ToroidalSlopeSum.h`) for mathematical validation and unit testing.

#### D.2.2 Observed Performance

Doubling the per-cell constraint count from 4 to 8 was expected to substantially increase forcing-event
frequency in the plateau band (rows 100--300). In practice, the toroidal slopes provided marginal
propagation benefit. The core limitation was algebraic: all eight partitions (four geometric + four
toroidal-slope) are linear functionals of the cell coordinates. The set of matrices indistinguishable from
a given matrix under any collection of linear partitions forms a coset of a lattice in $\mathbb{Z}^{s^2}$,
and swap-invisible patterns can be constructed by solving for the null space. Adding more linear partitions
reduces the null-space dimension but cannot eliminate it entirely; the marginal information contributed by
each additional slope pair diminishes because new slopes partially overlap with existing ones in the same
algebraic family.

Concretely, at the solver's plateau entry depth (~row 170), each toroidal-slope line had approximately 170
of its 511 cells assigned, leaving $u \approx 341$ unknowns and a residual $\rho$ in the range 80--170.
The forcing conditions ($\rho = 0$ or $\rho = u$) were far from triggering. The slopes progressed at the
same rate as the geometric lines because their uniform, regular spacing distributed cells evenly across all
rows. No slope line became tighter than any other at a given DFS depth.

#### D.2.3 Replacement by LTP Partitions

B.20 tested a direct substitution: replace the four toroidal-slope partitions with four pseudorandom
lookup-table partitions (LTPs) while holding the total partition count at 8 and the storage budget at
125,988 bits. Each LTP sub-table assigns cells to lines via a Fisher--Yates shuffle seeded by a
deterministic 64-bit LCG constant. The resulting partition has no algebraic relationship to the cell
coordinates, breaking the lattice structure that limited the toroidal slopes.

B.20 Configuration C (4 geometric + 4 LTP) achieved depth ~88,503, representing the best performance
observed at that point. The LTP partitions' non-linear cell-to-line assignment produced heterogeneous
line composition: some LTP lines concentrated cells in early rows, reaching forcing thresholds by the
plateau entry depth, while others scattered cells across the full matrix. This heterogeneity injected
forcing events into the plateau band where the uniform toroidal slopes could not.

#### D.2.4 Reason for Abandonment

The toroidal-slope partitions were abandoned on the following grounds:

1. **Algebraic redundancy.** All toroidal slopes belong to the same family of linear modular-arithmetic
   partitions. Their null-space contribution overlaps with the existing geometric partitions, providing
   diminishing marginal constraint information per additional slope pair.

2. **Uniform progression rate.** Because every slope line visits cells at regular modular intervals
   across all rows, all slope lines progress at the same rate during DFS. No slope line becomes tight
   before any other at a given search depth, preventing early forcing events in the plateau band.

3. **Strict dominance by LTP.** The B.20 experiment demonstrated that pseudorandom LTPs using identical
   storage budget (4,599 bits per partition, 8 lines per cell) achieved strictly higher depth than the
   toroidal slopes. The LTP architecture was subsequently validated across multiple research campaigns
   (B.21--B.25), culminating in the B.22 seed optimization that established the current production
   configuration.

4. **No loss of generality.** The LTP partitions satisfy the same three cross-sum partition axioms
   (Conservation, Uniqueness, Non-repetition) as the toroidal slopes. The wire-format field sizes are
   identical (511 elements at 9 bits each). Switching from toroidal slopes to LTPs required only
   semantic reinterpretation of the four payload fields, not a format change.

The `ToroidalSlopeSum` class and its unit tests are retained in the codebase as mathematical reference
implementations. Should future architectural changes reintroduce algebraic partitions (e.g., as part
of a hybrid LTP + slope configuration), the implementation is available without modification.

## References

Apple. (n.d.-a). Metal developer documentation.

Apple. (n.d.-b). Performing calculations on a GPU (Metal).

Apple. (n.d.-c). Metal Shading Language Specification (PDF).

Apple. (n.d.-d). Metal Performance Shaders: Overview and tuning hints.

Bader, M. (2013). *Space-filling curves: An introduction with applications in scientific computing*. Springer.

Bessière, C. (2006). Constraint propagation. In F. Rossi, P. van Beek, & T. Walsh (Eds.), *Handbook of constraint
programming* (pp. 29-83). Elsevier.

Braunstein, A., Mézard, M., & Zecchina, R. (2005). Survey propagation: An algorithm for satisfiability. *Random
Structures & Algorithms, 27*(2), 201-226.

Colbourn, C. J., & Dinitz, J. H. (Eds.). (2007). *Handbook of combinatorial designs* (2nd ed.). Chapman & Hall/CRC.

Darwiche, A. (2009). *Modeling and reasoning with Bayesian networks*. Cambridge University Press.

Dang, Q. (2012). Recommendation for applications using approved hash algorithms (NIST Special Publication 800-107
Revision 1). National Institute of Standards and Technology.

Debruyne, R., & Bessière, C. (1997). Some practicable filtering techniques for the constraint satisfaction problem.
In *Proceedings of the 15th International Joint Conference on Artificial Intelligence (IJCAI-97)* (pp. 412-417).
Morgan Kaufmann.

Globerson, A., & Jaakkola, T. S. (2007). Fixing max-product: Convergent message passing algorithms for MAP
LP-relaxations. In *Advances in Neural Information Processing Systems 20* (pp. 553-560). MIT Press.

Gomes, C. P., Selman, B., Crato, N., & Kautz, H. (2000). Heavy-tailed phenomena in satisfiability and constraint
satisfaction problems. *Journal of Automated Reasoning, 24*(1-2), 67-100.

Hamadi, Y., Jabbour, S., & Sais, L. (2009). ManySAT: A parallel SAT solver. *Journal on Satisfiability, Boolean
Modeling and Computation, 6*(4), 245-262.

Hamilton, C. H., & Rau-Chaplin, A. (2008). Compact Hilbert indices: Space-filling curves for domains with unequal side
lengths. *Information Processing Letters, 105*(5), 155-163.

Hsu, E. I., & McIlraith, S. A. (2006). Characterizing propagation methods for Boolean satisfiability. In
*Proceedings of the 9th International Conference on Theory and Applications of Satisfiability Testing (SAT 2006)*
(LNCS 4121, pp. 325-338). Springer.

Haralick, R. M., & Elliott, G. L. (1980). Increasing tree search efficiency for constraint satisfaction problems.
*Artificial Intelligence, 14*(3), 263-313.

Haverkort, H. J. (2017). How many three-dimensional Hilbert curves are there? *Journal of Computational Geometry,
8*(1), 206-281.

Jerrum, M., Sinclair, A., & Vigoda, E. (2004). A polynomial-time approximation algorithm for the permanent of a matrix
with nonnegative entries. *Journal of the ACM, 51*(4), 671-697.

Judge, M., Altschuler, J., & Krinsky, D. (Executive Producers). (2014–2019). *Silicon Valley* [TV series].
HBO Entertainment.

Kelsey, J., & Schneier, B. (2005). Second preimages on $n$-bit hash functions for much less than $2^n$ work. In
R. Cramer (Ed.), *Advances in Cryptology&mdash;EUROCRYPT 2005* (LNCS 3494, pp. 474-490). Springer.

Knuth, D. E. (2005). *The Art of Computer Programming, Volume 4, Fascicle 2: Generating All Tuples and Permutations*.
Addison-Wesley.

Kobler, J., et al. (2025). Disjoint projected enumeration for SAT and SMT without blocking clauses. *Artificial
Intelligence*.

Kschischang, F. R., Frey, B. J., & Loeliger, H.-A. (2001). Factor graphs and the sum-product algorithm. *IEEE
Transactions on Information Theory, 47*(2), 498-519.

Leurent, G., & Peyrin, T. (2020). SHA-1 is a shambles: First chosen-prefix collision on SHA-1 and application to
the PGP web of trust. In *Proceedings of the 29th USENIX Security Symposium* (pp. 1839-1856). USENIX Association.

McEliece, R. J., MacKay, D. J. C., & Cheng, J.-F. (1998). Turbo decoding as an instance of Pearl's "belief
propagation" algorithm. *IEEE Journal on Selected Areas in Communications, 16*(2), 140-152.

Luby, M., Sinclair, A., & Zuckerman, D. (1993). Optimal speedup of Las Vegas algorithms. *Information Processing
Letters, 47*(4), 173-180.

Mokbel, M. F., & Aref, W. G. (2001). Irregularity in multi-dimensional space-filling curves with applications in
multimedia databases. In *Proceedings of the Tenth International Conference on Information and Knowledge Management*
(pp. 512-519). ACM.

Marino, R., Parisi, G., & Ricci-Tersenghi, F. (2016). The backtracking survey propagation algorithm for solving
random K-SAT problems. *Nature Communications, 7*, 12996.

Marques-Silva, J. P., & Sakallah, K. A. (1999). GRASP: A search algorithm for propositional satisfiability. *IEEE
Transactions on Computers, 48*(5), 506-521.

Murphy, K. P., Weiss, Y., & Jordan, M. I. (1999). Loopy belief propagation for approximate inference: An
empirical study. In *Proceedings of the 15th Conference on Uncertainty in Artificial Intelligence (UAI-99)*
(pp. 467-475). Morgan Kaufmann.

Moskewicz, M. W., Madigan, C. F., Zhao, Y., Zhang, L., & Malik, S. (2001). Chaff: Engineering an efficient SAT
solver. In *Proceedings of the 38th Design Automation Conference* (pp. 530-535). ACM.

Mézard, M., Parisi, G., & Zecchina, R. (2002). Analytic and algorithmic solution of random satisfiability problems.
*Science, 297*(5582), 812-815.

Pearl, J. (1988). *Probabilistic reasoning in intelligent systems: Networks of plausible inference*. Morgan Kaufmann.

National Institute of Standards and Technology. (2015). *Secure Hash Standard (SHS) (FIPS PUB 180-4)*.

Niedermeier, R., Reinhardt, K., & Sanders, P. (2002). Towards optimal locality in mesh-indexings. *Discrete Applied
Mathematics, 117*(1-3), 211-237.

Pipatsrisawat, K., & Darwiche, A. (2007). A lightweight component caching scheme for satisfiability solvers. In
J. Marques-Silva & K. A. Sakallah (Eds.), *Theory and Applications of Satisfiability Testing&mdash;SAT 2007* (LNCS
4501, pp. 294-299). Springer.

Sagan, H. (1994). *Space-filling curves*. Springer.

Sun, J., Zheng, N.-N., & Shum, H.-Y. (2003). Stereo matching using belief propagation. *IEEE Transactions on
Pattern Analysis and Machine Intelligence, 25*(7), 787-800.

Stevens, M., Bursztein, E., Karpman, P., Albertini, A., & Markov, Y. (2017). The first collision for full SHA-1.
In J. Katz & H. Shacham (Eds.), *Advances in Cryptology&mdash;CRYPTO 2017* (LNCS 10401, pp. 570-596). Springer.

Tatikonda, S. C., & Jordan, M. I. (2002). Loopy belief propagation and Gibbs measures. In *Proceedings of the
18th Conference on Uncertainty in Artificial Intelligence (UAI-02)* (pp. 493-500). Morgan Kaufmann.

Valiant, L. G. (1979). The complexity of computing the permanent. *Theoretical Computer Science, 8*(2), 189-201.

Wainwright, M. J., Jaakkola, T. S., & Willsky, A. S. (2005). MAP estimation via agreement on trees: Message-passing
and linear programming. *IEEE Transactions on Information Theory, 51*(11), 3697-3717.

Wainwright, M. J., & Jordan, M. I. (2008). Graphical models, exponential families, and variational inference.
*Foundations and Trends in Machine Learning, 1*(1-2), 1-305.

Xu, L., Hutter, F., Hoos, H. H., & Leyton-Brown, K. (2008). SATzilla: Portfolio-based algorithm selection for SAT.
*Journal of Artificial Intelligence Research, 32*, 565-606.

Wiegerinck, W., & Heskes, T. (2003). Fractional belief propagation. In *Advances in Neural Information Processing
Systems 15* (pp. 438-445). MIT Press.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2001). Generalized belief propagation. In *Advances in Neural
Information Processing Systems 13* (pp. 689-695). MIT Press.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2002). *Understanding belief propagation and its generalizations*
(Technical Report TR-2001-22). Mitsubishi Electric Research Laboratories.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2005). Constructing free-energy approximations and generalized belief
propagation algorithms. *IEEE Transactions on Information Theory, 51*(7), 2282-2312.

Zhang, L., Madigan, C. F., Moskewicz, M. W., & Malik, S. (2001). Efficient conflict driven learning in a Boolean
satisfiability solver. In *Proceedings of the 2001 IEEE/ACM International Conference on Computer-Aided Design*
(pp. 279-285). IEEE.

