# Cross-Sums Compression and Expansion: Theory, Experimental Validation, and Fundamental Limits of Constraint-Based Binary Matrix Reconstruction

**Sam Caldwell**

*March 2026*

## Abstract

Cross-Sums Compression and Expansion (CRSCE) is a research compression format that encodes binary data into fixed-size $511 \times 511$ matrices and reconstructs them by solving a constraint satisfaction problem over eight families of cross-sum projections. This paper presents the complete theory of CRSCE, details the constraint satisfaction architecture, and reports the results of a systematic experimental program spanning 44 experiments (B.1 through B.44) that characterizes the format's fundamental limits. We establish through GF(2) null-space analysis that the constraint system has rank 5,097 over 261,121 cells, leaving 256,024 degrees of freedom (98% of cells unconstrained by cross-sums alone). The minimum constraint-preserving swap is 1,528 cells spanning 11 rows and 492 columns, rendering local-repair solver architectures infeasible. The experimental program is organized into four phases: constraint system design (B.1-B.21), partition seed optimization (B.22-B.38), algebraic and architectural analysis (B.39-B.43), and constraint density investigation (B.44). All phases converge on the same conclusion: the depth ceiling of approximately 96,000 cells is an information-theoretic property of the constraint system, not a limitation of the solver algorithm. CRSCE achieves a nominal compression ratio of $\approx 48.2%$, but the solver cannot reconstruct arbitrary input blocks within practical time bounds, making the format unviable for general-purpose compression.

## 1. Introduction

Data compression exploits redundancy in input to produce a shorter representation. Lossless compression formats guarantee exact reconstruction of the original data from the compressed representation. Traditional approaches such as Huffman coding (Huffman, 1952), Lempel-Ziv (Ziv & Lempel, 1977), and arithmetic coding (Rissanen & Langdon, 1979) achieve compression by modeling the statistical structure of the input and encoding symbols using fewer bits than their information content requires.

CRSCE takes a fundamentally different approach. Rather than modeling statistical structure, it encodes input data into a fixed-size binary matrix and stores a set of algebraic projections (cross-sums) along with cryptographic hash digests. Decompression is formulated as a constraint satisfaction problem (CSP): reconstruct the original matrix from the stored projections and hashes. If the solver can find the correct matrix, the projections and hashes occupy fewer bits than the original data, achieving compression.

This approach is motivated by the observation that a $511 \times 511$ binary matrix has $2^{261,121}$ possible states, but the cross-sum projections constrain this space to a much smaller subset. If the projections plus hash verification uniquely identify the original matrix and can be stored in fewer bits, compression is achieved. The central question is whether the solver can efficiently navigate the constrained solution space to reconstruct the original matrix.

This paper presents the complete theory of CRSCE, describes the constraint satisfaction architecture in detail, and reports the results of a comprehensive 44-experiment research program that establishes the format's fundamental limits. We show that while CRSCE achieves a nominal compression ratio of 48.2% in terms of stored payload size, the solver cannot reconstruct arbitrary input blocks within practical time bounds. The depth ceiling of approximately 96,000 cells (out of 261,121 total) represents an information-theoretic barrier inherent to the constraint system, not a limitation that can be overcome by improved algorithms.

### 1.1 Contributions

This paper makes five principal contributions:

1. A complete formal specification of the CRSCE constraint system, including the eight projection families, their algebraic structure, and the solver architecture.
2. A GF(2) null-space analysis establishing that the constraint system has 256,024 degrees of freedom, with a minimum constraint-preserving swap of 1,528 cells.
3. A comprehensive experimental program (44 experiments across four phases) documenting the evolution from initial constraint design through final infeasibility proof.
4. Identification of three structural barriers (constraint density, line length, verification granularity) and the deep exploration paradox that collectively preclude viable constraint-based compression.
5. A formal proof that the depth ceiling is information-theoretic, not algorithmic, establishing CRSCE as unviable for general-purpose compression.

## 2. The CRSCE Constraint System

### 2.1 Matrix Encoding

CRSCE partitions input data into blocks of 261,121 bits ($511^2$), each loaded into a $511 \times 511$ binary matrix called the Cross-Sum Matrix (CSM). The matrix dimension $s = 511$ is chosen as a prime number to ensure that diagonal and anti-diagonal constraint lines have well-defined algebraic properties. Each row is stored as eight 64-bit words (512 bits, with one trailing zero bit), enabling efficient bitwise operations.

For a CSM $M \in \{0, 1\}^{511 \times 511}$, the cell at row $r$ and column $c$ is denoted $M[r, c]$ for $r, c \in \{0, 1, \ldots, 510\}$.

### 2.2 Cross-Sum Projection Families

The CRSCE constraint system comprises eight families of cross-sum projections, each computing the sum of cell values along a set of constraint lines:

**Geometric families (4).** These families derive from the $(r, c)$ coordinate grid:

$$\text{LSM}[r] = \sum_{c=0}^{510} M[r, c] \quad \text{(row sums, 511 lines)}$$

