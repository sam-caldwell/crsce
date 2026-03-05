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
Matrix (CSM)---by replacing it with eight families of exact-sum projections (row, column, diagonal, anti-diagonal, and
four toroidal-slope families) together with independent per-row SHA-1 digests and a single per-block SHA-256 digest.
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
column sums (VSM), diagonal sums (DSM), and anti-diagonal sums (XSM). Four toroidal-slope partitions (HSM1, SFC1,
HSM2, SFC2) provide additional constraint lines using modular-arithmetic line families on the $s \times s$ torus.
An independent SHA-1 digest is computed for each row, producing the lateral hash vector (LH), and a single SHA-256
digest of the entire block produces the block hash (BH). A disambiguation index (DI) is appended to handle the rare
case of non-unique reconstruction. The compressed representation of each block is therefore the tuple $(\text{LH},
\text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{HSM1}, \text{SFC1}, \text{HSM2},
\text{SFC2})$.

Compression is realized because the cross-sum vectors and hash vector are smaller than the original matrix. LSM and VSM
each contain $s$ elements and each element requires $b = \lceil \log_2(s+1) \rceil$ bits. DSM and XSM each contain $2s -
1$ elements, but because straight diagonals have variable lengths ranging from 1 to $s$, each diagonal sum is encoded in
$\lceil \log_2(\text{len}(k)+1) \rceil$ bits, where $\text{len}(k) = \min(k+1,\; s,\; 2s-1-k)$ for diagonal index $k$.
The result defines the total bits for one diagonal family as:

$$
    B_d(s) = \lceil \log_2(s+1) \rceil + 2 \sum_{l=1}^{s-1} \lceil \log_2(l+1) \rceil
$$

LH consists of $s$ SHA-1 digests of 160 bits each. BH is a single SHA-256 digest of 256 bits. HSM1, SFC1, HSM2, and
SFC2 each contain $s$ elements of $b = 9$ bits. The disambiguation index (DI) is encoded as a fixed 8-bit unsigned
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

Given the constraint set $\mathcal{C} = (s, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{HSM1}, \text{SFC1},
\text{HSM2}, \text{SFC2}, \text{LH}, \text{BH})$, decompression seeks a $CSM$ satisfying all constraints. The premise
that $\mathcal{C}$ admits exactly one feasible matrix is a design intent, not a mathematical guarantee; the cross-sum
and hash constraints make non-uniqueness astronomically unlikely but cannot logically exclude it. The full compressed
payload $\mathcal{C}' = (\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{HSM1},
\text{SFC1}, \text{HSM2}, \text{SFC2})$ therefore includes an 8-bit disambiguation index (DI) that selects the intended
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
    \underbrace{4 \times 511}_{\text{toroidal-slope}} = 5{,}108 \text{ cardinality constraints}
$$

$$
    511 \text{ SHA-1 row-hash constraints} + 1 \text{ SHA-256 block-hash constraint}
$$

$$
    5{,}108 + 512 = 5{,}620 \text{ total constraints}
$$

This can also be represented as a factor graph: variable nodes for each $CSM_{r,c}$, factor nodes for each
row/column/diagonal/anti-diagonal/toroidal-slope sum constraint, and factor nodes for each SHA-1 row constraint and
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

A probability of $\sim 10^{-30{,}000}$ is not merely negligible in a practical sense---it exceeds the security margin of
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

The deterministic solver maintains a compact state for each line (row, column, diagonal, anti-diagonal, and four
toroidal-slope families) that enables incremental constraint propagation as cells are assigned. This representation
allows the solver to detect infeasibility early, force deterministic assignments when only one value is possible, and
verify completed rows against their SHA-1 digests without recomputing from scratch.

For each line $L$, the solver tracks four quantities: $u(L)$, the count of unknown cells on the line; $a(L)$, the count
of assigned ones; the required sum $S(L)$ from the corresponding cross-sum vector (LSM, VSM, DSM, or XSM); and the
residual $\rho(L) = S(L) - a(L)$.

A line is feasible iff

$$0 \leq \rho(L) \leq u(L).$$

When $\rho(L) = 0$, all unknown cells on $L$ are forced to $0$. When $\rho(L) = u(L)$, all unknown cells on $L$ are
forced to $1$. These are standard feasibility rules for cardinality constraints, and they can be applied repeatedly
until reaching a fixpoint.

Each assignment $CSM_{r,c} \leftarrow 0/1$ updates exactly eight lines' $(u, a, \rho)$ (one row, one column, one
diagonal, one anti-diagonal, and four toroidal-slope lines), enabling incremental propagation.

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
updates eight lines and may trigger further forcing on any of them. If propagation detects infeasibility---a negative
residual or a residual exceeding the unknown count on any line---the constraint set is inconsistent and no solutions
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
    Input:  constraints C = (s, LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2, LH, BH)
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
Input:  C' = (LH, BH, DI, LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2)
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
SHA-1 row digests and the SHA-256 block digest deterministically from the original matrix---this is straightforward
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
with the input---an implementation error, since the cross-sums and hashes were computed directly from $CSM$. The
compressor returns FAIL in this case as well, providing a safety net against bugs in the constraint computation or
enumerator logic.

```text
Algorithm 3: Compress(CSM, max_compression_time)
Input:  original matrix CSM, time limit T
Output: compressed payload C' or FAIL

StartTimer()

Compute LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2 from CSM
Compute LH[r] = SHA1(row_r) for all r    // per FIPS 180-4
Compute BH = SHA256(CSM_row_major)        // per FIPS 180-4

k <- 0
for Y in EnumerateSolutionsLex(C):
    if ElapsedTime() > T:
        return FAIL
    if Y == CSM:
        DI <- k
        return (LH, BH, DI, LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2)
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
over time. Concrete components --- the constraint propagation engine, the branching strategy, the hash verification
layer, and the enumeration controller --- should each be encapsulated behind well-defined interfaces, allowing
alternative implementations to be substituted without modifying the solver's control logic.

### 9.2 Polymorphism as an Architectural Principle

Polymorphism is not merely a stylistic preference; it is a structural requirement motivated by the algorithm's design.
The solver operates over eight families of cross-sum constraints (LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2) that differ in their indexing
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

**`ToroidalSlopeSum`** is a cross-sum vector for toroidal modular-slope partitions. Each partition consists of $s = 511$
lines defined by $L_k = \{(t, (k + pt) \bmod s) : t = 0, \ldots, s-1\}$ for a fixed slope $p$. The four partitions
use slopes $p \in \{256, 255, 2, 509\}$ (corresponding to HSM1, SFC1, HSM2, SFC2). Line index mapping is
$k = (c - pr) \bmod s$, computed by a single multiply-and-modulus operation.

**`CompressedPayload`** represents $\mathcal{C}' = (\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM},
\text{DSM}, \text{XSM}, \text{HSM1}, \text{SFC1}, \text{HSM2}, \text{SFC2})$ and provides serialization and
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
(\text{LH}, \text{BH}, \text{DI}, \text{LSM}, \text{VSM}, \text{DSM}, \text{XSM}, \text{HSM1}, \text{SFC1},
\text{HSM2}, \text{SFC2})$ defined in Section 4.2.

```text
Field   Elements    Bits/Element   Total Bits   Total Bytes   Encoding
------  ----------  ------------   ----------   -----------   --------
LH      511         160            81,760       10,220        20 bytes per SHA-1 digest, sequential
BH      1           256            256          32            32 bytes, SHA-256 of row-major CSM
DI      1           8              8            1             uint8
LSM     511         9              4,599        ---           MSB-first packed bitstream
VSM     511         9              4,599        ---           MSB-first packed bitstream
DSM     2s-1=1,021  variable       8,185        ---           MSB-first, ceil(log2(len(d)+1))
XSM     2s-1=1,021  variable       8,185        ---           MSB-first, ceil(log2(len(x)+1))
HSM1    511         9              4,599        ---           MSB-first packed bitstream
SFC1    511         9              4,599        ---           MSB-first packed bitstream
HSM2    511         9              4,599        ---           MSB-first packed bitstream
SFC2    511         9              4,599        ---           MSB-first packed bitstream
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
(Section 4.2). Values 0--255 are valid; if the original matrix's position exceeds 255, compression fails.

**LSM and VSM (Row and Column Sums).** Each vector contains $s = 511$ elements. Each element requires $b = \lceil
\log_2(s + 1) \rceil = 9$ bits and is serialized MSB-first into a packed bitstream. The $511 \times 9 = 4{,}599$ bits
per vector are packed continuously; the final partial byte (if any) is zero-padded to the byte boundary only at the end
of the entire cross-sum region, not between individual vectors.

**DSM and XSM (Diagonal and Anti-Diagonal Sums).** Each vector contains $2s - 1 = 1{,}021$ elements corresponding to
straight (non-wrapping) diagonals. Element $k$ requires $\lceil \log_2(\text{len}(k) + 1) \rceil$ bits, where
$\text{len}(k) = \min(k + 1,\; s,\; 2s - 1 - k)$ as defined in Section 2.2. Elements are serialized MSB-first in index
order ($k = 0, 1, \ldots, 2s - 2$), packed continuously into the bitstream.

**HSM1, SFC1, HSM2, and SFC2 (Toroidal-Slope Sums).** Each vector contains $s = 511$ elements, one per toroidal line.
Each element requires $b = 9$ bits and is serialized MSB-first, identical to LSM and VSM. The four partitions use
slopes $p \in \{256, 255, 2, 509\}$ respectively, where line $k$ of slope $p$ covers cells
$\{(t, (k + pt) \bmod 511) : t = 0, \ldots, 510\}$. At runtime, the solver maps $(r, c) \to k = (c - pr) \bmod 511$.

### 12.5 Cross-Sum Ranges and Validation

Each LSM, VSM, HSM1, SFC1, HSM2, and SFC2 entry must be in the range $[0, s]$ where $s = 511$. Each DSM entry at index
$d$ must be in $[0, \text{len}(d)]$, and each XSM entry at index $x$ must be in $[0, \text{len}(x)]$. A decoder should
reject any block containing out-of-range cross-sum values.

### 12.6 Block Acceptance Criteria

A block is accepted if and only if the reconstructed CSM simultaneously satisfies all three conditions: it reproduces
the stored LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, and SFC2 vectors exactly for all indices; it recomputes per-row LH
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
\text{XSM}, \text{HSM1}, \text{SFC1}, \text{HSM2}, \text{SFC2})$ is ordered for byte-alignment efficiency, with all
fixed-width fields preceding the bit-packed cross-sum vectors. Because enumeration can be expensive, the design includes
exactly one tolerance parameter --- `max_compression_time` --- beyond which compression fails, requiring a fallback to
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
substitution of alternative implementations --- including GPU-accelerated components --- without compromising the
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

### B.1 Conflict-Driven Learning from Hash Failures

When a SHA-1 lateral hash mismatch kills a subtree at depth ~87K, the solver currently backtracks one level ---
undoing the most recent branching assignment and trying the alternate value. If both values fail, it backtracks
another level, and so on up the stack. This is chronological backtracking: the solver retries assignments in
reverse order of when they were made, regardless of which assignments actually *caused* the conflict. It learns
nothing from the failure. If the root cause was an assignment made 500 levels earlier, the solver will exhaust an
exponential number of intermediate configurations before reaching that level.

This appendix proposes adapting conflict-driven clause learning (CDCL) --- the dominant technique in modern SAT
solvers (Marques-Silva & Sakallah, 1999; Moskewicz et al., 2001) --- to exploit hash failures as a source of
conflict information. The key insight is that an LH mismatch on row $r$ constitutes a *proof* that the current
partial assignment to the $s$ cells in row $r$ is infeasible. The solver can analyze this proof to identify a
small subset of earlier assignments that are jointly responsible for the conflict, record that combination as a
*nogood clause*, and backjump directly to the deepest responsible assignment rather than unwinding the stack one
frame at a time.

#### B.1.1 Background: CDCL in SAT Solvers

In a CDCL SAT solver, when unit propagation derives a conflict (an empty clause), the solver performs *conflict
analysis* by tracing the implication graph backward from the conflicting clause. The analysis identifies a *unique
implication point* (UIP) --- the most recent decision variable that, together with propagated literals, implies the
conflict. The solver learns a new clause encoding the negation of the responsible assignments, adds it to the clause
database, and backjumps to the decision level of the second-most-recent literal in the learned clause. The learned
clause immediately forces the UIP variable to its opposite value at the backjump level, avoiding the need to
re-explore the intervening search space (Marques-Silva & Sakallah, 1999).

The power of CDCL comes from two properties: *non-chronological backtracking* (backjumping past irrelevant decision
levels) and *clause learning* (preventing the solver from ever entering the same conflicting configuration again).
Together, these properties transform an exponential-time DFS into a procedure that can prune large regions of the
search space based on information extracted from each conflict.

#### B.1.2 Hash Failures as Conflict Sources

In the CRSCE solver, conflicts arise from two sources: *cardinality infeasibility* (a constraint line's residual
$\rho$ falls outside $[0, u]$) and *hash mismatch* (an SHA-1 lateral hash check fails on a completed row). The
cardinality conflicts are detected immediately during propagation and trigger standard backtracking. The hash
conflicts are detected only when a row is fully assigned ($u(\text{row}) = 0$) and the computed SHA-1 digest
disagrees with the stored LH value.

A hash mismatch on row $r$ means that the 511-bit assignment to row $r$ does not match the unique preimage expected
by the stored hash. However, some of those 511 bits were determined by branching decisions (either directly or via
propagation from decisions in other rows), while others were forced by cardinality constraints with no branching
alternative. The forced bits are *consequences* of earlier decisions; the branching decisions are the *causes*.
Conflict analysis must distinguish between the two.

The implication structure in the CRSCE solver is implicit rather than explicit. When the propagation engine forces
cell $(r, c)$ to value $v$ because $\rho(\text{line}) = 0$ or $\rho(\text{line}) = u(\text{line})$, that forcing
event depends on every prior assignment to cells on that line. In principle, the solver could maintain an implication
graph recording, for each forced assignment, which line triggered the forcing and which prior assignments reduced that
line's residual to a forcing state. In practice, the implication graph for 87K assignments would be large, and
maintaining it would add overhead to every propagation step.

#### B.1.3 Conflict Analysis for LH Mismatches

When row $r$ fails its LH check, the solver knows that the current assignment to row $r$'s 511 cells is wrong, but
not *which* cells are wrong. The conflict clause, in the SAT sense, is the conjunction of all 511 cell assignments in
the row --- negating it says "do not assign row $r$ to exactly this 511-bit pattern." This is a *very long clause*
(511 literals), and long clauses provide weak pruning: the clause is only violated when all 511 assignments recur in
exactly the same configuration, which is astronomically unlikely given the size of the search space.

To derive a shorter, more useful clause, the solver needs to identify which of the 511 assignments were *decisions*
(branching choices) versus *propagated* (forced consequences). A typical row in the plateau region has ~400 cells
forced by propagation and ~111 cells assigned by branching. The conflict clause can be reduced to the ~111 decision
literals, since the propagated literals are implied by the decisions and the constraint system. This is still a long
clause by SAT-solver standards, but it is a 78% reduction from the naive 511-literal clause.

Further reduction requires tracing the implication graph backward from the row-$r$ assignments to identify the
*first UIP* --- the most recent decision variable whose reversal, together with all prior decisions, would have
prevented the conflict. If the solver maintains an implication graph (or can reconstruct one from the undo stack),
standard 1-UIP analysis (Zhang et al., 2001) produces a learned clause whose length is bounded by the number of
decision levels involved in the conflict, which may be significantly smaller than 111.

#### B.1.4 Non-Chronological Backjumping

The learned clause identifies a set of decision variables responsible for the conflict. The deepest decision level
among the clause's literals is the *conflict level*; the second-deepest is the *backjump level*. The solver can
undo all assignments from the conflict level back to the backjump level in a single operation, skipping all
intermediate levels whose decisions are irrelevant to the conflict.

In the CRSCE solver, the current `undoToSavePoint` mechanism supports bulk undo to any prior save point, so the
mechanical infrastructure for backjumping exists. The missing component is the analysis that determines *where* to
jump. Without clause learning, the solver must unwind one frame at a time; with clause learning, it can skip
directly to the responsible level.

The potential savings are substantial. If a hash failure on row 170 was caused by a bad decision on row 50 (because
that decision propagated through column and slope constraints into row 170), chronological backtracking must exhaust
all $2^{111}$ configurations of the 111 branching cells between rows 50 and 170 before reaching row 50. Backjumping
skips directly to row 50 after a single conflict analysis, saving an exponential amount of work.

#### B.1.5 DI Determinism

CDCL introduces a fundamental tension with CRSCE's disambiguation index (DI) semantics. The DI is defined as the
ordinal position of the correct solution in the lexicographic enumeration of all feasible solutions. Lexicographic
order requires the enumerator to explore solutions in a canonical sequence: cell $(0,0)$ before $(0,1)$, value 0
before value 1, and so on. Clause learning and non-chronological backjumping can alter the *order* in which the
solver visits partial assignments, which changes the enumeration sequence and therefore the DI.

For CDCL to be compatible with DI semantics, one of two conditions must hold:

*Condition 1: Learned clauses are redundant.* If every learned clause is logically implied by the original constraint
system (cross-sums + hashes), then the clause merely prunes infeasible regions of the search space without excluding
any feasible solution. The enumeration order is preserved because the solver still visits all feasible solutions in
the same sequence --- it just skips infeasible subtrees faster. This condition holds for standard CDCL, where learned
clauses are derived by resolution from the original clauses and are therefore logically redundant.

