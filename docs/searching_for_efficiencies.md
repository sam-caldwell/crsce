# Searching for Efficiencies: Information Density and Compression in CRSCE Constraint Systems

**Samuel Caldwell**
Champlain College, CMIT-450: Senior Seminar

---

## Abstract

The Cross-Sums Compression and Expansion (CRSCE) format encodes binary matrices using cross-sum
projections and cryptographic hash digests, then reconstructs the original matrix by solving a
constraint satisfaction problem. The central challenge is that the constraint system is severely
underdetermined: the number of independent equations is far smaller than the number of unknown cell
values. This paper analyzes the information-theoretic efficiency of every constraint type available to
the CRSCE format—geometric cross-sums, pseudorandom lookup-table partitions, and CRC-32 hash
digests—and derives the fundamental tradeoff between compression ratio and information density that
governs all feasible configurations. The analysis reveals a two-phase solver architecture in which
integer cardinality constraints and GF(2) hash equations serve complementary, non-competing roles,
and identifies the optimal allocation of payload bits between these two constraint classes.

---

## 1. Introduction

CRSCE compresses data by partitioning an input bitstream into fixed-size blocks, loading each block
into a square binary matrix (the Cross-Sum Matrix, or CSM), computing a set of projection vectors and
hash digests that are smaller than the original matrix, and storing these projections as the compressed
payload (Caldwell, 2026). Decompression requires reconstructing the CSM from the stored projections—a
constrained reconstruction problem whose difficulty depends on the information content of the stored
constraints relative to the number of unknown cell values.

At the current operating parameters (matrix dimension $s = 127$, encoding width $b = 7$), the CSM
contains $s^2 = 16{,}129$ binary cells. The stored constraints must carry sufficient information to
determine all 16,129 cell values; any shortfall manifests as an underdetermined system with a
nontrivial null space, requiring exponential-time search over the free variables. The research
program documented in the CRSCE specification (experiments B.1 through B.60) has systematically
explored dozens of constraint configurations, solver architectures, and format variations. This paper
distills the quantitative findings into a unified information-theoretic framework.

## 2. Constraint Taxonomy

The CRSCE format supports three classes of stored constraints, each with distinct algebraic properties
and payload costs.

### 2.1 Integer Cross-Sum Families

A cross-sum family partitions the $s^2$ cells of the CSM into lines and stores the exact sum of cell
values along each line. The four geometric families—row sums (LSM), column sums (VSM), diagonal sums
(DSM), and anti-diagonal sums (XSM)—arise from the natural axes of the matrix (Caldwell, 2026,
Section 2.2). Pseudorandom lookup-table partitions (yLTP) provide additional families by assigning
cells to lines via Fisher-Yates shuffles seeded by fixed constants (Caldwell, 2026, Section B.20).
Variable-length lookup-table partitions (rLTP) use spatially structured assignments with non-uniform
line lengths (Caldwell, 2026, Section B.47).

Each cross-sum family with $s$ uniform-length lines requires $s \times b$ payload bits, where
$b = \lceil \log_2(s+1) \rceil$. At $s = 127$ and $b = 7$, this is 889 bits per family. The
standard diagonal and anti-diagonal families have $2s - 1$ lines of variable length, requiring
variable-width encoding at a cost of $B_d(s) = 1{,}531$ bits—72% more than a uniform family. Toroidal
variants, in which diagonal indices are computed modulo $s$, eliminate the variable-length structure:
all $s$ toroidal diagonals have length $s$, reducing the encoding cost to $s \times b = 889$ bits and
matching the uniform families exactly.

Each integer cross-sum provides a cardinality constraint: the sum of the binary variables on a line
equals a known integer $k$. Over GF(2), this reduces to a single parity equation (the least
significant bit of $k$). Over the integers, the constraint is substantially stronger: it eliminates
all binary assignments whose Hamming weight differs from $k$. For a line of length $n$ at 50% density
($k \approx n/2$), the cardinality constraint provides approximately $n - \log_2 \binom{n}{k} \approx
1$ bit of information—essentially just the parity (Cover & Thomas, 2006, Chapter 11). At lower
densities, the information content grows: a line of length 127 with sum 10 provides approximately
$127 - \log_2 \binom{127}{10} \approx 75$ bits of information.