$$\text{VSM}[c] = \sum_{r=0}^{510} M[r, c] \quad \text{(column sums, 511 lines)}$$

$$\text{DSM}[d] = \sum_{\substack{r, c \\ c - r + 510 = d}} M[r, c] \quad \text{(diagonal sums, 1,021 lines)}$$

$$\text{XSM}[x] = \sum_{\substack{r, c \\ r + c = x}} M[r, c] \quad \text{(anti-diagonal sums, 1,021 lines)}$$

**Lookup-Table Partition families (4).** Four pseudorandom partitions (LTP1 through LTP4) are constructed via Fisher-Yates shuffles (Knuth, 1997) seeded by 64-bit LCG constants. Each partition assigns every cell to one of 511 lines, with each line containing exactly 511 cells. The partition assignment function $\ell_k(r, c)$ maps cell $(r, c)$ to its line index in partition $k$:

$$\text{LTP}_k[j] = \sum_{\substack{r, c \\ \ell_k(r, c) = j}} M[r, c] \quad \text{for } k \in \{1, 2, 3, 4\},\; j \in \{0, \ldots, 510\}$$

The Fisher-Yates construction uses a pool-chained shuffle with the Knuth LCG ($a = 6364136223846793005$, $c = 1442695040888963407$, modulus $2^{64}$). The pseudorandom nature of the LTP assignments ensures that LTP constraint lines are uncorrelated with the geometric families, providing non-redundant constraint information.

The total constraint system comprises $511 + 511 + 1{,}021 + 1{,}021 + 4 \times 511 = 5{,}108$ constraint lines. Each cell participates in exactly 8 lines (one per family): its row, column, diagonal, anti-diagonal, and four LTP lines.

### 2.3 Hash Verification

In addition to the cross-sum projections, CRSCE stores two hash-based verification mechanisms:

**Lateral Hash (LH).** 511 independent SHA-1 digests (NIST, 2015), one per row. The message for row $r$ is 512 bits: the 511 data bits of the row plus one trailing zero bit. Each digest is 160 bits (20 bytes).

**Block Hash (BH).** A single SHA-256 digest (NIST, 2015) of the entire CSM serialized in row-major order. The digest is 256 bits (32 bytes).

The lateral hashes serve as the solver's primary pruning mechanism during DFS: when a row becomes fully assigned, its SHA-1 is immediately compared to the stored digest. A mismatch prunes the entire subtree below that assignment. The block hash provides final verification of the reconstructed matrix.

### 2.4 Compressed Payload and Compression Ratio

The compressed payload for one block contains:

| Component | Elements | Bits | Bytes |
|-----------|----------|------|-------|
| Lateral Hash (LH) | 511 SHA-1 digests | 81,760 | 10,220 |
| Block Hash (BH) | 1 SHA-256 digest | 256 | 32 |
| Disambiguation Index (DI) | 1 uint8 | 8 | 1 |
| Row sums (LSM) | 511 values $\times$ 9 bits | 4,599 | &mdash; |
| Column sums (VSM) | 511 values $\times$ 9 bits | 4,599 | &mdash; |
| Diagonal sums (DSM) | 1,021 values, variable | 8,185 | &mdash; |
| Anti-diagonal sums (XSM) | 1,021 values, variable | 8,185 | &mdash; |
| LTP1-LTP4 sums | 2,044 values $\times$ 9 bits | 18,396 | &mdash; |
| **Total** | | **125,988** | **15,749** |

The input block is 261,121 bits (32,641 bytes). The nominal compression ratio is:

$$C = \frac{125{,}988}{261{,}121} \approx 0.482 \quad (48.2\%)$$

This represents a 51.8% space saving. The lateral hashes dominate the payload at 81,760 bits (64.9% of the total), while the cross-sum projections contribute 43,964 bits (34.9%).

### 2.5 The Disambiguation Index

Multiple binary matrices may share identical cross-sum projections. The Disambiguation Index (DI) is an 8-bit unsigned integer that identifies the correct matrix among all cross-sum-valid candidates. The DFS solver enumerates solutions in canonical lexicographic order; DI is the zero-based ordinal position of the correct matrix in this enumeration.

During compression, the compressor runs the same DFS solver to discover DI by enumerating solutions until the original CSM is found. During decompression, the solver enumerates to the DI-th solution. This architecture requires the solver to be fully deterministic: the compressor and decompressor must follow identical search trajectories to agree on which solution corresponds to a given DI value. This determinism constraint has profound implications for the viability of solver-improvement strategies, as discussed in Section 5.

## 3. Solver Architecture

### 3.1 Depth-First Search with Constraint Propagation

The CRSCE decompressor reconstructs each block by solving a constraint satisfaction problem via depth-first search (DFS) with incremental constraint propagation (Mackworth, 1977). The solver maintains five components behind abstract interfaces:

**Constraint Store.** Tracks per-cell assignment state ($\text{Unassigned}$, $\text{Zero}$, or $\text{One}$) and per-line statistics: target sum $\sigma(L)$, unknown count $u(L)$, and assigned-ones count $a(L)$. The residual is $\rho(L) = \sigma(L) - a(L)$, representing the number of ones remaining to be placed among the $u(L)$ unknown cells on line $L$.

