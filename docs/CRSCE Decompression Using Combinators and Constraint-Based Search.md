---
title: "CRSCE Decompression Using Combinators and Constraint-Based Search"
author: "Samuel Caldwell"
affiliation: "Champlain College"
course: "CMIT-450 : Senior Seminar"
advisor: "Dr. Kenneth Revett"
date: "5 April 2026"
---

## Abstract

Cross-Sums Compression and Expansion (CRSCE) is a research compression format that replaces a fixed-size binary matrix with a set of exact-sum projections, cyclic redundancy check (CRC) hash digests, and a disambiguation index. Unlike classical compression algorithms that exploit statistical redundancy in the source, CRSCE encodes data by substituting the original content with constraint equations from which the original can, in principle, be reconstructed. Decompression is therefore a constraint satisfaction problem: recover a binary matrix consistent with all stored projections and hashes. This paper presents the theory and experimental evolution of CRSCE, with particular focus on the combinator-algebraic solver architecture that emerged from experiment series B.58 through B.63. The combinator solver replaces the original depth-first search (DFS) approach with a purely algebraic fixpoint pipeline: BuildSystem followed by iterated GF(2) Gaussian elimination, integer-bound cardinality forcing, and pairwise cross-deduction. At matrix dimension $s = 127$, this pipeline determines 13.9% of cells algebraically on 50% density data, which a hybrid Phase II cell-by-cell DFS with random restarts extends to 96.4%. The paper also reports the B.62 load-bearing constraint analysis, which demonstrates that five of six integer constraint families are redundant when CRC hash equations are present, and that only diagonal cross-sums (DSM) and variable-length rLTP CRC hashes are irreplaceable. The best configuration achieves a compression ratio of $C_r = 88.8\%$ with 96.4% cell determination on 50% density blocks at $s = 127$, leaving a structural residual of 583 cells (3.6%) that resists all tested search strategies. These results establish CRSCE as a viable, though incomplete, framework for constraint-based compression of binary data, and identify the information-theoretic barriers to full decompression.

## 1. Introduction

The digital compression landscape is dominated by algorithms that exploit statistical redundancy in source data. Huffman coding (Huffman, 1952), Lempel-Ziv variants (Ziv & Lempel, 1977, 1978), and arithmetic coding (Rissanen, 1976; Witten, Neal, & Cleary, 1987) all achieve compression by identifying patterns, repeated sequences, or non-uniform symbol distributions and encoding them more efficiently. These approaches are provably optimal for sources with exploitable statistical structure, and Shannon's source coding theorem (Shannon, 1948) establishes the theoretical limit: a source with entropy $H$ bits per symbol cannot be losslessly compressed below $H$ bits per symbol on average.

CRSCE (Cross-Sums Compression and Expansion) explores a fundamentally different compression paradigm. Rather than encoding the data more efficiently, CRSCE replaces the data entirely with a set of mathematical constraints from which the original can be reconstructed. The compressed representation consists of exact-sum projections across multiple axes of a binary matrix, CRC hash digests that provide algebraic equations over GF(2), and a small disambiguation index. The key distinction is that CRSCE's compression mechanism does not depend on statistical properties of the source: the same compression ratio is achieved regardless of whether the input is a natural language document, a compressed video stream, or encrypted random data.

This property makes CRSCE theoretically interesting because classical compression fails on high-entropy data. An encrypted file, for instance, has near-maximal entropy and is incompressible by Huffman, LZ77, or arithmetic coding. If CRSCE could fully decompress arbitrary data, it would represent a compression mechanism that operates orthogonally to the Shannon limit --- not by violating it, but by exploiting a different information-theoretic pathway.

The central challenge, however, is decompression. Given a set of cross-sum projections and hash digests, the decompressor must reconstruct the original binary matrix. This is a constraint satisfaction problem (CSP) over hundreds of thousands of Boolean variables with thousands of cardinality constraints and linear equations over GF(2). The research program documented in this paper spans over sixty experiments (B.1 through B.63) that progressively develop, test, and refine solver architectures for this reconstruction problem. The journey proceeds from DFS-based backtracking search to purely algebraic combinator pipelines, ultimately arriving at a hybrid architecture that combines algebraic fixpoint computation with randomized search.

The paper is organized as follows. Section 2 reviews the background literature on constraint satisfaction, GF(2) linear algebra, belief propagation, SAT solving, and information theory that underpins CRSCE. Section 3 formalizes CRSCE theory, including the cross-sum matrix model, the eight constraint families, hash verification, and the compression ratio formula. Section 4 traces the format evolution from the original $s = 511$ design through the transition to $s = 127$ and the experiments at $s = 191$. Section 5 contrasts CRSCE with classical compression. Section 6 presents the combinator-algebraic solver in detail. Section 7 describes the variable-length rLTP and interior cascade trigger breakthrough. Section 8 reports the load-bearing constraint analysis. Section 9 details the hybrid solver architecture and the random restart breakthrough. Section 10 develops the unified information budget. Section 11 analyzes collision resistance at both matrix dimensions. Section 12 discusses open questions and future directions. Section 13 concludes.

## 2. Background and Related Work

### 2.1 Constraint Satisfaction Problems

A constraint satisfaction problem (CSP) consists of a set of variables, each with a finite domain, and a set of constraints that restrict which combinations of variable values are permitted (Russell & Norvig, 2010). CRSCE's decompression problem is a CSP where the variables are the $s^2$ cells of the binary matrix (each with domain $\{0, 1\}$), and the constraints are exact cardinality requirements on groups of cells (cross-sum projections) together with hash equality constraints. The general CSP is NP-complete, but practical instances often exhibit structure that enables efficient solving through constraint propagation, backtracking search, and heuristic guidance (Dechter, 2003).

Arc consistency, a fundamental concept in CSP solving, requires that for every value in a variable's domain, there exists a consistent value in every related variable's domain. Maintaining arc consistency during search prunes the search space significantly (Mackworth, 1977). In CRSCE, the analogous concept is IntBound forcing: when a constraint line's residual $\rho$ equals 0, all remaining unknowns must be 0, and when $\rho$ equals the unknown count $u$, all must be 1. This is a specialized form of arc consistency for cardinality constraints.

### 2.2 GF(2) Gaussian Elimination

Gaussian elimination over the Galois field of two elements, GF(2), reduces a system of linear equations modulo 2 to row echelon form (Golub & Van Loan, 2013). Each equation is a XOR of variable subsets equaling a target bit. The rank of the system determines how many variables can be expressed as functions of free variables. CRSCE exploits GF(2) structure because CRC hash functions are linear over GF(2): the CRC-32 of a message $m$ can be expressed as $\text{CRC32}(m) = G \cdot m \oplus c$, where $G$ is a generator matrix and $c$ is a constant absorbing initialization and finalization (Peterson & Brown, 1961; Rivest, 1992).

The linearity of CRC over GF(2) is the foundational insight of the combinator solver. Unlike SHA-1 (a cryptographic hash with no exploitable algebraic structure), CRC-32 provides 32 linear equations per constraint line that can be incorporated into a global GF(2) constraint matrix before any cell is determined. This eliminates the row-completion barrier that prevented all prior solver architectures from exploiting hash information during search.

### 2.3 Belief Propagation

Belief propagation (BP), introduced by Pearl (1988), is a message-passing algorithm for probabilistic inference on graphical models. On trees, BP computes exact marginal distributions; on loopy graphs, it provides approximate marginals that are often useful for guiding search heuristics (Yedidia, Weiss, & Freeman, 2003). The CRSCE decompression problem can be represented as a factor graph with variable nodes for each cell and factor nodes for each constraint, making BP a natural candidate for guiding branching decisions.

In practice, the cardinality constraints in CRSCE are hard factors (zero probability when violated), which limits BP's applicability. The hash factors are extremely sharp: for a fully specified row, either the digest matches or it does not. Early CRSCE experiments (B.12) explored survey propagation and BP-guided decimation but found that the hard constraint structure led to convergence failures on loopy factor graphs of this density. The combinator solver's deterministic algebraic approach proved more effective than probabilistic message passing for this problem domain.

### 2.4 CDCL SAT Solvers

Conflict-Driven Clause Learning (CDCL) is the dominant paradigm in modern Boolean satisfiability (SAT) solving, building on the foundational DPLL algorithm (Davis, Logemann, & Loveland, 1962) with three key innovations: non-chronological backjumping, clause learning from conflict analysis, and restart strategies (Marques-Silva & Sakallah, 1999; Moskewicz, Madigan, Zhao, Zhang, & Malik, 2001). CDCL solvers routinely handle instances with millions of variables when the constraint structure admits short conflict clauses that effectively prune the search space.