### 2.2 CRC-32 Hash Digests

CRC-32 computes a 32-bit polynomial remainder of the input treated as an element of GF(2)[x] modulo
the CRC-32 generator polynomial (International Organization for Standardization, 1993). Unlike
cryptographic hash functions such as SHA-1 (National Institute of Standards and Technology, 2015),
CRC-32 is a linear function over GF(2): each of the 32 output bits is a linear combination of the
input bits. This linearity means that a CRC-32 digest stored in the payload provides 32 independent
GF(2) equations that can be incorporated directly into the constraint matrix, without requiring the
underlying data to be fully assigned first (Caldwell, 2026, Section B.58.2).

This property eliminates the row-completion barrier identified in experiment B.54: the DFS solver and
all tested alternatives (SAT, ILP, SMT) could only evaluate SHA-1 hashes after a row was fully
assigned ($u = 0$), creating a circular dependency in which the hash information was needed to
determine the row but was unavailable until the row was already determined (Caldwell, 2026, Section
B.54.11). With CRC-32, the 32 GF(2) equations participate in Gaussian elimination from the start,
alongside cross-sum parity equations.

A CRC-32 digest can be computed per row (lateral hash, LH), per column (vertical hash, VH), or per
toroidal diagonal (diagonal hash, DH). Each axis provides $s \times 32$ payload bits and
approximately $s \times 32 \times 0.992 \approx s \times 31.7$ independent GF(2) equations.

### 2.3 SHA-256 Block Hash

A single SHA-256 digest (256 bits) of the entire CSM provides a non-linear verification constraint.
Because SHA-256 is not algebraically tractable, it cannot be incorporated into the GF(2) system and
serves only as a post-reconstruction integrity check (National Institute of Standards and Technology,
2015). Its payload cost is fixed at 256 bits regardless of $s$.

## 3. Information Efficiency

The central metric for comparing constraint types is the number of independent GF(2) equations
contributed per payload bit. This quantity, denoted $\eta$, governs the information density achievable
within a given compression budget.

Experiment B.58a measured the GF(2) rank of the combined constraint system at $s = 127$: 5,037
independent equations from 5,078 total (1,014 cross-sum parity equations plus 4,064 CRC-32 LH
equations). The CRC-32 contribution was 4,032 of 4,064 possible—a 99.2% independence rate (Caldwell,
2026, Section B.58a).

| Constraint type | Payload (bits) | GF(2) eqs | $\eta$ (eq/bit) | Integer constraints |
|----------------|---------------|-----------|-----------------|-------------------|
| Cross-sum (uniform, $s$ lines) | 889 | 126 | 0.142 | 127 cardinality |
| Cross-sum (standard diagonal) | 1,531 | 251 | 0.164 | 253 cardinality |
| yLTP (uniform, $s$ lines) | 889 | 126 | 0.142 | 127 cardinality |
| CRC-32 per axis ($s$ digests) | 4,064 | 4,031 | 0.992 | 0 |

Cross-sums and yLTPs are identically efficient in GF(2) terms at $\eta = 0.14$. CRC-32 is 7$\times$
more efficient at $\eta = 0.99$. However, this comparison is misleading in isolation because it
ignores the integer cardinality constraints, whose value is not captured by the GF(2) analysis.

## 4. The Two-Phase Solver Architecture

Experiment B.58b measured the contributions of each combinator in the algebraic solver pipeline at
$s = 127$. The results revealed a stark division of labor:

- **IntBound** (integer cardinality reasoning): 3,596 of 3,600 total determined cells (99.9%).
- **GaussElim** (GF(2) Gaussian elimination): 4 cells (0.1%).
- **CrossDeduce** (pairwise intersection reasoning): 0 cells.