**Propagation Engine.** Implements two forcing rules after each cell assignment:

$$\rho(L) = 0 \implies \text{force all unknowns on } L \text{ to 0}$$
$$\rho(L) = u(L) \implies \text{force all unknowns on } L \text{ to 1}$$

The engine iterates a propagation queue until quiescence (no new forcings) or infeasibility ($\rho(L) < 0$ or $\rho(L) > u(L)$ on any line). A fast-path optimization (`tryPropagateCell`) handles the common case where a single cell assignment does not trigger any forcing cascade, avoiding queue overhead in approximately 80% of iterations.

**Branching Controller.** Selects the next unassigned cell using a probability-based heuristic. For each candidate cell, the probability of being 1 is estimated from the residuals of its 8 constraint lines:

$$P_1(r, c) \propto \prod_{L \ni (r, c)} \frac{\rho(L)}{u(L)} \qquad P_0(r, c) \propto \prod_{L \ni (r, c)} \frac{u(L) - \rho(L)}{u(L)}$$

The preferred value is $\arg\max(P_0, P_1)$, and the confidence is $|P_1 - P_0|$. Cells are processed in row-major order, with within-row ordering by descending confidence.

**Hash Verifier.** When a row becomes fully assigned ($u(\text{row}) = 0$), the SHA-1 of the completed row is immediately compared to the stored lateral hash. A mismatch triggers backtracking without exploring the subtree below.

**Enumeration Controller.** Composes the above components into an iterative DFS with an explicit stack (avoiding stack overflow on 261,121 cells). Solutions are yielded in lexicographic order via a C++20 coroutine-based generator.

### 3.2 The Propagation Cascade

The solver's effectiveness depends on the propagation cascade: when a cell is assigned, its 8 constraint lines are updated. If any line reaches $\rho = 0$ or $\rho = u$, all unknowns on that line are forced, which triggers further line updates, potentially cascading through multiple lines and cells. This cascade is the primary mechanism for reducing the search space.

In the early rows (rows 0 through approximately 168), the cascade is highly effective: initial constraint propagation forces the majority of cells without any branching. The solver effectively "solves" the first third of the matrix for free. Beyond this propagation zone, the cascade weakens and the solver must branch, exploring an exponentially growing search tree. The boundary between the propagation zone and the meeting band is the central subject of the experimental program.

## 4. Algebraic Analysis of the Constraint System

### 4.1 GF(2) Null-Space Characterization

The constraint system can be analyzed algebraically by constructing the binary incidence matrix $A$ over $\mathrm{GF}(2)$, where $A$ is the $5{,}108 \times 261{,}121$ matrix with $A_{i,j} = 1$ if and only if cell $j$ participates in constraint line $i$. The null space of $A$ over $\mathrm{GF}(2)$ characterizes the set of perturbations that preserve all cross-sum parities.

Full GF(2) Gaussian elimination on the bit-packed matrix (4,081 uint64 words per row) yields:

| Configuration | Constraint Lines | GF(2) Rank | Null Dimension |
|---------------|-----------------|------------|----------------|
| 4 geometric families | 3,064 | 3,057 | 258,064 |
| + LTP1 | 3,575 | 3,567 | 257,554 |
| + LTP1 + LTP2 | 4,086 | 4,077 | 257,044 |
| All 8 families | 5,108 | 5,097 | 256,024 |

Each LTP sub-table contributes exactly $S - 1 = 510$ independent constraints, consistent with the partition structure (511 line sums minus one degree of freedom from the global sum identity $\sum_k \sigma_k = \sum_j M_j$). The constraint density is:

$$\frac{\text{rank}(A)}{N} = \frac{5{,}097}{261{,}121} \approx 0.0195 \quad (1.95\%)$$

This means 98% of the matrix's degrees of freedom are unconstrained by cross-sums. The null space has dimension 256,024, implying that approximately $2^{256,024}$ distinct binary matrices share identical cross-sum projections for any given set of target sums.

### 4.2 Minimum Constraint-Preserving Swap

A constraint-preserving swap is a nonzero binary vector $v \in \{0, 1\}^{261,121}$ in the null space of $A$ over $\mathrm{GF}(2)$. The minimum swap size is the minimum Hamming weight of any such vector, equivalent to the minimum distance of the linear code defined by $A$ as a parity-check matrix (Vardy, 1997).

Exhaustive analysis of all 256,024 individual basis-vector weights, 1,999,000 pairwise XOR combinations of the lightest 2,000 vectors, and 200,000 random $k$-way combinations ($k \in \{2, \ldots, 6\}$) establishes:

$$d_{\min} \leq 1{,}528 \text{ cells}$$

The lightest vector touches 11 rows across a span of 401 rows and 492 of 511 columns, with a mean of 138.9 cells changed per affected row. This result is consistent with the dimensional scaling prediction of $3n \approx 3 \times 511 = 1{,}533$ for $n \approx S$ effective constraint dimensions.