*Condition 2: The solver enumerates in a modified order but compensates the DI.* If learned clauses alter the
enumeration order, the compressor and decompressor must use the same clause database to produce the same order. Since
the clause database depends on the solver's conflict history (which depends on the input), the compressor would need
to either (a) transmit the clause database as part of the payload, or (b) guarantee that both sides encounter the
same conflicts in the same order. Option (b) holds automatically if the solver is deterministic and the initial
state is identical, which it is: both sides start from the same constraint system derived from the payload.

Condition 1 is the safer path. If learned clauses are derived strictly from the constraint system (cross-sum
cardinality bounds and hash values), they are logically redundant and the lexicographic enumeration order is
preserved. The solver visits the same feasible solutions in the same order; it merely skips infeasible subtrees
without entering them. Condition 2 also holds in practice (deterministic solver, identical initial state), providing
a belt-and-suspenders guarantee.

#### B.1.6 Implementation Complexity

CDCL adaptation for CRSCE is substantially more complex than any other proposal in this appendix. The implementation
requires:

*Implication graph maintenance.* Every propagated assignment must record which constraint line triggered the forcing
and the set of prior assignments on that line. At 87K assignments with 8+ lines per cell, the graph is large. A
compact representation (storing only the triggering line index and the save-point token, not the full antecedent set)
can reduce memory overhead, but conflict analysis must still traverse the graph backward from the conflict.

*Conflict analysis procedure.* The 1-UIP algorithm (Zhang et al., 2001) traverses the implication graph in reverse
topological order, resolving antecedent clauses until a single literal from the current decision level remains. This
is well-understood but must be adapted for the CRSCE propagation model, where antecedents are cardinality forcing
events (not Boolean unit propagation) and the "clause" structure is implicit.

*Clause database.* Learned clauses must be stored and checked during propagation. In SAT solvers, the clause database
can grow to millions of clauses, managed by periodic garbage collection (removing low-activity clauses). For CRSCE,
the clause database would likely be smaller (hash conflicts are relatively rare compared to SAT conflicts), but the
watched-literal data structure used for efficient clause checking in SAT solvers would need adaptation.

*Backjump integration.* The existing `undoToSavePoint` mechanism must be extended to support jumping to an arbitrary
prior save point, not just the most recent one. The undo stack must correctly restore the constraint store, the
propagation queue, and the implication graph to the backjump state.

The engineering effort is significant, and the benefit depends on how often hash failures have *distant* root causes
(decisions many levels earlier). If most hash failures are caused by recent decisions (within the last 10--20
levels), backjumping provides modest savings and the overhead of implication-graph maintenance may not be justified.

#### B.1.7 Lightweight Alternative: Backjump Distance Estimation

A pragmatic alternative to full CDCL is *backjump distance estimation* without clause learning. When a hash failure
occurs on row $r$, the solver identifies the earliest row among the branching cells in row $r$ that has an unforced
assignment. This is a coarse approximation to conflict analysis: it assumes the root cause is in the earliest row
contributing a branching decision to the failed row, and backjumps there directly.

This heuristic requires no implication graph and no clause database. It only needs to scan the undo stack to find
which cells in row $r$ were branching decisions (not propagated) and determine the earliest row among them. The
backjump skips all intermediate levels whose decisions are in rows unrelated to row $r$. It does not learn a clause,
so it cannot prevent the solver from re-entering the same conflicting configuration via a different path, but it
avoids the worst case of chronological backtracking (exhausting exponentially many irrelevant configurations).

The DI determinism argument for this heuristic is straightforward: the backjump target is a deterministic function
of the undo stack and the failed row, both of which are identical in compressor and decompressor. No enumeration
order is changed; the solver merely skips subtrees that are provably empty (they contain no feasible completion of
the current partial assignment to row $r$, because the hash check already failed).

#### B.1.8 Open Questions

(a) How often do LH failures have distant root causes? If the typical hash failure is caused by a decision within
the last 5--10 levels, backjumping provides little benefit over chronological backtracking. If root causes are
frequently 50--500 levels deep, the savings are exponential. Instrumenting the current solver to log the distance
between hash failures and their causal decisions would answer this question without implementing CDCL.

(b) Can the implication graph be maintained cheaply enough to justify full CDCL? The overhead is per-propagation-step
(recording the triggering line for each forced assignment). If the propagation engine's fast path
(`tryPropagateCell`, ~80% of iterations) can record antecedents with minimal branch overhead, the cost may be
acceptable. If antecedent recording requires non-trivial bookkeeping on every forcing event, the per-iteration
slowdown could offset the backjumping savings.

(c) Is the lightweight backjump estimator (B.1.7) sufficient in practice? If coarse row-level backjumping captures
most of the benefit of full CDCL at a fraction of the implementation cost, it may be the preferred approach. The
estimator can be implemented and measured before committing to the full CDCL infrastructure.

(d) How does CDCL interact with the adaptive lookahead (B.8)? Lookahead probes at depth $k$ generate additional
conflicts that could feed the clause-learning machinery. A $k = 2$ probe that discovers a hash failure two levels
ahead could produce a learned clause that prunes the current level, combining the benefits of lookahead (deeper
probing) and CDCL (permanent learning). The interaction is potentially powerful but adds implementation complexity.

(e) What clause-management strategy is appropriate for CRSCE? SAT solvers use aggressive clause deletion to control
database growth. CRSCE's conflict rate is much lower (hash failures are infrequent compared to SAT conflicts), so the
clause database may remain small enough that no deletion is needed. Alternatively, clauses could be scoped to the
current block and discarded between blocks.

#### B.1.9 Prioritization Based on B.8 Telemetry (Implemented)

B.8 (adaptive lookahead via stall detection, $k = 1\ldots4$) was implemented and profiled at 16M+ iterations on a
representative input block. Telemetry showed:

- DFS depth plateau at $\approx 87{,}500$ regardless of probe depth $k = 1 \ldots 4$.
- $37\%$ hash mismatch rate ($6.1$M mismatches / $16.7$M iterations).
- `min_nz_row_unknown = 1` throughout --- rows frequently reach completion and fail SHA-1 verification immediately.
- $k = 4$ added $\approx 15\%$ per-iteration overhead with zero improvement in depth.

This establishes that the bottleneck is **search guidance, not constraint density or lookahead reach**. The
infeasible subtrees span many more than 4 levels of assignment; deeper probing cannot detect them. The solver reaches
row completion constantly, fails hash checks constantly, backtracks one level, and retries --- learning nothing from
each failure.

The primary value of B.1 is **non-chronological backjumping** (B.1.4), not clause learning (B.1.3). At $\approx 111$
decision literals per hash-failure clause, clause reuse provides negligible propagation power: a 111-literal clause
only fires when all 111 assignments recur in exactly the same configuration. The backjump target alone --- jumping
directly to the earliest responsible decision --- provides exponential savings without any clause infrastructure.

**Implementation sequence adopted:**

1. B.1.7 (lightweight backjump estimation) implemented first. When a hash failure occurs on row $r$, the solver scans
   the DFS stack bottom-up for the shallowest frame whose branching cell lies in row $r$, undoes all assignments back
   to that frame's save point, truncates the stack, and lets the loop try that frame's alternate value. No implication
   graph, no clause database, no new data structures. DI determinism holds trivially: the backjump target is a
   deterministic function of `stack[i].row` values and `failedRow`, both identical in compressor and decompressor
   from the same payload.

2. B.1.8(a) instrumentation implemented simultaneously. The metrics `backjumps`, `backjump_dist_max`,
   `backjump_dist_avg`, and `non_backjumpable` are emitted every $\approx 1$M iterations. These answer the key
   empirical question: are root causes shallow ($\leq 5$ levels, minimal benefit) or deep ($\geq 50$ levels,
   exponential savings)?

3. Full CDCL deferred. Implement only if `non_backjumpable` is high (most hash failures have no direct branch in the
   failed row, indicating the coarse stack scan is insufficient) or `backjump_dist_avg` remains low after B.1.7 is
   confirmed not to break the plateau.

#### B.1.10. CDCL Outcome

The initial CDCL integration produced no measurable improvement in plateau depth or iteration throughput. The
`ConflictAnalyzer`, `ReasonGraph`, `CellAntecedent`, and `BackjumpTarget` classes are fully implemented and unit-tested
but remain disconnected from the main DFS loop in `EnumerationController`, which continues to use chronological
backtracking.

The B.20.9 experimental results (Configuration C: 4 geometric + 4 uniform-length LTP partitions) provide a
retrospective explanation for CDCL's failure. At the plateau ($\text{depth} \approx 88{,}500$, row $\approx 170$),
every uniform-length-511 constraint line has $u \approx 341$ unknowns and $\rho \approx 170$, placing it in the
forcing dead zone ($\rho / u \approx 0.5$). Non-chronological backjumping identifies *which* decision caused a
conflict via the reason graph, but when all constraint lines are equally inert --- none generating forcing events ---
every decision level is equally stuck. There is nowhere productive to jump to, so CDCL degenerates to chronological
backtracking with additional bookkeeping overhead.

B.21 (joint-tiled variable-length LTP partitions) may change this calculus. Variable-length LTP lines (1--256 cells)
generate forcing events deep in the plateau where no 511-cell line can. A hash failure at row 172 might trace back
through a 10-cell LTP forcing event to a decision at row 90, giving the reason graph a meaningful antecedent chain
that CDCL could exploit for non-chronological backjumping. The two mechanisms are complementary: B.21 creates forcing
events, and CDCL exploits the structure those events reveal. CDCL should be revisited after B.21 telemetry is
available; if B.21's short LTP lines produce forcing cascades in the plateau band (indicated by a hash mismatch rate
below 20% per B.21.9), the reason graph will contain the rich antecedent structure that CDCL requires to outperform
chronological backtracking.

### B.2 Auxiliary Cross-Sum Partitions as Solver Accelerators (Implemented)

This appendix originally proposed adding auxiliary cross-sum partitions to increase constraint density and accelerate
decompression. Four toroidal-slope partitions --- HSM1 ($p = 256$), SFC1 ($p = 255$), HSM2 ($p = 2$), and SFC2
($p = 509$) --- have been integrated into the main specification (Sections 2.4, 3.1, 10.2--10.3, and 12.4). The
implementation uses the toroidal modular-slope construction described in B.5 rather than the Hilbert-curve geometry
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
that cross-sum constraints alone --- regardless of how many partitions are added --- cannot replace the nonlinear
collision resistance provided by SHA-1 LH and SHA-256 BH.

The following table summarizes the current and hypothetical configurations:

| Configuration                        | Cross-sum bits | Lines   | Lines/cell | $C_r$  |
|--------------------------------------|----------------|---------|------------|--------|
| 4 original (LSM/VSM/DSM/XSM)        | 25,568         | 3,064   | 4          | 0.6989 |
| 8 implemented (+ HSM1/SFC1/HSM2/SFC2)| 43,982        | 5,108   | 8          | 0.5175 |
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
fraction by a factor of $(0.95)^2 = 0.9025$ --- a 10% reduction in the number of cells requiring branching decisions.
Because DFS tree size is exponential in the number of branch points, even a modest reduction compounds significantly.

The toroidal-slope construction guarantees 1-cell orthogonality between all partition pairs (Section 2.2.1): any two
lines from different partitions share exactly 1 cell. This means every assignment propagates information into every
other partition without redundancy. Additional slope pairs preserve this property provided $\gcd(p, 511) = 1$ and
$\gcd(|p_i - p_j|, 511) = 1$ for all existing slopes $p_j$. The current slopes $\{2, 255, 256, 509\}$ leave
abundant room for expansion --- up to 255 total pairs are theoretically possible (see B.5.1(b)).

#### B.2.4 Implemented Design: Toroidal Modular-Slope Partitions

The original B.2 proposed Hilbert-curve-based PSM/NSM partitions implemented via static lookup tables. The actual
implementation uses toroidal modular-slope partitions instead, for three reasons:

*Analytical orthogonality.* The toroidal-slope construction provides a proof-backed guarantee that any two lines from
different partitions intersect in exactly 1 cell, via the $\gcd$-based argument in Section 2.2.1. Hilbert-curve
partitions have no such guarantee --- their crossing density depends on the specific curve geometry and must be verified
empirically.

*Zero-overhead index computation.* Slope line membership is computed as $k = (c - pr) \bmod s$, which compiles to a
multiply-subtract-modulo sequence costing 1--2 cycles on Apple Silicon. The `ConstraintStore` precomputes flat
stat-array indices for all 4 slope families via `slopeFlatIndices(r, c)`, eliminating even the modular arithmetic from
the hot path. Hilbert-curve partitions would require a 510 KB lookup table per partition pair.

*Trivial extensibility.* Adding a new slope pair requires selecting a slope $p$ with $\gcd(p, 511) = 1$ and
$\gcd(|p - p_j|, 511) = 1$ for all existing slopes. No code-generation scripts, build-time lookup tables, or
curve-adaptation logic is needed.

#### B.2.5 Cost--Benefit of Further Partitions

Adding a fifth slope pair (10 partitions total) costs 9,198 bits per block, reducing $C_r$ from 51.8% to 48.2% ---
a 3.5 percentage-point penalty. The benefit is 1,022 additional constraint lines and an increase from 8 to 10
lines per cell. Whether this tradeoff is justified depends on the empirical decompression speedup.

The current solver plateaus at depth ~87K (row ~170) with 8 partitions. The plateau is a combinatorial phase
transition in the mid-rows where cardinality forcing is inactive and LH checks are the only strong pruning
mechanism. Additional partitions increase the per-assignment propagation radius but do not create new hash
checkpoints. The marginal benefit is therefore an increase in cardinality forcing frequency --- more lines per cell
means more chances for a line's residual to reach 0 or $u$ --- but this benefit diminishes as the number of partitions
grows, because the residuals on the new lines are initially large (each slope line has $s = 511$ cells) and individual
assignments reduce them by at most 1.

The following qualitative assessment applies to the current plateau regime:

*Strong case for a 5th pair (10 partitions).* At 10 lines per cell, the probability of at least one forcing trigger
per assignment increases measurably. The 3.5-point compression cost is modest, and the propagation benefit is
concentrated exactly where the solver needs it --- in the mid-rows where 8-line propagation is insufficient.

*Diminishing returns beyond 12 partitions.* At 12 lines per cell, the incremental forcing probability gain from each
additional pair is $(1-f)^2 \approx 0.90$, the same 10% relative reduction. But the compression cost accumulates
linearly: 12 partitions yield $C_r \approx 44.7\%$, and 16 partitions yield $C_r \approx 37.7\%$. The compression
ratio approaches the regime where CRSCE's overhead rivals the input size, undermining the format's purpose.

*Hard limit.* At $n = 28$ partitions (the maximum fitting within the information-theoretic budget), the compression
ratio drops to approximately 11% --- effectively no compression. Long before this point, the solver's constraint
density is high enough that other bottlenecks (hash verification throughput, propagation queue overhead) dominate.

#### B.2.6 Open Questions

(a) What is the empirical decompression speedup from adding a 5th slope pair (10 partitions) on Apple Silicon,
measured on random, all-zeros, all-ones, and adversarial inputs at $s = 511$? This is the minimal experiment needed
to determine whether further partitions address the depth plateau.

(b) At what partition count does the propagation engine's per-iteration cost (updating line statistics for $n$ lines
per assignment) become a throughput bottleneck? Currently, the `PropagationEngine` fast path
(`tryPropagateCell`) checks 8 lines per assignment. Scaling to 10 or 12 lines increases this cost linearly, but the
fast-path exit condition (no forcing needed) may absorb the additional checks with minimal throughput impact.

(c) What slope values should a 5th pair use? The current slopes $\{2, 255, 256, 509\}$ correspond to geometric
slopes $\{2, -\frac{1}{2}, \frac{1}{2}, -2\}$. A 5th pair must satisfy the coprimality constraints with all existing
slopes. Candidate pairs include $\{3, 508\}$ (slopes $\{3, -3\}$), $\{4, 507\}$ (slopes $\{4, -4\}$), and
$\{128, 383\}$ (slopes $\{\frac{1}{4}, -\frac{1}{4}\}$, subject to verification that
$\gcd(|128 - p_j|, 511) = 1$ for all existing $p_j$).

### B.3 Variable-Length Curve Partitions as LH Replacement

Sections B.1 and B.2 explore modifications that *complement* LH. This section considers a more aggressive alternative:
replacing LH entirely with $n$ variable-length partitions structured identically to DSM and XSM --- each consisting of
$2s - 1$ lines with lengths $1, 2, \ldots, s, \ldots, 2, 1$ --- but using space-filling curves rather than straight
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
unconditionally solved --- no search, no ambiguity.

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

Beyond approximately $l = 10$, the lines behave similarly to the uniform-length lines of Section B.2 --- they provide
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
from $\binom{3}{\sigma}$ to $\binom{2}{\sigma - v}$ where $v$ is the anchor's value --- a reduction by factor
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
when a line's residual reaches 0 or equals its unknown count --- unlikely for a fresh line with 511 unknowns. With
variable-length partitions, the short lines begin with small unknown counts and are far more likely to trigger forcing
immediately.

Consider the propagation sequence on initial state. The $2n$ length-1 lines solve their cells unconditionally. Each
solved cell updates its row, column, diagonal, anti-diagonal, and the lines it belongs to in all $n$ new partitions.
These updates decrease $u(L)$ on each affected line by 1 and (if the solved value is 1) increase $a(L)$ and decrease
$\rho(L)$ by 1. For a length-2 line that already had $\sigma \in \{0, 2\}$, the single update is sufficient to force
the remaining cell. For a length-3 line, the update may push it into a forcing state. The cascade propagates inward
from the short lines toward the long lines, with each wave of solved cells enabling the next tier.