IntBound is 4,500$\times$ more productive per constraint than GaussElim during the propagation phase.
However, IntBound saturates after determining approximately 3,600 cells (22.3% of the matrix),
leaving 12,529 free variables that only GF(2) reasoning can address (Caldwell, 2026, Section B.58b).

This saturation is structural, not an artifact of insufficient constraint families. Experiment B.57b
tested 144 yLTP seed pairs and found that all produced exactly 3,600 $\pm$ 51 forced cells.
Experiment B.45 tested configurations with up to 12 LTP families and observed depth unchanged at
96,673. Experiment B.27 added LTP5 and LTP6 at $s = 511$ with zero effect on depth. The propagation
ceiling is invariant to family count, family type, and seed choice—it is a property of the constraint
geometry at the given matrix dimension and input density (Caldwell, 2026, Sections B.27, B.45, B.57b).

These findings establish a two-phase architecture:

1. **Propagation phase.** Integer cardinality constraints drive IntBound to determine approximately
   3,600 cells. The minimum number of integer families required to reach this ceiling is an empirical
   question (likely 3–4 families), but additional families beyond the minimum provide zero marginal
   benefit.

2. **Residual phase.** The remaining approximately 12,500 free variables must be resolved by GF(2)
   Gaussian elimination or residual search. CRC-32 equations are the dominant information source in
   this phase. The null-space dimension after the fixpoint determines the difficulty of residual
   resolution.

## 5. The Density–Compression Pareto Barrier

Define the GF(2) information density as $\delta = r / s^2$, where $r$ is the GF(2) rank of the full
constraint system. Define the compression ratio as $C_r = 1 - P / s^2$, where $P$ is the total
payload in bits. Both quantities depend on $s$ and the choice of stored constraints.

CRC-32 equations scale as $O(s)$ per axis (each axis stores $s$ digests of 32 bits). Cell count
scales as $O(s^2)$. Therefore:

$$
\delta \approx \frac{c_{\text{geo}} + 32 k s}{s^2} = \frac{c_{\text{geo}}}{s^2} + \frac{32k}{s}
$$

where $k$ is the number of CRC-32 axes and $c_{\text{geo}}$ is the contribution from cross-sum
parity. The CRC-32 term decays as $O(1/s)$: larger matrices dilute the CRC information. Simultaneously:

$$
C_r \approx 1 - \frac{n_{\text{int}} \cdot sb + 32ks + c_0}{s^2}
$$

where $n_{\text{int}}$ is the number of integer families and $c_0$ is the fixed overhead. As $s$
increases, $C_r$ approaches 1 (better compression) but $\delta$ approaches 0 (sparser constraints).
The two quantities are inversely related and cannot both be made large simultaneously.

Exhaustive enumeration of all configurations at $C_r = 30\%$ confirms this barrier:

| Configuration | $s$ at $C_r = 30\%$ | Density | Null-space |
|--------------|---------------------|---------|-----------|
| 2 integer + 2 CRC | 124 | 0.515 | 7,135 |
| 3 integer + 2 CRC | 124 | 0.536 | 7,135 |
| 4 integer + 3 CRC | 184 | 0.539 | 15,604 |
| 4 integer + 4 CRC | 229 | 0.572 | 22,453 |

Adding more CRC axes forces a larger $s$ to maintain 30% compression. The larger $s$ introduces more
cells than the additional CRC equations can constrain, yielding a larger absolute null-space despite
the higher density fraction. No configuration achieves both $C_r \geq 30\%$ and $\delta \geq 0.80$
(Caldwell, 2026, Section B.60 analysis).

## 6. Toroidal Geometry

Standard diagonal and anti-diagonal families at dimension $s$ produce $2s - 1$ lines of variable
length (1 through $s$ and back), requiring variable-width encoding and creating short lines that waste
CRC-32 payload bits. A diagonal of length 1 (a corner cell) costs 32 bits of CRC-32 storage for a
single GF(2) equation—on a cell already fully determined by its cross-sum.