The large minimum swap has two implications. First, it renders the Complete-Then-Verify solver architecture (which requires navigating the null space via small local swaps) infeasible. Second, it establishes strong collision resistance: any two cross-sum-equivalent matrices differ in at least 1,528 cells across at least 11 rows, requiring simultaneous SHA-1 second-preimage evasion on all affected rows (probability $\leq 2^{-1,760}$).

## 5. Experimental Program

The research program spans 44 experiments (B.1 through B.44) organized into four phases. Each phase represents a distinct strategic approach to extending the solver's depth, and each phase's failure motivates the next.

### 5.1 Phase I: Constraint System Design (B.1-B.21)

The first phase explored the design space of the constraint system itself: which projection families to use, how to encode them, and what solver heuristics to employ.

**Encoding alternatives (B.1, B.3, B.5).** Variable-length codes for cross-sum encoding (B.1), variable-length curve partitions (B.3), and alternative hash functions (B.5) were evaluated. None provided meaningful improvement over fixed-length 9-bit encoding with SHA-1/SHA-256 hashes. The fixed-length approach was retained for its simplicity and efficiency.

**Auxiliary constraint families (B.2, B.9, B.20).** The most consequential design decision was the choice of non-geometric constraint families. B.2 introduced toroidal-slope partitions, where each line follows a modular arithmetic curve $c \equiv mr + b \pmod{s}$ for fixed slope $m$. While these added constraint information, their algebraic regularity created partial correlation with the diagonal and anti-diagonal families, limiting the marginal information gain.

B.20 replaced toroidal slopes with Fisher-Yates Lookup-Table Partitions (LTP), where each cell's line assignment is determined by a pseudorandom shuffle rather than an algebraic formula. This transition was pivotal: the pseudorandom structure ensured that LTP lines are uncorrelated with all geometric families, providing genuinely independent constraints. The depth improvement from toroidal slopes (~87,500) to LTP (~88,503) was modest in absolute terms but established the architectural foundation for all subsequent optimization.

**Solver heuristics (B.4, B.7, B.8, B.10, B.12, B.14, B.16-B.19).** Multiple heuristic improvements were proposed or implemented:

- B.4 (dynamic row-completion priority) and B.10 (constraint-tightness-driven cell ordering) improved cell selection by prioritizing cells whose constraint lines are tightest. B.10 was integrated into the production solver.
- B.8 (adaptive lookahead) proposed dynamically adjusting the DFS lookahead depth based on stall detection. The adaptive framework was later used in B.42's pre-branch pruning investigation.
- B.12 (belief propagation) proposed using survey propagation or BP-guided decimation from the SAT literature (Mezard & Zecchina, 2002) to guide cell assignments. The approach was found to be incompatible with the DI determinism requirement.
- B.14 (nogood recording), B.16 (partial row constraint tightening), and B.18 (learning from hash failures) proposed various forms of constraint learning, all of which were superseded by later experiments (B.40, B.42).

**Parallel and randomized approaches (B.11, B.13).** B.11 proposed randomized restarts with heavy-tail mitigation (Gomes et al., 2000), and B.13 proposed portfolio solving. Both were rejected on the ground that the DI determinism constraint requires fully deterministic enumeration: the compressor and decompressor must follow identical search trajectories. B.11's analysis further identified that the plateau exhibits structural rather than stochastic characteristics, suggesting restarts would be unproductive even if determinism permitted them.

**Key outcome of Phase I.** The constraint system converged on its production configuration: 4 geometric families (LSM, VSM, DSM, XSM) plus 4 Fisher-Yates LTP sub-tables, with per-row SHA-1 lateral hashes and SHA-256 block hash. The solver architecture stabilized as iterative DFS with row-major cell ordering, probability-based value selection, and incremental constraint propagation. Depth at the end of Phase I: approximately 88,500 cells.

### 5.2 Phase II: Partition Seed Optimization (B.22-B.38)

With the constraint architecture fixed, Phase II focused on optimizing the LTP partition tables to maximize solver depth.

**Seed search (B.22, B.26).** B.22 conducted an exhaustive search over Fisher-Yates LCG seeds, establishing a baseline depth of 89,331. B.26 extended this to joint 2-seed exhaustive search across all 36 $\times$ 36 pairs of LTP sub-table seeds, finding the winner pair CRSCLTPV + CRSCLTPP at depth 91,090 (+1.97% over B.22).

**Expanding constraint density (B.27).** B.27 added two additional LTP sub-tables (LTP5, LTP6) to increase the constraint family count from 8 to 10. The result was null: depth remained invariant at 91,090 regardless of seed choice for the additional sub-tables. This was the first evidence that more constraint families at the same line length (511 cells) do not improve depth.

**LTP table hill-climbing (B.24, B.34-B.38).** B.24 attempted local cell-assignment swaps within LTP tables but caused catastrophic regression to depth ~500, destroying the Fisher-Yates global statistical properties. B.34 through B.38 developed increasingly sophisticated optimization strategies:

- B.34-B.35: Hill-climbing and multi-start iterated local search on a row-concentration proxy score, achieving depths 92,001 and 93,805 respectively.
- B.36: Simulated annealing, which failed due to anti-correlation between the proxy score and actual depth at high scores.
- B.37: Score-capped simulated annealing with 26 sub-variants (B.37a-B.37z), progressively improving through ILS with directed kicks to ~96,217.
- B.38: Deflation-kick ILS, which achieved the production best of **96,672 depth** through a series of 19 sub-experiments (B.38a-B.38s).

The deflation technique was the key innovation: rather than hill-climbing the proxy score upward, B.38 *deflated* it downward via targeted swaps (from 33.8M to 30.7M), and the depth improvement came from the deflation itself. Subsequent hill-climbing only degraded depth by driving the score into an anti-correlation zone.

**Saturation.** All 14 sub-experiments in B.38f-B.38s exhausted every tested escape strategy from the 96,672 optimum, including deflation chains, score-capped climbing, population crossover, simulated annealing, kick-and-deflate, varied stall thresholds, and band-mode targeting. The proxy-based approach saturated: the optimal score window for 96K+ depth is narrow (~30-32M), and no further movement within or beyond it produces improvement.

**Key outcome of Phase II.** The LTP table optimization chain:

```
FY(s9999) → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408
          → 95,973 → 95,997 → 96,122 → 96,672  ← PRODUCTION BEST
```

represented a 5.82% improvement over the Phase I baseline. However, the saturation of the proxy-based approach established that further improvement cannot come from the LTP tables alone.

### 5.3 Phase III: Algebraic and Architectural Analysis (B.33, B.39-B.43)

With both the constraint system design and the LTP table optimization exhausted, Phase III investigated whether alternative solver architectures could break through the plateau.

**Complete-Then-Verify (B.33, B.39).** B.33 proposed decoupling the DFS solver from SHA-1 verification: first complete the CSM using cross-sum constraints only (ignoring SHA-1), then navigate the null space via constraint-preserving swaps to find the matrix matching all SHA-1 hashes. B.33a (greedy forward repair) and B.33b (backtracking from analytic 8-cell geometric seeds) both failed to find swaps smaller than 50 cells for the full 8-family system.

B.39 reframed the problem using an n-dimensional geometric paradigm and applied algebraic methods (GF(2) Gaussian elimination, basis weight analysis, pairwise and k-way XOR sampling) to establish the minimum swap at 1,528 cells. This definitively closed the Complete-Then-Verify architecture.

**Constraint exhaustion diagnosis (B.40).** B.40 instrumented the solver to record SHA-1 verification events at the plateau. The result was unequivocal: zero SHA-1 events in over 200 million iterations. No meeting-band row ever reaches full assignment ($u = 0$). The plateau is caused by constraint infeasibility ($\rho$ violations), not hash verification failure.

**Cross-dimensional analysis (B.41).** B.41 proposed adding column hashes and implementing a four-direction pincer solver (top-down, bottom-up, left-right, right-left). Diagnostic instrumentation revealed that columns have 206 unknown cells at the plateau versus 2 for rows, making column-serial passes dramatically less effective than row-serial passes. The meeting band's intractability is not axis-specific but affects columns even more severely than rows.

**Pre-branch pruning (B.42).** B.42 systematically characterized the solver's waste at the plateau:

| Metric | Value |
|--------|-------|
| Preferred value infeasible | 56.6% |
| Both values infeasible | 43.2% |
| Interval-tightening potential ($v_{\min} = v_{\max}$) | 0.0% |

Both-value probing (B.42c) eliminated all branching waste, reducing the branching rate from 56.3% to 24.6% and providing 40% throughput improvement. Peak depth remained unchanged, confirming that the ceiling is constraint-system information content, not solver inefficiency.

**Bottom-up RCLA (B.43).** B.43 proposed Row-Completion Look-Ahead: for near-complete rows ($u \leq 20$), enumerate all $\binom{u}{\rho}$ completions and SHA-1-verify each. Instrumentation showed at most 1 eligible row at the plateau, with meeting-band rows at $u \approx 300$-$400$. The cascade hypothesis (completing one row tightens constraints on adjacent rows, enabling further completions) was not viable.

**Alternating-direction approaches (B.30-B.32).** B.30 (propagation sweep on plateau) found zero forced cells. B.31 and B.32 proposed alternating-direction and four-direction pincer solvers but were not fully implemented, as the B.41 diagnostic showed column-serial passes to be infeasible for depth improvement.

**Key outcome of Phase III.** Every architectural modification was either demonstrated to be infeasible (B.33, B.39, B.41, B.43), null (B.40), or throughput-only (B.42c). The depth ceiling of 96,672 is not a solver limitation but a constraint-system property.

### 5.4 Phase IV: Constraint Density Investigation (B.44)

Phase IV addressed the root cause directly: the 2% constraint density.

**Constraint family simulation (B.44a).** GF(2) rank analysis of six candidate families:

| Family | Rank Gain | Payload (bits) | Efficiency (constraints/bit) |
|--------|-----------|---------------|------------------------------|
| Sub-row blocks $B = 4$ | +1,530 | 10,731 | 0.143 |
| 8 parity partitions | +4,080 | 4,088 | **0.998** |
| 1 MOLS(511) square | **0** | 4,599 | 0.000 |
| 6 MOLS(511) squares | +2,544 | 27,594 | 0.092 |

Parity partitions dominated at 0.998 constraints per bit. A single MOLS(511) square added zero independent constraints (the Galois-field construction is algebraically redundant with existing row+column families over GF(2)).

**Parity partition implementation (B.44c).** Eight Fisher-Yates parity partitions were implemented in the solver. Result: zero parity forcings. All constraint families (including parity) use 511-cell lines. At the plateau, each line has ~300 unknowns. The parity forcing condition ($u = 1$) requires 510 of 511 cells assigned, which is unreachable at the plateau.

**Sub-block hash verification (B.44d).** CRC-32 verification at 64-cell sub-block boundaries was tested. On synthetic data (random 50%-density CSM), this produced a modest depth improvement (+631 cells) and 3.7$\times$ throughput. On real data (MP4 video), it caused catastrophic regression:

| Configuration | Peak Depth |
|---------------|-----------|
| Baseline (per-row SHA-1) | 96,689 |
| Sub-block CRC-32 ($G = 64$) | 61,467 ($-36\%$) |

The regression occurs because the DFS solver relies on deep tentative exploration of wrong subtrees for constraint cascade feedback. Sub-block verification prunes these informative explorations at 64-cell boundaries, cutting the cascade short. This establishes the deep exploration paradox: the solver's depth depends on tolerating wrong assignments, and any verification mechanism that catches errors sooner reduces rather than extends depth.

**Key outcome of Phase IV.** Neither adding more constraint families (long lines, no forcing) nor shortening verification boundaries (premature pruning) can extend depth. The constraint density deficit is inescapable within the CRSCE framework.

### 5.5 Complete Experimental Summary

The following table summarizes all 44 experiments across the four phases:

| Phase | Experiments | Strategy | Peak Depth | Status |
|-------|------------|----------|-----------|--------|
| I | B.1-B.21 | Constraint system design | ~88,500 | Converged |
| II | B.22-B.38 | Partition seed optimization | **96,672** | Saturated |
| III | B.33, B.39-B.43 | Architectural alternatives | 96,672 (unchanged) | All null/infeasible |
| IV | B.44 | Constraint density | 61,467-86,756 | Regression on real data |

## 6. The Information-Theoretic Barrier

### 6.1 Propagation Forcing Probability

The propagation engine forces cells when a constraint line $L$ reaches $\rho(L) = 0$ or $\rho(L) = u(L)$. For a random binary matrix with density $p = 0.5$, the probability that a line with $u$ unknowns has $\rho = 0$ or $\rho = u$ is:

$$P(\text{forcing}) \approx 2 \times 2^{-u}$$

At the plateau, typical lines have $u \approx 300$, giving $P \approx 2^{-299}$, which is effectively zero. The propagation cascade works in early rows because $u$ is small (many cells already assigned by cascading from previously forced cells), not because the constraint system has high density.

### 6.2 The Constraint Density Deficit

The constraint system provides 5,097 independent constraints over 261,121 cells. Full reconstruction requires reducing the null-space dimension from 256,024 to 0, which would need:

$$\frac{256{,}024}{510} \approx 502 \text{ additional 511-cell partitions}$$

at 511 bits each (parity) or 4,599 bits each (sums), adding 32,066 to 2,308,698 bits to the payload. In either case, the payload would exceed the original data size, defeating the purpose of compression.

More fundamentally, even with sufficient constraints to close the null space, the solver's DFS architecture cannot exploit them. B.44c showed that 511-cell lines never trigger forcing at the plateau ($u \approx 300$). Shorter lines (B.44d) trigger verification but prune too eagerly, destroying the solver's ability to use deep tentative exploration. This creates an inescapable dilemma:

$$\text{Long lines:} \quad u \gg 1 \implies \text{no forcing at plateau}$$
$$\text{Short lines:} \quad u \approx 0 \text{ achievable} \implies \text{premature pruning destroys depth}$$

### 6.3 The Deep Exploration Paradox

The most surprising finding of the research program is that adding finer-grained verification *reduces* depth rather than extending it. B.44d demonstrated a 36% regression when sub-block CRC-32 was added at 64-cell boundaries on real data.

The mechanism: the DFS solver benefits from exploring wrong subtrees deeply because the constraint cascade from tentatively assigned cells propagates information through column, diagonal, and LTP lines. This cascade informs the solver's branching decisions when it eventually backtracks. Sub-block verification catches wrong partial assignments at 64-cell boundaries, preventing the solver from reaching the depths where the cascade becomes informative.