The CRSCE research program tested CDCL in two contexts. B.21 attempted to bit-blast CRC equations into conjunctive normal form (CNF), producing millions of clauses that overwhelmed the solver. B.63f implemented CDCL on the integer constraint system (cardinality bounds), where conflict analysis identifies the cell assignments that collectively violated a cardinality constraint. Both attempts failed for the same root cause: cardinality constraint antecedents are inherently $O(s)$ literals. When IntBound forces cell $c$ to 0 via $\rho = 0$ on line $L$, the reason is all approximately 64 one-valued cells on $L$ at 50% density. First-UIP resolution replaces one large antecedent with another, producing learned clauses of 50 to 100 literals that provide negligible pruning. This is a known limitation of standard CDCL on cardinality constraints; pseudo-Boolean reasoning or cutting-plane methods (Chai & Kuehlmann, 2005) would be needed for effective learning, representing a fundamentally different solver architecture.

### 2.5 LDPC Codes and Capacity-Approaching Codes

Low-density parity-check (LDPC) codes, introduced by Gallager (1962) and revived by MacKay and Neal (1996), achieve near-Shannon-limit performance on noisy channels through iterative decoding on sparse factor graphs. Richardson and Urbanke (2001) proved that properly designed LDPC code ensembles approach channel capacity. The connection to CRSCE is structural: both involve recovering unknown bits from parity-check equations (GF(2) constraints) using iterative message passing. The CRSCE combinator solver's GaussElim + IntBound + CrossDeduce fixpoint is analogous to iterative LDPC decoding, though operating on exact cardinality constraints rather than probabilistic channel observations.

A critical difference is that LDPC decoding operates at rates below channel capacity, where the system is information-theoretically solvable. CRSCE at 50% density operates near the boundary: the payload carries fewer bits than the CSM's entropy, and the solver must recover the deficit through constraint structure amplification. The 583-cell residual at $s = 127$ suggests that the current constraint system falls slightly short of the information-theoretic threshold for full recovery at the tested compression ratio.

### 2.6 Information Theory and the Shannon Limit

Shannon's source coding theorem (Shannon, 1948) establishes that a discrete memoryless source with entropy $H$ can be losslessly compressed to an average rate of $H$ bits per symbol, and no lower. For a uniform binary source (each bit independently and uniformly distributed), the entropy is 1 bit per symbol, and compression is impossible. A 50% density $127 \times 127$ binary matrix has near-maximal entropy: $H \approx 16{,}129$ bits.

The CRSCE payload at $C_r = 88.8\%$ contains 14,325 bits --- 1,804 bits fewer than the Shannon limit. This deficit is consistent with the observation that the combinator determines only 13.9% of cells algebraically. The remaining 82.5% is recovered by search (exploiting constraint structure), and the final 3.6% remains undetermined. The constraint structure provides an information amplification factor of approximately 1.085, meaning the solver extracts 8.5% more information than the raw bit count of the stored payload. This amplification arises from cross-constraint interactions: each cell participates in four or more constraint lines, creating algebraic dependencies that the fixpoint exploits.

### 2.7 CRC Hash Properties

Cyclic Redundancy Check (CRC) functions compute the remainder of polynomial division over GF(2). CRC-32, standardized as ISO 3309/ITU-T V.42, uses generator polynomial $g(z) = z^{32} + z^{26} + z^{23} + z^{22} + z^{16} + z^{12} + z^{11} + z^{10} + z^8 + z^7 + z^5 + z^4 + z^2 + z + 1$ (hexadecimal: 0x04C11DB7 in normal form). The linearity property $\text{CRC32}(a \oplus b) = \text{CRC32}(a) \oplus \text{CRC32}(b)$ (modulo affine constants from initialization and finalization) is the algebraic foundation of the combinator solver (Rivest, 1992; Koopman & Chakravarty, 2004).

CRC-32 provides 32-bit error detection with a Hamming distance of at least 4 for messages up to 32,767 bits, meaning it detects all single-bit, double-bit, and triple-bit errors, as well as all burst errors up to 32 bits. For CRSCE, CRC-32's collision resistance is modest ($2^{-32}$ per line), but the aggregate collision resistance across hundreds of lines combined with SHA-256 block hash verification provides sufficient integrity for non-adversarial applications.

## 3. CRSCE Theory

### 3.1 The Cross-Sum Matrix

In the CRSCE scheme, input data is partitioned into fixed-size blocks and loaded into a binary Cross-Sum Matrix (CSM). Let $s$ denote the matrix dimension. The CSM is defined as:

$$
CSM = (CSM_{r,c}) \in \{0, 1\}^{s \times s}, \quad r, c \in \{0, \ldots, s-1\}
$$

Input bits are mapped into the CSM in a deterministic row-major order. Each block contains $s^2$ bits of input data. The matrix dimension $s$ is a format parameter; the original specification uses $s = 511$, while the current research focus is $s = 127$.

### 3.2 The Eight Constraint Families

Four cross-sum projection vectors are computed from the CSM, each partitioning the $s^2$ cells into lines and computing the exact Hamming weight (sum of ones) along each line.

**Row sums (LSM).** For each row $r$:

$$
\sum_{c=0}^{s-1} CSM_{r,c} = \text{LSM}[r]
$$

**Column sums (VSM).** For each column $c$:

$$
\sum_{r=0}^{s-1} CSM_{r,c} = \text{VSM}[c]
$$

**Diagonal sums (DSM).** For diagonal index $d \in \{0, \ldots, 2s-2\}$, the diagonal consists of all cells $(r, c)$ satisfying $c - r = d - (s-1)$:

$$
\sum_{\substack{(r,c) : c - r = d-(s-1) \\ 0 \leq r,c \leq s-1}} CSM_{r,c} = \text{DSM}[d]
$$

The diagonal length is $\text{len}(d) = \min(d+1, s, 2s-1-d)$, ranging from 1 at the corners to $s$ at the main diagonal.

**Anti-diagonal sums (XSM).** For anti-diagonal index $x \in \{0, \ldots, 2s-2\}$, the anti-diagonal consists of all cells $(r, c)$ satisfying $r + c = x$:

$$
\sum_{\substack{(r,c) : r + c = x \\ 0 \leq r,c \leq s-1}} CSM_{r,c} = \text{XSM}[x]
$$

At the original dimension $s = 511$, four additional pseudorandom lookup-table partitions (LTP1 through LTP4) provide supplementary constraint lines. Each LTP sub-table assigns every cell to one of $s$ lines via a Fisher-Yates shuffle seeded by a 64-bit linear congruential generator (LCG) constant (Knuth, 1997). At the research dimension $s = 127$, the LTP sub-tables are replaced by variable-length rLTP (center-outward spiral) partitions with graduated CRC hashing.

Each cell participates in exactly one line per constraint family. Any valid cross-sum partition must satisfy three axioms: conservation (all partitions sum to the matrix Hamming weight), uniqueness (no two lines cover the identical cell set), and non-repetition (each line references each cell at most once).

### 3.3 Lateral Hashes and Block Hash

**Lateral Hash (LH).** An independent hash digest is computed for each row. At $s = 511$, LH uses SHA-1 (160 bits per row) as specified by FIPS 180-4 (NIST, 2015). At $s = 127$, LH uses CRC-32 (32 bits per row), which provides algebraic equations over GF(2) in addition to verification capability.

**Block Hash (BH).** A single SHA-256 digest (256 bits) of the entire CSM serialized in row-major order provides final verification. SHA-256 is retained at all matrix dimensions for its adversarial collision resistance of $2^{128}$ under birthday-bound assumptions (NIST, 2015; Dang, 2012).

**Disambiguation Index (DI).** An 8-bit unsigned integer selecting the intended reconstruction from the canonical lexicographic enumeration of feasible solutions. The DI ensures deterministic decompression even in the pathological case of multiple feasible reconstructions.

### 3.4 Compression Ratio

LSM and VSM each contain $s$ elements encoded in $b = \lceil \log_2(s+1) \rceil$ bits. DSM and XSM each contain $2s - 1$ elements with variable-width encoding, where element $k$ requires $\lceil \log_2(\text{len}(k)+1) \rceil$ bits. The total bits for one diagonal family is:

$$
B_d(s) = \lceil \log_2(s+1) \rceil + 2 \sum_{l=1}^{s-1} \lceil \log_2(l+1) \rceil
$$

The per-block compression ratio for the original $s = 511$ format with SHA-1 lateral hashes is:

$$
C_r = 1 - \frac{6sb + 2 B_d(s) + 160s + 256 + 8}{s^2}
$$

where $s = 511$, $b = 9$, and $B_d(511) = 8{,}185$. Substituting:

$$
C_r = 1 - \frac{27{,}594 + 16{,}370 + 81{,}760 + 256 + 8}{261{,}121} = 1 - \frac{125{,}988}{261{,}121} \approx 0.5175
$$

This yields approximately 51.8% compression per block. For any input, regardless of its statistical properties, CRSCE at $s = 511$ produces a compressed file that is 48.2% smaller than the original.