This "warm start" effect is unique to variable-length partitions. Uniform-length partitions offer no free solutions at
initialization --- the solver must wait for branching and propagation to create forcing conditions. Variable-length
partitions front-load the easy constraints, reducing the effective matrix size before the first branch point. If the
cascade solves a sufficient fraction of cells (even 5--10\% of the matrix), the remaining search tree is dramatically
smaller.

#### B.3.5 Storage Budget and Partition Count

Each variable-length partition requires $B_d(s) = 8{,}185$ bits (identical to DSM and XSM), compared to $4{,}599$ bits
for a uniform-length partition. Because the existing partitions are organized as orthogonal pairs (LSM--VSM,
DSM--XSM), any new partitions must likewise come in pairs to maintain the interlocking pair structure. This gives
$n = 16$ partitions (8 orthogonal pairs), costing $16 \times 8{,}185 = 130{,}960$ bits --- exceeding the LH budget of
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
definition: two families of parallel lines with slopes $m$ and $-1/m$ satisfy the *maximum crossing property* --- every
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

**Natural pairing via transposition.** The simplest orthogonal pairing mirrors the LSM--VSM relationship: given a
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

(1--2) The canonical Hilbert curve and a Hilbert variant with reversed sub-quadrant orientations at the first recursion
level. These differ in which quadrant is visited first and the direction of traversal between quadrants, producing
traversals with complementary spatial coherence at the coarsest scale.

(3--4) The Z-order (Morton) curve and a reflected Z-order curve (horizontal reflection of the bit-interleaving
pattern). Z-order curves have weaker locality than Hilbert curves --- Niedermeier et al. (2002) showed that the Hilbert
curve is optimal among all $2 \times 2$ recursion-based curves for locality, while the Z-order curve trades locality
for computational simplicity --- which means Z-order partitions cross Hilbert partitions at different spatial scales,
providing complementary constraint structure.

(5--6) Two Hilbert curve variants with distinct recursion patterns at the second level of subdivision. At each level,
the Hilbert curve recursion offers four orientation choices per sub-quadrant; varying these at level 2 (the $128
\times 128$ sub-grid scale) produces traversals that agree at the coarsest level but diverge within each quadrant.

(7--8) A Peano-type curve adapted to $512 \times 512$ (via nested $3 \times 3$ subdivisions with padding) and a
rotated variant. The $3 \times 3$ recursion base produces a traversal structure incommensurable with the $2 \times 2$
Hilbert and Z-order families, maximizing geometric independence (Bader, 2013).

Each base curve $C_i$ paired with its transpose $C_i^T$ yields one orthogonal pair, giving 8 pairs and 16 partitions
total. The variable-length segment schedule ($1, 2, \ldots, s, \ldots, 2, 1$) is applied identically to each curve.

**Axiom verification.** Axiom 1 (Conservation) holds by construction: each curve visits all $s^2$ cells, and the
segment schedule sums to $s^2$. Axiom 3 (Non-repetition) holds because each curve is a Hamiltonian path, so no cell is
visited twice. Axiom 2 (Uniqueness) requires that no two lines across any pair of the 20 total partitions (4 original

- 16 new) cover the identical set of cells. For lines derived from geometrically distinct curves, cell-set identity
would require two different traversals to visit exactly the same cells in the same contiguous block of the segment
schedule --- a combinatorial coincidence with probability vanishing in $s$. A build-time verification step (the code
generator checks all $\binom{20 \times 1021}{2}$ pairs of lines for set equality) provides a hard guarantee at zero
runtime cost. Hamilton and Rau-Chaplin (2008) demonstrated that compact Hilbert index computations for non-square
grids are efficient and deterministic, confirming that the adaptation from $512 \times 512$ to $511 \times 511$ is
computationally tractable for the code generator.

**Limitations of the curve-based orthogonality guarantee.** Unlike linear partitions, where the maximum crossing
property is a mathematical theorem, the dense distributed crossing property for curve-based partitions is a structural
expectation rather than a proof. The recursive self-similarity of Hilbert-type curves provides strong heuristic grounds
--- Bader (2013) showed that Hilbert curves achieve near-optimal locality in the sense that spatially nearby cells
are visited close together in the traversal, which implies that segments cover compact regions and cross other
partitions' segments broadly --- but a formal proof that all 8 pairs satisfy property (b) for all possible matrices
does not yet exist. The build-time verification described above addresses property (a) computationally; property (b)
can be assessed empirically by computing the intersection matrices for the chosen curves and checking for degenerate
alignment patterns.

**Connection to Latin square theory.** For uniform-length partitions ($s$ groups of $s$ cells), the existence of
mutually orthogonal families is governed by the theory of MOLS. Colbourn and Dinitz (2007) established that for any
prime power $q$, exactly $q - 1$ MOLS of order $q$ exist, and for composite orders, lower bounds on the number of MOLS
are given by the MacNeish--Mann theorem: $N(n) \geq \min(q_i - 1)$ where $n = \prod q_i^{a_i}$. For $s = 511 = 7
\times 73$, this gives $N(511) \geq 6$, guaranteeing at least 6 mutually orthogonal uniform partitions --- more than
enough for 8 pairs if uniform partitions were used. However, the variable-length structure required by B.3 does not
correspond to a Latin square, so the MOLS bound does not directly apply. The MOLS result does confirm that the
underlying combinatorial space of the $511 \times 511$ grid is rich enough to support many mutually orthogonal
decompositions, lending plausibility to the curve-based construction even absent a formal equivalence theorem.

#### B.3.8 Open Questions

(a) What is the empirically measured $k_{\min}$ (minimum invisible swap size) for a 6-partition system comprising LSM,
VSM, DSM, XSM, and two variable-length curve partitions at small matrix sizes ($s = 15$, $s = 31$)? How does
$k_{\min}$ scale as $n$ increases from 2 to 16?

(b) For 16 variable-length curve partitions (8 orthogonal pairs) with well-chosen geometries, does the
cascade from anchor cells and short lines solve a sufficient fraction of the matrix that the remaining search tree fits
within a 1-second wall-clock budget on Apple Silicon?

(c) Is the collision resistance from interlocking variable-length partitions sufficient to eliminate LH entirely, or
should a hybrid approach retain a reduced LH (e.g., truncated 32-bit hashes, costing $32 \times 511 = 16{,}352$ bits)
alongside a smaller number of auxiliary partitions?

(d) What is the optimal space-filling curve family and orientation strategy for maximizing cross-partition anchor
dispersion --- i.e., ensuring that each partition's length-1 endpoints fall on short lines of other partitions?

(e) Can the variable-length segment schedule be optimized beyond the triangular sequence $1, 2, \ldots, s, \ldots, 2,
1$? For example, a schedule with more short segments (e.g., repeating the $1, 2, 3$ prefix) would provide more anchors
at the cost of fewer long segments, changing the collision/propagation tradeoff.

(f) Can the dense distributed crossing property (Section B.3.7) be formally proved for any specific family of 8 curve
pairs, or must it remain an empirically verified property? Is there a combinatorial framework analogous to MOLS theory
that governs the existence of mutually orthogonal variable-length partitions on finite grids?

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

The LH check (Section 5.2) fires when $u(\text{row}_r) = 0$ --- when every cell in row $r$ has been assigned. A SHA-1
mismatch prunes the entire subtree rooted at the most recent branching decision, providing cryptographic-strength
pruning that is far more powerful than cardinality-based propagation alone. Current performance data shows approximately
200,000 hash mismatches per second at the depth plateau (~87K cells, row ~170), confirming that LH verification is the
dominant pruning mechanism in the mid-solve.

The current `ProbabilityEstimator` does not account for this, and more critically, the cell ordering is computed once
and never revisited. During the DFS, propagation cascades routinely force cells across multiple rows. A row that began
the solve with $u = 511$ may reach $u = 3$ due to column, diagonal, or slope forcing --- but its remaining cells sit
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
50,000 assignments --- but its confidence score still reflects the initial state.

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
already iterates forced assignments to record them on the undo stack (lines 245--247 in the current implementation).

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
comparisons against $\tau$. This is a scan of 511 cached line statistics --- comparable to the existing forced-
assignment recording loop and negligible relative to the propagation cost itself.

*Heap operations.* Each insert or extract-min costs $O(\log s) = O(9)$. At most $s$ rows can be in the heap
simultaneously. In practice, the number of rows crossing $\tau$ per propagation wave is small (typically 0--3),
so the amortized heap cost per decision is a few dozen instructions.

*Per-row scoring.* When the priority queue selects a row $r^*$, the solver must identify the best cell within
that row. A simple scan of the row's $u \leq \tau$ remaining cells, reading their 7 line residuals, costs $O(7\tau)$
operations. At $\tau = 8$, this is 56 stat lookups --- under 200 ns on Apple Silicon.

The total per-decision overhead is approximately 200--500 ns, less than 10% of the current ~2 $\mu$s per iteration.
The throughput impact is negligible.

#### B.4.6 Expected Impact

The primary benefit is more frequent LH checks. In the current static-ordering regime, rows complete only when the
sequential scan happens to reach their remaining cells. With the priority queue, a row that reaches $u \leq \tau$ is
immediately prioritized, and its remaining cells are assigned within the next $\tau$ decisions. This reduces the
*latency* between a row becoming nearly complete (due to propagation) and the LH check firing.

The impact is largest in the mid-solve (rows 100--300), where propagation cascades from column, diagonal, and slope
constraints frequently force cells in rows far ahead of the current sequential position. These "accidentally
nearly-complete" rows represent free LH checks that the static ordering leaves on the table.

A conservative estimate: if the priority queue triggers even 10% more LH checks per unit time in the plateau region,
and each LH mismatch prunes an average subtree of depth $d$, the effective search-space reduction compounds
exponentially. At 200K mismatches/sec, a 10% increase yields 20K additional prunes/sec, each eliminating a subtree
of $O(2^d)$ nodes.

#### B.4.7 Threshold Selection

The threshold $\tau$ controls the tradeoff between responsiveness and distraction:

*$\tau = 1$.* The priority queue activates only when a single cell remains in a row. This is the most conservative
setting --- the solver interrupts the static ordering only for guaranteed one-step row completions. The cost is
zero (no per-row scoring needed, only one candidate cell). The drawback is that rows at $u = 2$ or $u = 3$ are not
prioritized, missing opportunities where 2--3 assignments would yield an LH check.

*$\tau = 4$--$8$.* The priority queue activates for rows within a few cells of completion. This is the expected sweet
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

(a) What is the optimal threshold $\tau$? A value of $\tau \in [4, 8]$ is a plausible starting range, but empirical
measurement on representative inputs (all-zeros, all-ones, random, alternating) is needed to determine the
throughput--pruning tradeoff.

(b) Should the priority queue also consider line-completion proximity for non-row lines (columns, diagonals, slopes)?
Completing a column or diagonal does not trigger a hash check, but it does trigger cardinality forcing that may
cascade to row completions. A multi-line priority heuristic could accelerate these secondary completions at the cost
of a larger and more complex priority structure.

(c) When the priority queue selects a row, should the solver complete the entire row (assigning all $u$ remaining
cells before returning to the static ordering), or should it assign one cell, re-propagate, and re-evaluate the
priority queue? Completing the row in a burst maximizes the chance of an immediate LH check but may miss
opportunities to switch to a different row that propagation has driven to an even lower $u$.

(d) Does the priority queue interact adversely with the `FailedLiteralProber`? Probing a cell on a nearly-complete
row triggers an LH check during the probe itself, providing stronger pruning information. This suggests the priority
queue and probing are synergistic, but empirical verification is needed.

### B.5 Hash Alternatives to Improve Search Depth

The changes originally proposed in this appendix --- replacing per-row SHA-256 with per-row SHA-1 plus a whole-block
SHA-256 digest (BH), and adding four toroidal-slope partitions (HSM1/SFC1, HSM2/SFC2) --- have been integrated into
the main specification (Sections 1, 2.3, 3.1--3.3, 5.1--5.6, 10.2--10.4, and 12.4--12.7). Subsections B.5.1 through
B.5.8 have been removed. The open questions below remain.

#### B.5.1 Open Questions

(a) What is the empirical decompression throughput difference between SHA-1 and SHA-256 row hashing on Apple
Silicon M-series processors, measured as hash evaluations per second during constraint solving with frequent
backtracking?

(b) Is the 4-partition (2-pair) allocation optimal, or does scaling to 10 partitions (5 pairs) at
compression-neutral cost (41.2% versus 40.1%) yield a sufficiently larger propagation benefit to justify the
reduced compression improvement? The toroidal-slope construction trivially extends to additional pairs by
selecting new slopes $p$ with $\gcd(p, 511) = 1$ and $\gcd(|p_i - p_j|, 511) = 1$ for all existing slopes
$p_j$, which is achievable for any number of pairs up to $(511 - 1)/2 = 255$.

(c) Are there slope values other than $\{2, 255, 256, 509\}$ (i.e., slopes $\{\frac{1}{2}, -\frac{1}{2}, 2,
-2\}$) that provide superior propagation interaction with DSM and XSM? The current slopes visit 511 of 1,021
diagonals and anti-diagonals per line; alternative slopes may distribute diagonal visits differently, though
the 1-cell intersection guarantee with all axis-aligned partitions holds for any slope coprime to 511.

### B.6 Singleton Arc Consistency

The current propagation engine applies cardinality-based forcing rules: when the residual $\rho(L) = 0$
all unknowns on line $L$ are forced to 0, and when $\rho(L) = u(L)$ they are forced to 1 (Section 5.1).
The existing `FailedLiteralProber` strengthens this by tentatively assigning each value to an unassigned
cell, propagating, and detecting immediate contradictions --- a technique known as *failed literal
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
propagating to fixpoint yields an infeasible state --- that is, any line $L$ with $\rho(L) < 0$ or
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
deeper than in typical binary CSPs, because each forcing event touches 8 rather than 2--4 lines.

The primary benefit is fewer DFS backtracks. At the current depth plateau (~87K cells, roughly row 170),
the solver encounters approximately 200,000 hash mismatches per second, indicating that ~60% of
iterations reach a completed row only to discover a SHA-1 mismatch. SAC would detect many of these
doomed subtrees earlier --- when the tentative assignment that eventually causes the mismatch first
creates a singleton inconsistency at an intermediate cell.

#### B.6.4 Cost Analysis

The cost of maintaining SAC is substantial. Let $n$ denote the number of currently unassigned cells. A
single SAC maintenance pass probes $2n$ value--cell pairs (both values for each cell). Each probe invokes
the propagation engine, which touches up to 8 lines per forced cell. In the worst case, a single probe
propagates $O(s)$ forced cells, each updating 8 line statistics, giving $O(8s)$ work per probe and
$O(16ns)$ per pass. Multiple passes may be needed to reach the SAC fixpoint, though empirically the
number of passes is small (typically 2--4 in structured CSPs; Bessière, 2006).

At the current depth plateau ($n \approx 174{,}000$), a single SAC pass performs roughly $348{,}000$
probes. At an estimated 2--5 $\mu$s per probe (dominated by propagation and undo), one pass costs
0.7--1.7 seconds. If SAC is maintained after every assignment, this cost multiplies by the number of
assignments per second (~510,000 at current throughput), which is prohibitive as a per-assignment
invariant.

#### B.6.5 Practical Variants

Two practical relaxations avoid the full SAC cost:

*SAC as preprocessing.* Run SAC to fixpoint once before the DFS begins (extending the current
`probeToFixpoint` to iterate until the SAC fixpoint, not just the single-pass failed-literal fixpoint).
This imposes a one-time cost proportional to the number of SAC passes times $O(ns)$ and reduces the
DFS search space without per-node overhead. The existing `probeToFixpoint` infrastructure supports this
directly --- the change is to continue iterating until the stricter SAC fixpoint condition holds.

*Partial SAC during search.* Rather than re-probing all unassigned cells after every assignment,
re-probe only cells whose constraint lines were affected by the most recent propagation wave. If
propagation forced $k$ cells, re-probe only the unassigned cells sharing a line with those $k$ cells.
This limits re-probe scope to $O(8k \cdot s)$ cells in the worst case but is typically much smaller,
since most propagation waves are local.

I'm proud of myself.  I've made it this far without making any jokes about probing, cost or relaxation.
I'm sure there's a proctology joke here somewhere...

#### B.6.6 Open Questions

(a) What is the empirical SAC fixpoint depth for random and structured 511×511 binary matrices? If SAC
preprocessing resolves a significant fraction of cells beyond what failed literal probing achieves, the
depth plateau may shift materially.

(b) Does partial SAC (re-probing only affected neighborhoods) converge to the same fixpoint as full SAC,
or does it settle at a weaker consistency level? If weaker, is the gap practically significant for CRSCE
instances?

(c) Can the probe loop be parallelized on GPU via Metal? Each probe is independent (tentative assign,
propagate, undo), making the loop embarrassingly parallel in principle, though the shared constraint
store requires careful synchronization or per-thread copies.

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
throughput. Second, the middle rows (approximately rows 100--300) enter a combinatorial phase transition
where cardinality forcing alone produces negligible propagation, and the solver needs stronger pruning
to sustain forward progress. An adaptive strategy applies each level of cost precisely when the solver
needs it.