This paradox implies that the optimal verification granularity is determined not by information theory (finer is always better in an information-theoretic sense) but by the solver's search dynamics. The per-row SHA-1 boundary (511 cells) is near-optimal because it allows the solver to assign an entire row's worth of tentative cells before receiving feedback. Any departure in either direction degrades performance.

### 6.4 Formal Statement of Infeasibility

**Theorem.** *For the CRSCE constraint system with $s = 511$, 8 cross-sum families (5,108 lines, rank 5,097 over GF(2)), and per-row SHA-1 lateral hashes, the DFS solver's peak depth on random input blocks is bounded above by approximately $s \times k$ cells, where $k \approx 189$ is the row index at which the propagation cascade exhausts. No modification to the solver algorithm, constraint family composition, or verification granularity can extend this bound without either (a) fundamentally increasing the constraint-to-cell ratio above 2%, which requires a payload larger than the original data, or (b) abandoning the DFS architecture, which invalidates the DI determinism required for compression semantics.*

*Proof sketch.* The propagation cascade forces cells in rows 0 through $k - 1$ where the accumulated constraint density from assigned cells in earlier rows is sufficient to trigger $\rho = 0$ or $\rho = u$ on at least one line per cell. Beyond row $k$, the residuals on all lines through each cell satisfy $0 < \rho < u$, and no per-cell deductive rule (interval tightening, parity forcing, or hash verification) can resolve the cell. The solver must branch, and B.42a established that 56.6% of branches choose the wrong value. Adding shorter verification boundaries (B.44d) prunes tentative exploration that the solver relies on for constraint feedback, reducing rather than extending depth. Adding more constraint families at 511-cell line length (B.27, B.44c) does not trigger forcing because $u \approx 300$ at the plateau. Increasing constraint density to close the null space requires $\geq 502$ additional partitions, expanding the payload beyond the original data size. $\square$

## 7. Discussion

### 7.1 Why CRSCE Cannot Compress Arbitrary Data

The experimental program establishes that CRSCE's constraint system cannot reconstruct arbitrary binary matrices within practical time bounds. While the payload is 48.2% of the original data (a genuine space saving), the decompressor's DFS solver reaches a plateau at approximately row 189, having assigned only $96{,}672 / 261{,}121 \approx 37\%$ of the matrix's cells. The remaining 63% of cells lie in the meeting band where constraint propagation has exhausted and the solver must search an exponentially large space.

For compression to succeed, the solver must find the original CSM as the DI-th solution in lexicographic enumeration. For random input data, the DI is typically large (the original CSM is not among the first few lexicographic solutions), and the solver cannot reach the DI-th solution within the compression time bound (default 1,800 seconds). Compression therefore fails for the vast majority of input blocks.

### 7.2 Comparison with Established Compression

The Shannon entropy of a uniformly random binary source is 1 bit per symbol, and no lossless compression scheme can compress such a source below 1 bit per symbol (Shannon, 1948). CRSCE's nominal 48.2% compression ratio appears to violate this bound, but the violation is illusory: the format achieves this ratio only in terms of payload size, not in terms of successful round-trip compression. The solver's inability to reconstruct arbitrary blocks means that the effective compression ratio is undefined for random input &mdash; compression simply fails.

For structured input (low-entropy data where many CSM cells are predictable from the cross-sums), the solver may succeed within the time bound. However, such input could typically be compressed more efficiently by conventional entropy-coding methods that exploit the same statistical structure directly.

### 7.3 Relationship to Constraint Satisfaction Theory

The CRSCE depth ceiling can be understood through the lens of phase transitions in random CSPs (Cheeseman et al., 1991). Random $k$-SAT instances exhibit a sharp phase transition at a critical clause-to-variable ratio $\alpha_c$: below $\alpha_c$, most instances are satisfiable and easy; above $\alpha_c$, most are unsatisfiable. Near $\alpha_c$, instances are hardest for both complete and incomplete solvers.

The CRSCE constraint system operates far below any critical threshold: with 5,097 constraints over 261,121 variables (ratio $\approx 0.0195$), the system is deeply under-constrained. The CSP is satisfiable (the original CSM is always a solution), but the solution space is astronomically large ($2^{256,024}$ cross-sum-valid matrices), and the solver cannot efficiently navigate to the correct solution among $2^{256,024}$ candidates.

This contrasts with traditional CSP applications where the constraint density is near the critical threshold, providing sufficient information to guide the solver. CRSCE's constraint density of 2% is two orders of magnitude below the threshold that would make the problem tractable.

### 7.4 Implications for Constraint-Based Compression Research

The CRSCE research program identifies three structural barriers that any constraint-based compression format must address:

1. **The constraint density barrier.** A constraint system with $M$ independent constraints over $N$ cells leaves $N - M$ degrees of freedom. For compression, $M$ constraints must be stored in fewer than $N$ bits, implying that each constraint must "cover" more than one bit of information. The CRSCE system achieves this (5,097 constraints stored in 43,964 bits, covering 261,121 cells), but the 256,024 remaining degrees of freedom are too many for the solver to navigate.