At $s = 127$ with CRC-32 lateral hashes and the B.63c constraint configuration (LSM, VSM, DSM, LH, VH, DH64, XH64, rLTP, BH, DI), the payload is 14,325 bits against $127^2 = 16{,}129$ input bits, yielding $C_r = 88.8\%$ --- a compression savings of 11.2%.

### 3.5 Decompression as Constraint Satisfaction

Decompression seeks a CSM satisfying all stored constraints. The feasible set may be large under cross-sum constraints alone, but the hash constraints dramatically reduce it. Under standard cryptographic assumptions, treating SHA-256 as a random oracle, the combined collision probability at $s = 511$ is approximately $10^{-30{,}000}$. The DI mechanism provides deterministic correctness regardless of collision probability: even if multiple feasible matrices exist, the DI selects the intended one from the canonical lexicographic enumeration.

The compressor discovers the DI by running the decompressor as an enumerator on the original CSM. If the original is the first feasible solution (DI = 0), the cost is minimal. Compression is explicitly time-bounded by `max_compression_time`; if DI discovery exceeds this limit, compression fails and the caller must use an alternative algorithm.

## 4. Format Evolution

### 4.1 The Original Format: $s = 511$ with SHA-1 and DFS

The original CRSCE format at $s = 511$ with $b = 9$ encodes each block as 261,121 bits of input data compressed into 125,988 bits of payload (15,749 bytes per block). The solver uses incremental constraint propagation with DFS backtracking in canonical lexicographic order. The per-block payload contains:

| Field | Elements | Bits/Element | Total Bits | Encoding |
|-------|----------|-------------|------------|----------|
| LH | 511 | 160 | 81,760 | SHA-1 per row |
| BH | 1 | 256 | 256 | SHA-256 of full CSM |
| DI | 1 | 8 | 8 | uint8 |
| LSM | 511 | 9 | 4,599 | MSB-first packed |
| VSM | 511 | 9 | 4,599 | MSB-first packed |
| DSM | 1,021 | variable | 8,185 | Variable-width packed |
| XSM | 1,021 | variable | 8,185 | Variable-width packed |
| LTP1--LTP4 | 4 x 511 | 9 | 18,396 | MSB-first packed |
| **Total** | | | **125,988** | |

The DFS solver maintains per-line state (unknown count $u$, assigned ones $a$, residual $\rho = S - a$) and applies two propagation rules: if $\rho = 0$, all unknowns on the line are forced to 0; if $\rho = u$, all unknowns are forced to 1. Each cell assignment updates exactly eight lines (one per constraint family). SHA-1 row verification fires when a row becomes fully assigned ($u_{\text{row}} = 0$); a mismatch triggers backtracking.

The research program (B.1 through B.52) extensively optimized this architecture. The best LTP table configuration (B.38e) reached a DFS depth of 96,672 cells out of 261,121 (37%) on a representative MP4 test block. Numerous alternative strategies were tested: adaptive lookahead (B.8), randomized restarts (B.11), survey propagation (B.12), portfolio solving (B.13), nogood recording (B.14), SAT/CDCL encoding (B.21, B.54), ILP and LP relaxation (B.54), SMT (B.54), and direct depth hill-climbing on LTP tables (B.52b). All converged on the same depth ceiling.

The root cause, established algebraically by B.39 using GF(2) Gaussian elimination, is that the cross-sum constraint system at $s = 511$ has only 2% constraint density: 5,097 independent equations over 261,121 cells, leaving a null space of 256,024 dimensions. No solver architecture can navigate this space efficiently because the stored constraints carry insufficient information to determine the matrix.

### 4.2 The Transition to $s = 127$ with CRC-32

Experiment B.57 proposed reducing the matrix dimension from $s = 511$ to $s = 127$. This changes the problem fundamentally:

| Parameter | $s = 511$ | $s = 127$ | Improvement |
|-----------|-----------|-----------|-------------|
| Cells per block | 261,121 | 16,129 | 16.2x fewer |
| Constraint density | 2.0% | 6.2% | 3.1x denser |
| Null-space dimension (est.) | 256,024 | ~14,872 | 17.2x smaller |
| Bits per cross-sum ($b$) | 9 | 7 | 22% smaller |
| Search space | $2^{256{,}024}$ | $2^{14{,}872}$ | Astronomically smaller |

Most critically, B.57 replaced SHA-1 lateral hashes with CRC-32. SHA-1 is a cryptographic hash with no useful algebraic structure --- it serves only as a pass/fail verification oracle after a row is fully assigned. CRC-32, by contrast, is a linear function over GF(2):

$$
\text{CRC32}(a \oplus b) = \text{CRC32}(a) \oplus \text{CRC32}(b)
$$

This linearity means that each CRC-32 row digest provides 32 GF(2) equations that can be incorporated into the constraint system before any cell is determined. The row-completion barrier that stymied all prior solver architectures is eliminated: the hash information is available from the start, not after completing a 127-cell or 511-cell row.

B.58 exploited this insight to build the combinator-algebraic solver, which replaces DFS entirely with a fixpoint of GF(2) Gaussian elimination, integer-bound forcing, and pairwise cross-deduction. The combinator determines cells algebraically from the combined cross-sum + CRC-32 system, with no branching, no backtracking, and no sequential row processing. B.60 extended the approach with vertical CRC-32 hashes (VH, one per column) and diagonal/anti-diagonal CRC hashes (DH, XH), achieving the first fully algebraic block reconstructions on favorable-density data.

### 4.3 Experiments at $s = 191$

Experiment B.62 investigated $s = 191$ ($b = 8$), motivated by the observation that the payload scales as $O(s)$ while input scales as $O(s^2)$, leaving more headroom for additional constraint families at larger $s$. At $s = 127$, the B.60r configuration consumed 87.0% of the input; at $s = 191$, the same configuration consumes only 56%, leaving 44% headroom for additional CRC hash families.

The B.62 experiments yielded three important results. First, the baseline at $s = 191$ did not improve over $s = 127$: the 50% density wall persisted at 2,112 cells (5.8%) across all constraint configurations tested (B.62a through B.62c). Second, variable-length rLTP with graduated CRC hashing (B.62d) broke the 50% density floor for the first time, raising it from 2,112 to 2,706 cells (+28%). Third, two orthogonal rLTP spirals (B.62f) achieved the first 100% algebraic determination of an $s = 191$ block (block 1, BH-verified), though at an expansion ratio of $C_r = 115.6\%$.

### 4.4 Why $s = 127$ Is the Current Research Focus

Despite the larger headroom at $s = 191$, the research program returned to $s = 127$ for B.63 because the combinator solver is proportionally stronger at smaller $s$ (the equation-to-variable ratio $32/s$ is higher), rows are shorter (making per-row DFS tractable), and the DFS solver's depth ceiling is higher relative to the matrix size. At $s = 127$, the B.63 hybrid architecture (combinator algebra + cell-by-cell DFS with random restarts) reached 96.4% determination on 50% density blocks --- a result not achievable at $s = 191$ with the same architecture.

## 5. CRSCE vs Classical Compression

### 5.1 Fundamental Architectural Difference

Classical compression algorithms --- Huffman coding (Huffman, 1952), LZ77 (Ziv & Lempel, 1977), LZ78 (Ziv & Lempel, 1978), arithmetic coding (Rissanen, 1976), and their modern variants (deflate, zstd, brotli) --- operate by identifying and exploiting statistical redundancy in the source. Repeated patterns are replaced with shorter references; non-uniform symbol distributions are encoded with variable-length codes. The compression ratio depends on the source's entropy: low-entropy sources (natural language, structured data) compress well; high-entropy sources (encrypted or already-compressed data) compress poorly or not at all.

CRSCE operates on a fundamentally different principle. It does not analyze the source for patterns or statistical properties. Instead, it replaces the source data with a set of mathematical constraints --- cross-sum projections across multiple axes of a binary matrix, CRC hash digests, and a disambiguation index --- from which the original data can (in principle) be reconstructed. The compression ratio is fixed by the format parameters ($s$, $b$, the number and type of constraint families, hash widths) and is independent of the source's statistical properties. A random bitstream and a highly structured document achieve the same $C_r$.

### 5.2 The Potential and the Limitation

This source-independence is both CRSCE's most intriguing property and its greatest challenge. It implies the possibility of compressing data that is incompressible by classical methods --- encrypted files, random data, already-compressed archives. If CRSCE could fully decompress arbitrary data, it would provide a compression mechanism orthogonal to the Shannon limit.

However, this potential has not been realized for 50% density data (the worst case for binary matrices, corresponding to maximum entropy). At $s = 127$ with $C_r = 88.8\%$, the payload carries 14,325 bits against the CSM's entropy of 16,129 bits --- a deficit of 1,804 bits. The solver recovers most of this deficit through constraint structure amplification (the fixpoint extracts more information than the raw bit count through cross-constraint interactions), but 583 cells (3.6%) remain undetermined. Shannon's theorem is not violated: the payload does not carry enough information to uniquely determine the CSM, and the solver cannot close the gap through search alone.