The cap at $k = 4$ is deliberate. Exhaustive lookahead at depth $k$ explores $2^k$ probe paths per
branching decision, giving it the strong pruning guarantee that an assignment is marked doomed only when
*all* continuations fail. At $k = 4$, this costs 16 probes per decision --- roughly 32--80 $\mu$s at
2--5 $\mu$s per probe --- keeping throughput in the 12K--30K iter/sec range. Beyond $k = 4$, exhaustive
lookahead becomes prohibitively expensive ($k = 5$ requires 32 probes, $k = 6$ requires 64), and
approximations such as linear-chain sampling sacrifice the completeness guarantee that makes lookahead
effective (see B.8.7). Each probe propagates through all 8 constraint lines, so $k = 4$ explores up to
$4 \times 8 = 32$ constraint-line interactions per path --- a detection radius sufficient to catch
cardinality contradictions that span multiple lines without requiring a row-boundary SHA-1 check.

#### B.8.1 Stall Detection

The solver maintains a sliding window of the last $W$ branching decisions (e.g., $W = 10{,}000$). At
each decision, it records the *net depth advance*: +1 for a forward assignment, $-d$ for a backtrack
that unwinds $d$ levels. The *stall metric* $\sigma$ is the ratio of net depth advance to window size:

$$\sigma = \frac{\Delta_{\text{depth}}}{W}$$