Toroidal diagonals, defined by the index $(c - r) \bmod s$, partition the matrix into exactly $s$
lines of uniform length $s$. This eliminates both the variable-width encoding overhead (saving 642
bits per family at $s = 127$) and the short-line CRC waste (saving 4,032 bits for the diagonal CRC
axis at $s = 127$). Every CRC-32 digest on a toroidal diagonal produces the full 32 independent GF(2)
equations, with no overconstrained short lines reducing the effective equation count.

The toroidal geometry was used in early CRSCE experiments as toroidal-slope partitions before being
replaced by LTP partitions in experiment B.20 (Caldwell, 2026, Section B.20). Its reintroduction in
the context of CRC-32 diagonal hashing represents a different application: the toroidal structure is
not used for its constraint-propagation properties (which were found inferior to LTP in the DFS
solver) but for its encoding efficiency in the algebraic solver, where uniform line length maximizes
the GF(2) equations per payload bit.

## 7. Optimal Payload Allocation

Given the two-phase architecture and the density–compression barrier, the optimal payload allocation
follows directly:

1. **Allocate the minimum integer families needed to reach the propagation ceiling.** Experiments
   suggest 3–4 families suffice. Each costs $s \times b$ bits and contributes $s$ integer cardinality
   constraints that IntBound exploits during propagation. Additional families beyond the minimum are
   wasteful: they cost 889 bits each for zero marginal propagation benefit.

2. **Allocate all remaining payload budget to CRC-32 axes.** Each axis costs $32s$ bits and
   contributes $\sim 32s$ independent GF(2) equations—7$\times$ more efficient per bit than a
   cross-sum family. The CRC-32 equations are useless during propagation (they provide no integer
   information) but are the only tool for resolving the residual null-space.

3. **Use toroidal geometry for any CRC-hashed diagonal or anti-diagonal axis.** This avoids the
   variable-width encoding penalty and the short-line CRC waste, maximizing the GF(2) equations per
   payload bit on diagonal axes.

4. **Never replace an integer family with its corresponding CRC hash.** The integer constraint and
   the CRC hash serve non-competing phases. Removing the integer constraint destroys propagation
   capability; the CRC hash cannot compensate because it provides no integer information.

At $s = 127$ and $b = 7$, the configuration "3 integer families + 2 CRC-32 axes" yields $C_r =
31.4\%$, $\delta = 0.523$, and 381 integer constraint lines. Whether 3 integer families (rather than
the 6 used in experiment B.57) suffice to reach the 3,600-cell propagation ceiling remains an
empirical question that requires direct measurement.

## 8. Conclusion

The CRSCE constraint system exhibits a fundamental density–compression Pareto barrier: CRC-32
equations scale linearly with matrix dimension while cell count scales quadratically, making high
information density and high compression mutually exclusive at any single matrix dimension. Within
this barrier, the optimal strategy is a two-phase architecture that maximizes integer constraints for
the propagation phase and CRC-32 equations for the residual phase. The toroidal diagonal geometry
provides the most efficient encoding for CRC-hashed diagonal axes. These findings constrain the design
space for future CRSCE format revisions and establish that no purely algebraic solver can achieve full
reconstruction at compression ratios above approximately 10% without supplementary search over the
residual null-space.

---

## References

Caldwell, S. (2026). CRSCE decompression using a constraint-based graph-theoretical approach.
Champlain College, CMIT-450: Senior Seminar.

Cover, T. M., & Thomas, J. A. (2006). *Elements of information theory* (2nd ed.). Wiley-Interscience.

International Organization for Standardization. (1993). *Information technology—Telecommunications
and information exchange between systems—High-level data link control (HDLC) procedures—Frame
structure* (ISO/IEC 3309:1993). ISO.

National Institute of Standards and Technology. (2015). *Secure hash standard (SHS)* (FIPS PUB
180-4). U.S. Department of Commerce.