At $s = 511$, the situation is worse: the DFS solver stalls at 37% of the matrix, meaning 63% of cells are never determined. The constraint density is too low for any solver architecture to navigate the exponential search space.

For favorable data (low density or structured blocks), CRSCE achieves complete decompression. At $s = 127$, 5 of 1,331 blocks from an MP4 test file were fully solved by the combinator alone, and at $s = 191$, one block achieved 100% BH-verified algebraic determination. These favorable blocks have density below 20%, where integer constraints carry more information per line and the IntBound cascade can propagate from short-line seeds to full matrix determination.

### 5.3 Honest Assessment

CRSCE is research, not a finished product. It demonstrates a novel compression paradigm with properties that classical methods cannot match (source-independent compression ratio), but it cannot yet fully decompress all blocks. The 50% density wall is a structural barrier related to the information content of the constraint system, not a solver deficiency. Closing the 583-cell residual at $s = 127$ would require either more efficient constraint encoding (lower $C_r$, leaving less compression savings) or a qualitatively new constraint mechanism that provides information beyond cardinality and GF(2) linearity.

## 6. The Combinator-Algebraic Solver

### 6.1 Architecture Overview

The combinator solver, introduced in B.58, replaces the DFS-based reconstruction with a purely algebraic pipeline of composable constraint-reduction operators. Each combinator is a function $\mathcal{C}: \mathcal{S} \to \mathcal{S}$ that produces a strictly more determined (or equally determined) system. The solver is their composition:

$$
\text{solve} = \text{VerifyBH} \circ \text{EnumerateFree} \circ \text{Fixpoint}(\text{GaussElim}, \text{IntBound}, \text{CrossDeduce}) \circ \text{BuildSystem}
$$

The solver operates on a constraint system state $\mathcal{S} = (G, b_G, A, b_A, D, F, \text{val}, \text{cell\_lines})$ where $G \in \text{GF}(2)^{m \times n}$ is the GF(2) constraint matrix, $b_G \in \text{GF}(2)^m$ is the target vector, $A$ is the integer constraint structure (per-line target sums, membership sets, residuals $\rho$, and unknown counts $u$), $D$ is the set of determined cells with known values, and $F$ is the set of undetermined (free) cells. The cell-to-constraint index $\text{cell\_lines}[j]$ maps each cell $j$ to all constraint lines containing it, enabling efficient propagation.

Unlike the DFS solver, which processes cells sequentially in row-major order and maintains an explicit backtracking stack, the combinator solver operates on all $s^2$ cells simultaneously. There is no concept of "depth," "progress through rows," or "meeting band." All constraints --- across all rows, all columns, all diagonals, all anti-diagonals --- are active from the first operation.

### 6.2 BuildSystem: Constructing the Constraint Matrix

The BuildSystem combinator assembles the full GF(2) constraint matrix and integer constraint structures from the compressed payload. At $s = 127$ with the B.63c configuration, the system comprises:

**Cross-sum parity equations (760 rows).** For each of the 760 geometric constraint lines (127 rows + 127 columns + 253 diagonals + 253 anti-diagonals), a GF(2) equation with all-ones coefficients at the cells on that line, with the target bit equal to the cross-sum modulo 2.

**CRC-32 LH equations (4,064 rows).** For each row $r$, the stored CRC-32 digest $h_r$ yields 32 GF(2) equations:

$$
G_{\text{CRC}} \cdot \mathbf{x}_r = h_r \oplus \mathbf{c}
$$

where $G_{\text{CRC}} \in \text{GF}(2)^{32 \times 127}$ is the CRC-32 generator matrix and $\mathbf{c} = \text{CRC32}(\mathbf{0}_{128})$ is the affine constant absorbing CRC-32 initialization and finalization.

**CRC-32 VH equations (4,064 rows).** Identical structure for column CRC-32 digests, with variables $\{x_{0,c}, x_{1,c}, \ldots, x_{126,c}\}$ for each column $c$. The cross-axis structure is critical: LH for row $r$ constrains row variables $\{x_{r,0}, \ldots, x_{r,126}\}$, while VH for column $c$ constrains column variables $\{x_{0,c}, \ldots, x_{126,c}\}$. They share exactly one variable ($x_{r,c}$) per (row, column) pair, enabling cross-axis cascade interactions.

**DH and XH equations.** CRC equations for the 64 shortest diagonals and 64 shortest anti-diagonals, with graduated widths: CRC-8 for lines of length 1--8, CRC-16 for 9--16, CRC-32 for 17--64.

**Integer constraint lines.** For each geometric constraint line, the target sum, residual $\rho$, and unknown count $u$ are maintained. XSM integer constraints are excluded (redundant per B.62j).

The CRC-32 generator matrix $G_{\text{CRC}}$ is constructed by computing the CRC-32 of each unit vector and XORing with the all-zeros CRC to isolate the linear component:

```cpp
for (uint16_t col = 0; col < len; ++col) {
    std::array<uint8_t, 32> msg{};
    msg[col / 8] = static_cast<uint8_t>(1U << (7U - (col % 8U)));
    const auto crcUnit = crc32_ieee(msg.data(), totalBytes);
    const auto colVal = crcUnit ^ c0;
    for (uint8_t bit = 0; bit < 32; ++bit) {
        gm[bit][col] = static_cast<uint8_t>((colVal >> bit) & 1U);
    }
}
```

This produces a $32 \times s$ binary matrix where each row gives one GF(2) equation in the $s$ cell variables of a constraint line.

### 6.3 GaussElim: GF(2) Gaussian Elimination

The GaussElim combinator performs Gaussian elimination over GF(2) on the full constraint matrix, producing reduced row echelon form (RREF). The implementation uses packed 64-bit word arrays for each row, with $\lceil s^2 / 64 \rceil$ words per row, enabling efficient XOR operations during elimination.

The algorithm iterates over columns (cell indices) in order. For each undetermined cell, it searches for a pivot row with a 1 in that column, swaps it to the current pivot position, and XORs the pivot row into all other rows that have a 1 in that column. After RREF, a pivot row with no free-variable dependencies determines its pivot cell uniquely: the cell's value equals the target bit for that equation.

The rank of the system determines the number of algebraically constrained degrees of freedom. At $s = 127$ with LH + VH + cross-sum parity, the measured rank is 7,793 out of 16,129 variables. Adding DH and XH equations raises the effective rank further, reducing the null space and increasing the number of determined cells.

An important optimization exploits the block-diagonal structure of CRC equations: LH equations for different rows share no variables, and VH equations for different columns share no variables. Each block of 32 equations on 127 variables can be reduced independently, then the pivots substituted into the global cross-sum parity equations. This three-stage approach is mathematically equivalent to full GaussElim on the combined matrix but exploits sparsity for significant speedup.

### 6.4 IntBound: Integer Cardinality Forcing

The IntBound combinator exploits the full integer value of each cross-sum, not just its parity. For each constraint line $L$:

- If $\rho(L) = 0$: all remaining unknown cells on $L$ must be 0.
- If $\rho(L) = u(L)$: all remaining unknown cells on $L$ must be 1.
- Otherwise: no forcing is possible.

The implementation uses a worklist-driven propagation loop:

```cpp
while (head < queue.size()) {
    const auto li = queue[head++];
    auto &line = intLines_[li];
    if (line.u == 0) continue;
    if (line.rho < 0 || line.rho > line.u) return inconsistent;

    int8_t fv = -1;
    if (line.rho == 0) fv = 0;
    else if (line.rho == line.u) fv = 1;
    if (fv < 0) continue;

    for (const auto flat : line.cells) {
        if (cellState_[flat] >= 0) continue;
        assignCell(flat, static_cast<uint8_t>(fv));
        // Re-enqueue all constraint lines through this cell
        for (const auto li2 : cellToLines_[flat]) {
            if (!inQueue[li2]) { queue.push_back(li2); inQueue[li2] = true; }
        }
    }
}
```

When a cell is forced, all constraint lines through that cell are re-enqueued for checking, because the updated $\rho$ and $u$ may trigger forcing on adjacent lines. The propagation continues until quiescence or infeasibility.

At 50% density, IntBound contributes zero cells because every line of length $l$ has $\rho \approx l/2$, deep in the forcing dead zone. IntBound's value emerges on favorable (low-density) blocks where short lines at the matrix corners have extreme sum ratios, providing seed assignments that cascade through longer lines.

### 6.5 CrossDeduce: Pairwise Cross-Line Deduction