When $\sigma > 0$, the solver is making forward progress. When $\sigma \leq 0$, the solver is stalled
--- backtracking at least as often as it advances. The stall metric is cheap to maintain (one counter
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
  ~12K--30K iter/sec.

*De-escalation.* When $\sigma > \sigma^{+}$ for a sustained period of at least $2W$ decisions and
$k > 0$, the solver decrements $k$ by one. The sustained-period requirement provides hysteresis,
preventing oscillation between depths. De-escalation is essential because the CRSCE constraint landscape
is not monotonically harder with depth: the late rows (approximately 300--511) have small unknown counts
per line, causing cardinality forcing to become aggressive again. A solver locked at $k = 4$ in this
region pays a 17--42× throughput penalty for pruning it no longer needs.

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

The recursive structure naturally extends the existing `FailedLiteralProber` --- the $k = 1$ case is
exactly `probeAlternate`, and each additional level wraps another layer of tentative-assign-propagate-
undo around it.

#### B.8.4 Expected Behavior Profile

The adaptive strategy partitions the solve into distinct phases:

*Rows 0--100 ($k = 0$).* Cross-sum residuals are large and SHA-1 row-hash checks at each row boundary
provide strong pruning. The solver operates at maximum throughput (~510K iter/sec) with no lookahead
overhead. Depth advances rapidly through the first ~51,000 cells.

*Rows 100--170 ($k = 0 \to 1$).* As residuals shrink and the constraint landscape tightens, the solver
enters the current plateau zone. The stall detector triggers the first escalation to $k = 1$, enabling
failed-literal probing on alternate branches. Throughput drops to ~250K iter/sec but the additional
pruning may sustain forward progress through this region.

*Rows 170--300 ($k = 1 \to k_{\max} \leq 4$).* If $k = 1$ is insufficient (as current performance data
suggests), further stalls trigger incremental escalation to $k = 2$, then $k = 3$, then $k = 4$. Each
increment doubles the per-decision probe count, and the solver self-tunes to the minimum $k$ that
sustains forward progress. At $k = 4$, the 16-probe exhaustive tree explores up to 128 constraint-line
interactions per decision, substantially expanding the detection radius for cardinality contradictions.

*Rows 300--511 (de-escalation).* In the final rows, unknown counts per line are small and cardinality
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
existing `BranchingController` undo stack supports this naturally --- each tentative assignment pushes a
frame, and unwinding pops frames in reverse order. Care is needed to ensure that SHA-1 hash
verification is not triggered at intermediate lookahead levels: only the outermost probe should check
SHA-1 on completed rows, since intermediate levels will be undone regardless. A simple depth counter
passed to the hash verifier suffices.

The memory cost is negligible. The lookahead tree is explored depth-first, so at most $k = 4$ undo
frames are live simultaneously, each storing $O(s)$ forced-cell records in the worst case. The total
additional memory is $O(ks) = O(2{,}044)$ entries --- under 20 KB.

#### B.8.6 Cost--Benefit Summary

| Depth $k$ | Probes/Decision | Approx. Throughput | Constraint-Line Reach | Pruning Guarantee |
|:----------:|:---------------:|:------------------:|:---------------------:|:-----------------:|
| 0          | 0               | 510K iter/sec      | 8 (propagation only)  | None              |
| 1          | 2               | 250K iter/sec      | 16                    | Exhaustive        |
| 2          | 4               | 125K iter/sec      | 32                    | Exhaustive        |
| 3          | 8               | 60K iter/sec       | 64                    | Exhaustive        |
| 4          | 16              | 12--30K iter/sec   | 128                   | Exhaustive        |

All depths maintain the full exhaustive pruning guarantee: an assignment is marked doomed only when
every continuation in the $2^k$ lookahead tree leads to a cardinality violation or hash mismatch. This
guarantee is what makes failed-literal probing ($k = 1$) effective in the existing solver, and the
adaptive strategy preserves it at every escalation level.

#### B.8.7 Open Questions

(a) What is the optimal window size $W$ for stall detection? Too small and the detector triggers on
transient fluctuations; too large and it reacts slowly to genuine plateaus. A value of $W = 10{,}000$
(~20 ms at 510K iter/sec) is a reasonable starting point but should be tuned empirically.

(b) What are the optimal stall and recovery thresholds ($\sigma^{-}$ and $\sigma^{+}$)? Setting
$\sigma^{-} = 0$ (escalate when net progress is zero) and $\sigma^{+} = 0.5$ (de-escalate when at
least half of decisions advance depth) are initial candidates. The gap between the two thresholds
controls hysteresis width.

(c) For depths beyond $k = 4$, is a *linear-chain* approximation viable? Rather than exploring the
full $2^k$ tree, the solver could follow a single most-constrained path of $k$ probes, reducing cost
from $O(2^k)$ to $O(k)$. This sacrifices the exhaustive pruning guarantee --- the chain detects a
contradiction only along one specific continuation, not all of them --- and its effectiveness depends
on how well the most-constrained-cell heuristic correlates with actual failure paths. In CRSCE, where
contradictions are often SHA-1 mismatches at row boundaries (essentially pseudorandom relative to
constraint tightness), the false-negative rate of a linear chain may be unacceptably high. Empirical
measurement is needed to determine whether linear-chain lookahead at $k = 8$ or $k = 16$ outperforms
exhaustive lookahead at $k = 4$.

(d) Should the lookahead tree explore both values at the branching cell (as in B.7.3), or only the
alternate value (as in the existing `probeAlternate`)? Probing only the alternate value halves the
tree size at each depth, effectively doubling the affordable $k$. However, probing the canonical value
as well can detect doomed states earlier, reducing backtrack depth.

### B.9 Non-Linear Lookup-Table Partitions

All eight implemented constraint families are *linear*: each defines its cell-to-line mapping via an arithmetic
formula over $(r, c)$. Rows use $r$, columns use $c$, diagonals use $c - r + (s-1)$, anti-diagonals use $r + c$, and
toroidal slopes use $(c - pr) \bmod s$ for various $p$. This linearity is what makes the algebraic kernel analysis
in B.2.2 possible --- swap-invisible patterns exist because the constraint system is a set of linear functionals over
$\mathbb{Z}$, and the null space of any finite set of linear functionals over a 261,121-dimensional binary vector
space is necessarily large.

This appendix proposes *non-linear partitions* defined by an optimized lookup table rather than an algebraic formula.
The partition uses $s$ uniform-length lines of $s$ cells each, encoded at $b$ bits per element --- the same structure
and per-partition storage cost as LSM, VSM, or any toroidal-slope family. The table is constructed offline,
optimized against simulated DFS trajectories on random inputs to maximize constraint tightening in the plateau
region (rows ~100--300), then hardcoded into the source as an immutable literal. Because the mapping has no
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
\lceil \log_2(s + 1) \rceil = 9$ bits to encode --- identical to the LSM, VSM, and toroidal-slope families.

Construction is a two-phase offline process. The first phase generates a *baseline table* from a deterministic PRNG
seeded with a fixed constant (e.g., the first 32 bytes of SHA-256("CRSCE-LTP-v1")). A Fisher--Yates shuffle of all
$s^2$ cell indices produces a random assignment of $s$ cells per line, satisfying the uniform-length constraint.
This baseline serves as the starting point for the second phase.

The second phase *optimizes* the baseline against simulated DFS trajectories. The procedure generates a test suite
of $N$ random $511 \times 511$ binary matrices, runs the full decompression solver on each (using the current 8
linear partitions plus the candidate LTP pair), and records the total backtrack count in the plateau band (rows
100--300). The optimizer applies a local-search heuristic --- swapping the line assignments of two randomly chosen
cells, accepting the swap if it reduces the mean backtrack count across the test suite, rejecting it otherwise. This
hill-climbing loop iterates until convergence. The swap preserves the uniform-length invariant (both lines still
have $s$ cells after the exchange), so the storage cost remains $sb = 4{,}599$ bits throughout.

The resulting table is hardcoded into the source code as a `constexpr std::array<uint16_t, 261121>` literal. The
table is part of the committed source, not a build-time artifact --- there is no code-generation step, no optimizer
invocation at compile time, and no possibility of variation between builds or platforms. This guarantees bit-identical
tables in compressor and decompressor unconditionally. At runtime, cell-to-line resolution is a single indexed
memory access. The table occupies $s^2 \times 2 = 522{,}242$ bytes ($\approx 510$ KB) of static read-only memory
per partition.

#### B.9.2 Optimization Objective

The solver's plateau at depth ~87K (row ~170) occurs because all 8 existing constraint lines through any given cell
have length $s = 511$. At the plateau entry point, each line has roughly 170 of its 511 cells assigned, leaving
$u \approx 341$ unknowns and a residual $\rho$ in the range 80--170. The forcing conditions ($\rho = 0$ or
$\rho = u$) are far from triggering. The solver branches on nearly every cell because no line is tight enough to
force anything.

A uniform pseudorandom LTP has the same problem: its lines scatter cells uniformly across all rows, so at any given
DFS depth, all LTP lines progress at roughly the same rate and none reaches the forcing threshold before the others.
The optimization phase addresses this by biasing line composition so that *some* lines become tight early while
others stay loose.

The ideal optimized table has the following property: a substantial fraction of LTP lines (say 50--100) have a
majority of their $s$ cells concentrated in rows 0--170 (the region already solved when the DFS enters the plateau).
By row 170, these lines have $u \lesssim 170$ --- nearly half their cells are still unknown, but the residual is
correspondingly reduced. More importantly, the cells that remain unknown on these lines are scattered through the
plateau band (rows 100--300), so when the DFS assigns one of them, the line's residual drops toward a forcing
threshold faster than any length-511 linear line can achieve at the same depth. The remaining ~400 LTP lines absorb
the peripheral cells and stay loose, but those cells are already well-served by DSM/XSM (short lines at the corners)
and cardinality forcing on rows and columns.

The optimization objective is therefore: **minimize the mean backtrack count in the plateau band across a
representative suite of random inputs.** This objective is agnostic to the mechanism by which the table achieves its
benefit --- it rewards any line-composition pattern that helps the solver, whether through early tightening, improved
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
cells share a line" structure cannot be analyzed via rank-nullity over $\mathbb{Z}$ --- the partition's geometry is
opaque to linear-algebraic attack.

A 5th toroidal-slope pair, by contrast, is still a linear functional mod $s$. Its null-space contribution partially
overlaps with the existing 4 slope pairs, because all slope constraints live in the same algebraic family. An LTP
line's membership has zero algebraic correlation with any linear family, so the information it contributes is
maximally independent in the combinatorial sense. For swap-invisible patterns, a $k$-cell swap must simultaneously
balance across all linear partitions *and* the non-linear LTP --- a strictly harder condition than balancing across
linear partitions alone.

#### B.9.4 Storage Cost

An LTP with $s$ uniform-length lines encodes identically to LSM, VSM, or any toroidal-slope family. Each line's
cross-sum requires $b = 9$ bits. The per-partition storage cost is:

$$
    B_u(s) = s \times b = 511 \times 9 = 4{,}599 \text{ bits}
$$

Adding one LTP pair costs $2 \times 4{,}599 = 9{,}198$ bits per block --- identical to a toroidal-slope pair:

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

*Early-tightening lines* have a majority of their cells in rows 0--170. By the time the DFS reaches the plateau,
these lines have small $u$ and small $\rho$, approaching forcing thresholds. When the solver assigns a plateau-band
cell that belongs to one of these lines, the residual update may trigger $\rho = 0$ (forcing all remaining unknowns
to 0) or $\rho = u$ (forcing all remaining unknowns to 1). These forcing events propagate through the cell's 8
linear lines, potentially cascading into further assignments. The early-tightening lines thus inject forcing events
into the plateau band --- exactly where the linear partitions provide none.

*Late-tightening lines* have most of their cells in the plateau band and beyond. These lines remain loose during the
plateau but gradually tighten as the DFS progresses past row 300. They provide little forcing during the critical
phase but contribute long-range information transfer: a cell assigned in row 150 updates a late-tightening line
whose other cells span rows 200--400, carrying residual information forward into rows the solver has not yet reached.

Linear partitions, by contrast, have uniform line composition: every row line has 511 cells spanning one row, every
column line has 511 cells spanning one column, and every slope line visits cells at regular modular intervals across
the entire matrix. No linear line becomes tighter than any other at a given DFS depth. The LTP's heterogeneous line
composition is a qualitative difference that uniform-length linear partitions cannot replicate.

#### B.9.6 Runtime Cost

Each LTP partition adds $s = 511$ lines to the `ConstraintStore` --- the same count as a toroidal-slope partition.
At 8 bytes per line statistic (target, assigned, unknown), one LTP partition adds $511 \times 8 = 4{,}088$ bytes,
identical to a slope partition. The per-assignment cost increases by 2 line-stat updates per LTP pair (one lookup per
partition, one stat update each). The lookup is a single array access into the precomputed table --- $O(1)$, typically
an L1 cache hit after the first few accesses. The `PropagationEngine` fast path (`tryPropagateCell`) checks 2
additional lines per LTP pair, extending the current 8-line check to 10 --- a 25% increase in per-iteration cost,
partially offset by the additional forcing events that reduce the total iteration count.

The runtime footprint is identical to adding a 5th toroidal-slope pair, with the sole difference being the 510 KB
lookup table in static read-only memory (versus a computed index formula for slopes).

#### B.9.7 Crossing Density and Orthogonality

Toroidal-slope partitions enjoy perfect 1-cell orthogonality with rows and columns: because $\gcd(p, s) = 1$ for
the implemented slopes ($s = 511$ is prime), every (slope-line, row) pair and every (slope-line, column) pair
intersects in exactly one cell. An optimized LTP does not guarantee this property. A random baseline produces
approximately uniform crossing density --- each (LTP-line, row) pair contains roughly 1 cell in expectation --- but
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
through different mechanisms --- the slope pair through uniform constraint density, the LTP through targeted early
tightening --- and their benefits may be additive.

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

(a) Does an optimized LTP pair provide measurable propagation benefit beyond a 5th toroidal-slope pair, given
identical storage costs (9,198 bits per pair)? The LTP's advantage is non-linearity, targeted early tightening, and
long-range information transfer; the slope pair's advantage is guaranteed 1-cell orthogonality and zero runtime
memory overhead (computed index vs. 510 KB table). Empirical comparison on representative inputs is needed.

(b) How large must the optimization test suite be to produce a table that generalizes to unseen inputs? The
table is optimized against $N$ random matrices, but the hardcoded result must work well on all inputs. Overfitting
to the test suite would produce a table that exploits statistical quirks of the $N$ matrices rather than the
structural properties of the plateau. Cross-validation (optimizing on $N/2$ matrices, evaluating on the other half)
can detect overfitting, but the required $N$ is unknown.

(c) What local-search heuristic converges most efficiently? The hill-climbing approach described in B.9.1 evaluates
$N$ full DFS trajectories per candidate swap, which is expensive. Approximations --- such as evaluating only the
first 100K DFS nodes per matrix, or using the plateau-band backtrack count as a proxy for full-solve cost --- could
accelerate the search by orders of magnitude. Simulated annealing, genetic algorithms, or gradient-free optimization
(e.g., CMA-ES over a parameterized shuffle-bias function) are alternatives to pure hill-climbing.

(d) Can multiple LTP partitions (each optimized from a different seed) be stacked? Two independently optimized
tables would impose two structurally independent non-linear constraint sets. The cost is linear (9,198 bits per
additional pair, 510 KB per additional table), and the diminishing-returns curve is unknown.

(e) Should the optimization objective incorporate the adaptive lookahead (B.8) or optimize against the base solver
($k = 0$) only? Optimizing with lookahead enabled would tune the table for the combined system, but makes the
optimization loop significantly more expensive (each DFS trajectory is slower with probing). Optimizing against
$k = 0$ and then deploying with lookahead may yield most of the benefit at a fraction of the optimization cost.

### B.10 Constraint-Tightness-Driven Cell Ordering

The probability-guided cell ordering computed by `ProbabilityEstimator::computeGlobalCellScores` is static: it is
calculated once before DFS begins and never updated. B.4 partially addressed this by introducing a dynamic
row-completion priority queue that overrides the static order for cells in nearly-complete rows
($u(\text{row}) \leq \tau$). The priority queue is effective at steering the solver toward LH checkpoints, but it
considers only the row dimension --- it ignores constraint tightness on columns, diagonals, slopes, and LTP lines.

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
because completing a row enables an LH check --- the strongest pruning event in the solver. Column and slope lines
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
$\theta$ for each unassigned cell on each of the 8--10 affected lines. This is $O(\sum_L u(L))$ per assignment ---
roughly $8 \times 341 \approx 2{,}700$ updates at the plateau midpoint.

At each DFS node, the solver selects the unassigned cell with the highest $\theta$. A max-heap keyed on $\theta$
supports this in $O(\log n)$ per extraction. The heap replaces B.4's min-heap on $u(\text{row})$, generalizing the
same priority-queue infrastructure to the multi-line score. When cells' $\theta$ values change due to neighboring
assignments, the heap must be updated --- either via decrease/increase-key operations ($O(\log n)$ each) or via lazy
deletion (mark stale entries and re-insert, with periodic rebuilds).

A cheaper approximation is to recompute $\theta$ only for cells in the priority queue (those with
$u(\text{row}) \leq \tau$). This limits the update set to cells in nearly-complete rows, which is a small fraction
of the total. Outside the queue, the static ordering remains in effect.

#### B.10.4 Cost Analysis

The incremental $\theta$ maintenance adds ~2,700 score updates per assignment in the plateau. Each update is a
subtract-and-add on a floating-point accumulator (remove the old $g(u)$ contribution, add the new one). At ~2 ns per
update, this is ~5.4 $\mu$s per assignment --- roughly 2.5$\times$ the current per-iteration cost of ~2 $\mu$s. The
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
correct branch. The two techniques operate on different timescales --- ordering is per-node, lookahead is per-stall
--- and their benefits should be additive.

#### B.10.7 Open Questions

(a) What are the optimal line-type weights $w_L$? The values proposed in B.10.2 are heuristic starting points. An
offline parameter sweep --- evaluating decompression speed on random inputs across a grid of weight vectors --- could
identify the weight combination that minimizes mean backtrack count.

(b) Is the incremental $\theta$ maintenance cost justified by the ordering improvement? If most of the benefit comes
from row-completion priority (the B.4 special case) and the non-row tightness contributions are marginal, the
2.5$\times$ overhead is wasted. A staged experiment --- measuring B.4 alone, then B.10 with non-row weights, and
comparing backtrack counts --- would quantify the marginal value.

(c) Should $\theta$ incorporate the `ProbabilityEstimator` confidence score as an additional term? The confidence
score captures information about residual *balance* (whether a cell's lines favor 0 or 1), which is orthogonal to
tightness (how close lines are to forcing). A combined score $\alpha \cdot \theta + (1 - \alpha) \cdot
\text{confidence}$ could outperform either metric alone.

(d) Can the tightness score predict hash failures before row completion? If $\theta$ for a row's remaining cells
is low (no lines through those cells are tight), the row is unlikely to benefit from prioritization. The solver
could deprioritize such rows and focus on rows where $\theta$ is high, concentrating effort where constraint
feedback is strongest.

### B.11 Randomized Restarts with Heavy-Tail Mitigation

Gomes, Selman, and Kautz (2000) demonstrated that backtracking search times on hard CSP instances follow *heavy-
tailed distributions*: there is a non-negligible probability of catastrophically long runs caused by early bad
decisions that trap the solver deep in a barren subtree. The standard remedy is *randomized restarts* --- the solver
periodically abandons its current search, randomizes its decision heuristic (e.g., by shuffling variable ordering
or perturbing value selection), and begins a fresh search from the root. Luby, Sinclair, and Zuckerman (1993) proved
that the optimal universal restart schedule is geometric: restart after $1, 1, 2, 1, 1, 2, 4, 1, 1, 2, \ldots$
units of work, guaranteeing at most a logarithmic-factor overhead relative to the optimal fixed cutoff.

The CRSCE solver's plateau at row ~170 has the hallmarks of a heavy-tailed stall. The solver commits to assignments
in rows 0--100 during a fast initial phase, but some of those early decisions propagate into the mid-rows via column,
diagonal, and slope constraints, creating an infeasible partial assignment that is only discovered much later when
hash checks begin failing. Chronological backtracking unwinds these bad decisions one level at a time, exploring an
exponential number of intermediate configurations before reaching the root cause. A restart strategy would abandon
the current trajectory, perturb the ordering heuristic, and re-enter the search with a different sequence of early
decisions --- potentially avoiding the bad branch entirely.

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
post-restart heuristic state, are pure functions of the solver's search history --- the sequence of assignments,
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
($\sigma$) applied at a coarser granularity --- B.8 escalates lookahead depth, B.11 restarts the search.

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
CRSCE, the DI is typically small (0--255), so the buffer is at most 256 solutions --- negligible memory.

The safest approach is a *partial restart*: instead of restarting from the root, the solver backtracks to a
*checkpoint depth* (e.g., the beginning of the plateau band at row 100) and re-enters with a modified heuristic for
the subproblem below the checkpoint. The partial assignment above the checkpoint is preserved, so the lexicographic
prefix is unchanged and order preservation is automatic. The solver is effectively restarting only the hard subproblem
(the plateau), not the easy prefix (rows 0--100).

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
from the root? If the root cause of most stalls is in rows 0--100, partial restarts cannot fix them. If the root
cause is in the plateau region itself (rows 100--300), partial restarts are sufficient and avoid re-doing the
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
propagation (BSP) --- which uses SP-guided decimation with backtracking fallback --- solves random K-SAT instances
in practically linear time up to the SAT-UNSAT threshold, a region unreachable by CDCL solvers.

#### B.12.1 Application to CRSCE

The CRSCE constraint system is a factor graph with $s^2 = 261{,}121$ binary variable nodes (CSM cells) and
$10s - 2 = 5{,}108$ factor nodes (constraint lines), plus $s = 511$ hash-verification factors (LH). Each variable
participates in 8 factors (or 10 with an LTP pair). The graph is sparse: each factor connects to $s$ or fewer
variables, and the total edge count is $\sum_L \text{len}(L) \approx 2{,}000{,}000$.

A single BP iteration passes messages along all edges: each factor sends a message to each of its variables
summarizing the constraint's belief about that variable given messages received from all other variables. One
iteration costs $O(\text{edges}) \approx 2 \times 10^6$ multiply-add operations. Convergence typically requires
10--50 iterations for sparse factor graphs, so a full BP computation costs $20$--$100 \times 10^6$ operations
($\approx 20$--100 ms on Apple Silicon at $\sim 10^9$ operations/second).

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
solver makes ~261K assignments per block. Running BP at every step would take ~3.6 hours per block --- far slower
than the current solver. The practical approach is *batch decimation*: run BP, decimate the top $D$ most-biased
cells (e.g., $D = 1{,}000$), propagate, and repeat. This amortizes the BP cost across $D$ assignments, reducing
the overhead to $\sim 50$ ms per 1,000 assignments ($\sim 50$ $\mu$s/assignment) --- roughly 25$\times$ the current
per-assignment cost, but potentially offset by a dramatic reduction in backtracking.

#### B.12.3 Survey Propagation for Backbone Detection

SP extends BP by introducing a third message type: the probability that a variable is *frozen* (forced to a specific
value in all satisfying assignments). In the CRSCE context, a frozen variable is a cell whose value is determined
by the constraint system regardless of how other cells are assigned. SP identifies these frozen cells without
exhaustive enumeration.

Frozen cells discovered by SP can be assigned immediately without branching, effectively shrinking the search space.
If SP identifies 10,000 frozen cells in the plateau region, the DFS has 10,000 fewer branching decisions to make ---
a potentially exponential reduction in tree size. The cost is one SP computation ($\sim 100$--500 ms, as SP
converges more slowly than BP), amortized across thousands of forced assignments.

The caveat is convergence. SP was designed for random CSP instances near the phase transition, where the factor graph
has tree-like local structure. The CRSCE factor graph is not random --- it has regular structure (every row line has
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
which reduces the number of stall points where lookahead is needed. The two techniques are complementary ---
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
remained at approximately 87,487--87,498, indistinguishable from the B.8-only baseline. The Gaussian approximation
may be too coarse for CRSCE's extremely loopy factor graph (each cell participates in 8 constraint families
simultaneously). The BP fixed point may not accurately reflect the true marginals of the constraint satisfaction
subproblem at the DFS frontier, leading to branch-value hints that neither help nor reliably harm the search.

(b) What is the optimal checkpoint interval for BP-guided reordering? Too frequent and the BP overhead dominates;
too infrequent and the ordering becomes stale. The interval should be tuned to the plateau structure: more frequent
checkpoints in the plateau band (rows 100--300), less frequent in the easy prefix and suffix.

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
diversification --- instantiating solvers with different decision strategies, learning schemes, and random seeds ---
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
$S_{\text{DI}}$ in order --- the other instances provide "hints" that accelerate the search but are not trusted
for ordering.

Condition 1 is simpler and sufficient for CRSCE. The cell ordering affects which subtrees are explored first, but
constraint propagation and hash verification are ordering-independent: the same partial assignments are feasible or
infeasible regardless of the order in which they are explored. Different orderings prune different infeasible
subtrees at different speeds, but all instances agree on which solutions are feasible and in what lexicographic
order.

More precisely: the *value* ordering (which value to try first at each branch) and the *lookahead* policy change
only the speed at which the solver traverses the canonical tree. They do not change the tree's structure. The *cell*
ordering, however, changes the tree structure --- different cell orderings produce different DFS trees with different
branching sequences. For DI determinism under cell-ordering diversification, the solver must either (a) restrict
diversification to value ordering and pruning strategies only (preserving the canonical DFS tree), or (b) use
Condition 2 (solution buffering with a canonical-order verifier).

#### B.13.3 Hardware Mapping

Apple Silicon M-series chips provide a natural platform for portfolio solving. The M3 Pro has 6 performance cores
and 6 efficiency cores; the M3 Max has 12 performance cores and 4 efficiency cores. A portfolio of $P = 4$--6
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
speedups of 3--10$\times$ for $P = 4$--8 on hard instances (Hamadi et al., 2009).

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
recorded nogood, the solver backtracks immediately --- saving the ~200 ns SHA-1 computation and, more importantly,
any propagation work that would have followed a (doomed) hash match.

The per-nogood storage is 64 bytes (511 bits rounded to 512 bits = 64 bytes). The hash-table lookup is a single
64-byte comparison --- cheaper than SHA-1. The hash table is keyed on a fast hash of the bitvector (e.g., the first
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
constraint states from other rows). The pruning power is proportional to $2^{511 - k}$ --- the number of full-row
assignments that the partial nogood eliminates.

Identifying which cells are decisions versus forced requires inspecting the undo stack at the point of failure. Each
entry on the stack records whether the assignment was a branch or a propagation. Scanning the stack for row $r$'s
cells is $O(s)$ --- negligible compared to the cost of the DFS that produced the failure.

#### B.14.3 Nogood Checking Efficiency

Partial row nogoods must be checked during the search. The naive approach checks all recorded nogoods for row $r$
whenever a new cell in row $r$ is assigned. This is expensive if many nogoods accumulate.

A more efficient approach checks nogoods only at row completion ($u(\text{row}) = 0$), immediately before the SHA-1
computation. The check is: for each nogood recorded for row $r$, verify whether the current assignment matches the
nogood's decision cells. If any nogood matches, skip SHA-1 and backtrack. The cost is proportional to the number of
nogoods recorded for row $r$ times the number of decision cells per nogood. If the solver records at most $N$
nogoods per row and each nogood has $k \approx 100$ decision cells, the check costs $\sim 100N$ byte comparisons
--- negligible for $N < 1{,}000$.

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
~130 KB --- negligible relative to the constraint store.

Nogoods are scoped to the current block and discarded between blocks. Within a block, nogoods from early failures
remain valid throughout the solve because the constraint system does not change (the same cross-sums and hashes
apply throughout). There is no need for clause deletion or garbage collection.

#### B.14.7 Open Questions

(a) How many hash failures does the solver encounter per block, and how many of those failures recur (same row with
the same decision-cell assignments)? If failures rarely recur, nogoods provide little pruning benefit. Instrumenting
the solver to log failure patterns would quantify the potential.

(b) Are partial row nogoods (B.14.2) significantly more powerful than full row nogoods (B.14.1)? The answer depends
on how much of the row is determined by decisions versus forcing. If ~400 of 511 cells are forced (leaving ~111
decision cells), partial nogoods are $2^{400}$ times more general than full nogoods --- a dramatic increase in
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

Content moved to Appendix C.1.

### B.16 Partial Row Constraint Tightening

The current propagation engine applies two forcing rules: when a line's residual $\rho(L) = 0$, all unknowns are
forced to 0; when $\rho(L) = u(L)$, all unknowns are forced to 1. These rules activate only at the extremes ---
either the residual is fully consumed or it exactly matches the unknown count. Between these extremes, the
propagator does nothing: a line with $\rho = 47$ and $u = 103$ generates no forced assignments, even though the
constraint significantly restricts the feasible completions. This binary behavior is the root cause of the plateau
at depth ~87K: in the middle rows (100--300), residuals are far from both 0 and $u$, and the propagator becomes
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
this is approximately 2.6 ms per full pass --- too expensive to run at every DFS node (which processes at ~500K
nodes/sec), but feasible as a periodic tightening step (e.g., at row boundaries or when the stall detector fires).

#### B.16.4 Row-Specific Early Detection

A targeted variant focuses exclusively on rows approaching completion. When a row has $u \leq u_{\text{threshold}}$
unknowns remaining (e.g., $u_{\text{threshold}} = 20$), the solver performs the interval analysis of B.16.2
restricted to the $u$ unknown cells in that row, considering all 8 constraint lines through each cell. The cost
is $O(8u)$ per row --- $O(160)$ operations at $u = 20$ --- negligible even at full DFS throughput.

This targeted variant captures the most valuable tightening: as a row nears completion, the row residual
constrains the remaining unknowns more tightly (fewer cells share the same residual budget), and cross-line
constraints interact more strongly (fewer unknowns means each cell's value has more impact on other lines). The
combination is most likely to produce forced cells in precisely the regime where the current propagator falls silent.

#### B.16.5 DI Determinism

PRCT is purely deductive: it infers forced cell values from the current constraint state using deterministic
arithmetic. No random or heuristic elements are involved. The forced cells identified by PRCT are logically
implied by the constraint system --- they would be assigned the same values by any correct solver. Therefore, PRCT
preserves DI semantics: the compressor and decompressor produce identical forced assignments and identical search
trajectories.

The iteration order for bounds propagation (B.16.3) must be deterministic (e.g., process lines in index order,
iterate until no cell bounds change). Floating-point arithmetic is not involved --- all quantities are integers
(residuals, unknown counts, binary bounds) --- so bitwise reproducibility is automatic.

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
typically 1--2 iterations, the amortized cost is low. If it is $O(s)$ iterations, the cost is prohibitive except
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

| $u$ | $\rho$ (worst case $= u/2$) | $\binom{u}{\rho}$ | Cost |
|:---:|:---------------------------:|:------------------:|:----:|
| 3   | 1--2                        | 3                  | 0.6 $\mu$s |
| 5   | 2--3                        | 10                 | 2 $\mu$s |
| 8   | 4                           | 70                 | 14 $\mu$s |
| 10  | 5                           | 252                | 50 $\mu$s |
| 15  | 7--8                        | 6,435              | 1.3 ms |
| 20  | 10                          | 184,756            | 37 ms |

At $u \leq 10$, the cost is under 50 $\mu$s --- comparable to a single iteration of the DFS loop at $k = 2$
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
costing ~2--5 $\mu$s), the solver pays one $\binom{u}{\rho} \times 200$ ns enumeration.

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
operations per candidate, or $252 \times 80 = 20{,}160$ operations total --- negligible compared to the SHA-1 cost.

If cross-line filtering eliminates most candidates, the effective SHA-1 count drops substantially. In the plateau
band, where cross-line residuals are moderately constrained, filtering might reduce the candidate count by 2--10×,
making RCLA feasible at higher $u$ thresholds.

#### B.17.4 Incremental SHA-1 Optimization

The $\binom{u}{\rho}$ completions differ only in the positions of $u$ bits within the 512-bit row message. If the
$u$ unknowns are clustered in the same 32-bit word of the SHA-1 input, the solver can precompute the SHA-1
intermediate state up to the word boundary, then finalize only the last few rounds for each candidate. SHA-1
processes its input in 32-bit words; if all unknowns fall within one or two words, the precomputation saves
approximately 80--90\% of the SHA-1 cost per candidate.

In practice, the $u$ unknowns are scattered across the 16-word (512-bit) input block, because the unknowns are the
cells not yet forced by propagation, and forcing patterns depend on cross-line interactions across the full row.
Therefore, the incremental optimization is unlikely to help in the general case. However, when RCLA is invoked at
very small $u$ (3--5 unknowns), the unknowns may happen to cluster, and the optimization is worth attempting.

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
block (200 rows × 511 completions per row in the backtracking process --- many rows are completed multiple times
due to backtracking). If RCLA detects infeasibility at $u = 10$ instead of $u = 0$, each detection saves the cost
of 10 assign-propagate cycles plus any wasted work in subsequent rows. At 2--5 $\mu$s per cycle, the direct saving
is 20--50 $\mu$s per detection. The indirect saving (preventing descent into subsequent rows on a doomed path) is
potentially much larger --- up to hundreds of milliseconds per doomed subtree if the solver would have explored
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

(c) For rows in the fast regime (rows 0--100 and 300--511), is RCLA ever triggered? In these regions, propagation
forces most cells, so $u$ drops to 0 rapidly. RCLA may be relevant only in the plateau band, where propagation
stalls and $u$ remains high.

(d) Should RCLA check cross-line constraints (B.17.3) before or after the SHA-1 check? Cross-line filtering is
cheaper per candidate ($O(8u)$ vs. SHA-1's $\sim 200$ ns), so checking cross-lines first and SHA-1 only on
survivors is more efficient when many candidates fail cross-line checks. But if most candidates pass cross-lines
(as expected when $u$ is small and residuals are loose), the filtering overhead is wasted.

### B.18 Learning from Repeated Hash Failures

B.14 proposes lightweight nogood recording: when an SHA-1 hash check fails, the solver records the failed row
assignment (or the decision-cell subset thereof) as a nogood to prevent re-entering the same configuration. This
appendix extends that idea in a different direction: rather than recording exact failed configurations, the solver
*statistically tracks* which (row, partial assignment) patterns are associated with repeated failures and uses this
information to *bias branching decisions* away from failure-prone patterns. The distinction from B.14 is that B.18
operates at a coarser granularity --- it does not require exact matches to trigger, and it influences branching
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
information (the partial assignment above the row may be fundamentally wrong). A row that fails intermittently ---
passing its hash on some attempts but not others --- provides the most useful signal: the failures are localized
to specific cell configurations within the row.

The solver can distinguish these cases by tracking the *success rate* $P_s(r) = S(r) / (S(r) + F(r))$, where
$S(r)$ is the number of successful hash checks on row $r$. A row with $P_s \approx 0$ is chronically failing (the
problem lies above the row), while a row with $0 < P_s < 1$ has configuration-specific failures (the problem is
within the row's decision cells).

Failure-biased reordering is most effective for the $0 < P_s < 1$ regime: the failure statistics distinguish
good configurations from bad ones. For the $P_s \approx 0$ regime, reordering within the row is futile ---
the solver should backtrack to a higher row instead, which is the domain of B.1 (CDCL) and B.11 (restarts).

#### B.18.5 Storage and Lifetime

The failure counters require $O(s^2)$ storage: one counter per cell per value, plus per-row counters. At 4 bytes
per counter and 2 counters per cell: $261{,}121 \times 2 \times 4 = 2{,}088{,}968$ bytes ($\approx 2$ MB). The
per-row counters add $511 \times 4 = 2{,}044$ bytes. Total: $\approx 2$ MB --- negligible relative to the
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
progress stalls. In practice, the solver reaches $k = 4$ in the deep plateau (rows ~170--300) and remains there,
because the stall metric $\sigma$ never recovers sufficiently to trigger de-escalation. At $k = 4$, the solver
processes 12--30K iterations/second with 16 exhaustive probes per decision. If this throughput is still insufficient
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

At $B = 8$ and $k = 16$, the cost is $8 \times 16 = 128$ probes per decision --- comparable to $k = 7$ exhaustive
($2^7 = 128$) but reaching 16 levels deep. The tradeoff is that beam search is incomplete: it may miss
contradictions that lie outside the beam. However, for detecting cardinality violations that manifest at depth
8--16 (spanning 1--3 cells into the next row), beam search's heuristic filtering is likely to retain the
relevant branches.

*Strategy B: Row-boundary probing.* Rather than probing to a fixed depth $k$, probe forward until the next row
boundary and check the SHA-1 hash. If the current cell is at position $(r, c)$ with $511 - c$ cells remaining
in the row, the probe completes the row (assigning $511 - c$ cells greedily, using constraint-tightness ordering),
checks the SHA-1 hash, and backtracks if the hash fails.

The cost is $O(511 - c)$ propagation steps per probe, which varies from $O(s)$ (if $(r, c)$ is at the beginning
of the row) to $O(1)$ (if $(r, c)$ is near the row end). On average, the probe cost is $O(s/2) \approx 256$
propagation steps ($\sim 500$ $\mu$s). The solver runs two probes per decision (one for each value), costing
$\sim 1$ ms per decision. At $\sim 1{,}000$ decisions per row ($\sim 200{,}000$ plateau-band decisions total), the
overhead is $\sim 200$s per block --- comparable to the current solve time and thus marginally acceptable.

The advantage of row-boundary probing over fixed-depth lookahead is that it exploits the SHA-1 hash as a
verification oracle, which is far more powerful than cardinality-based pruning. A fixed-depth probe at $k = 4$ can
detect only cardinality violations within 4 cells; a row-boundary probe can detect hash mismatches for the entire
row, which catches failures that no finite $k$ would detect.

*Strategy C: Stochastic lookahead.* Rather than exhaustively exploring all $2^k$ paths, sample $M$ random paths
of depth $k$ and check each for feasibility. This is a Monte Carlo estimation of the feasibility rate at depth $k$:
if all $M$ samples are infeasible, the current assignment is likely doomed. The false-negative probability (failing
to detect a feasible path) is $(1 - p)^M$, where $p$ is the fraction of feasible paths. At $p = 0.01$ (1\% of
paths are feasible) and $M = 100$, the false-negative rate is $(0.99)^{100} \approx 0.37$ --- too high for
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
whose variable geometry triggers early forcing at the matrix periphery. The toroidal slopes provide none of these ---
they are uniformly length-511 lines with no hash interaction and no variable-length early-forcing behavior.

This observation raises the possibility that the slopes' underperformance is not a property of their *algebraic
structure* (modular arithmetic, linear null space) but of their *position* in the solver's constraint hierarchy: any
uniform-length-511 line that lacks hash verification will underperform, because at the plateau entry point (row
~170), all such lines have $u \approx 341$ unknowns and residual $\rho \approx 170$, placing them deep in the
forcing dead zone ($\rho \gg 0$ and $\rho \ll u$). Adding more lines of the same length --- whether algebraic or
pseudorandom --- cannot change this arithmetic.

If this *positional hypothesis* is correct, replacing the slopes with optimized LTP pairs will produce similarly
poor marginal performance, despite the LTP's non-linear structure and plateau-targeted optimization. Conversely, if
the LTP pairs significantly outperform the slopes, the *geometric hypothesis* is supported: the non-linear
structure and early-tightening line composition (B.9.5) provide a qualitatively different kind of constraint that
uniform algebraic lines cannot replicate.

#### B.20.2 Experiment Design

The experiment consists of four solver configurations, benchmarked on a common test suite of $N \geq 50$ random
$511 \times 511$ binary matrices:

**Configuration A (baseline).** The current 8-partition solver: 4 geometric (LSM, VSM, DSM, XSM) + 4 toroidal
slopes (HSM1, SFC1, HSM2, SFC2). This is the production configuration.

**Configuration B (geometric only).** The 4 geometric partitions alone, with toroidal slopes removed. This
establishes the baseline performance of the geometric partitions without any supplementary lines, quantifying the
slopes' actual marginal contribution.

**Configuration C (slopes replaced by LTP).** 4 geometric + 4 independently optimized LTP pairs, replacing the
toroidal slopes entirely. Each LTP pair is optimized per B.9.1--B.9.2 (Fisher--Yates baseline + hill-climbing
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

(a) **Total backtrack count** in the plateau band (rows 100--300). This is the primary metric: the plateau is where
the solver spends 90% of its time, and backtrack reduction is the clearest signal of constraint effectiveness.

(b) **Maximum sustained depth** before the first major regression ($d_{\text{bt}} > 1{,}000$ cells). This measures
how far the solver penetrates into the plateau before stalling. Deeper penetration indicates stronger constraint
propagation in the early plateau.

(c) **Forcing event rate** in the plateau band: the fraction of cell assignments that are forced by propagation
(rather than selected by branching). This directly measures constraint tightening. The geometric partitions produce
a high forcing rate in the easy regime (rows 0--100) and a near-zero rate in the plateau. The question is whether
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
the solver's constraint power. The slopes were never contributing meaningfully, and any supplementary partition ---
linear or non-linear --- faces the same positional barrier. This is the strongest evidence for the positional
hypothesis and redirects the research program entirely toward non-partition approaches (B.1 CDCL, B.11 restarts,
B.16--B.19).

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

Each of the four LTP tables is constructed per B.9.1 (Fisher--Yates baseline from a fixed seed) and optimized per
B.9.2 (hill-climbing against simulated DFS). The four seeds are deterministic and distinct:

$$\text{seed}_i = \text{SHA-256}(\texttt{"CRSCE-LTP-v1-table-"} \| i) \quad \text{for } i \in \{0, 1, 2, 3\}$$

Sequential optimization order matters. Table 0 is optimized with the geometric partitions only (Configuration B as
the base solver). Table 1 is optimized with geometric + Table 0. Table 2 with geometric + Tables 0--1. Table 3
with geometric + Tables 0--2. This greedy-sequential approach ensures each table complements the existing constraint
set rather than duplicating it. The alternative --- joint optimization of all four tables simultaneously --- is a
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
This requires no architectural changes --- only observability instrumentation via the existing `IO11y` interface.

*Stage 2: Implement Configuration B.* Disable the four toroidal-slope partitions (set their line count to 0 in
`ConstraintStore`, skip their updates in `PropagationEngine`). Run the test suite. If Outcome 3 materializes
(slopes provide no marginal benefit), the experiment is already informative and Stage 3 can be prioritized or
deprioritized accordingly.

*Stage 3: Construct and integrate LTP tables.* Implement the `LookupTablePartition` class (a `CrossSumVector`
subtype backed by a `constexpr std::array<uint16_t, 261121>` literal). Run the B.9.1 optimization loop to produce
four tables. Integrate into `ConstraintStore` as drop-in replacements for the four `ToroidalSlopeSum` instances.
Run Configurations C and D.

*Stage 4: Analyze and decide.* Compare metrics across A/B/C/D per B.20.4. If Outcome 2 or 4, proceed to
production integration of LTP pairs. If Outcome 1 or 3, redirect research toward B.16--B.19.

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
on the table? If Table 3 is optimized against a solver that already has Tables 0--2, it may converge to a table
that merely patches the remaining gaps in the first three tables' coverage, rather than providing independent
constraint power. A partial remedy is to re-optimize Table 0 after Tables 1--3 exist (iterating the sequential
process), but convergence of this outer loop is not guaranteed.

(b) How sensitive is the experiment to the choice of test suite? Random matrices may not be representative of
real-world inputs (which may have structure --- e.g., natural-language text produces non-uniform bit distributions).
The test suite should include structured inputs (all-zeros, all-ones, alternating patterns, natural data) alongside
random matrices.

(c) Can the truncated-DFS proxy objective (B.20.5) mislead the optimizer? A table that minimizes backtracks in the
first $100{,}000$ nodes may not minimize backtracks in the full solve if the plateau's difficulty profile changes
beyond the truncation window. The truncation point should be validated by comparing proxy rankings with full-solve
rankings on a small subset of matrices.

(d) If Outcome 1 materializes (positional hypothesis confirmed), is there a *variable-length* LTP design that
escapes the dead zone? B.9 specifies uniform-length lines ($s$ cells each), but a generalized LTP could assign
lines of varying length --- some with 100 cells (reaching forcing thresholds early) and others with 900 cells
(providing long-range information). Variable-length LTP lines would encode with variable-width cross-sums
($\lceil \log_2(\text{len}(k) + 1) \rceil$ bits each, as DSM/XSM do), complicating the storage cost analysis but
potentially breaking the positional barrier that uniform lines cannot.

(e) Should the experiment include a Configuration E that replaces the toroidal slopes with a 5th--8th slope pair
(different slope parameters, still algebraic)? This would test whether the slopes' underperformance is specific to
the chosen parameters $\{2, 255, 256, 509\}$ or inherent to the algebraic family. If alternative slope parameters
perform equally poorly, the algebraic family is exhausted and only non-linear alternatives remain.

#### B.20.9 Observed Results (Implemented)

B.20 Configuration C was implemented: 4 geometric partitions + 4 uniform-length LTP partitions (seeds
`"CRSCLTP1"`--`"CRSCLTP4"`, Fisher--Yates baseline, no hill-climbing optimization). The toroidal slopes
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
dropped from $37\%$ to $25.2\%$ --- a reduction of 11.8 percentage points --- indicating that the LTP partitions
generate substantially more propagation in the plateau band than the toroidal slopes did. The iteration rate was
unchanged ($\approx 198{,}000$/sec), confirming that the LTP table lookup imposes no measurable per-iteration
overhead relative to the slope modular-arithmetic formula.

**Outcome classification:** The result falls between Outcome 1 and Outcome 2 from B.20.4. The depth improvement
($+1.1\%$) is far below the $2\times$ backtrack reduction threshold of Outcome 2, but the $-12$ pp reduction in
hash mismatch rate is material. The positional hypothesis is partially supported: uniform-length-511 lines (both
slopes and LTP) cannot break the plateau, but LTP's non-linear structure provides a qualitatively better
constraint signal within the plateau --- evidenced by the mismatch rate falling --- without changing the depth
ceiling.

**Interpretation:** The toroidal slopes were providing weak constraint power in the plateau band. LTP partitions
provide modestly stronger constraint power (fewer dead-end explorations per iteration), but not enough to push
through the fundamental depth barrier at $\approx 88{,}500$. This suggests both families share the same positional
weakness: at plateau entry (row $\approx 170$), every uniform-length-511 line has $u \approx 341$ and
$\rho \approx 170$, placing it in the forcing dead zone. The LTP lines' non-linear structure reduces the fraction
of explorations that terminate in hash failure --- perhaps by providing slightly tighter cross-line bounds --- but
cannot generate the forcing events necessary to push the solver past row $\approx 173$.

**Recommendation:** Proceed to B.21 (joint-tiled variable-length LTP partitions). The B.20 result rules out
uniform-length supplementary lines as a plateau-breaking mechanism; variable-length lines with the triangular
length distribution of DSM/XSM may escape the dead zone by allowing short lines ($\ll 511$ cells) to reach
forcing thresholds earlier in the plateau.

## B.21 Joint-Tiled Variable-Length LTP Partitions

B.20 replaced four toroidal-slope partitions with four uniform-length LTP partitions, each containing
511 lines of 511 cells. Every cell belongs to all four LTP partitions, yielding 8 constraint lines per
cell (4 basic + 4 LTP). The uniform line length, however, forces a fixed 9-bit encoding for every
cross-sum element, and the large line size means that the ratio $\rho / u$ stays near 0.5 for most of
the solve, suppressing forcing events on LTP lines past the first ~50 rows. This section proposes
a *joint-tiled* LTP architecture in which the four LTP partitions collectively tile the matrix once,
each partition contains variable-length lines following the DSM/XSM triangular distribution, and each
cell belongs to exactly one LTP partition. The result is fewer but *stronger* LTP constraints per cell,
a naturally lossless variable-length encoding, and a modest reduction in block payload size.

### B.21.1 Motivation

The DSM and XSM families encode cross-sums with variable bit widths because their line lengths vary
from 1 to $s$. The encoding cost for one diagonal family over $2s - 1$ lines totals $B_d(s) = 8{,}185$
bits (Section 1), far less than the $s \times b = 4{,}599$ bits that a hypothetical 511-element
fixed-width diagonal family would require. The savings arise because short lines have small maximum
sums, requiring fewer bits.

Uniform-length LTP partitions cannot exploit this structure: every line has $s = 511$ cells, so every
cross-sum ranges $[0, 511]$ and requires $b = 9$ bits. If instead each LTP partition contained lines
of varying length---some with 1 cell, some with 256, some with 511---the encoding would mirror the
DSM/XSM scheme, with short lines encoded cheaply and only the longest lines requiring the full 9 bits.

The challenge is arithmetic: a single partition with 511 lines whose lengths sum to $s^2 = 261{,}121$
must average 511 cells per line. A symmetric triangular distribution peaking at 511 sums to only
65,536---approximately one quarter of the matrix. No symmetric distribution ranging from 1 to 511
can average 511 over 511 elements; the only such distribution is the degenerate uniform case. The
resolution is to abandon per-partition completeness: each LTP partition covers approximately one
quarter of the matrix, and the four partitions jointly tile the full $s \times s$ grid.

### B.21.2 Partition Structure

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

### B.21.3 Encoding and Payload Impact

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

### B.21.4 Constraint Strength Analysis

Reducing from 4 LTP lines per cell to 1 appears to weaken the constraint system, but the analysis
is more nuanced. Under B.20's uniform design, each of the 4 LTP lines has 511 cells. During the
DFS solve, a 511-cell line's $\rho / u$ ratio stays near 0.5 until the line is nearly complete,
meaning it almost never triggers forcing ($\rho = 0$ or $\rho = u$). Empirically, the LTP lines
contribute no forcing events past approximately row 50---well before the plateau at row ~170 where
the solver stalls.

Under joint tiling, a cell's single LTP line may be as short as 1 cell (immediately forced) or as
long as 256 cells (reaching forcing thresholds much earlier than a 511-cell line). The distribution
of line lengths follows the triangular pattern: half of all LTP lines have 128 cells or fewer.
A 10-cell line reaches $\rho = 0$ or $\rho = u$ after just 10 assignments---an event that occurs
in the first few rows. A 50-cell line forces within the first 50 assignments to that line. These
short-line forcing events propagate through the basic constraint lines (rows, columns, diagonals),
creating cascading reductions that the uniform 511-cell LTP lines never produce.

The trade-off is therefore not "8 lines down to 5" but rather "4 weak lines replaced by 1 strong
line." The net propagation yield per cell assignment may increase if the short LTP lines' frequent
forcing events outweigh the loss of three long, inert constraint lines. The 1,023 cells assigned to
two sub-tables receive a sixth constraint line, and these privileged cells should be placed
deliberately at the matrix corners where diagonal coverage is weakest (see B.21.5).

### B.21.5 Spatial Layout: Center-Cross / Corner-Long Distribution

The joint-tiled architecture enables a deliberate spatial arrangement of line lengths across the
matrix. The design principle is *complementarity*: place short LTP lines where the basic partitions
are already strong, and long LTP lines where they are weakest.

Diagonal and anti-diagonal line lengths vary with position. A cell at $(r, c)$ lies on diagonal
$d = c - r + (s-1)$ of length $\min(d+1, 2s-1-d)$ and anti-diagonal $x = r + c$ of length
$\min(x+1, 2s-1-x)$. Cells near the matrix center (row $\approx 255$, column $\approx 255$) lie on
diagonals and anti-diagonals of length $\approx 511$, providing maximum constraint coverage from the
basic partitions. Cells in the corners---$(0,0)$, $(0,510)$, $(510,0)$, $(510,510)$---lie on
diagonals and anti-diagonals of length 1, the weakest possible basic coverage.

The spatial layout therefore assigns:

*Short LTP lines (indices near 0 and 510, length 1--64) along the center cross.* The center cross
comprises cells near row 255 (horizontal axis) and column 255 (vertical axis). These cells already
have full-length diagonal and anti-diagonal coverage, so short LTP lines sacrifice little. The LTP
constraint on these cells is minimal but also unnecessary: the basic partitions already constrain
them heavily.

*Long LTP lines (indices near 255, length 192--256) toward the corners.* Corner cells have length-1
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

### B.21.6 Table Construction Protocol

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

### B.21.7 Implications for the Solver

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
solve---exactly the region where propagation is currently most effective.

### B.21.8 Comparison with B.20 Uniform LTP

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

### B.21.9 Predicted Telemetry Targets

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
--- fewer dead-end explorations per iteration. If it remains at ~25% despite variable line lengths,
the non-linear structure was already providing all available constraint power and line length is
irrelevant. A mismatch rate above 30% would indicate that the loss of three LTP lines per cell
(from 4 to 1) outweighs the benefit of shorter, more forceful lines.

*Indicator 3: Plateau depth (lagging, definitive).* The depth ceiling is the ultimate success
metric but moves only when forcing events push the solver into previously unreachable territory.
A jump from 88,503 to 90,000 or above would be a qualitative breakthrough --- the first time any
partition-level change has moved the depth ceiling by more than ~1%. This would confirm that short
LTP lines ($\ell \leq 50$) generate forcing events deep in the plateau where no 511-cell line ever
could. If the depth remains at ~88,500, the positional barrier is deeper than line-length alone can
address, and the research program should pivot decisively toward non-partition approaches: B.1
(conflict-driven learning), B.11 (randomized restarts), and B.16--B.19 (row-completion look-ahead,
failure-biased branching, stall detection).

### B.21.10 Outcome

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
1--2, a 50--75% reduction in LTP-derived coupling.  B.20's four toroidal-slope lines each spanned
all 511 rows (one cell per row), providing global cross-row coupling.  B.21's spatially-partitioned
lines are clustered by spatial score: each line's cells are drawn from the same spatial region
rather than distributed across rows.  At row 98, the LTP lines containing those cells carry little
information about the assignment state of rows 0--97, eliminating the inter-row signal that
stabilized B.20's descent to row 170.

**Interpretation of the hash mismatch improvement.**  The 0.20% hash mismatch rate (vs. 25.2% for
B.20) is the most dramatic constraint-quality improvement recorded in this research program.  It
demonstrates conclusively that variable-length LTP lines ($\ell = 1$--$256$) generate qualitatively
stronger row-level pruning than uniform-511-cell lines.  When a row does complete under B.21, it
almost always satisfies the SHA-256 lateral hash.  The constraint signal is excellent; the problem
is that the stronger constraints create a hard barrier the chronological DFS cannot cross.

**The paradox.**  B.21 simultaneously achieves the best constraint quality and the worst depth in
the experiment history.  This is not contradictory.  Shorter LTP lines commit cells earlier and
more forcefully.  Early commitments that are wrong propagate deeply into the search tree before the
contradiction surfaces at row ~98.  Chronological backtracking must then unwind all 50,272 branching
choices to reach the actual source of the conflict, which may lie at row 10--20.  Without
non-chronological backjumping, the adaptive lookahead (B.8) at probe depth 4 cannot reach far
enough back to identify and prune the bad decision.

**Implication for CDCL (B.1).**  B.21.9 predicted that CDCL would benefit if B.21 dropped the hash
mismatch rate below 20%, because a low mismatch rate implies that the reason graph contains rich
antecedent chains tracing hash failures back to early decisions.  The observed 0.20% rate satisfies
this condition by a wide margin.  However, B.21 also imposes a 43% depth penalty that CDCL alone
cannot recover; re-adding the fourth LTP line per cell (restoring cross-row coupling) while
preserving variable line lengths is a prerequisite for CDCL to operate in the plateau band rather
than below it.  A hybrid design --- four variable-length LTP lines per cell, each spanning multiple
rows, with triangular length distribution --- is the logical B.22 candidate.

**Disposition.**  B.21 is retained in the codebase as a correct, complete implementation.  The
variable-length encoding it introduces (saving 502 bytes per block relative to B.20) is a permanent
improvement regardless of the partition topology.  The plateau regression requires a new partition
design before the depth ceiling can be attacked further.

### B.21.11 Next Iteration Improvement

B.21 established three empirical facts that constrain the design space for the next iteration.

*Fact 1: Variable line lengths work.* The 0.20% hash mismatch rate is the strongest constraint-quality
signal recorded in this project.  The triangular length distribution $\ell(k) = \min(k+1, 511-k)$,
$k = 0 \ldots 510$ generates short lines that force cells early and deeply enough to match the
correct assignment on virtually every row completion.  This distribution, or the variable-length
encoding mechanism it implies, should be preserved in any successor design.

*Fact 2: Lines per cell cannot fall below 4.* Reducing from 4 LTP lines per cell (B.20) to 1--2
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
CDCL fails to move the depth needle, the depth wall at row 98 is structural --- the partial
assignment at depth 50K is globally inconsistent with the LTP sums but no single early decision
is singularly responsible --- and a new partition design is required.

---

**Candidate 2: Full-coverage variable-length partitions (B.22, medium cost)**

The root cause of B.21's depth regression is the joint-tiling constraint: covering all 261,121
cells with only $4 \times 65{,}536 = 262{,}144$ cell-slots forces each cell into at most 1--2
sub-tables, destroying the 4-line-per-cell coupling B.20 provided.  The fix is to restore full
coverage: each sub-table assigns every cell to exactly one line, giving $\sum_k \ell(k) = 261{,}121$
per sub-table and exactly 4 LTP lines per cell.

The construction must enforce row diversity.  Rather than sorting cells by spatial score (which
clusters spatial neighbours together), cells should be ordered by a row-interleaving key before
assignment to lines --- for example, column-major order $(c, r)$ rather than row-major $(r, c)$.
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
at most 10 bits per LTP sum element (vs. 9 bits for B.20 and 1--9 bits for B.21).  Total encoding
per sub-table is $\sum_{k=0}^{510} \lceil \log_2(\ell'(k)+1) \rceil$ bits; a rough estimate gives
$\approx 4{,}200$--$5{,}000$ bits per sub-table, comparable to B.20's $511 \times 9 = 4{,}599$
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

If step 1 achieves depth $\geq 90{,}000$, steps 2--3 become optional optimizations rather than
required fixes, and the research program can pivot to compression-ratio improvements or the
parallel-restart strategy (B.11).

### B.21.13 Open Questions

(a) What is the optimal distribution of the 1,023 dual-covered cells? Placing them at extreme corners
maximizes complementarity with diagonal coverage, but other strategies (e.g., distributing them along
the anti-diagonal extremes) may yield better propagation cascades. Empirical testing with the existing
solver can compare strategies by measuring depth-to-backtrack statistics.

(b) Does the sequential sub-table construction introduce ordering bias? $T_0$ gets first pick of the
cell pool and may receive a higher-quality spatial layout than $T_3$, which operates on the residual.
An iterative refinement step (re-constructing $T_0$ after $T_1$--$T_3$ exist, then repeating) could
equalize quality across sub-tables, at the cost of construction complexity.

(c) Is the triangular length distribution optimal, or would a different distribution (e.g., quadratic,
logarithmic) better match the solver's propagation dynamics? The triangular distribution mirrors
DSM/XSM for encoding simplicity, but the solver's performance depends on the distribution of short
lines relative to the DFS traversal order. A distribution with more short lines and fewer long ones
might increase early forcing at the cost of weaker mid-solve constraints.

(d) How does joint tiling interact with the belief-propagation-guided branching heuristic (Section 7)?
The BP estimator currently assumes each cell participates in 8 factor nodes (one per constraint line).
Reducing to 5--6 factors changes the BP update equations and convergence properties. The BP estimator
must be updated to handle variable factor counts per cell.

(e) Can the joint-tiling principle extend to the basic partitions? If diagonal and anti-diagonal
families were jointly tiled (each cell on exactly one of the two), the total constraint count would
drop further but diagonal encoding would change. This is unlikely to be beneficial because DSM and XSM
provide qualitatively different constraint information (different slope directions), whereas the four
LTP sub-tables are qualitatively interchangeable.

## Appendix C: Abandoned Ideas

This appendix collects research directions that were explored in detail but ultimately abandoned on
cost-benefit grounds. Each section is preserved in full for archival purposes: the analysis may become
relevant if future architectural changes alter the assumptions that led to abandonment.

### C.1 Loopy Belief Propagation as Integrated Inference (formerly B.15)

B.12 proposes belief propagation (BP) as a periodic reordering oracle: the solver pauses DFS at checkpoint rows,
runs BP to convergence on the residual subproblem, and uses the resulting marginals to guide cell ordering or
batch decimation. This appendix examines a more aggressive alternative: *loopy belief propagation* (LBP)
embedded directly into the propagation loop, running continuously alongside constraint propagation rather than
at discrete checkpoints. The question is whether LBP can provide sufficiently accurate marginal estimates on the
CRSCE factor graph to break the 87K-depth stalling barrier, and whether the computational cost can be amortized
to a tolerable level.

#### C.1.1 Background and Theoretical Foundations

Belief propagation computes exact marginal distributions on tree-structured factor graphs by passing messages between
variable nodes and factor nodes until convergence (Pearl, 1988). Each variable-to-factor message summarizes the
variable's belief about its own state given all constraints *except* the recipient factor, and each factor-to-variable
message summarizes the factor's constraint on the variable given messages from all other variables in the factor's
scope. On trees, BP converges in a number of iterations equal to the graph diameter, and the resulting marginals are
exact (Kschischang, Frey, & Loeliger, 2001).

When the factor graph contains cycles --- as the CRSCE graph invariably does, since every cell participates in 8
constraint lines and those lines share cells --- BP is no longer guaranteed to converge, and the marginals it
produces are approximations. This regime is called *loopy belief propagation*. Despite the lack of formal
convergence guarantees on loopy graphs, LBP has achieved remarkable empirical success in several domains: turbo
decoding and LDPC codes in communications (McEliece, MacKay, & Cheng, 1998), stereo vision and image segmentation
in computer vision (Sun, Zheng, & Shum, 2003), and random satisfiability near the phase transition (Mézard, Parisi,
& Zecchina, 2002).

The theoretical justification for LBP's empirical success rests on two results. First, Yedidia, Freeman, and Weiss
(2001) showed that fixed points of LBP correspond to stationary points of the Bethe free energy, a variational
approximation to the true log-partition function. The Bethe approximation is exact on trees and typically accurate
when the factor graph has long cycles (low density of short loops). Second, Tatikonda and Jordan (2002) proved that
LBP converges to a unique fixed point when the spectral radius of the dependency matrix is less than one --- a
condition related to the graph's coupling strength and cycle structure.

#### C.1.2 The CRSCE Factor Graph: Structural Analysis

The CRSCE factor graph has specific structural properties that bear directly on LBP's applicability. The graph
contains $s^2 = 261{,}121$ binary variable nodes and $10s - 2 = 5{,}108$ factor nodes (with an additional $s = 511$
LH verification factors). Each variable participates in exactly 8 factors (one per constraint-line family: row,
column, diagonal, anti-diagonal, and four toroidal slopes), and each factor connects to exactly $s = 511$ variables
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
factor graph's neighborhood is locally tree-like --- that the subgraph reachable from any node within $k$ hops
contains few cycles. With $O(s)$ 4-cycles per cell, this assumption is violated at $k = 2$. The practical
consequence is that LBP's marginal estimates will overcount correlations: a cell's belief will incorporate the same
constraint information multiple times through different cycle paths, producing overconfident (polarized) marginals.

#### C.1.3 The Case for LBP in CRSCE

Despite the structural concerns, several arguments favor LBP for CRSCE plateau-breaking.

*Argument 1: Marginal accuracy need not be high.* LBP is proposed not as an exact inference engine but as a
heuristic guide for branching decisions. The solver needs only a *ranking* of cells by how constrained they are, not
precise probabilities. If LBP's marginals are monotonically correlated with true marginals --- even with substantial
absolute error --- the resulting ordering will be superior to the current static `ProbabilityEstimator`. Empirical
studies in SAT solving have shown that even crude BP approximations improve branching heuristics relative to purely
local measures (Hsu & McIlraith, 2006).

*Argument 2: LBP captures long-range correlations that local propagation misses.* The 87K-depth stalling barrier
arises because cardinality forcing becomes inactive in the plateau band (rows ~100--300): residuals are neither 0 nor
equal to the unknown count, so the propagation engine cannot force any cells. The information needed to break the
stalemate exists in the constraint system --- distant cells' assignments constrain the residuals of lines passing
through the plateau --- but the current propagator does not transmit this information because it operates only on
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
assignment --- approximately $10\times$ the cost of the current constraint-propagation step, but far cheaper than
a full BP computation ($\sim 50$ ms).

*Argument 4: Warm-starting preserves convergence quality.* Because LBP runs continuously, each new assignment
perturbs the message state only slightly. The messages are already close to the fixed point of the previous
subproblem, so convergence to the new fixed point (given the new assignment) requires only a few iterations of the
affected messages. This warm-start property is well-established in the graphical models literature (Murphy, Weiss,
& Jordan, 1999) and is the key to making continuous LBP computationally feasible.

#### C.1.4 The Case Against LBP in CRSCE

The arguments against LBP are substantial and grounded in both theory and the specific structure of the CRSCE
problem.

*Objection 1: Non-convergence on densely loopy graphs.* Tatikonda and Jordan's (2002) convergence condition requires
that the spectral radius of the graph's dependency matrix be less than one. For the CRSCE factor graph, each factor
connects to $s = 511$ variables, and each variable participates in 8 factors. The coupling strength is directly
related to the factor's constraint tightness: a cardinality constraint with residual $\rho$ and $u$ unknowns has
coupling strength proportional to $|\rho / u - 0.5|$ (how far the constraint is from maximum entropy). In the
plateau band, $\rho / u \approx 0.5$ (the constraint is maximally uninformative), so coupling is weak and LBP may
converge. But at the matrix edges (early and late rows), constraints are tight and coupling is strong. The spectral
radius condition may be satisfied only in the plateau band --- exactly where LBP is needed but also where the
marginals carry the least information. This creates a paradox: LBP converges where it is least useful and diverges
where it could provide the strongest guidance.

*Objection 2: Overconfident marginals from short cycles.* The $O(s^3)$ 4-cycles in the CRSCE graph cause LBP to
double-count constraint evidence. A cell $(r, c)$ receives a message from its row factor and a message from its
column factor, but these messages are not independent: they share information about cells in the same row-column
intersection. The result is overconfident marginals --- beliefs close to 0 or 1 even when the true marginal is near
0.5. Overconfident marginals produce aggressive branching decisions that frequently lead to conflicts, increasing
rather than decreasing the backtrack count. Wainwright and Jordan (2008, Section 4.2) document this failure mode
extensively and show that it is intrinsic to the Bethe approximation on graphs with dense short cycles.

Mitigation strategies exist. Tree-reweighted BP (TRW-BP) (Wainwright, Jaakkola, & Willsky, 2005) assigns
edge-appearance probabilities that correct for double-counting, guaranteeing an upper bound on the log-partition
function. However, TRW-BP requires computing edge-appearance probabilities from a set of spanning trees, which is
$O(|E|^2)$ for the CRSCE graph ($|E| \approx 2 \times 10^6$, so $O(4 \times 10^{12})$) --- prohibitively
expensive. Fractional BP (Wiegerinck & Heskes, 2003) and convex variants (Globerson & Jaakkola, 2007) similarly
incur overhead that exceeds the computational budget.

*Objection 3: Cardinality factors are expensive.* Standard BP message computation for a factor with $k$ variables
requires summing over $2^k$ joint configurations. For CRSCE's cardinality constraints (each connecting $s = 511$
binary variables), the naive cost is $2^{511}$ --- clearly intractable. Efficient message computation for cardinality
factors is possible using the convolution trick: the sum-product messages for a cardinality constraint on $k$ binary
variables with target sum $\sigma$ can be computed in $O(k^2)$ time using dynamic programming (Darwiche, 2009,
Section 12.4). At $k = 511$, this is $\sim 261{,}000$ operations per factor per iteration. With 5,108 factors and
10 iterations, the total cost is $\sim 1.3 \times 10^{10}$ operations ($\sim 13$ seconds on Apple Silicon) ---
orders of magnitude slower than the current solver's throughput of $\sim 500{,}000$ assignments/second.

The incremental approach (C.1.3, Argument 3) mitigates this by updating only 8 factors per assignment rather than
all 5,108, but each factor update still costs $O(s^2) \approx 261{,}000$ operations. At 8 factors per assignment,
the incremental cost is $\sim 2 \times 10^6$ operations per assignment --- a $4{,}000\times$ overhead relative to
the current propagation step ($\sim 500$ operations per assignment). Even with Apple Silicon's throughput, this
reduces the solver from 500K assignments/second to $\sim 125$ assignments/second, extending block solve times from
minutes to weeks.

*Objection 4: LBP provides no pruning, only ordering.* Unlike constraint propagation (which prunes infeasible
assignments) or CDCL (which prunes infeasible subtrees), LBP provides only soft guidance: a ranking of cells and
value preferences. The solver still explores the same search tree; it merely traverses it in a different order. If
the search tree's branching factor is dominated by the 87K-depth plateau (where $\sim 111$ cells per row are
unconstrained), reordering the traversal cannot reduce the tree's exponential size --- it can only improve the
constant factor in front of the exponential. The fundamental problem is that the plateau's combinatorial explosion
requires *pruning* (eliminating subtrees) rather than *reordering* (visiting subtrees in a better sequence). LBP
does not prune.

This objection does not apply if LBP's marginals enable other pruning mechanisms to activate. For example, if LBP
reveals that a cell is near-forced ($p \approx 0.01$), the solver can treat it as forced and propagate, triggering
constraint-propagation cascades that genuinely prune. But this amounts to using LBP as a soft forcing heuristic ---
converting approximate beliefs into hard assignments --- which risks correctness violations if the marginal is wrong.
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

#### C.1.5 Quantitative Cost-Benefit Estimate

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
$2.5 \times 10^6 \times 8{,}000\,\mu\text{s} = 20{,}000$s ($\approx 5.6$ hours) --- still slower than the 540s
baseline, because the LBP overhead applies to all $\sim 100{,}000$ assignments in the plateau, not just to the
$2.5 \times 10^6$ backtracks. The full accounting: $100{,}000$ assignments $\times$ $8{,}000\,\mu\text{s/assignment}
= 800$s for the forward pass, plus $2.5 \times 10^6$ backtracks $\times$ $8{,}000\,\mu\text{s} = 20{,}000$s for
backtracking. Total: $20{,}800$s. The LBP overhead on the forward pass alone ($800$s) exceeds the $540$s baseline.

*Reduced-overhead scenario.* If the incremental wavefront is limited to $\delta = 1$ hop (only the 8 directly
affected factors, with no outward propagation), the per-assignment cost drops to $\sim 8 \times 261{,}000 \approx
2 \times 10^6$ operations ($\sim 2$ms). At $100{,}000$ forward assignments: $200$s. At a $100\times$ backtrack
reduction ($10^8$ backtracks $\times$ $2$ms): $200{,}000$s. Still a net loss. Even at $\delta = 1$ and a
$10{,}000\times$ backtrack reduction: $100{,}000 \times 2\text{ms} + 10^6 \times 2\text{ms} = 200\text{s} +
2{,}000\text{s} = 2{,}200$s --- roughly $4\times$ worse than baseline.

The arithmetic is unforgiving. LBP's per-node cost on length-511 cardinality factors is fundamentally too high for
integration into a DFS loop that processes hundreds of thousands of nodes.

#### C.1.6 Comparison with B.12's Checkpoint Approach

B.12 avoids LBP's per-node overhead by running BP only at checkpoints (every 50 rows, or at plateau entry). The
amortized cost is $\sim 50$ms per $25{,}000$ assignments ($\sim 2\,\mu\text{s/assignment}$) --- nearly identical to
the baseline per-assignment cost. The tradeoff is that BP's marginals become stale between checkpoints: after
$25{,}000$ assignments, the factor graph has changed substantially, and the checkpoint marginals may no longer
reflect the current constraint state.

LBP's theoretical advantage over checkpoint BP is freshness: by updating messages after every assignment, LBP
always reflects the current constraint state. But as Section C.1.5 shows, the cost of freshness is prohibitive. The
checkpoint approach trades marginal accuracy for computational feasibility, and this trade is overwhelmingly
favorable. If the solver needs more accurate marginals between checkpoints, the natural remedy is to increase the
checkpoint frequency (e.g., every 10 rows) rather than to switch to continuous LBP. Even at 1 checkpoint per row,
the amortized cost is $\sim 50\text{ms} / 511 \approx 100\,\mu\text{s/assignment}$ --- a $50\times$ overhead rather
than $4{,}000\times$.

#### C.1.7 Mitigation Strategies and Their Limitations

Several techniques could reduce LBP's overhead, though none sufficiently to change the fundamental cost-benefit
calculus.

*Approximate message updates.* Rather than computing exact sum-product messages for cardinality factors ($O(s^2)$
per factor), the solver could use Gaussian approximation: approximate the binomial distribution of the cardinality
factor's output with a Gaussian and compute messages in $O(s)$ time. This reduces the per-factor cost from
$\sim 261{,}000$ to $\sim 511$ operations, yielding a per-assignment cost of $\sim 4{,}000$ operations ($8\times$
overhead). However, the Gaussian approximation is poor for binary cardinality factors when the residual is small
($\rho \ll s$) or large ($\rho \approx u$) --- precisely the regime where the constraint is informative and LBP
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

#### C.1.8 Verdict

The weight of evidence is against continuous LBP as an integrated propagation mechanism for CRSCE. The core problem
is the mismatch between LBP's computational cost and the DFS solver's throughput requirements. The CRSCE solver
processes $\sim 500{,}000$ nodes/second in the fast regime and needs to maintain high throughput even in the plateau
band to solve blocks within practical time bounds. LBP's per-node overhead on length-511 cardinality factors ---
even with Gaussian approximation and selective application --- reduces throughput by at least an order of magnitude.
The backtrack reduction LBP provides (improved ordering but no pruning) cannot compensate for this throughput loss
except under implausibly optimistic assumptions ($> 4{,}000\times$ backtrack reduction).

B.12's checkpoint approach captures most of LBP's benefit (global marginal information for branching guidance)
at a fraction of the cost (one BP computation every $N$ rows, amortized across thousands of assignments). The
marginal-staleness cost of checkpoint BP is small relative to the computational savings, and can be reduced by
increasing checkpoint frequency.

If future solver development reveals that checkpoint BP's marginal estimates are critically stale between
checkpoints --- causing branching decisions that are actively worse than the current `ProbabilityEstimator` ---
then selective LBP with Gaussian approximation in the plateau band (C.1.7) merits empirical evaluation. But this
scenario is unlikely: checkpoint BP with fresh marginals at plateau entry should outperform the static estimator
over the entire plateau span, and the incremental degradation over 50 rows is unlikely to be catastrophic.

The recommendation is to pursue B.12's checkpoint BP approach and to treat continuous LBP as a theoretical
alternative that is dominated on cost-benefit grounds.

#### C.1.9 Open Questions

(a) What is the empirical convergence behavior of LBP on the CRSCE factor graph at various depths? If LBP converges
rapidly in the plateau band (where coupling is weak) but slowly at the edges, the selective-LBP strategy (C.1.7)
may be more viable than the quantitative estimates suggest, because the plateau is where the solver spends most of
its time.

(b) Can the cardinality-factor message computation be restructured to exploit SIMD on Apple Silicon? The $O(s^2)$
dynamic programming has data dependencies that prevent naive vectorization, but the 8 independent factor updates per
assignment could be pipelined across NEON lanes, potentially reducing the $8\times$ factor to $2$--$3\times$.

(c) Does LBP's overconfidence (Objection 2) systematically harm branching in the CRSCE instance, or does the ranking
correlation with true marginals (Argument 1) dominate? This is an empirical question that could be answered by
running LBP on the CRSCE factor graph, comparing the resulting marginals with exact marginals (computed on small
subproblems), and measuring the rank correlation.

(d) Is there a hybrid approach where LBP runs asynchronously on a background thread, and the DFS solver queries the
most recent marginal estimates without blocking? This decouples LBP's throughput from the DFS throughput, at the
cost of using stale (but continuously improving) marginals. The determinism concern (Objection 5) is severe for
this approach, as the asynchronous schedule is inherently non-deterministic.

(e) Would region-based generalizations of BP --- such as generalized belief propagation (GBP) on Kikuchi clusters
(Yedidia, Freeman, & Weiss, 2005) --- provide more accurate marginals on the densely-loopy CRSCE graph? GBP
operates on clusters of variables rather than individual variables, explicitly accounting for short cycles within
each cluster. The cost is $O(2^{|C|})$ per cluster of size $|C|$, which is tractable only for small clusters
($|C| \leq 20$). Whether meaningful clusters exist in the CRSCE graph's regular structure is an open question.

## References

Apple. (n.d.-a). Metal developer documentation.

Apple. (n.d.-b). Performing calculations on a GPU (Metal).

Apple. (n.d.-c). Metal Shading Language Specification (PDF).

Apple. (n.d.-d). Metal Performance Shaders: Overview and tuning hints.

Bader, M. (2013). *Space-filling curves: An introduction with applications in scientific computing*. Springer.

Bessière, C. (2006). Constraint propagation. In F. Rossi, P. van Beek, & T. Walsh (Eds.), *Handbook of constraint
programming* (pp. 29--83). Elsevier.

Braunstein, A., Mézard, M., & Zecchina, R. (2005). Survey propagation: An algorithm for satisfiability. *Random
Structures & Algorithms, 27*(2), 201--226.

Colbourn, C. J., & Dinitz, J. H. (Eds.). (2007). *Handbook of combinatorial designs* (2nd ed.). Chapman & Hall/CRC.

Darwiche, A. (2009). *Modeling and reasoning with Bayesian networks*. Cambridge University Press.

Dang, Q. (2012). Recommendation for applications using approved hash algorithms (NIST Special Publication 800-107
Revision 1). National Institute of Standards and Technology.

Debruyne, R., & Bessière, C. (1997). Some practicable filtering techniques for the constraint satisfaction problem.
In *Proceedings of the 15th International Joint Conference on Artificial Intelligence (IJCAI-97)* (pp. 412--417).
Morgan Kaufmann.

Globerson, A., & Jaakkola, T. S. (2007). Fixing max-product: Convergent message passing algorithms for MAP
LP-relaxations. In *Advances in Neural Information Processing Systems 20* (pp. 553--560). MIT Press.

Gomes, C. P., Selman, B., Crato, N., & Kautz, H. (2000). Heavy-tailed phenomena in satisfiability and constraint
satisfaction problems. *Journal of Automated Reasoning, 24*(1--2), 67--100.

Hamadi, Y., Jabbour, S., & Sais, L. (2009). ManySAT: A parallel SAT solver. *Journal on Satisfiability, Boolean
Modeling and Computation, 6*(4), 245--262.

Hamilton, C. H., & Rau-Chaplin, A. (2008). Compact Hilbert indices: Space-filling curves for domains with unequal side
lengths. *Information Processing Letters, 105*(5), 155--163.

Hsu, E. I., & McIlraith, S. A. (2006). Characterizing propagation methods for Boolean satisfiability. In
*Proceedings of the 9th International Conference on Theory and Applications of Satisfiability Testing (SAT 2006)*
(LNCS 4121, pp. 325--338). Springer.

Haralick, R. M., & Elliott, G. L. (1980). Increasing tree search efficiency for constraint satisfaction problems.
*Artificial Intelligence, 14*(3), 263--313.

Haverkort, H. J. (2017). How many three-dimensional Hilbert curves are there? *Journal of Computational Geometry,
8*(1), 206--281.

Jerrum, M., Sinclair, A., & Vigoda, E. (2004). A polynomial-time approximation algorithm for the permanent of a matrix
with nonnegative entries. *Journal of the ACM, 51*(4), 671--697.

Kelsey, J., & Schneier, B. (2005). Second preimages on $n$-bit hash functions for much less than $2^n$ work. In
R. Cramer (Ed.), *Advances in Cryptology --- EUROCRYPT 2005* (LNCS 3494, pp. 474--490). Springer.

Knuth, D. E. (2005). *The Art of Computer Programming, Volume 4, Fascicle 2: Generating All Tuples and Permutations*.
Addison-Wesley.

Kobler, J., et al. (2025). Disjoint projected enumeration for SAT and SMT without blocking clauses. *Artificial
Intelligence*.

Kschischang, F. R., Frey, B. J., & Loeliger, H.-A. (2001). Factor graphs and the sum-product algorithm. *IEEE
Transactions on Information Theory, 47*(2), 498--519.

Leurent, G., & Peyrin, T. (2020). SHA-1 is a shambles: First chosen-prefix collision on SHA-1 and application to
the PGP web of trust. In *Proceedings of the 29th USENIX Security Symposium* (pp. 1839--1856). USENIX Association.

McEliece, R. J., MacKay, D. J. C., & Cheng, J.-F. (1998). Turbo decoding as an instance of Pearl's "belief
propagation" algorithm. *IEEE Journal on Selected Areas in Communications, 16*(2), 140--152.

Luby, M., Sinclair, A., & Zuckerman, D. (1993). Optimal speedup of Las Vegas algorithms. *Information Processing
Letters, 47*(4), 173--180.

Mokbel, M. F., & Aref, W. G. (2001). Irregularity in multi-dimensional space-filling curves with applications in
multimedia databases. In *Proceedings of the Tenth International Conference on Information and Knowledge Management*
(pp. 512--519). ACM.

Marino, R., Parisi, G., & Ricci-Tersenghi, F. (2016). The backtracking survey propagation algorithm for solving
random K-SAT problems. *Nature Communications, 7*, 12996.

Marques-Silva, J. P., & Sakallah, K. A. (1999). GRASP: A search algorithm for propositional satisfiability. *IEEE
Transactions on Computers, 48*(5), 506--521.

Murphy, K. P., Weiss, Y., & Jordan, M. I. (1999). Loopy belief propagation for approximate inference: An
empirical study. In *Proceedings of the 15th Conference on Uncertainty in Artificial Intelligence (UAI-99)*
(pp. 467--475). Morgan Kaufmann.

Moskewicz, M. W., Madigan, C. F., Zhao, Y., Zhang, L., & Malik, S. (2001). Chaff: Engineering an efficient SAT
solver. In *Proceedings of the 38th Design Automation Conference* (pp. 530--535). ACM.

Mézard, M., Parisi, G., & Zecchina, R. (2002). Analytic and algorithmic solution of random satisfiability problems.
*Science, 297*(5582), 812--815.

Pearl, J. (1988). *Probabilistic reasoning in intelligent systems: Networks of plausible inference*. Morgan Kaufmann.

National Institute of Standards and Technology. (2015). *Secure Hash Standard (SHS) (FIPS PUB 180-4)*.

Niedermeier, R., Reinhardt, K., & Sanders, P. (2002). Towards optimal locality in mesh-indexings. *Discrete Applied
Mathematics, 117*(1--3), 211--237.

Pipatsrisawat, K., & Darwiche, A. (2007). A lightweight component caching scheme for satisfiability solvers. In
J. Marques-Silva & K. A. Sakallah (Eds.), *Theory and Applications of Satisfiability Testing --- SAT 2007* (LNCS
4501, pp. 294--299). Springer.

Sagan, H. (1994). *Space-filling curves*. Springer.

Sun, J., Zheng, N.-N., & Shum, H.-Y. (2003). Stereo matching using belief propagation. *IEEE Transactions on
Pattern Analysis and Machine Intelligence, 25*(7), 787--800.

Stevens, M., Bursztein, E., Karpman, P., Albertini, A., & Markov, Y. (2017). The first collision for full SHA-1.
In J. Katz & H. Shacham (Eds.), *Advances in Cryptology --- CRYPTO 2017* (LNCS 10401, pp. 570--596). Springer.

Tatikonda, S. C., & Jordan, M. I. (2002). Loopy belief propagation and Gibbs measures. In *Proceedings of the
18th Conference on Uncertainty in Artificial Intelligence (UAI-02)* (pp. 493--500). Morgan Kaufmann.

Valiant, L. G. (1979). The complexity of computing the permanent. *Theoretical Computer Science, 8*(2), 189--201.

Wainwright, M. J., Jaakkola, T. S., & Willsky, A. S. (2005). MAP estimation via agreement on trees: Message-passing
and linear programming. *IEEE Transactions on Information Theory, 51*(11), 3697--3717.

Wainwright, M. J., & Jordan, M. I. (2008). Graphical models, exponential families, and variational inference.
*Foundations and Trends in Machine Learning, 1*(1--2), 1--305.

Xu, L., Hutter, F., Hoos, H. H., & Leyton-Brown, K. (2008). SATzilla: Portfolio-based algorithm selection for SAT.
*Journal of Artificial Intelligence Research, 32*, 565--606.

Wiegerinck, W., & Heskes, T. (2003). Fractional belief propagation. In *Advances in Neural Information Processing
Systems 15* (pp. 438--445). MIT Press.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2001). Generalized belief propagation. In *Advances in Neural
Information Processing Systems 13* (pp. 689--695). MIT Press.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2002). *Understanding belief propagation and its generalizations*
(Technical Report TR-2001-22). Mitsubishi Electric Research Laboratories.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2005). Constructing free-energy approximations and generalized belief
propagation algorithms. *IEEE Transactions on Information Theory, 51*(7), 2282--2312.

Zhang, L., Madigan, C. F., Moskewicz, M. W., & Malik, S. (2001). Efficient conflict driven learning in a Boolean
satisfiability solver. In *Proceedings of the 2001 IEEE/ACM International Conference on Computer-Aided Design*
(pp. 279--285). IEEE.