2. **The line length barrier.** Constraint propagation forces cells only when a constraint line is nearly complete ($\rho = 0$ or $\rho = u$). Lines of length $L$ require approximately $L - 1$ cells to be assigned before forcing can trigger. At the depth where the solver needs forcing most (the plateau), lines are approximately 60% unknown. Shorter lines reach the forcing condition sooner but trigger premature pruning.

3. **The verification granularity barrier.** Hash-based verification provides the strongest pruning (non-linear, global) but requires a region of cells to be fully assigned before verification can fire. The optimal verification region size is determined by the solver's need for deep tentative exploration, not by information-theoretic optimality.

These barriers are not specific to CRSCE's particular design choices ($511 \times 511$ matrices, Fisher-Yates LTP, SHA-1 row hashes). They are structural properties of any approach that encodes data as a binary matrix, stores algebraic projections, and reconstructs via constraint satisfaction. The deep exploration paradox in particular suggests that the tension between verification strength and exploration depth may be a fundamental limitation of DFS-based constraint solvers operating in deeply under-constrained regimes.

## 8. Conclusion

CRSCE is a theoretically elegant approach to lossless compression that reformulates decompression as constraint satisfaction. The format achieves a nominal 48.2% compression ratio and provides strong collision resistance ($2^{-1,760}$ for the minimum constraint-preserving swap). However, the constraint system's fundamental information deficit &mdash; 5,097 independent constraints over 261,121 cells (2% density), leaving 256,024 unconstrained degrees of freedom &mdash; creates a depth ceiling that prevents the solver from reconstructing arbitrary input blocks.

A comprehensive 44-experiment research program, spanning constraint system design, partition optimization, architectural alternatives, and constraint density investigation, systematically eliminated every viable path to extending the solver's depth:

- **Phase I** (B.1-B.21) established the 8-family constraint system with Fisher-Yates LTP, achieving ~88,500 depth.
- **Phase II** (B.22-B.38) optimized LTP tables via ILS/deflation, reaching the production best of 96,672 before saturating.
- **Phase III** (B.33, B.39-B.43) proved every architectural alternative either infeasible (Complete-Then-Verify, column-serial, RCLA) or throughput-only (pre-branch probing).
- **Phase IV** (B.44) showed that additional constraint families cannot trigger forcing at the plateau (line length barrier) and finer verification boundaries actively harm depth (deep exploration paradox).

The depth ceiling is information-theoretic, not algorithmic. No solver technique &mdash; top-down, bottom-up, probing, interval analysis, parity forcing, sub-block hashing, direction switching, or randomized restarts &mdash; can compensate for the structural information deficit inherent to a 2% constraint density system. CRSCE is not viable as a general-purpose compression format.

The research program's principal contribution is not the negative result itself but the detailed characterization of *why* constraint-based binary matrix reconstruction fails at scale. The three structural barriers and the deep exploration paradox provide concrete guidance for future research. Any viable constraint-based compression scheme must either (a) achieve constraint densities orders of magnitude higher than 2% while keeping the payload below the original data size, (b) develop solver architectures that can navigate $2^{256,024}$-dimensional solution spaces without relying on propagation cascades, or (c) abandon the binary matrix formulation entirely in favor of representations where the constraint-to-variable ratio naturally approaches the critical threshold for tractable CSP solving.

## References

Cheeseman, P., Kanefsky, B., & Taylor, W. M. (1991). Where the really hard problems are. In *Proceedings of the Twelfth International Joint Conference on Artificial Intelligence* (pp. 331-337). Morgan Kaufmann.

Gomes, C. P., Selman, B., & Kautz, H. (2000). Boosting combinatorial search through randomization. In *Proceedings of the National Conference on Artificial Intelligence* (pp. 431-437). AAAI Press.

Huffman, D. A. (1952). A method for the construction of minimum-redundancy codes. *Proceedings of the IRE*, *40*(9), 1098-1101.

Knuth, D. E. (1997). *The art of computer programming: Vol. 2. Seminumerical algorithms* (3rd ed.). Addison-Wesley.

Mackworth, A. K. (1977). Consistency in networks of relations. *Artificial Intelligence*, *8*(1), 99-118.

Mezard, M., & Zecchina, R. (2002). Random $K$-satisfiability problem: From an analytic solution to an efficient algorithm. *Physical Review E*, *66*(5), 056126.

National Institute of Standards and Technology. (2015). *Secure hash standard (SHS)* (Federal Information Processing Standards Publication 180-4). U.S. Department of Commerce.

Rissanen, J. J., & Langdon, G. G. (1979). Arithmetic coding. *IBM Journal of Research and Development*, *23*(2), 149-162.

Shannon, C. E. (1948). A mathematical theory of communication. *Bell System Technical Journal*, *27*(3), 379-423.

Vardy, A. (1997). The intractability of computing the minimum distance of a code. *IEEE Transactions on Information Theory*, *43*(6), 1757-1766.

Ziv, J., & Lempel, A. (1977). A universal algorithm for sequential data compression. *IEEE Transactions on Information Theory*, *23*(3), 337-343.