The CrossDeduce combinator performs singleton arc consistency: for each undetermined cell, it tests whether assigning 0 would violate any constraint line ($\rho' < 0$ or $\rho' > u'$) and whether assigning 1 would violate any constraint line. If exactly one value is feasible, the cell is forced:

```cpp
for (uint32_t flat = 0; flat < kN; ++flat) {
    if (cellState_[flat] >= 0) continue;
    int8_t forced = -1;
    for (uint8_t v = 0; v <= 1; ++v) {
        bool feasible = true;
        for (const auto li : cellToLines_[flat]) {
            auto newRho = intLines_[li].rho - v;
            auto newU = intLines_[li].u - 1;
            if (newRho < 0 || newRho > newU) { feasible = false; break; }
        }
        if (!feasible) {
            if (forced >= 0) return inconsistent; // both infeasible
            forced = static_cast<int8_t>(1 - v);
        }
    }
    if (forced >= 0) assignCell(flat, forced);
}
```

This is the most fine-grained algebraic operation, examining every undetermined cell against all its participating constraint lines. A cell that cannot be 0 (because some line would become infeasible) must be 1, and vice versa. If both values are infeasible, the system is inconsistent.

### 6.6 The Fixpoint Loop

The three combinators execute in a loop until convergence:

1. Run GaussElim: extract determined cells from RREF, propagate values into the GF(2) system.
2. Run IntBound: force cells on lines with $\rho = 0$ or $\rho = u$, propagate into GF(2) system.
3. Run CrossDeduce: test each undetermined cell for singleton arc consistency, propagate.
4. If no new cells were determined, or all cells are determined, terminate.

Each iteration is monotone: the set of determined cells grows or stays the same. GaussElim pivots from iteration $k$ may enable IntBound forcings in iteration $k+1$ (by reducing $u$ on constraint lines), which in turn may create new GaussElim pivots in iteration $k+2$ (by substituting newly known cell values into GF(2) equations). This cascade between algebraic and integer reasoning is the mechanism that drives determination beyond what either system achieves alone.

## 7. Variable-Length rLTP and Interior Cascade Triggers

### 7.1 The 50% Density Floor

The combinator fixpoint at 50% density hits a structural floor: IntBound contributes zero cells (every line has $\rho \approx u/2$), and GaussElim determines only cells on short diagonal/anti-diagonal lines where the CRC width equals or exceeds the line length. At $s = 127$ with the B.63a A2 configuration, this floor is 2,112 cells (13.1%) --- corresponding to the shortest diagonals fully determined by their CRC equations.

All cells determined at 50% density come from GaussElim operating on CRC equations. The short diagonal lines (length 1--32) have CRC widths equal to or exceeding their length, meaning the GF(2) system has full rank on those lines and can determine every cell. Longer lines (33--127 cells) have CRC-32 providing only 32 equations on 33 or more variables --- insufficient for complete determination.

### 7.2 The B.62d Breakthrough: Interior Cascade Triggers

The original constraint geometry places short lines only at the matrix edges: DSM and XSM diagonals of length 1--8 occupy the four corners. The interior of the matrix has no short lines, and at 50% density, all interior lines are deep in the IntBound dead zone.

Experiment B.62d introduced variable-length rLTP: a center-outward spiral partition where line $k$ has $k+1$ cells. Starting from the matrix center (row 95, column 95 at $s = 191$), the spiral assigns cells to progressively longer lines. The first 32 lines (lengths 1--32) cover 528 cells in the matrix interior, all fully determined by graduated CRC hashing:

| Line lengths | CRC width | Determines? |
|-------------|-----------|-------------|
| 1--8 | CRC-8 | Fully (8 equations on 8 or fewer cells) |
| 9--16 | CRC-16 | Fully (16 equations on 16 or fewer cells) |
| 17--32 | CRC-32 | Fully (32 equations on 32 or fewer cells) |
| 33+ | CRC-32 | Partial (32 equations on 33+ cells) |

These are interior cascade triggers: the first time short-line GaussElim determinations occur in the matrix interior rather than at the edges. B.62d raised the 50% density floor from 2,112 to 2,706 cells (+28%), the first experiment in the entire B.60--B.62 series to improve determination at 50% density.

### 7.3 The B.62f Full Solve: Two Orthogonal rLTPs

Experiment B.62f added a second rLTP spiral centered at (0,0), orthogonal to the first spiral at (95,95). Cells sharing a line in rLTP1 are on different lines in rLTP2, creating cross-axis coupling: when GaussElim determines cells via rLTP1's CRC equations, those values propagate into rLTP2's CRC equations, potentially creating new pivots. On block 1 (favorable density at $s = 191$), this coupling cascaded to complete matrix determination:

- Phase 1 (diagonal lengths 1--8, CRC-8): 10,934 cells (30.0%)
- Phase 2 (diagonal lengths 9--16, CRC-16): 11,697 cells (32.1%)
- Phase 3 (diagonal lengths 17--32, CRC-32): 36,481 cells (100.0%)

In the final Phase 3 iteration, GaussElim alone produced 21,997 cells in a single cascade --- the two orthogonal rLTP spirals created mutual information that closed the entire 36,481-cell matrix. This was the first 100% algebraic determination of a block at $s = 191$, verified by SHA-256 block hash.

The 50% density blocks gained 707 cells (from 2,706 to 3,413), reaching 9.4% but not approaching a full solve. The configuration's $C_r = 115.6\%$ represented expansion rather than compression. The practical challenge remained: achieving both compression and full reconstruction on 50% density data.

## 8. Load-Bearing vs Redundant Constraints

### 8.1 The B.62 Ablation Series

The B.62 experiment series (B.62e through B.62l) systematically eliminated individual constraint families to determine which are load-bearing (removal degrades performance) and which are redundant (removal has zero effect). The experiments revealed a clear structural hierarchy.

**B.62e: Drop LH (row CRC-32).** Removal produced identical results on every block --- zero cells lost. LH's GF(2) equations are linearly dependent on the rLTP and DH/XH equations that cover the same cells from different axes.

**B.62g: Drop rLTP cross-sums.** The rLTP integer cardinality constraints (4,082 bits across two sub-tables) were removed while retaining all rLTP CRC hash equations. Every block produced identical results. At 50% density, rLTP IntBound contributes zero cells because $\rho \approx u/2$ on every line regardless of how many rLTP families are present.

**B.62j: Drop XSM cross-sums (keep XH CRC).** Anti-diagonal integer constraints (2,554 bits) were removed. Identical results on every block. This brought $C_r$ below 100% for the first time, achieving the first BH-verified full solve under true compression at $C_r = 97.4\%$.

**B.62k: Drop DSM cross-sums (keep DH CRC).** Diagonal integer constraints (2,554 bits) were removed. Block 1 collapsed from 100% (36,481 cells) to 9.4% (3,413 cells). IntBound on block 1 dropped from 11,589 cells to zero. This was the first family whose removal caused any degradation whatsoever.

**B.62l: Drop DSM, re-add XSM.** To test whether the cascade requires any diagonal-family IntBound or specifically DSM, the experiment swapped DSM and XSM. Block 1 remained at 9.4%. XSM IntBound contributed only 190 cells versus DSM's 11,589 --- a 60-fold deficit. The cascade trigger is geometry-specific, not family-generic.

### 8.2 The Redundancy Principle

The experimental evidence supports a general principle: integer cardinality constraints are redundant when CRC hash equations of sufficient width cover the same constraint lines. GaussElim determines cells individually via linear algebra, independent of line completion status. IntBound requires $\rho = 0$ or $\rho = u$, which occurs only when a line is nearly complete. On any line where CRC provides $w \geq l$ equations (for line length $l$), GaussElim fully determines the line without IntBound assistance.

The exception --- DSM --- violates this principle not because DSM IntBound determines cells that GaussElim cannot, but because DSM IntBound fires earlier in the fixpoint iteration. The short corner diagonals (lengths 1--8 at positions (0,0), (0,190), (190,0), (190,190)) reach $\rho = 0$ or $\rho = l$ on the very first IntBound pass, before GaussElim has processed the rLTP CRC equations that would eventually determine those cells. These early IntBound assignments provide GaussElim with the seed values it needs to begin cascading through the rLTP system. Without DSM's early seeds, GaussElim's initial pass produces fewer pivots, the cascade stalls, and the fixpoint converges at 9.4% instead of 100%.

This is a temporal dependency, not an informational one. DSM IntBound does not provide information that GaussElim lacks; it provides information sooner, enabling a cascade that GaussElim alone cannot initiate.

### 8.3 The Minimum Compressing Configuration

The ablation analysis yields a concrete minimum configuration for full-solve compression at $s = 191$:

| Component | Bits | Status |
|-----------|------|--------|
| DSM cross-sums | 2,554 | Load-bearing (cascade trigger) |
| VH + DH + XH (CRC) | 12,000 | Retained (GaussElim rank) |
| 2x rLTP CRC-32 | 17,664 | Load-bearing (non-substitutable GF(2) rank) |
| BH + DI | 264 | Required (verification + disambiguation) |
| LSM + VSM | 3,056 | Potentially droppable (0 IntBound on block 1) |
| **Total** | **35,538** | **$C_r = 97.4\%$** |

This configuration demonstrates that compression and complete reconstruction can coexist. The load-bearing constraints (DSM sums + rLTP CRC) together cost 20,218 bits, establishing a hard floor for any algebraic-only solver at this matrix dimension.

## 9. The Hybrid Solver Architecture

### 9.1 Motivation for the Hybrid

The combinator fixpoint at $s = 127$ determines 2,248 cells (13.9%) on 50% density blocks. This is the algebraic floor: no additional fixpoint iterations produce new determinations. However, 86.1% of cells remain undetermined, and the combinator provides no mechanism to extend beyond this floor without search.

The B.63 experiment series at $s = 127$ developed a hybrid architecture that combines Phase I combinator algebra with Phase II search. Five search strategies were tested: multi-axis line DFS (B.63c), cell-by-cell DFS with residual-guided ordering (B.63d), random restarts (B.63e), CDCL (B.63f), and beam search (B.63g). The shared constraint configuration across all B.63 experiments is LSM + VSM + DSM + LH + VH + DH64 + XH64 + rLTP (lines 1--16) + BH/DI, at $C_r = 88.8\%$.

### 9.2 Multi-Axis Cycling (B.63c)

The first hybrid approach cycled through five phases: Phase I (combinator algebra), Phase II (row DFS with LH CRC-32 verification), Phase III (column DFS with VH CRC-32 verification), Phase IV (diagonal DFS with DH CRC verification), and Phase V (anti-diagonal DFS with XH CRC verification). After any phase made progress, the solver returned to Phase I for combinator re-invocation.

On block 5 (50% density), Phase V solved 2 anti-diagonal lines, gaining 157 cells (14.9% total). This was the first progress beyond the algebraic floor on a 50% density block. However, the solver stalled after one round: the 157 new cells did not reduce unknown counts on other axes enough to open new solvable lines. The cross-axis coupling coefficient was too low --- 2 solved anti-diagonals with 157 cells spread across 127 rows averages approximately 1.2 cells per row, while rows need approximately 20 additional determined cells each to become DFS-tractable.

### 9.3 Cell-by-Cell DFS with Residual Guidance (B.63d)

B.63d changed the search granularity from line-completion to cell-assignment. Instead of solving a 110-unknown row in one DFS, it assigns one cell at a time with IntBound propagation after each assignment and LH CRC-32 verification at row boundaries. The search uses residual-guided cell ordering (most-constrained-first), selecting the cell whose participating lines have the smallest unknown counts.

Starting from 2,405 cells (14.9%), the cell-by-cell DFS reached 3,682 cells (22.8%) in the first 500 decisions, then plateaued. 19.3 million subsequent decisions produced zero progress beyond depth 662. This is the same constraint-exhaustion wall observed in the production DFS at $s = 511$: all remaining lines have $\rho$ well within the interior of $[0, u]$, and no single cell assignment tips any line into forcing.

### 9.4 The Random Restart Breakthrough (B.63e)

B.63e tested whether the 22.8% plateau was ordering-dependent or structural by running the cell-by-cell DFS from the combinator base with different random cell orderings. The results were transformative:

| Strategy | Restarts | Peak determination |
|----------|----------|-------------------|
| Most-constrained-first (MCF) | 1 | 13,044 (80.9%) |
| Random orderings | 100 | mean 14,889 (92.3%), max 15,340 (95.1%) |
| Random orderings | 1,000 | max 15,453 (95.8%) |
| Random orderings | 10,001 | max 15,546 (96.4%) |

The plateau is ordering-dependent: different cell orderings produce dramatically different propagation chains. MCF is the worst ordering tested because it commits to locally-optimal decisions that are globally suboptimal, creating early conflicts that require extensive backtracking (4,542 backtracks vs approximately 2,000 for random orderings). Random orderings distribute decisions across the matrix, producing more effective early cascades.

Each restart makes approximately 8,800 decisions, but IntBound propagation determines approximately 280,000 cells --- a 32-fold amplification. The DFS assigns a cell, propagation forces hundreds more, creating a virtuous cycle. The mechanism is inherently stochastic: different orderings reach different propagation chains, and the best ordering among many restarts reaches deeper than any single fixed ordering.

The 10,001-restart census (B.63h) confirmed diminishing returns: each 10x increase in restarts gains approximately 100 fewer cells. The distribution is stable with mean 92.2% across all runs. The last 583 cells are a structural residual that resists all orderings.

### 9.5 Why CDCL Fails on Cardinality Constraints (B.63f)

B.63f implemented CDCL on the IntBound constraint system, with conflict analysis identifying cell assignments that collectively violated cardinality constraints. Two implementations were tested: coarse analysis (collecting all assigned cells on the conflicting line) and first-UIP resolution (tracing propagated cells to decision antecedents).

Both produced zero learned clauses. The root cause is that cardinality constraint antecedents are inherently $O(s)$ literals. When IntBound forces cell $c$ to 0 via $\rho = 0$ on line $L$, the reason is all approximately 64 one-valued cells on $L$ at 50% density. First-UIP resolution replaces one large antecedent with another from a different line. The resulting clause contains 50--100 literals, exceeding any practical size limit for effective pruning.

This is a known limitation of standard CDCL on cardinality constraints (Chai & Kuehlmann, 2005). Boolean clauses typically have 2--3 literals, so resolution produces small learned clauses. Cardinality constraints involve $O(s)$ cells per line, so learned clauses are $O(s)$. Pseudo-Boolean (PB) reasoning or cutting-plane proof systems operate directly on linear inequalities, potentially learning effective constraints from cardinality violations, but these represent a fundamentally different solver architecture not yet implemented for CRSCE.

### 9.6 Why Beam Search Fails (B.63g)

B.63g maintained 1,000 parallel search states, expanding the most constrained cell at each step and keeping the top-$K$ states ranked by total determined cells. The beam collapsed: by step 500, all 1,000 states had converged to the same determination level. The peak was identical to single-path DFS (22.8%).

IntBound propagation is deterministic: given the same cell assignment, propagation produces the same cascade in all states. The single-cell branch difference between states is overwhelmed by the subsequent propagation cascade, which determines dozens of additional cells identically in both branches. By the next branching step, the two branches have been homogenized by propagation into nearly identical states.

The contrast with random restarts is illuminating. Restarts succeed because each uses a different cell ordering, producing different propagation chains from the very first decision (macroscopic diversity). Beam search provides only microscopic diversity (single-cell branches within a fixed ordering), which propagation erases.

## 10. The Information Budget

### 10.1 Per-Component Accounting

Experiment B.63i performed a unified accounting of the 14,325-bit payload at $C_r = 88.8\%$. The per-component efficiency, measured in determined cells per payload bit on block 5 (50% density) and block 1 (favorable density):

| Component | Bits | Cells/bit (50%) | Cells/bit (favorable) | Role |
|-----------|------|-----------------|----------------------|------|
| DH + XH | ~2,432 | 0.928 | 4.976 | Primary algebraic engine |
| rLTP CRC | 192 | 1.526 | 0 | Interior cascade triggers |
| VH | 4,064 | 0.039 | 1.953 | Column GaussElim axis |
| LH | 4,064 | 0.039 | 1.002 | Row GaussElim + Phase II verification |
| DSM | 1,531 | load-bearing | load-bearing | IntBound cascade trigger |
| BH | 256 | 0 (verification) | 0 (verification) | Block-level pass/fail |
| DI | 8 | 0 | 0 | Disambiguation |

DH/XH is the most efficient algebraic component at 0.928 cells/bit on 50% density data and 4.976 cells/bit on favorable blocks. The rLTP CRC, despite costing only 192 bits, provides 1.526 cells/bit --- the highest marginal return at 50% density. LH and VH each cost 4,064 bits but provide only 0.039 cells/bit on 50% density; their value emerges on favorable blocks, where they provide cross-axis GaussElim equations that complete the cascade.

### 10.2 Functional Decomposition

The payload decomposes into four functional categories:

| Role | Bits | % of payload | Contribution |
|------|------|-------------|-------------|
| Algebraic determination (DH+XH+rLTP) | ~2,624 | 18.3% | Phase I: 2,248--2,405 cells (13.9%--14.9%) |
| Search pruning (DSM+LSM+VSM IntBound) | ~3,309 | 23.1% | DFS cascade (14.9% to 96.4%) |
| Dual algebraic + verification (LH+VH) | ~8,128 | 56.7% | GaussElim on favorable blocks; CRC-32 row/col verification |
| Verification only (BH) | 256 | 1.8% | Block-level pass/fail |
| Overhead (DI) | 8 | 0.1% | Disambiguation |

The majority of the payload (56.7%) serves a dual role: LH and VH provide GF(2) equations for GaussElim (algebraic determination on favorable blocks) and CRC-32 verification for the DFS search (pruning wrong paths on all blocks). These bits cannot be cleanly classified as purely algebraic or purely verification --- they serve both functions simultaneously.

### 10.3 Shannon Comparison

At 50% density, the CSM has maximal binary entropy: $H = 16{,}129$ bits. The payload carries 14,325 bits --- an 1,804-bit deficit from the Shannon limit. Yet the solver determines up to 96.4% of cells (15,546 bits of information recovered from 14,325 stored bits).

Information amplification: $15{,}546 / 14{,}325 = 1.085\times$. The constraint structure amplifies stored information by 8.5% through cross-constraint interactions. The decomposition:

$$
16{,}129 = \underbrace{14{,}325}_{\text{stored}} + \underbrace{N_{\text{amplified}}}_{\text{algebra}} + \underbrace{N_{\text{searched}}}_{\text{DFS}} - \underbrace{N_{\text{redundant}}}_{\text{wasted}} - \underbrace{583}_{\text{residual}}
$$

The 583-cell residual represents information that cannot be recovered by either algebra or search at this payload size. These cells are on constraint lines where $\rho$ is deeply interior to $[0, u]$ regardless of ordering. The residual is not axis-specific --- it spans all four geometric axes and the rLTP lines uniformly.

## 11. Collision Resistance Analysis

### 11.1 $s = 511$ with SHA-1 Per Row

At $s = 511$, each of the 511 rows has an independent SHA-1 digest (160 bits). Under the random oracle model, the probability that an alternative matrix matches all 511 row hashes simultaneously is:

$$
P(\text{LH collision}) = \left(\frac{1}{2^{160}}\right)^{511} = 2^{-81{,}760}
$$

Converting to base 10: $\log_{10} P(\text{LH collision}) = -81{,}760 \times \log_{10} 2 \approx -24{,}614$.

The block hash (SHA-256, 256 bits) provides an additional independent barrier:

$$
P(\text{BH collision}) = 2^{-256} \approx 10^{-77}
$$

The cross-sum collision probability, considering only row sums under the uniform model, is:

$$
P(\text{LSM collision}) = \left[\frac{\binom{2s}{s}}{4^s}\right]^s \approx 10^{-819}
$$

The combined collision probability, treating these as independent events (a conservative assumption):

$$
P_{\text{CRSCE}} \leq P(\text{LSM}) \times P(\text{LH}) \times P(\text{BH}) \approx 10^{-819} \times 10^{-24{,}614} \times 10^{-77} \approx 10^{-25{,}510}
$$

This probability is smaller than the estimated number of atoms in the observable universe ($\sim 10^{80}$) by a factor exceeding $10^{25{,}000}$.

SHA-1's collision resistance is broken at approximately $2^{63}$ work (Stevens et al., 2017), but the SHA-256 block hash neutralizes this: constructing two matrices that collide on all 511 SHA-1 row hashes AND the SHA-256 block hash requires at minimum $2^{128}$ work (the birthday bound on SHA-256). The DI mechanism provides deterministic correctness regardless of collision probability.

### 11.2 $s = 127$ with CRC-32 Per Row

At $s = 127$, CRC-32 replaces SHA-1 for lateral hashes. Each row has a 32-bit CRC-32 digest. The probability that all 127 rows collide simultaneously under the random oracle model:

$$
P(\text{LH collision at } s=127) = \left(\frac{1}{2^{32}}\right)^{127} = 2^{-4{,}064}
$$

With the B.63c configuration adding column CRC-32 (VH), a constraint-preserving swap touching $k$ rows and $j$ columns must evade both axes:

$$
P(\text{LH+VH evasion}) = 2^{-32(k+j)}
$$

The minimum constraint-preserving swap at $s = 127$ is estimated at approximately 250--380 cells touching approximately 10 rows and 120 columns. Joint evasion probability:

$$
P(\text{LH+VH evasion}) = 2^{-32 \times (10 + 120)} = 2^{-4{,}160}
$$

Combined with the SHA-256 block hash:

$$
P_{\text{CRSCE at } s=127} = \min(2^{-4{,}160}, 2^{-256}) = 2^{-256}
$$

The block hash dominates. Collision resistance at $s = 127$ with CRC-32 lateral hashes is $2^{-256}$ --- the birthday bound of SHA-256. This is sufficient for all practical, non-adversarial purposes.

### 11.3 Birthday Attack Considerations

Under birthday attack assumptions, an adversary computing $q$ candidate matrices faces a collision probability of approximately $q^2 / 2^{n+1}$ for an $n$-bit hash. For SHA-256 ($n = 256$), achieving collision probability 0.5 requires $q \approx 2^{128}$ evaluations.

For the CRC-32 lateral hashes alone (without BH), the multi-row birthday bound requires finding a single alternative matrix that matches all 127 row CRCs. This is a multi-target preimage problem with effective security $2^{32 \times 127} = 2^{4{,}064}$, requiring $q \approx 2^{2{,}032}$ candidates --- computationally infeasible by any known or foreseeable means.

The aggregate collision resistance of the CRSCE system at $s = 127$ is therefore determined by the SHA-256 block hash: $2^{-256}$ against adversarial attack, or $2^{-4{,}064}$ against the lateral hash system alone. Both exceed the security requirements of any practical application.

## 12. Open Questions and Future Directions

### 12.1 The 583-Cell Structural Residual

The most pressing open question is the 583-cell residual that resists all 10,001 random restarts at $s = 127$ (B.63h). These cells are on constraint lines where $\rho$ is deeply interior to $[0, u]$ regardless of the search path. The residual is not axis-specific: it spans all geometric axes and rLTP lines uniformly. Closing it requires either:

1. **Higher $C_r$**: adding more constraint information (more rLTP lines, wider CRC coverage, additional hash axes) at the cost of reduced compression savings.
2. **More efficient constraint encoding**: finding constraint types that provide more information per payload bit than current CRC-on-cardinality approaches.
3. **A qualitatively different search mechanism**: techniques that do not rely on IntBound forcing, such as pseudo-Boolean reasoning, SAT-based approaches with effective clause learning on cardinality constraints, or heuristic methods that exploit structure beyond single-line feasibility.

### 12.2 Pseudo-Boolean Reasoning

Standard CDCL fails on CRSCE because cardinality constraint antecedents produce learned clauses of $O(s)$ literals. Pseudo-Boolean (PB) reasoning operates directly on linear inequalities over integer variables, using cutting-plane proof rules instead of resolution. A PB solver could learn constraints like "the sum of these 10 cells must be at least 3" rather than enumerating all forbidden combinations. Whether PB solvers can effectively handle the specific structure of CRSCE's constraint system --- 500+ cardinality constraints on overlapping 127-cell lines --- is an open empirical question that represents the most promising avenue for closing the residual gap.

### 12.3 Better Cell Ordering Heuristics

B.63e demonstrated that cell ordering dramatically affects determination: MCF achieves 80.9% while the best random ordering achieves 96.4%. This 15.5 percentage point gap suggests that a well-designed ordering heuristic could push beyond 96.4%. Candidates include:

- **VSIDS-style activity scoring**: prioritize cells that frequently appear in recent constraint violations, analogous to the activity heuristics used in CDCL solvers (Moskewicz et al., 2001).
- **Look-ahead ordering**: select the cell whose assignment produces the largest propagation cascade, measured by a short look-ahead computation.
- **Machine-learned orderings**: train a model on the constraint graph structure and historical propagation data to predict which cell orderings lead to deeper determination.

### 12.4 Alternative Matrix Dimensions

The research has tested $s = 127$, $s = 191$, and $s = 511$. The equation-to-variable ratio $32/s$ favors smaller $s$, but smaller $s$ also reduces the compression savings. An optimal $s$ may exist that maximizes the product of compression savings and solve rate. Candidates include $s = 64$ (where the equation density doubles relative to $s = 127$ but the compression savings are minimal), $s = 89$ or $s = 97$ (primes that may have favorable algebraic properties), and $s = 255$ (which matches common byte boundaries).

### 12.5 Block-Adaptive Compression

Not all blocks have the same density. An MP4 file contains header blocks (low density, easily solvable) and payload blocks (near-50% density, difficult). A block-adaptive compressor could apply CRSCE to favorable blocks and fall back to classical compression (or store uncompressed) for unfavorable blocks. The file format already supports this via the `max_compression_time` failure mechanism, but a more sophisticated strategy could select the constraint configuration per block based on a quick density probe, optimizing the aggregate compression ratio across the entire file.

### 12.6 The Encrypted Data Compression Question

CRSCE's source-independent compression ratio raises the theoretical question of whether encrypted data could be compressed. Encrypted data has near-maximal entropy, and Shannon's theorem says it cannot be compressed on average. CRSCE does not violate Shannon's theorem: the 50% density wall means that CRSCE cannot fully decompress maximum-entropy data at any $C_r < 100\%$.

However, if the 583-cell residual could be closed at $C_r < 100\%$, it would demonstrate a compression mechanism that achieves a fixed compression ratio on data that classical methods cannot compress at all. Whether this is achievable remains the central open question of CRSCE research. The information budget analysis (Section 10) suggests that the constraint system amplifies stored information by 8.5%, but 3.6% of the matrix remains beyond the amplification's reach. Bridging this gap may require fundamentally new constraint types that exploit structure beyond GF(2) linearity and integer cardinality.

## 13. Conclusion

This paper has presented the theory, implementation, and experimental evolution of CRSCE decompression, tracing the research trajectory from DFS-based backtracking search at $s = 511$ through the combinator-algebraic revolution at $s = 127$ to the hybrid combinator + random restart architecture that represents the current state of the art.

The principal contributions of the CRSCE research program are:

**The combinator-algebraic solver.** By exploiting CRC-32's linearity over GF(2), the combinator replaces sequential DFS with a global algebraic fixpoint: BuildSystem, followed by iterated GaussElim, IntBound, and CrossDeduce. This solver operates on all cells simultaneously, with no concept of row ordering, meeting bands, or depth. It determines 13.9% of cells purely algebraically on 50% density data at $s = 127$.

**Variable-length rLTP with interior cascade triggers.** Center-outward spiral partitions with graduated CRC hashing (CRC-8/16/32 matching line length) place short, fully-determined lines in the matrix interior, breaking the 50% density floor for the first time. Two orthogonal rLTP spirals achieved the first 100% algebraic solve at $s = 191$ (block 1, BH-verified).

**The load-bearing constraint analysis.** The B.62 ablation series demonstrated that five of six integer constraint families are redundant when CRC hash equations are present. Only DSM diagonal cross-sums are load-bearing, due to a temporal dependency: their short-line IntBound forcings bootstrap the GaussElim cascade before the algebraic system can self-start. This finding refines the design space for CRSCE payload optimization.

**The random restart breakthrough.** Cell-by-cell DFS with IntBound propagation and random cell orderings extends determination from 14.9% to 96.4% on 50% density data at $s = 127$. The plateau is ordering-dependent, not structural: different orderings produce different propagation chains, and the best among 10,001 trials reaches 96.4% (583 cells remaining). CDCL fails because cardinality antecedents are too large for effective clause learning. Beam search fails because IntBound propagation homogenizes parallel states.

**The information budget.** DH/XH CRC is the most efficient algebraic component (0.93 cells/bit at 50% density). rLTP CRC provides the highest marginal return at 50% density (1.53 cells/bit for 192 bits). 56.7% of the payload serves dual algebraic + verification roles (LH + VH). The constraint system amplifies stored information by 8.5% beyond the raw payload bit count.

The best configuration at $s = 127$ achieves $C_r = 88.8\%$ with 96.4% determination on 50% density blocks. A structural residual of 583 cells (3.6%) resists all tested search strategies. Closing this residual --- and thereby achieving complete decompression at sub-100% compression ratio on maximum-entropy data --- remains the central open problem.

CRSCE demonstrates that constraint-based compression is a viable paradigm with properties that classical methods cannot match: a source-independent compression ratio that does not depend on statistical redundancy. Whether this paradigm can cross the threshold from 96.4% to 100% determination on arbitrary data is a question that lies at the intersection of information theory, constraint satisfaction, and algebraic combinatorics. The tools developed in this research program --- the combinator fixpoint, the load-bearing constraint analysis, the random restart architecture, and the information budget framework --- provide a foundation for continued investigation.

## References

Chai, D., & Kuehlmann, A. (2005). A fast pseudo-Boolean constraint solver. *IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems, 24*(2), 305--317.

Dang, Q. H. (2012). *Recommendation for applications using approved hash algorithms* (NIST Special Publication 800-107 Revision 1). National Institute of Standards and Technology.

Davis, M., Logemann, G., & Loveland, D. (1962). A machine program for theorem-proving. *Communications of the ACM, 5*(7), 394--397.

Dechter, R. (2003). *Constraint processing*. Morgan Kaufmann.

Gallager, R. G. (1962). Low-density parity-check codes. *IRE Transactions on Information Theory, 8*(1), 21--28.

Golub, G. H., & Van Loan, C. F. (2013). *Matrix computations* (4th ed.). Johns Hopkins University Press.

Huffman, D. A. (1952). A method for the construction of minimum-redundancy codes. *Proceedings of the IRE, 40*(9), 1098--1101.

Jerrum, M., Sinclair, A., & Vigoda, E. (2004). A polynomial-time approximation algorithm for the permanent of a matrix with nonnegative entries. *Journal of the ACM, 51*(4), 671--697.

Knuth, D. E. (1997). *The art of computer programming, Volume 2: Seminumerical algorithms* (3rd ed.). Addison-Wesley.

Knuth, D. E. (2005). *The art of computer programming, Volume 4A: Combinatorial algorithms, Part 1*. Addison-Wesley.

Kobler, J., Biere, A., Heule, M. J. H., van Maaren, H., & Walsh, T. (Eds.). (2025). *Handbook of satisfiability* (2nd ed.). IOS Press.

Koopman, P., & Chakravarty, T. (2004). Cyclic redundancy code (CRC) polynomial selection for embedded networks. *Proceedings of the International Conference on Dependable Systems and Networks*, 145--154.

Kschischang, F. R., Frey, B. J., & Loeliger, H.-A. (2001). Factor graphs and the sum-product algorithm. *IEEE Transactions on Information Theory, 47*(2), 498--519.

MacKay, D. J. C., & Neal, R. M. (1996). Near Shannon limit performance of low density parity check codes. *Electronics Letters, 32*(18), 1645--1646.

Mackworth, A. K. (1977). Consistency in networks of relations. *Artificial Intelligence, 8*(1), 99--118.

Marques-Silva, J. P., & Sakallah, K. A. (1999). GRASP: A search algorithm for propositional satisfiability. *IEEE Transactions on Computers, 48*(5), 506--521.

Moskewicz, M. W., Madigan, C. F., Zhao, Y., Zhang, L., & Malik, S. (2001). Chaff: Engineering an efficient SAT solver. *Proceedings of the 38th Annual Design Automation Conference*, 530--535.

National Institute of Standards and Technology. (2015). *Secure hash standard (SHS)* (Federal Information Processing Standard 180-4). U.S. Department of Commerce.

Pearl, J. (1988). *Probabilistic reasoning in intelligent systems: Networks of plausible inference*. Morgan Kaufmann.

Peterson, W. W., & Brown, D. T. (1961). Cyclic codes for error detection. *Proceedings of the IRE, 49*(1), 228--235.

Richardson, T. J., & Urbanke, R. L. (2001). The capacity of low-density parity-check codes under message-passing decoding. *IEEE Transactions on Information Theory, 47*(2), 599--618.

Rissanen, J. J. (1976). Generalized Kraft inequality and arithmetic coding. *IBM Journal of Research and Development, 20*(3), 198--203.

Rivest, R. L. (1992). *The MD5 message-digest algorithm* (RFC 1321). Internet Engineering Task Force.

Russell, S. J., & Norvig, P. (2010). *Artificial intelligence: A modern approach* (3rd ed.). Prentice Hall.

Shannon, C. E. (1948). A mathematical theory of communication. *Bell System Technical Journal, 27*(3), 379--423.

Stevens, M., Bursztein, E., Karpman, P., Albertini, A., & Markov, Y. (2017). The first collision for full SHA-1. *Advances in Cryptology -- CRYPTO 2017*, 570--596.

Valiant, L. G. (1979). The complexity of computing the permanent. *Theoretical Computer Science, 8*(2), 189--201.

Wainwright, M. J., & Jordan, M. I. (2008). Graphical models, exponential families, and variational inference. *Foundations and Trends in Machine Learning, 1*(1--2), 1--305.

Witten, I. H., Neal, R. M., & Cleary, J. G. (1987). Arithmetic coding for data compression. *Communications of the ACM, 30*(6), 520--540.

Yedidia, J. S., Freeman, W. T., & Weiss, Y. (2003). Understanding belief propagation and its generalizations. In *Exploring Artificial Intelligence in the New Millennium* (pp. 239--269). Morgan Kaufmann.

Ziv, J., & Lempel, A. (1977). A universal algorithm for sequential data compression. *IEEE Transactions on Information Theory, 23*(3), 337--343.

Ziv, J., & Lempel, A. (1978). Compression of individual sequences via variable-rate coding. *IEEE Transactions on Information Theory, 24*(5), 530--536.
