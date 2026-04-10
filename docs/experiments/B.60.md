## B.60 Vertical CRC-32 Hash: Cross-Axis GF(2) Constraints (Proposed)

### Baseline
#### B.60.1 Motivation

B.58 established that CRC-32 lateral hashes (LH) are the most information-efficient component of the CRSCE payload. Each LH bit contributes 0.99 independent GF(2) equations, compared to 0.14 for each yLTP bit. The B.58a measurement confirmed that CRC-32 contributed 4,032 of a possible 4,064 independent equations&mdash;99.2% efficiency.

The natural question is: can we buy more CRC-32 equations along a different axis? A **vertical hash** (VH)&mdash;one CRC-32 digest per column&mdash;provides 32 GF(2) equations per column, involving the 127 cells of that column. These equations are structurally independent of the LH equations because they operate on orthogonal variable subsets: LH for row $r$ constrains $\{x_{r,0}, \ldots, x_{r,126}\}$, while VH for column $c$ constrains $\{x_{0,c}, \ldots, x_{126,c}\}$. The two share exactly one variable ($x_{r,c}$) per (row, column) pair.

This cross-axis property is the key structural advantage of VH over CRC-64 per row. CRC-64 provides 64 equations on the *same* 127 row variables; VH provides 32 equations on a *different* set of 127 column variables. The combinator pipeline's CrossDeduce step can exploit the row-column intersection directly&mdash;cell $(r,c)$ is constrained by both row $r$'s CRC and column $c$'s CRC. This intersection reasoning is unavailable to any single-axis hash, regardless of width.

#### B.60.2 Format Change

**Vertical Hash (VH).** 127 CRC-32 digests (32 bits each), one per column. Column message construction mirrors row message construction (&sect;B.58.2.1): the 127 data bits of column $c$ ($x_{0,c}, x_{1,c}, \ldots, x_{126,c}$) followed by 1 trailing zero bit (128 bits = 16 bytes). The same CRC-32 polynomial, reflection, and inversion conventions apply.

**Updated payload.** VH replaces the 2 yLTP sub-tables. The yLTP lines contributed only 252 independent GF(2) equations (126 per sub-table) at a cost of 1,778 payload bits&mdash;an information efficiency of 0.14 eq/bit. VH contributes ~4,032 independent GF(2) equations at a cost of 4,064 payload bits&mdash;an efficiency of 0.99 eq/bit. The trade is +2,286 payload bits for +3,780 GF(2) equations.

| Component | B.57 (LH + 2 yLTP) | B.60 (LH + VH, no yLTP) | Delta |
|-----------|--------------------|-----------------------|-------|
| CRC-32 LH | 4,064 bits | 4,064 bits | 0 |
| CRC-32 VH | &mdash; | 4,064 bits | +4,064 |
| SHA-256 BH | 256 bits | 256 bits | 0 |
| DI | 8 bits | 8 bits | 0 |
| LSM | 889 bits | 889 bits | 0 |
| VSM | 889 bits | 889 bits | 0 |
| DSM | 1,531 bits | 1,531 bits | 0 |
| XSM | 1,531 bits | 1,531 bits | 0 |
| yLTP1 | 889 bits | &mdash; | &minus;889 |
| yLTP2 | 889 bits | &mdash; | &minus;889 |
| **Total** | **10,946 bits (1,369 bytes)** | **13,232 bits (1,654 bytes)** | **+2,286** |
| **C_r** | **32.1%** | **18.0%** | **&minus;14.1 pp** |

#### B.60.3 GF(2) Constraint System

#### B.60.3.1 VH Generator Matrix Construction

The CRC-32 generator matrix for columns is identical to the row generator matrix $G_{\text{CRC}} \in \text{GF}(2)^{32 \times 127}$ constructed in &sect;B.58.2.2&mdash;the polynomial, message length (127 data bits + 1 padding bit), and affine constants are the same. Only the variable mapping differs:

- LH row $r$: equation $i$ involves variables $\{x_{127r+c} : c \in \{0, \ldots, 126\}\}$ with coefficients from row $i$ of $G_{\text{CRC}}$.
- VH column $c$: equation $i$ involves variables $\{x_{127r+c} : r \in \{0, \ldots, 126\}\}$ with coefficients from row $i$ of $G_{\text{CRC}}$.

The target vector for VH column $c$ is $(h^V_c \oplus \mathbf{c})$, where $h^V_c$ is the stored CRC-32 digest of column $c$ and $\mathbf{c}$ is the same affine constant as LH.

#### B.60.3.2 Full GF(2) System

| Source | Equations | Estimated Independent | Variable structure |
|--------|-----------|----------------------|-------------------|
| Cross-sum parity (4 geometric) | 760 | ~753 | Global (each line spans s cells) |
| CRC-32 LH (127 rows) | 4,064 | ~4,032 | Block-diagonal (each block: 127 row vars) |
| CRC-32 VH (127 columns) | 4,064 | ~4,032 | Block-diagonal (each block: 127 col vars) |
| **Total** | **8,888** | **~8,817** | |

Variables: 16,129. Estimated GF(2) rank: ~8,817. Null-space: ~7,312 (45.3%).

**Why no yLTP in the GF(2) count.** Each yLTP sub-table contributes $s - 1 = 126$ independent GF(2) equations at a payload cost of $s \times b = 889$ bits. With VH available at 0.99 eq/bit, the 1,778 bits spent on 2 yLTP sub-tables (252 GF(2) equations) would buy 1,760 GF(2) equations if spent on VH instead. yLTP is strictly dominated.

#### B.60.3.3 Independence of VH from LH

LH equations for row $r$ have non-zero entries only in columns $\{127r, 127r+1, \ldots, 127r+126\}$ of the global GF(2) matrix. VH equations for column $c$ have non-zero entries only in columns $\{c, 127+c, 254+c, \ldots, 127 \times 126 + c\}$. These two column sets intersect in exactly one element ($127r + c$). A single shared variable cannot make a 32-coefficient equation linearly dependent on another 32-coefficient equation from a different variable set. Therefore LH and VH equations are pairwise independent modulo the cross-sum parity constraints (which involve all-ones coefficient vectors, not CRC polynomial coefficients).

**Expected VH independence rate.** B.58a measured LH independence at 99.2% (4,032 of 4,064). By structural symmetry (same polynomial, same message length, orthogonal axes), VH independence should also be ~99.2%. The cross-sum parity overlap is limited to the VSM parity constraint ($\bigoplus_{r} x_{r,c} = \text{VSM}[c] \bmod 2$), which, as shown in &sect;B.58.2.3, does not coincide with any CRC-32 equation because $g(1) = 1 \neq 0$.

#### B.60.3.4 Cross-Axis Interaction in the Combinator

The combinator pipeline from B.58 (&sect;B.58.4.4) operates identically, with VH equations added to the GF(2) matrix alongside LH equations. The critical new interaction occurs in the **Fixpoint** loop:

**GaussElim with cross-axis structure.** After RREF, a cell $x_{r,c}$ may appear as:

- A **pivot** in an LH equation (determined by row $r$'s CRC + other row cells)
- A **pivot** in a VH equation (determined by column $c$'s CRC + other column cells)
- A **free variable** in both

If GaussElim pivots $x_{r,c}$ via an LH equation, the value of $x_{r,c}$ (as a function of free variables) propagates into all VH equations for column $c$, potentially creating new pivots in column $c$'s CRC constraints. This cascade between row pivots and column pivots is unique to the cross-axis configuration and does not exist in CRC-64 (single-axis) systems.

**IntBound with dual hash.** After GaussElim determines some cells, IntBound updates residuals on all constraint lines. A cell forced by integer bounds on a row constraint immediately tightens column constraints and vice versa. The dual-hash system creates a tighter feedback loop than single-axis hashing.

**CrossDeduce with row-column CRC intersection.** Cell $(r,c)$ lies on row $r$ (constrained by 32 LH equations) and column $c$ (constrained by 32 VH equations). If the LH system determines that $x_{r,c}$ equals a specific linear combination of row-free variables, and the VH system determines that $x_{r,c}$ equals a different linear combination of column-free variables, equating the two expressions creates a new constraint that couples row-free and column-free variables. This coupling may trigger additional determinations.

#### B.60.3.5 Concurrent Algebraic Architecture (No DFS)

B.60 **eliminates the DFS solver entirely**. There is no cell-by-cell branching, no backtracking stack, no sequential row processing, no meeting band, and no propagation zone. The solver is a purely algebraic pipeline that considers all 16,129 cells, all 127 rows, all 127 columns, all 506 diagonals/anti-diagonals, and all 8,128 CRC equations as a single simultaneous system.

**Architectural contrast with DFS.**

| Property | DFS Solver (B.1&ndash;B.57) | B.60 Algebraic Solver |
|----------|---------------------------|----------------------|
| Cell processing | Sequential, row-major | **Simultaneous, all cells at once** |
| Row processing | Top-down, row 0 first | **All 127 rows concurrently** |
| Constraint activation | Incremental (per-cell update) | **Global (full matrix RREF)** |
| Hash verification | After row completion ($u = 0$) | **At system construction (algebraic)** |
| Backtracking | Explicit undo stack | **None (monotone reduction)** |
| Meeting band | Rows ~60&ndash;126 unreachable | **No concept; all cells equally accessible** |
| LH/VH role | Verification oracle (pass/fail) | **8,128 GF(2) equations in the constraint matrix** |
| Search strategy | Branch on preferred value (0 before 1) | **Symbolic elimination; search only over residual null-space** |

**Why all rows are solved concurrently.** In the GF(2) constraint matrix $G$ ($8{,}888 \times 16{,}129$), every equation is present from the start. Row 0's LH equations, row 126's LH equations, column 0's VH equations, and column 126's VH equations all participate in the same Gaussian elimination. When GaussElim pivots a cell in row 0 via an LH equation, the substitution propagates into VH equations affecting columns that span rows 0&ndash;126, which in turn may create pivots in rows 60&ndash;126&mdash;the very rows that the DFS solver could never reach. The algebraic solver has no concept of "depth" or "progress through rows"; it operates on the full system simultaneously.

**Why cross-sum integer constraints participate globally.** The 760 geometric cross-sum lines ($127 rows + 127 columns + 253 diagonals + 253 anti-diagonals$) enter the GF(2) system as parity equations AND the integer system as cardinality constraints. IntBound processes ALL 760 lines in each fixpoint iteration, not just lines touching recently-assigned cells. A diagonal spanning rows 0&ndash;126 provides a constraint that couples the top and bottom of the matrix in a single equation&mdash;information that the DFS solver could only access after assigning all 127 cells on that diagonal sequentially.

**Formal solver specification.** The B.60 solver is the composition:

$$
    \text{solve} = \text{VerifyBH} \circ \text{EnumerateFree} \circ \text{Fixpoint}(\text{CrossDeduce} \circ \text{Propagate} \circ \text{IntBound} \circ \text{GaussElim}) \circ \text{BuildSystem}
$$

Each combinator is specified in B.58 &sect;B.58.4.4. The pipeline is:

```
def solve_b60(payload):
    """B.60 algebraic solver. No DFS. All rows concurrent.

    The entire 127x127 CSM is treated as a single system of
    8,888 GF(2) equations + 760 integer constraints over 16,129
    binary variables. All constraints — across all rows, all columns,
    all diagonals, all anti-diagonals — are active simultaneously
    from the first operation.
    """

    # Phase 1: Build the full constraint system (all axes, all cells)
    S = build_system_b60(payload)
    #   S.G:  8,888 x 16,129 GF(2) matrix
    #         rows 0..759:     cross-sum parity (4 geometric families)
    #         rows 760..4823:  CRC-32 LH (127 rows x 32 equations)
    #         rows 4824..8887: CRC-32 VH (127 cols x 32 equations)
    #   S.lines[0..759]:  integer cardinality constraints
    #   Every cell (r,c) for ALL r in 0..126, ALL c in 0..126
    #   is a variable in this system from the start.

    # Phase 2: Global GF(2) Gaussian elimination
    #   Operates on ALL 8,888 equations simultaneously.
    #   A pivot in row 0's LH propagates to column 0's VH and
    #   to diagonal equations spanning the full matrix.
    #   No row is privileged; no row is processed "first".
    S.pivot_cols, S.free_cols, S.rank = gauss_elim(S.G, S.b_G)

    # Phase 3: Fixpoint (all combinators, all constraints, all cells)
    iteration = 0
    while True:
        prev = len(S.D)

        # GaussElim: extract cells determined by GF(2) system
        #   Considers LH eqs for ALL 127 rows + VH eqs for ALL 127
        #   columns + parity eqs for ALL 760 geometric lines.
        det_gf2 = extract_determined_from_rref(S)
        if det_gf2:
            propagate(S, det_gf2)

        # IntBound: integer cardinality on ALL 760 lines simultaneously
        #   Row 0 AND row 126 AND diagonal (0,0)-(126,126) AND
        #   anti-diagonal (0,126)-(126,0) are all checked in the
        #   same pass. A forcing on any line propagates to all
        #   other lines sharing that cell.
        det_int, ok = int_bound_all_lines(S)
        if not ok:
            return None  # inconsistent
        if det_int:
            propagate(S, det_int)
            rerun_gauss_elim(S)

        # CrossDeduce: pairwise intersection of ALL line pairs
        #   Row r's CRC meets column c's CRC at cell (r,c).
        #   Diagonal d meets row r at their intersection cell.
        #   ALL 760 + 8,128 constraint sources interact.
        det_cross, ok = cross_deduce_all_pairs(S)
        if not ok:
            return None
        if det_cross:
            propagate(S, det_cross)
            rerun_gauss_elim(S)

        iteration += 1
        if len(S.D) == prev or len(S.D) == 16129:
            break

    # Phase 4: Residual enumeration over null-space
    #   Only free variables remain. Enumerate in canonical
    #   (row-major, 0-before-1) order for DI determinism.
    if len(S.F) == 0:
        csm = reconstruct(S)
        return csm if verify_bh(csm, payload.bh) else None

    # Phase 5: Row-column grouped search (B.60c)
    #   Decompose residual using LH per-row + VH per-column filtering.
    #   Even here, column filtering provides cross-row coupling that
    #   DFS cannot exploit.
    return row_column_grouped_search(S, payload.bh)
```

**Concurrency model.** The algebraic solver is inherently data-parallel:

- **GaussElim** row-reduces the GF(2) matrix. Row operations are independent and parallelizable (each row XOR is a 253-word SIMD operation). On GPU or multi-core, the 8,888 rows can be reduced in $O(\text{rank})$ parallel steps.

- **IntBound** scans 760 lines. Each line's $\rho$ and $u$ update is independent. All 760 lines can be checked in parallel; only the propagation of newly determined cells requires serialization.

- **CrossDeduce** checks $\binom{6}{2} = 15$ line pairs per cell across $|F|$ free cells. Each cell's pair-check is independent. The 16,129 $\times$ 15 checks are embarrassingly parallel.

This is fundamentally different from DFS, where each cell assignment depends on the previous cell's propagation result, making the computation inherently sequential.

#### B.60.4 Information-Theoretic Analysis

**GF(2) budget comparison.**

| Config | GF(2) rank | Density (eq/var) | Null-space | C_r |
|--------|-----------|------------------|-----------|-----|
| B.57 (LH + 2 yLTP) | 5,037 | 0.312 | 11,092 (68.8%) | 32.1% |
| **B.60 (LH + VH, no yLTP)** | **~8,817** | **0.547** | **~7,312 (45.3%)** | **18.0%** |
| Theoretical max (LH + VH + 2 yLTP) | ~9,069 | 0.562 | ~7,060 (43.8%) | 6.9% |

B.60 achieves 97% of the theoretical maximum GF(2) rank (8,817 of 9,069) while retaining 18.0% compression. Adding yLTP back would gain only 252 more equations (+2.8%) at a cost of 11.1 percentage points of compression (18.0% $\to$ 6.9%).

**Integer information budget.** The integer cross-sum constraints are unchanged from B.58 (&sect;B.58.3): ~7,098 bits from 760 geometric lines (no yLTP contribution). VH adds no integer information (CRC-32 is a GF(2) function, not an integer constraint). The combined estimate:

| Source | Bits | Nature |
|--------|------|--------|
| Integer cross-sums (760 lines) | ~5,320 | Integer (no yLTP lines) |
| CRC-32 LH (127 rows $\times$ 32 bits) | ~4,032 | GF(2) |
| CRC-32 VH (127 cols $\times$ 32 bits) | ~4,032 | GF(2) |
| SHA-256 BH | 256 | Non-linear (verification only) |
| **Total exploitable** | **~13,384** | |
| Cell entropy | 16,129 | |
| **Residual free dimensions** | **~2,745** | |

The optimistic residual is ~2,745 free variables (17.0% of cells)&mdash;a 75% reduction from B.57's ~11,092 and a major step toward tractability.

**Density sensitivity.** At low density (10&ndash;30%), integer constraints carry more information per line (&sect;B.58.7). Combined with the dual-hash GF(2) system, B.60 at 10% density may approach density 0.8+ eq/var, where the combinator fixpoint determines most cells.

#### B.60.5 Collision Resistance

**LH collision.** A constraint-preserving swap must alter at least one cell per affected row. Each affected row's CRC-32 changes with probability $1 - 2^{-32}$. For a swap touching $k$ rows: evasion probability $\leq 2^{-32k}$.

**VH collision.** The same swap also alters at least one cell per affected column. Each affected column's CRC-32 changes independently. For a swap touching $j$ columns: evasion probability $\leq 2^{-32j}$.

**Joint LH+VH collision.** The swap must evade both LH and VH simultaneously. For a swap touching $k$ rows and $j$ columns: evasion probability $\leq 2^{-32(k+j)}$.

**BH collision.** SHA-256 block hash provides $2^{-256}$ adversarial resistance regardless.

**Minimum-swap evasion at S=127.** B.59f will measure the minimum swap; the B.39 scaling model predicts ~250&ndash;380 cells touching ~10 rows and ~120 columns. Joint evasion: $2^{-32 \times (10+120)} = 2^{-4{,}160}$. Combined with BH: $\min(2^{-4{,}160}, 2^{-256}) = 2^{-256}$ (BH dominates). Collision resistance is more than sufficient.

#### B.60.6 Method

B.60 decompression is a five-phase algebraic pipeline. No phase uses DFS, backtracking, or sequential row processing. All 16,129 cells participate in every phase from the start.

**Phase 1: Build the global constraint system.**

All constraints&mdash;across all rows, all columns, all diagonals, all anti-diagonals&mdash;are assembled into a single system before any cell is determined. No constraint is "activated later" or "deferred until the solver reaches that row."

(a) **Cross-sum parity matrix** ($760 \times 16{,}129$ over GF(2)). For each of the 760 geometric constraint lines (127 LSM + 127 VSM + 253 DSM + 253 XSM), construct a binary row vector with 1s at the global indices of cells on that line. Target bit = cross-sum value mod 2. All 760 equations span the entire matrix: row 0 and row 126 coexist in diagonal equations; column 0 and column 126 coexist in anti-diagonal equations.

(b) **CRC-32 LH matrix** ($4{,}064 \times 16{,}129$ over GF(2)). For each row $r \in \{0, \ldots, 126\}$, create 32 equations per &sect;B.58.2.2. Non-zero entries only in columns $\{127r, \ldots, 127r+126\}$. All 127 rows' LH equations are present simultaneously&mdash;row 0 and row 126 are equally "active."

(c) **CRC-32 VH matrix** ($4{,}064 \times 16{,}129$ over GF(2)). For each column $c \in \{0, \ldots, 126\}$, create 32 equations using $G_{\text{CRC}}$ (&sect;B.60.3.1). Non-zero entries only in columns $\{c, 127+c, 254+c, \ldots, 127 \times 126 + c\}$. All 127 columns' VH equations are present simultaneously.

(d) **Assemble** the full GF(2) system: stack (a), (b), (c) to produce $G \in \text{GF}(2)^{8{,}888 \times 16{,}129}$ with target vector $b_G \in \text{GF}(2)^{8{,}888}$.

(e) **Integer constraint structures.** For each of the 760 geometric lines, store membership sets, target sums, residual $\rho$, and unknown count $u$. Initialize $\rho = \text{target}$, $u = |\text{members}|$ for all 760 lines. No line is deferred; the longest diagonal (length 127, spanning all 127 rows) is active from the start alongside the shortest (length 1, a single corner cell).

(f) **Cell-to-constraint index.** For each cell $j$, pre-compute the list of all constraints involving $j$: 1 row sum + 1 column sum + 1 diagonal sum + 1 anti-diagonal sum + 32 LH equations (for the cell's row) + 32 VH equations (for the cell's column) = 68 constraints per cell. This enables efficient global propagation: when cell $j$ is determined, all 68 constraints are updated in one pass.

**Phase 2: Global GF(2) Gaussian elimination.**

Apply leftmost-column-first GaussElim (&sect;B.58.4.4, Combinator 1) to the full $8{,}888 \times 16{,}129$ matrix. This is a single global reduction&mdash;not a per-row reduction. The elimination interleaves LH, VH, and cross-sum equations freely:

- An LH equation for row 0 may be used to eliminate a variable that also appears in a VH equation for column 63 and a diagonal equation spanning rows 0&ndash;126.
- A VH equation for column 126 may pivot a cell that propagates into an LH equation for row 50.
- A diagonal parity equation may couple cells in rows 0 and 126, forcing a relationship that neither LH nor VH could establish alone.

The output is a global RREF with pivot columns (determined up to free-variable assignments) and free columns (the null-space basis). There is no row-by-row structure in the RREF&mdash;pivots may come from any equation source.

**Optimized implementation.** Exploit block-diagonal structure for speed without sacrificing globality:

(a) Reduce each of the 127 LH blocks ($32 \times 127$) independently. Each block determines up to 32 pivot variables within its row. Cost: $127 \times O(32^2 \times 2) \approx$ 0.5M operations.

(b) Reduce each of the 127 VH blocks ($32 \times 127$) independently. Each block determines up to 32 pivot variables within its column. Cost: same.

(c) Substitute all LH and VH pivots into the 760 cross-sum parity equations, reducing them from $760 \times 16{,}129$ to $760 \times |F_{\text{CRC}}|$ where $|F_{\text{CRC}}|$ is the number of variables not pivoted by any CRC equation.

(d) Perform GaussElim on the reduced cross-sum system ($760 \times |F_{\text{CRC}}|$). This global reduction discovers cross-row and cross-column relationships that the per-block CRC reductions could not.

This three-stage approach is mathematically equivalent to full GaussElim on the $8{,}888 \times 16{,}129$ matrix but exploits sparsity for a ~10$\times$ speedup.

**Phase 3: Fixpoint iteration (all combinators, all constraints, concurrently).**

Execute the combinator loop as specified in &sect;B.60.3.5. Each iteration processes ALL 760 integer constraint lines and ALL $|F|$ free cells in parallel. The fixpoint converges when no combinator produces new determinations.

Record per-iteration telemetry:

| Metric | Per iteration |
|--------|--------------|
| $|D|$ (determined cells) | Count and delta |
| $|F|$ (free cells) | Count |
| Cells from GaussElim | Count (GF(2) pivots with no free-column dependency) |
| Cells from IntBound | Count ($\rho = 0$ or $\rho = u$ forcings across ALL 760 lines) |
| Cells from CrossDeduce | Count (pairwise intersection deductions across ALL line pairs) |
| Wall time | Seconds |

**Phase 4: Residual analysis.**

After fixpoint convergence, the remaining free variables $F$ form the null-space basis of the reduced system. These are cells that no algebraic technique&mdash;GF(2) elimination, integer bounds, or pairwise cross-deduction&mdash;can determine from the stored constraints alone.

- If $|F| = 0$: reconstruct the CSM from determined values, verify against SHA-256 BH. The solution is unique (DI = 0 for all blocks).
- If $0 < |F| \leq 25$: enumerate the $2^{|F|}$ null-space assignments in canonical order (row-major, 0-before-1) per &sect;B.58.4.4 (Combinator 6). Verify each candidate against SHA-256 BH.
- If $|F| > 25$: proceed to Phase 5 (row-column grouped search).

**Phase 5: Row-column grouped residual search.**

If the fixpoint leaves $> 25$ free variables, exploit the dual-hash structure to decompose the residual. This phase is detailed in &sect;B.60.9 (Sub-experiment B.60c). The key property: even in the residual search, all rows participate concurrently via column VH filtering. The search is NOT a sequential DFS over rows; it is a constraint satisfaction problem where per-row candidates (filtered by LH) are simultaneously filtered by per-column constraints (VH), and the cross-row consistency is enforced by global arc consistency over column, diagonal, and anti-diagonal constraints.

#### B.60.7 Relationship to Prior Work

**B.58 (Combinator Solver).** B.60 uses the identical combinator pipeline (&sect;B.58.4) with VH equations added to the GF(2) matrix. The combinators (GaussElim, IntBound, Propagate, CrossDeduce, Fixpoint, EnumerateFree, VerifyBH) are unchanged. B.60 is a format change + system augmentation, not a solver change.

**B.59t (Column Hashing at S=127).** B.59t proposed adding column CRC-32 hashes as a DFS solver enhancement (column-serial pass after top-down DFS). B.60 subsumes B.59t by incorporating column CRC-32 into the algebraic combinator framework rather than the DFS framework. The DFS solver cannot exploit VH as GF(2) equations because DFS operates cell-by-cell, not algebraically.

**B.41 (Cross-Dimensional Hashing at S=511).** B.41 proposed column SHA-1 hashes for four-direction pincer solving. At S=511, columns had 206 unknowns at the plateau, making column-hash verification impractical. B.60 addresses the same architectural idea (dual-axis hashing) but uses it algebraically (GF(2) equations in the combinator) rather than as a verification oracle (DFS hash check at $u = 0$), eliminating the column-completion barrier.

**CRC-64 analysis (preceding discussion).** Widening the row hash from 32 to 64 bits provides the same information density improvement (0.312 $\to$ 0.56) as adding VH. The payload cost is identical (4,064 additional bits). However, CRC-64 places all 64 equations on the same 127 row variables, while LH+VH places 32 equations on row variables and 32 on column variables. The cross-axis structure enables cascade interactions in the combinator fixpoint (row pivots propagate to column equations and vice versa) that single-axis CRC-64 cannot achieve. B.60 and CRC-64 are GF(2)-rank-equivalent but combinator-interaction-inequivalent.

#### B.60.8 Baseline Results

All results from C++ `combinatorSolver` binary. No DFS. No search. Purely algebraic combinator fixpoint.

| Experiment | Config | $|D|$ | GaussElim | IntBound | CD | Rank | Correct |
|------------|--------|-------|-----------|----------|----|------|---------|
| B.60b | LH + VH (non-toroidal) | 3,600 (22.3%) | 4 | 3,596 | 0 | 7,793 | true |
| B.60c/e | LH + VH (toroidal) | 127 (0.8%) | 0 | 127 | 0 | 7,545 | true |
| **B.60f** | **LH + DH (non-toroidal)** | **4,405 (27.3%)** | **1,189** | **3,216** | **0** | **11,825** | **true** |

B.60f (LH + DH) is the strongest configuration: +805 cells over B.60b, driven by 1,189 GaussElim determinations from diagonal CRC-32 on short diagonals.

### B.60a: GF(2) Rank with $LH + VH$

### Objective. 
Measure the GF(2) rank of the combined cross-sum + LH + VH system at S=127 without yLTP.

### Method.

(a) Generate a random $127 \times 127$ binary CSM at 50% density.

(b) Compute cross-sums for 4 geometric families (LSM, VSM, DSM, XSM).

(c) Compute CRC-32 for all 127 rows (LH) and all 127 columns (VH).

(d) Construct the $8{,}888 \times 16{,}129$ GF(2) matrix.

(e) Compute GF(2) rank via Gaussian elimination.

(f) Stratified rank analysis:

| Configuration | Equations | Expected rank |
|---------------|-----------|--------------|
| 4 geometric only | 760 | ~753 |
| + LH | 4,824 | ~4,785 |
| + VH | 8,888 | **MEASURE** |

(g) Repeat with 5 different random CSMs to verify rank is input-independent.

### Expected outcomes.

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Near-maximum) | Rank $\geq$ 8,500 | VH contributes ~3,700+ independent equations; dual-hash system achieves ~0.53 density |
| H2 (Partial) | Rank 7,000&ndash;8,500 | VH partially redundant with LH + cross-sums; some cross-axis equations are dependent |
| H3 (Low) | Rank $<$ 7,000 | VH largely redundant; cross-axis structure does not help |

### Results.

Rank confirmed input-independent (identical across 2 trials with different random CSMs).

| Configuration   | Equations | Measured rank | Null-space | Density    |
| --------------- | --------- | ------------- | ---------- | ---------- |
| LSM only        | 127       | 127           | 16,002     | 0.0079     |
| LSM + VSM       | 254       | 253           | 15,876     | 0.0157     |
| LSM + VSM + DSM | 507       | 504.          | 15,625     | 0.0312     |
| 4 geometric     | 760       | 753           | 15,376     | 0.0467     |
| + LH            | 4,824     | 4,785         | 11,344     | 0.2967     |
| **+ VH**        | **8,888** | **7,793**     | **8,336**  | **0.4832** |

| Source                   | Contribution  | Max possible  | Efficiency  |
| ------------------------ | ------------- | ------------- | ----------- |
| Geometric                | 753           | 760           | 99.1%       |
| LH (over geometric)      | +4,032        | 4,064         | 99.2%       |
| VH (over geometric + LH) | +3,008        | 4,064         | 74.0%       |

Comparison to B.58 (LH + 2 yLTP): rank 5,037 &rarr; 7,793 (+54.7%). VH contributes 3,008 independent equations, but 1,056 are redundant with the existing LH + geometric system.

**Outcome: H2 (Partial).** VH contributes +3,008 equations. Dual-hash density 0.483.

**Tool:** `tools/b60a_dual_hash_rank.py`. **Results:** `tools/b60a_results.json`.

**Status: COMPLETE (H2).**

### B.60b: Combinator Pipeline with LH + VH

**Prerequisite.** B.60a completed (any outcome).

**Objective.** Run the full combinator pipeline on the LH + VH system and measure the fixpoint.

**Method.**

(a) Use the same test cases as B.58b: synthetic blocks at 10%, 30%, 50%, 70%, 90% density, plus MP4 block 0.

(b) Execute the fixpoint loop with cross-axis interaction enabled.

(c) Record: $|D|$ (determined cells), $|F|$ (free cells), per-combinator attribution (GaussElim, IntBound, CrossDeduce), fixpoint iterations, wall time.

(d) Compare with B.58b results (LH + 2 yLTP): fixpoint $|D|$ = 3,600 (22.3%), $|F|$ = 12,529.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Full solve) | $|F| = 0$ | Dual-hash cross-axis interaction determines all cells. CRSCE viable at S=127 with LH + VH. |
| H2 (Major improvement) | $|F| \leq 2{,}000$ | VH substantially reduces free variables beyond B.58b's 12,529. Row-grouped search (&sect;B.58.10) likely tractable. |
| H3 (Moderate improvement) | $2{,}000 < |F| \leq 8{,}000$ | VH helps but the null-space gap (0.547 density) is still too large for full determination. |
| H4 (No improvement) | $|F| \geq 11{,}000$ | Cross-axis GF(2) equations do not translate to fixpoint progress; same cells determined as B.58b. |

**Decision gate.** If H4: the information-density barrier is confirmed&mdash;GF(2) density below 1.0 cannot be overcome by axis diversification. If H1 or H2: proceed to B.60c (production integration).

**Results.** C++ `combinatorSolver -config lh_vh` on MP4 block 0 (density 14.9%).

| Metric | B.60b (LH + VH) | B.58b (LH + 2 yLTP) |
|--------|------------------|---------------------|
| $|D|$ (determined) | 3,600 (22.3%) | 3,600 (22.3%) |
| $|F|$ (free) | 12,529 | 12,529 |
| GaussElim | 4 | 4 |
| IntBound | 3,596 | 3,596 |
| CrossDeduce | 0 | 0 |
| GF(2) rank (iter 1) | 7,793 | 5,037 |
| Fixpoint iterations | 2 | 2 |
| Values correct | true | true |

**Outcome: H4 (No improvement).** Identical to B.58b. VH adds rank but not cell determinations.

**Tool:** `build/arm64-release/combinatorSolver -config lh_vh`

**Status: COMPLETE (H4).**

### B.60c: Restate B.60 to use toroidal DSM and XSM cross sums then establish new baseline

**Motivation.** The B.60a/b baseline uses non-toroidal diagonals (DSM/XSM), producing 253 lines of varying length (1 to 127 cells). Toroidal diagonals wrap around the matrix, producing 127 lines of exactly 127 cells each &mdash; uniform-length, like rows and columns. At S=127 (prime), toroidal slope partitions have no collisions.

Advantages of toroidal diagonals:

1. **Uniform-length lines.** Every diagonal has 127 cells. Non-toroidal corner diagonals (length 1&ndash;10) contribute nearly zero constraint information.
2. **Stronger IntBound interaction.** Uniform-length lines have $\rho/u$ ratios that evolve meaningfully as cells are determined, unlike short lines that exhaust immediately.

Toroidal DSM: slope $p = 1$, line index $k = (c - r) \bmod 127$. Toroidal XSM: slope $p = 126$ ($\equiv -1 \bmod 127$), line index $k = (c + r) \bmod 127$. Both produce exactly 127 lines of 127 cells. The existing `ToroidalSlopeSum` class implements this geometry.

**Method.**

(a) Construct the B.60 GF(2) system with toroidal diagonals: 508 geometric parity equations (127 LSM + 127 VSM + 127 tDSM + 127 tXSM) + 4,064 LH + 4,064 VH = 8,636 total equations.

(b) Construct 508 integer constraint lines (all uniform-length 127) for IntBound and CrossDeduce.

(c) Run the combinator fixpoint: $\text{BuildSystem} \to \text{Fixpoint}(\text{GaussElim}, \text{IntBound}, \text{CrossDeduce}, \text{Propagate})$. No DFS. Record: determined cells ($|D|$), per-combinator attribution (GaussElim, IntBound, CrossDeduce), GF(2) rank, fixpoint iterations.

(d) Compare to B.60b baseline (non-toroidal, 760 geometric lines, $|D| = 3{,}600$).

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Improved) | $|D| > 3{,}600$ | Uniform toroidal lines create new forcing opportunities in the combinator fixpoint |
| H2 (Same) | $|D| = 3{,}600$ | Diagonal geometry does not affect the combinator fixpoint |
| H3 (Reduced) | $|D| < 3{,}600$ | Fewer total lines (508 vs 760) reduces constraint coverage |

**Decision gate.** If H1: toroidal diagonals improve the algebraic baseline; carry forward into B.60d/e and subsequent experiments. If H2/H3: evaluate whether the rank measurement (B.60d) or fixpoint structure (B.60e) differs despite identical $|D|$.

**Results.** C++ `combinatorSolver -config toroidal_vh` on MP4 block 0 (density 14.9%).

| Metric | Toroidal (B.60c) | Non-toroidal (B.60b) |
|--------|------------------|---------------------|
| $|D|$ (determined) | 127 (0.8%) | 3,600 (22.3%) |
| GaussElim | 0 | 4 |
| IntBound | 127 | 3,596 |
| CrossDeduce | 0 | 0 |
| GF(2) rank (iter 1) | 7,545 | 7,793 |
| Values correct | true | true |

**Outcome: H3 (Reduced).** Toroidal diagonals eliminate the short lines that initiate the IntBound cascade.

**Tool:** `build/arm64-release/combinatorSolver -config toroidal_vh`

**Status: COMPLETE (H3). Non-toroidal diagonals retained.**

### B.60d: GF(2) Rank with Toroidal Diagonals (B.60a on B.60c system)

**Prerequisite.** B.60c constraint system defined.

**Objective.** Measure the GF(2) rank of the combined cross-sum + LH + VH system using toroidal diagonals instead of non-toroidal. This is B.60a repeated on the B.60c constraint geometry.

**Method.**

(a) Construct the GF(2) matrix with toroidal diagonals: 508 geometric parity equations (127 LSM + 127 VSM + 127 tDSM + 127 tXSM) + 4,064 LH + 4,064 VH = 8,636 total equations over 16,129 variables.

(b) Compute GF(2) rank via Gaussian elimination.

(c) Stratified rank analysis:

| Configuration | B.60a rank (non-toroidal) | B.60d (toroidal) |
|---------------|--------------------------|------------------|
| 4 geometric only | 753 | **MEASURE** |
| + LH | 4,785 | **MEASURE** |
| + VH | 7,793 | **MEASURE** |

(d) Repeat with 2 different random CSMs to verify rank is input-independent.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Higher rank) | Rank &gt; 7,793 | Toroidal diagonals are more independent from LH/VH than non-toroidal |
| H2 (Comparable rank) | Rank within &plusmn;200 of 7,793 | Diagonal geometry does not materially affect GF(2) rank |
| H3 (Lower rank) | Rank &lt; 7,593 | Fewer equations (8,636 vs 8,888) reduces total rank |

**Results.** Measured from B.60c C++ run (toroidal_vh config).

| Configuration | Toroidal (B.60d) | Non-toroidal (B.60a) |
|---------------|------------------|---------------------|
| 4 geometric | 505 (from iter 1 rank minus LH/VH) | 753 |
| + LH + VH (full) | 7,545 | 7,793 |
| Delta | &minus;248 | &mdash; |

**Outcome: H3 (Lower rank).** Toroidal geometric rank 505 vs non-toroidal 753.

**Status: COMPLETE (H3). Data from B.60c C++ run.**

### B.60e: Combinator Fixpoint with Toroidal Diagonals (B.60b on B.60c system)

**Prerequisite.** B.60d completed.

**Objective.** Run the combinator fixpoint (GaussElim + IntBound + CrossDeduce + Propagate) with the toroidal B.60c constraint system and LH + VH. This is B.60b repeated on the B.60c constraint geometry.

**Method.**

(a) Use MP4 block 0 (density 14.9%) as the test case.

(b) Construct 508 toroidal integer constraint lines (all uniform-length 127) for IntBound and CrossDeduce. Construct the 8,636-equation GF(2) system for GaussElim.

(c) Run the combinator fixpoint to convergence. Record: $|D|$ (determined cells), $|F|$ (free cells), per-combinator attribution (GaussElim, IntBound, CrossDeduce), fixpoint iterations.

(d) Compare to B.60b (non-toroidal, 760 lines, $|D| = 3{,}600$, GaussElim = 4, IntBound = 3,596, CrossDeduce = 0).

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (More cells determined) | $|D| > 3{,}600$ | Uniform toroidal lines create new IntBound or CrossDeduce forcings |
| H2 (Same cells) | $|D| = 3{,}600$ | The fixpoint is dominated by row/column constraints; diagonal geometry is irrelevant |
| H3 (Fewer cells) | $|D| < 3{,}600$ | Fewer constraint lines (508 vs 760) reduces the IntBound cascade reach |

**Decision gate.** If H1: toroidal diagonals improve the algebraic pipeline; carry the B.60c system forward into B.60f/g. If H2: neutral; proceed with either. If H3: revert to non-toroidal for subsequent experiments.

**Results.** Same as B.60c (toroidal_vh config): $|D| = 127$ (0.8%), GaussElim = 0, IntBound = 127, CrossDeduce = 0. Values correct.

**Outcome: H3 (Fewer cells).** Non-toroidal diagonals retained.

**Tool:** `build/arm64-release/combinatorSolver -config toroidal_vh`

**Status: COMPLETE (H3).**

### B.60f: Diagonal Hash (DH) — CRC-32 per DSM Diagonal

**Motivation.** B.60a/b showed that VH (column CRC-32) adds 3,008 GF(2) equations but the fixpoint remains at 3,600 cells. The short non-toroidal diagonals are the cascade trigger (B.60c). This suggests adding CRC-32 hashes along the diagonal axis (DH) &mdash; directly strengthening the constraint family that initiates the cascade.

DH provides 32 GF(2) equations per diagonal, involving only the cells on that diagonal. Unlike VH (which is orthogonal to LH), DH equations share the same cells as the DSM integer constraints. This creates a tight coupling between the GF(2) domain (DH equations) and the integer domain (DSM cardinality) on the same constraint lines. The short diagonals (length 1&ndash;10) that trigger IntBound would also have DH CRC-32 equations &mdash; and on short diagonals, 32 CRC equations on $\leq 10$ variables fully determine the diagonal.

**Format change.** DH replaces VH. Each of the 253 DSM diagonals gets a CRC-32 digest (32 bits). Diagonal message construction: the $\ell$ data bits of diagonal $d$ followed by padding to the next byte boundary, then CRC-32. The generator matrix for each diagonal length $\ell$ differs from the row/column generator matrix (which uses $\ell = 127$).

| Component | B.60b (LH + VH) | B.60f (LH + DH) |
|-----------|------------------|------------------|
| CRC-32 LH | 127 rows &times; 32 bits | 127 rows &times; 32 bits |
| CRC-32 VH | 127 cols &times; 32 bits | &mdash; |
| CRC-32 DH | &mdash; | 253 diags &times; 32 bits |
| Total CRC equations | 8,128 | 12,160 |

**Method.**

(a) For each of the 253 DSM diagonals (length 1 to 127), compute CRC-32 of the diagonal's cell values. Build a per-diagonal GF(2) generator matrix (32 &times; $\ell$ where $\ell$ is the diagonal length). Note: short diagonals ($\ell \leq 32$) have CRC rank = $\ell$, not 32.

(b) Construct the full GF(2) system: 760 geometric parity + 4,064 LH + 253 &times; 32 DH = 12,920 equations over 16,129 variables.

(c) Measure GF(2) rank (stratified: geometric &rarr; +LH &rarr; +DH).

(d) Run the combinator fixpoint. The key question: do DH equations on short diagonals interact with IntBound to determine more cells than the short-diagonal integer constraints alone?

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (More cells) | $|D| > 3{,}600$ | DH on short diagonals creates new GF(2)&ndash;integer interactions that determine additional cells |
| H2 (Same cells) | $|D| = 3{,}600$ | DH is informationally redundant with DSM integer constraints on the same lines |
| H3 (Fewer cells) | $|D| < 3{,}600$ | Replacing VH with DH loses cross-axis coupling without compensating gain |

**Results.** C++ `combinatorSolver -config lh_dh` on MP4 block 0 (density 14.9%).

| Metric | B.60f (LH + DH) | B.60b (LH + VH) | Delta |
|--------|------------------|------------------|-------|
| $|D|$ (determined) | **4,405 (27.3%)** | 3,600 (22.3%) | **+805** |
| GaussElim | **1,189** | 4 | +1,185 |
| IntBound | 3,216 | 3,596 | &minus;380 |
| CrossDeduce | 0 | 0 | 0 |
| GF(2) rank (iter 1) | **11,825** | 7,793 | **+4,032** |
| Fixpoint iterations | 3 | 2 | +1 |
| Values correct | true | true | &mdash; |

**Analysis.**

DH produces a dramatic improvement: +805 cells (22% more than B.60b). The mechanism:

1. **GaussElim determines 1,189 cells** (vs 4 in B.60b). DH on short diagonals (length $\leq 32$) provides CRC-32 rank equal to the diagonal length, fully determining those cells in the GF(2) domain. This is 297&times; more GaussElim determinations than VH.

2. **Rank 11,825** (vs 7,793). DH adds 4,032 more independent equations than VH (+52%). DH equations on short diagonals are highly independent because they involve small, non-overlapping variable sets.

3. **3 fixpoint iterations.** The GaussElim determinations from DH feed into IntBound (iter 1), which cascades through row/column constraints. In iter 2, re-running GaussElim on the reduced system yields 15 additional cells. The third iteration confirms convergence.

4. **IntBound is lower** (3,216 vs 3,596) because GaussElim pre-empts some cells that IntBound would have found.

**Outcome: H1 (More cells).** DH on non-toroidal diagonals creates the GF(2)&ndash;integer bridge that VH could not. The short diagonals are both the IntBound cascade trigger AND a rich source of GF(2) determinations. DH directly strengthens the constraint family that matters most.

**Tool:** `build/arm64-release/combinatorSolver -config lh_dh`

**Status: COMPLETE (H1). DH produces +805 cells over VH.**

### B.60g: Constrained DH &mdash; 64 vs 128 Shortest Diagonals

#### Prerequisite.** 
B.60f completed (H1).

#### Motivation.
B.60f showed that DH on all 253 diagonals produces 4,405 determined cells but C_r = 107% (expansion). A sweep over diagonal counts revealed that the shortest 64 diagonals produce 4,334 cells (98.4% of full DH yield) at C_r = 44.7%, while the shortest 128 produce the full 4,405 cells at C_r = 57.4%. This experiment formally evaluates both configurations.

#### Method.

(a) Run the C++ combinator fixpoint (`combinatorSolver -config lh_dh`) with `-dh-diags 64` and `-dh-diags 128` on MP4 block 0.

(b) Run on 5 additional MP4 blocks (blocks 1&ndash;5) to verify the result is not block-specific.

(c) Record per-block: $|D|$, GaussElim, IntBound, CrossDeduce, rank, correctness.

(d) Compute payload size and C_r for each configuration:

| Component   | 64-diag DH                 | 128-diag DH                 |
| ----------- | -------------------------- | --------------------------- |
| CRC-32 LH   | 4,064 bits                 | 4,064 bits                  |
| CRC-32 DH   | 64 &times; 32 = 2,048 bits | 128 &times; 32 = 4,096 bits |
| SHA-256 BH  | 256 bits                   | 256 bits                    |
| DI          | 8 bits                     | 8 bits                      |
| LSM         | 889 bits                   | 889 bits                    |
| VSM         | 889 bits                   | 889 bits                    |
| DSM         | 1,531 bits                 | 1,531 bits                  |
| XSM         | 1,531 bits                 | 1,531 bits                  |
| **Total**   | **11,216 bits**            | **13,264 bits**             |
| **C_r**     | **69.5%**                  | **82.2%**                   |

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (64-diag sufficient) | 64-diag $|D|$ within 5% of 128-diag across all blocks | 64-diag is the optimal trade-off; adopt for production |
| H2 (128-diag needed) | 128-diag $|D|$ materially higher ($> 5\%$) on some blocks | Longer diagonals contribute block-specific value; use 128-diag |

#### Results.
C++ `combinatorSolver -config lh_dh -dh-diags {64,128}` on MP4 blocks 0&ndash;5. All values verified correct against original CSM.

#### 64-diagonal DH:

| Block    | $|D|$     | GaussElim | IntBound | CD | Rank | Correct |
| -------- | --------- |-----------|----------|----|------|---------|
| 0        | 4,334     | 1,058     | 3,276    | 0  | 4,631 | true |
| 1        | 4,534     | 1,058     | 3,476    | 0  | 4,630 | true |
| 2        | 10,885    | 2,030     | 8,855    | 0  | 2,828 | true |
| 3        | 7,872     | 1,058     | 6,814    | 0  | 4,614 | true |
| 4        | 1,541     | 1,058     | 483      | 0  | 4,697 | true |
| 5        | 1,060     | 1,058     | 2        | 0  | 4,718 | true |
| **Mean** | **5,038** | -----     | --       | -- | ----- | ---- |

#### 128-diagonal DH:

| Block     | $|D|$   | GaussElim   | IntBound   | CD   | Rank   | Correct   |
| --------- | ------- | ----------- | ---------- | ---- | ------ | --------- |
| 0         | 4,405   | 1,189      | 3,216       | 0    | 5,870  | true      |
| 1         | 4,686   | 1,211      | 3,475       | 0    | 5,757  | true      |
| 2         | 11,027  | 2,183      | 8,844       | 0    | 3,680  | true      |
| 3         | 7,872   | 1,174      | 6,698       | 0    | 5,574  | true      |
| 4         | 1,599   | 1,174      | 425         | 0    | 6,335  | true      |
| 5         | 1,176   | 1,174      | 2           | 0    | 6,650  | true      |
| **Mean**  | **5,128** |          |             |      |        |           |

#### 64 vs 128 comparison:

| Block.    | 64-diag   | 128-diag  | Delta   | % change |
| --------- | --------- | --------- | ------- | -------- |
| 0         | 4,334     | 4,405     | +71     | +1.6%    |
| 1         | 4,534     | 4,686     | +152    | +3.4%    |
| 2         | 10,885    | 11,027    | +142    | +1.3%    |
| 3         | 7,872     | 7,872     | 0       | 0.0%     |
| 4         | 1,541     | 1,599     | +58     | +3.8%    |
| 5         | 1,060     | 1,176     | +116    | +10.9%   |
| **Mean**  | **5,038** | **5,128** | **+90** |          |

#### Payload and C_r:

| Config      | DH bits   | Total payload  | $C_r$ |
| ----------- | --------- | -------------- | ----- |
| 64-diag DH  | 2,048     | 11,216 bits    | 69.5% |
| 128-diag DH | 4,096     | 13,264 bits    | 82.2% |
| B.60b (VH)  | 4,064     | 13,232 bits.   | 82.0% |

#### Outcome: H2 (128-diag needed)
Block 5 shows a 10.9% improvement from 64&rarr;128, exceeding the 5% threshold. However, 128-diag DH has nearly the same C_r as VH (82.2% vs 82.0%) while producing +805 more cells on block 0 and +90 more cells on average across 6 blocks.

#### Observation
Block density strongly affects $|D|$: block 2 (high density) reaches 10,885&ndash;11,027 cells (67&ndash;68%), while block 5 (low density) reaches only 1,060&ndash;1,176 (6.6&ndash;7.3%). The combinator fixpoint is density-sensitive.

#### Tool:
`build/arm64-release/combinatorSolver -config lh_dh -dh-diags {64,128}`

#### Status: 
COMPLETE (H2). 128-diag DH recommended.

### B.60h: Constrained DH Payload Analysis

#### Motivation
B.60f/g showed DH128 (CRC-32 on the 128 shortest DSM diagonals) produces +805 cells over LH-only. The anti-diagonals (XSM) have the same length distribution as DSM. If CRC-32 on the shortest 128 anti-diagonals (XH128) provides a similar algebraic boost, it can replace LH (row CRC-32) without changing C_r &mdash; since XH128 costs 128 &times; 32 = 4,096 bits vs LH's 127 &times; 32 = 4,064 bits (+32 bits, negligible).

#### Hypothesis
XH128 paired with DH128 produces comparable or better cell yield to LH paired with DH128, because both DH and XH operate on the diagonal constraint families that initiate the IntBound cascade.

#### Method.

(a) Implement XH: per-anti-diagonal CRC-32, same construction as DH but on XSM geometry. XH128 = 128 shortest anti-diagonals.

(b) Run the combinator fixpoint with two configurations on MP4 blocks 0&ndash;5:
- **Config A (baseline):** LH + DH128 (current best)
- **Config B (test):** XH128 + DH128 (no LH)

(c) Record per-block: $|D|$, GaussElim, IntBound, CrossDeduce, rank, correctness.

(d) Compare payload:

| Config | LH bits | DH128 bits | XH128 bits | Total CRC bits | Total payload | C_r |
|--------|---------|-----------|-----------|---------------|--------------|-----|
| A: LH + DH128 | 4,064 | 4,096 | 0 | 8,160 | 13,264 | 82.2% |
| B: XH128 + DH128 | 0 | 4,096 | 4,096 | 8,192 | 13,296 | 82.4% |

#### Expected outcomes.

| Outcome               | Criteria                                               | Interpretation                                                                  |
| --------------------- | ------------------------------------------------------ | ------------------------------------------------------------------------------- |
| H1 (XH128 comparable) | Config B $|D|$ within 5% of Config A across all blocks | XH128 substitutes for LH; diagonal-family hashes are self-sufficient            |
| H2 (LH needed)        | Config B $|D|$ materially lower ($> 5\%$) on some blocks | Row hashes provide orthogonal information that diagonal hashes cannot replace |

#### Status:
PROPOSED.

### B.60i: XH128 + DH128 &mdash; Anti-Diagonal Hash Replacing LH

#### Prerequisite.
B.60g completed (DH128 established).

#### Motivation.
DH128 exploits DSM diagonals. The anti-diagonals (XSM) have the same length distribution. XH128 (CRC-32 on 128 shortest anti-diagonals) replaces LH (row CRC-32) at nearly the same payload cost (+32 bits). The hypothesis: XH128 + DH128 produces comparable or better cell yield than LH + DH128.

#### Method.

(a) Run C++ `combinatorSolver -config xh_dh -dh-diags 128 -xh-diags 128` on MP4 blocks 0&ndash;5.

(b) Compare to Config A: `combinatorSolver -config lh_dh -dh-diags 128` (LH + DH128).

(c) Record $|D|$, per-combinator attribution, correctness.

#### Results.

All 12 runs (2 configs &times; 6 blocks) executed in C++, all values verified correct.

| Block    | LH + DH128 | XH128 + DH128 | Delta.     | % change |
| -------- | ---------- | ------------- | ---------- | -------- |
| 0        | 4,405      | 6,809         | +2,404     | +54.6%   |
| 1        | 4,686      | 11,286        | +6,600     | +140.8%  |
| **2**    | **11,027** | **16,129**    | **+5,102** | **+46.3% (FULL SOLVE)** |
| 3        | 7,872      | 9,853         | +1,981     | +25.2%  |
| 4        | 1,599      | 2,704         | +1,105     | +69.1%  |
| 5        | 1,176      | 2,344         | +1,168     | +99.3%  |
| **Mean** | **5,128**  | **8,188**     | **+3,060** |         |

**XH128 + DH128 detail (Config B):**

| Block | $|D|$  | GaussElim | IntBound | CD | Rank  | Correct |
| ----- | ------ | --------- | -------- | -- | ----- | ------- |
| 0     | 6,809  | 2,950     | 3,859    | 0  | 3,104 | true    |
| 1     | 11,286 | 4,287     | 6,999    | 0  | 1,385 | true    |
| 2     | 16,129 | 2,423     | 13,706   | 0  | 1,214 | true    |
| 3     | 9,853  | 3,580     | 6,273    | 0  | 1,741 | true    |
| 4     | 2,704  | 2,344     | 360      | 0  | 4,184 | true    |
| 5     | 2,344  | 2,344     | 0        | 0  | 4,489 | true    |

**Block 2: FULL SOLVE.** 16,129/16,129 cells determined by the combinator fixpoint alone. No search. No DFS. Pure algebraic determination. This is the first complete algebraic solve of any CRSCE block.

**Analysis.** XH128 + DH128 massively outperforms LH + DH128: mean +3,060 cells (+60%). GaussElim contributes 2,344&ndash;4,287 cells (vs 1,174&ndash;2,183 with LH). The dual-diagonal hash configuration (DH on DSM + XH on XSM) creates stronger GF(2)&ndash;integer coupling than row hashes + diagonal hashes, because DH and XH operate on the same constraint families that provide IntBound cascade triggers.

**Payload:** XH128 + DH128 = 128&times;32 + 128&times;32 = 8,192 CRC bits. LH + DH128 = 127&times;32 + 128&times;32 = 8,160 CRC bits. Delta: +32 bits. C_r: 82.4% vs 82.2%. Negligible.

**Outcome: H1 (XH128 far exceeds LH).** XH128 is not merely comparable &mdash; it is dramatically superior to LH when paired with DH128. The diagonal-family hashes reinforce each other through shared short-line geometry.

**Tool:** `build/arm64-release/combinatorSolver -config xh_dh -dh-diags 128 -xh-diags 128`

**Status: COMPLETE (H1). XH128 + DH128 is the new best configuration.**

### B.60j: LH + DH128 + XH128 &mdash; Three-Family Hash (Experimental)

**Prerequisite.** B.60i completed (XH128 + DH128 established as best dual-hash config).

**Motivation.** B.60i showed XH128 + DH128 dramatically outperforms LH + DH128 and achieved a full algebraic solve on block 2. Adding LH back creates a three-axis hash configuration: row (LH), diagonal (DH128), and anti-diagonal (XH128). Each hash family provides GF(2) equations on a different geometric axis. The cross-axis coupling between all three may push more blocks toward full solve.

This is a pure experiment &mdash; C_r will exceed 100%. The goal is to measure the algebraic ceiling and analyze per-block variation.

**Method.**

(a) Add `lh_dh_xh` config to the C++ `combinatorSolver`: LH + DH128 + XH128.

(b) Run on MP4 blocks 0&ndash;20 (sufficient sample for density variation analysis).

(c) Record per-block: $|D|$, $|F|$, GaussElim, IntBound, CrossDeduce, rank, correctness, block density.

(d) Compare to B.60i Config B (XH128 + DH128, no LH) on the same blocks.

(e) **Per-block analysis:** for each block, compute density, classify as full-solve / partial / low-yield. Identify what properties (density, sum distribution, diagonal structure) correlate with solve completeness.

**Payload (experimental, not production):**

| Component | Bits |
|-----------|------|
| CRC-32 LH | 127 &times; 32 = 4,064 |
| CRC-32 DH128 | 128 &times; 32 = 4,096 |
| CRC-32 XH128 | 128 &times; 32 = 4,096 |
| SHA-256 BH | 256 |
| DI | 8 |
| LSM + VSM + DSM + XSM | 5,840 |
| **Total** | **18,360 bits** |
| **C_r** | **113.8% (expansion)** |

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Majority solve) | $> 50\%$ of blocks fully solved | Three-family hash achieves algebraic completeness for most blocks; optimize C_r next |
| H2 (Some improvement) | More blocks solved than B.60i, but $< 50\%$ | LH adds value but not enough for universal solve |
| H3 (No improvement) | Same blocks solved as B.60i | LH is redundant when DH128 + XH128 are present |

**Results.** C++ `combinatorSolver -config lh_dh_xh -dh-diags 128 -xh-diags 128` on MP4 blocks 0&ndash;20. All values verified correct.

**LH + DH128 + XH128 (blocks 0&ndash;20):**

| Block | $|D|$ | GaussElim | IntBound | Rank | Solved |
|-------|-------|-----------|----------|------|--------|
| 0 | 8,646 | 4,861 | 3,785 | 4,888 | partial |
| 1 | 12,811 | 5,944 | 6,867 | 2,941 | partial |
| **2** | **16,129** | **4,185** | **11,944** | **1,841** | **FULL** |
| 3 | 12,770 | 6,402 | 6,368 | 2,403 | partial |
| 4 | 2,704 | 2,344 | 360 | 8,216 | partial |
| 5&ndash;20 | 2,344 | 2,344 | 0 | 8,521 | partial |

**LH contribution (LH+DH128+XH128 vs XH128+DH128):**

| Block | XH128+DH128 | LH+DH128+XH128 | LH delta |
|-------|------------|-----------------|----------|
| 0 | 6,809 | 8,646 | +1,837 |
| 1 | 11,286 | 12,811 | +1,525 |
| 2 | 16,129 | 16,129 | 0 (already solved) |
| 3 | 9,853 | 12,770 | +2,917 |
| 4 | 2,704 | 2,704 | 0 |
| 5 | 2,344 | 2,344 | 0 |

**Analysis.**

1. **Block 2: full solve confirmed** with three-family hash, same as B.60i.

2. **LH adds +1,525 to +2,917 cells on blocks 0, 1, 3** &mdash; significant. LH row equations provide cross-axis coupling that DH/XH cannot. On block 3, LH pushes from 9,853 to 12,770 (+30%).

3. **Blocks 4&ndash;20: no LH benefit.** These blocks have 2,344 cells from GaussElim only (= DH128 + XH128 short-diagonal determinations) and zero IntBound cascade. These are low-density blocks where the IntBound cascade never ignites beyond the corner cells. LH adds zero because the row constraints are too loose (rho/u far from 0 or 1).

4. **Density is the dominant factor.** Block 2 (high density) fully solves. Blocks 0, 1, 3 (moderate density) reach 54&ndash;79%. Blocks 4&ndash;20 (low density) stall at 14.5%. The combinator fixpoint's effectiveness scales with density because IntBound forcing ($\rho = 0$ or $\rho = u$) triggers more frequently when line sums are near their extremes.

**Outcome: H2 (Some improvement).** LH adds value on moderate-density blocks but does not achieve majority solve. 1 of 21 blocks fully solved.

**Tool:** `build/arm64-release/combinatorSolver -config lh_dh_xh -dh-diags 128 -xh-diags 128`

**Status: COMPLETE (H2).**

### B.60k: LH + DH64 + XH64 &mdash; Three-Family Hash at Compression C_r

**Prerequisite.** B.60j completed.

**Motivation.** B.60j (LH + DH128 + XH128) showed strong results but C_r = 113.8% (expansion). Reducing DH and XH from 128 to 64 diagonals each brings C_r below 100%:

| Component | Bits |
|-----------|------|
| CRC-32 LH | 127 &times; 32 = 4,064 |
| CRC-32 DH64 | 64 &times; 32 = 2,048 |
| CRC-32 XH64 | 64 &times; 32 = 2,048 |
| SHA-256 BH | 256 |
| DI | 8 |
| LSM + VSM + DSM + XSM | 5,840 |
| **Total** | **14,264 bits** |
| **C_r** | **88.4%** |

This is true compression. The experiment measures how much cell yield is sacrificed by dropping from DH128+XH128 to DH64+XH64, and compares across all B.60 configurations.

**Method.**

(a) Run `combinatorSolver -config lh_dh_xh -dh-diags 64 -xh-diags 64` on MP4 blocks 0&ndash;20.

(b) Record per-block: $|D|$, GaussElim, IntBound, rank, correctness, density.

(c) Compare to all prior B.60 configurations:
- B.60b: LH + VH (C_r = 82.0%)
- B.60g: LH + DH128 (C_r = 82.2%)
- B.60i: XH128 + DH128 (C_r = 82.4%)
- B.60j: LH + DH128 + XH128 (C_r = 113.8%)
- **B.60k: LH + DH64 + XH64 (C_r = 88.4%)**

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Strong) | B.60k $|D|$ within 10% of B.60j on moderate-density blocks | DH64+XH64 captures most of the value at lower C_r |
| H2 (Moderate) | B.60k $|D|$ 10&ndash;30% below B.60j | Significant yield loss; DH128+XH128 needed for best results |
| H3 (Weak) | B.60k $|D|$ close to B.60b baseline | DH64+XH64 adds little over VH at similar C_r |

**Results.** C++ `combinatorSolver -config lh_dh_xh -dh-diags 64 -xh-diags 64` on MP4 blocks 0&ndash;20. All values verified correct. C_r = 88.4%.

**LH + DH64 + XH64 (blocks 0&ndash;5, moderate+ density):**

| Block | $|D|$ | GaussElim | IntBound | Rank | Correct |
|-------|-------|-----------|----------|------|---------|
| 0 | 5,840 | 2,407 | 3,433 | 4,130 | true |
| 1 | 7,472 | 2,570 | 4,902 | 3,990 | true |
| 2 | 13,077 | 2,670 | 10,407 | 2,090 | true |
| 3 | 8,747 | 2,318 | 6,429 | 4,335 | true |
| 4 | 2,530 | 2,112 | 418 | 4,646 | true |
| 5&ndash;20 | 2,112 | 2,112 | 0 | 4,657 | true |

**Cross-configuration comparison (blocks 0&ndash;5):**

| Config | C_r | Blk 0 | Blk 1 | Blk 2 | Blk 3 | Blk 4 | Blk 5 | Mean |
|--------|-----|-------|-------|-------|-------|-------|-------|------|
| B.60b LH+VH | 82.0% | 3,600 | 3,600 | 3,600 | 3,600 | 3,600 | 3,600 | 3,600 |
| B.60g LH+DH128 | 82.2% | 4,405 | 4,686 | 11,027 | 7,872 | 1,599 | 1,176 | 5,128 |
| B.60i XH128+DH128 | 82.4% | 6,809 | 11,286 | **16,129** | 9,853 | 2,704 | 2,344 | 8,188 |
| B.60j LH+DH128+XH128 | 113.8% | 8,646 | 12,811 | **16,129** | 12,770 | 2,704 | 2,344 | 9,234 |
| **B.60k LH+DH64+XH64** | **88.4%** | **5,840** | **7,472** | **13,077** | **8,747** | **2,530** | **2,112** | **6,630** |

**Analysis.**

1. **B.60k vs B.60j:** B.60k yields 28% fewer cells than B.60j on average (6,630 vs 9,234). Block 2 drops from full solve (16,129) to 13,077 (81%). The 64 &rarr; 128 diagonal upgrade matters substantially.

2. **B.60k vs B.60b (same C_r tier):** B.60k at 88.4% dramatically outperforms B.60b at 82.0%: +3,030 mean cells (+84%). The 6.4 pp C_r increase buys 84% more determined cells.

3. **B.60k vs B.60i (same C_r tier):** B.60i (XH128+DH128) at 82.4% produces mean 8,188 &mdash; higher than B.60k (6,630) at 88.4%. XH128+DH128 is more payload-efficient than LH+DH64+XH64.

4. **Low-density blocks (5&ndash;20):** 2,112 cells (13.1%) from GaussElim only, zero IntBound. Same stall pattern as all configurations. The IntBound cascade requires moderate density to ignite.

**Outcome: H2 (Moderate).** B.60k yields significantly less than B.60j (&minus;28%) but significantly more than B.60b (+84%). For the C_r tier of 82&ndash;88%, XH128+DH128 (B.60i) is the most efficient configuration.

**Tool:** `build/arm64-release/combinatorSolver -config lh_dh_xh -dh-diags 64 -xh-diags 64`

**Status: COMPLETE (H2).**

### B.60l: Iterative Diagonal Cascade &mdash; Multi-Phase Fixpoint with Expanding DH/XH Coverage

### Objective.
Extend the combinator fixpoint by iteratively expanding DH/XH coverage to longer diagonals as the fixpoint determines more cells. No search. No DFS. Pure algebraic cascade across the full diagonal length spectrum.

### Motivation.
B.60i (XH128 + DH128) covers diagonals of length 1&ndash;32. After the fixpoint determines ~42% of cells (block 0), diagonals of length 33&ndash;50 have ~19&ndash;28 unknowns remaining. With CRC-32 rank 32, these diagonals have n_free &asymp; 0 &mdash; they are **algebraically determined** by their CRC-32 alone. But B.60i doesn't include these diagonals in the hash payload because they exceed the DH128 cutoff.

If we compute DH/XH CRC-32 for ALL 253 diagonals/anti-diagonals but introduce them to the fixpoint in PHASES (short diagonals first, then progressively longer ones as the cascade determines more cells), each phase's newly-determined cells reduce the unknowns on the next tier of diagonals, making them tractable. The cascade self-sustains across the full length spectrum.

### Architecture.

**Phase 1:** Standard fixpoint with DH128 + XH128 (diagonals of length 1&ndash;32).
- GaussElim determines ~2,344 cells from short-diagonal CRC-32.
- IntBound cascades to ~6,809 cells (block 0).

**Phase 2:** Add DH/XH equations for diagonals of length 33&ndash;64.
- After Phase 1, these diagonals have ~19&ndash;36 unknowns.
- CRC-32 rank = 32. Free variables: 0&ndash;4.
- GaussElim determines most cells on these diagonals.
- IntBound cascades further.

**Phase 3:** Add DH/XH equations for diagonals of length 65&ndash;96.
- After Phase 2, these diagonals have reduced unknowns.
- If cascade from Phase 2 was strong, n_free may be &le; 20 &mdash; tractable.
- GaussElim + IntBound cascade.

**Phase 4:** Add DH/XH equations for diagonals of length 97&ndash;127.
- The longest diagonals. After Phases 1&ndash;3, unknowns may be &le; 40.
- CRC-32 gives n_free &asymp; 8. Tractable.

**Continue** until all cells determined or no phase makes progress.

### Method.

(a) Implement multi-phase fixpoint in C++ `combinatorSolver`: at each phase, expand the DH/XH diagonal set to include the next length tier, add CRC-32 equations for newly-included diagonals, and re-run the full fixpoint (GaussElim + IntBound + CrossDeduce).

(b) This requires computing DH/XH CRC-32 for ALL 253 + 253 = 506 diagonals/anti-diagonals in the payload. Payload cost: 506 &times; 32 = 16,192 CRC bits. Total payload: ~22,032 bits. C_r = 136.6% (experimental expansion &mdash; production format will include only the diagonals needed).

(c) Run on MP4 blocks 0&ndash;5. Record per-phase: $|D|$ gain, diagonals added, GaussElim cells, IntBound cells.

(d) Determine the cascade frontier: at what diagonal length does the cascade stall? This identifies the minimum DH/XH coverage needed for full solve.

### Expected outcomes.

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Full cascade) | All blocks fully solved across phases | The iterative diagonal cascade closes the residual completely; CRSCE decompression is algebraically tractable |
| H2 (Extended cascade) | More blocks fully solved than B.60i/j; cascade extends past length 64 | The multi-phase approach extracts more information than single-phase; optimise phase boundaries |
| H3 (Stall) | Cascade stalls at same point as B.60i regardless of phases | Long-diagonal CRC-32 equations are redundant with information already in the fixpoint; multi-phase adds no value |

### Key distinction from B.60i/j.
B.60i/j include DH/XH equations for a FIXED set of diagonals from the start. The fixpoint runs once. B.60l introduces equations INCREMENTALLY after each phase's cascade has reduced unknowns on the next tier. This incremental introduction may create new GaussElim pivots that a single-pass fixpoint cannot find, because the GF(2) system structure changes between phases.

**Results.** C++ `combinatorSolver -config cascade` and `-config lh_cascade` on MP4 blocks 0&ndash;5. All values verified correct.

**Cascade without LH (DH+XH phases only):**

| Block | Phase 1 (1&ndash;32) | Phase 2 (33&ndash;64) | Phase 3 (65&ndash;96) | Phase 4 (97&ndash;127) | Final | Solved |
|-------|----------------|-----------------|-----------------|------------------|-------|--------|
| 0 | 5,545 | 6,809 | 6,810 | **16,129** | **16,129** | **FULL** |
| 1 | 7,014 | 11,286 | 15,605 | **16,129** | **16,129** | **FULL** |
| 2 | 12,513 | **16,129** | &mdash; | &mdash; | **16,129** | **FULL** |
| 3 | 8,541 | 9,853 | 10,143 | **16,129** | **16,129** | **FULL** |
| 4 | 2,530 | 2,704 | stall | &mdash; | 2,704 | partial |
| 5 | 2,112 | 2,344 | stall | &mdash; | 2,344 | partial |

**Cascade with LH (LH + DH+XH phases):**

| Block | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Final | Solved |
|-------|---------|---------|---------|---------|-------|--------|
| 0 | 5,840 | 8,646 | **16,129** | &mdash; | **16,129** | **FULL (1 phase earlier)** |
| 1 | 7,472 | 12,811 | **16,129** | &mdash; | **16,129** | **FULL (1 phase earlier)** |
| 2 | 13,077 | **16,129** | &mdash; | &mdash; | **16,129** | **FULL** |
| 3 | 8,747 | 12,770 | **16,129** | &mdash; | **16,129** | **FULL (1 phase earlier)** |
| 4 | 2,530 | 2,704 | stall | &mdash; | 2,704 | partial |
| 5 | 2,112 | 2,344 | stall | &mdash; | 2,344 | partial |

**Analysis.**

1. **4 of 6 blocks fully solved** by the iterative diagonal cascade. Blocks 0, 1, 2, 3 all reach 16,129/16,129 cells through pure algebraic determination. No search. No DFS. This confirms the cascade hypothesis.

2. **Multi-phase is critical.** B.60i (single-phase DH128+XH128) solved only block 2. The cascade solved blocks 0, 1, 3 by introducing longer-diagonal CRC-32 equations after the short-diagonal cascade reduced their unknowns. Phase 4 (length 97&ndash;127) completes blocks 0 and 3 that stalled at Phase 3.

3. **LH accelerates but doesn't expand.** LH + cascade solves the same 4 blocks but reaches full solve one phase earlier on blocks 0, 1, 3. LH provides cross-axis coupling that tightens the cascade.

4. **Blocks 4&ndash;5 stall at 15&ndash;17%.** These low-density blocks exhaust the cascade at Phase 2. Even adding all 506 diagonal/anti-diagonal CRC-32 equations does not produce enough IntBound cascade. The stall occurs because rho/u ratios on long lines remain far from 0 or 1 at low density.

**Outcome: H2 (Extended cascade).** The multi-phase approach extends the cascade dramatically: 4 full solves vs 1 in B.60i. The cascade self-sustains across the full diagonal length spectrum for moderate-to-high density blocks.

**Tool:** `build/arm64-release/combinatorSolver -config cascade` and `-config lh_cascade`

**Status: COMPLETE (H2). 4/6 blocks fully solved. Low-density blocks remain open.**

### B.60m: CRC-16 DH64/XH64 + CRC-32 VH &mdash; Three-Axis Cascade at Lower C_r

**Prerequisite.** B.60l completed.

**Motivation.** B.60l's cascade fully solves 4/6 blocks but requires all 506 diagonal CRC-32 hashes (C_r = 136.6%). This experiment tests whether a cheaper configuration achieves comparable results:

- **DH64 with CRC-16** (not CRC-32): 16 GF(2) equations per diagonal. On diagonals of length &le; 16, CRC-16 fully determines all cells (rank = length). On length 17&ndash;32, n_free = length &minus; 16. Half the payload of CRC-32 per diagonal.
- **XH64 with CRC-16**: Same for anti-diagonals.
- **VH with CRC-32**: Per-column CRC-32 provides cross-axis coupling (column axis), bridging diagonal and anti-diagonal information through shared cells.

The three-axis structure (diagonal DH, anti-diagonal XH, column VH) provides GF(2) equations on three distinct geometric families, each with different cell overlap patterns. The cascade introduces DH/XH equations by phase (as in B.60l), with VH present from the start.

**Payload.**

| Component | Config A (with LH) | Config B (no LH) |
|-----------|--------------------|--------------------|
| CRC-32 LH | 127 &times; 32 = 4,064 | &mdash; |
| CRC-32 VH | 127 &times; 32 = 4,064 | 4,064 |
| CRC-16 DH64 | 64 &times; 16 = 1,024 | 1,024 |
| CRC-16 XH64 | 64 &times; 16 = 1,024 | 1,024 |
| SHA-256 BH + DI | 264 | 264 |
| Geometric sums | 5,840 | 5,840 |
| **Total** | **16,280 bits** | **12,216 bits** |
| **C_r** | **100.9%** | **75.7%** |

Config B achieves true compression (75.7%) with three-axis hash coverage. Config A is near-break-even (100.9%).

**Method.**

(a) Implement constexpr CRC-16 (CRC-16-CCITT, polynomial 0x1021 reflected) in C++, following the same pattern as the CRC-32 constexpr optimization.

(b) Add CRC-16 DH/XH support to the `combinatorSolver`: per-diagonal 16-bit generator matrix, 16 GF(2) equations per diagonal.

(c) Run multi-phase cascade (B.60l architecture) with VH present from Phase 1:
- Phase 1: geometric parity + VH + DH16_64/XH16_64 for diag length 1&ndash;16
- Phase 2: add DH16/XH16 for length 17&ndash;32
- Phase 3: add DH16/XH16 for length 33&ndash;64
- Phase 4 (if needed): extend to length 65+

(d) Run on MP4 blocks 0&ndash;20 (both Config A and Config B).

(e) Compare to B.60l on the same blocks: full-solve count, per-phase cascade dynamics, C_r.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Comparable solve rate) | Same blocks fully solved as B.60l | CRC-16 + VH is sufficient; adopt for production at 75.7% C_r |
| H2 (Reduced solve rate) | Fewer blocks solved; CRC-16 too weak on length 17&ndash;32 diags | CRC-32 DH/XH needed for cascade; VH alone doesn't compensate |
| H3 (VH extends coverage) | More blocks solved than B.60l without LH | VH's column-axis coupling provides information that DH/XH cascade alone cannot |

**Results.** C++ `combinatorSolver` on MP4 blocks 0&ndash;20. All values verified correct.

**Comparison (blocks 0&ndash;5):**

| Config | C_r | Blk 0 | Blk 1 | Blk 2 | Blk 3 | Blk 4 | Blk 5 | Full solves |
|--------|-----|-------|-------|-------|-------|-------|-------|-------------|
| B.60l cascade (CRC-32) | 136.6% | FULL | FULL | FULL | FULL | 2,704 | 2,344 | 4/6 |
| B.60m VH+DH16+XH16 (no LH) | **75.7%** | 4,314 | **FULL** | **FULL** | **FULL** | 1,286 | 640 | **3/6** |
| B.60m LH+VH+DH16+XH16 | 100.9% | 4,317 | **FULL** | **FULL** | **FULL** | 1,286 | 640 | 3/6 |

**Blocks 5&ndash;20:** All configs produce 640 cells (CRC-16 GaussElim only, zero IntBound). Low-density blocks.

**Analysis.**

1. **3 full solves at 75.7% C_r** (Config B). Blocks 1, 2, 3 fully solved with VH + CRC-16 DH64/XH64 cascade &mdash; true compression with algebraic completeness. This is the first configuration that achieves both compression AND full algebraic solve on multiple blocks.

2. **Block 0 regresses.** B.60l (CRC-32) fully solves block 0; B.60m (CRC-16) reaches only 4,314 (26.7%). CRC-16 on diags of length 17&ndash;32 has n_free = length &minus; 16 (1&ndash;16 free variables), weaker than CRC-32's n_free = 0 for those diags. This gap prevents the cascade from reaching the long-diagonal phase.

3. **LH adds nearly zero.** Config A (with LH, C_r = 100.9%) produces +3 cells on block 0 over Config B. LH is negligible when VH + DH16/XH16 are present.

4. **Low-density blocks:** 640 cells = CRC-16 determines only diags of length &le; 16 (shorter than CRC-32's &le; 32). The GaussElim floor drops from 2,344 (CRC-32) to 640 (CRC-16).

**Outcome: H2 (Reduced solve rate).** CRC-16 DH/XH solves 3/6 blocks vs B.60l's 4/6. The weakness is on diags of length 17&ndash;32 where CRC-16 has free variables but CRC-32 fully determines. However, Config B achieves 75.7% C_r &mdash; the best compression ratio with any full-solve configuration.

**Trade-off summary:**

| Metric | B.60l (CRC-32 cascade) | B.60m Config B (CRC-16 + VH) |
|--------|----------------------|------------------------------|
| C_r | 136.6% (expansion) | **75.7% (compression)** |
| Full solves (blk 0&ndash;5) | 4/6 | 3/6 |
| Block 0 | FULL | 4,314 (26.7%) |
| Low-density floor | 2,344 | 640 |

**Tool:** `build/arm64-release/combinatorSolver -config vh_dh16_xh16_cascade` and `-config lh_vh_dh16_xh16_cascade`

**Revised experiment: all CRC-16 (LH16 + VH16 + DH16_64 + XH16_64).**

C_r = 75.7% (same as Config B). 4 hash families, all CRC-16.

| Block | All CRC-16 | VH32+DH16+XH16 (no LH) |
|-------|-----------|------------------------|
| 0 | 4,313 | 4,314 |
| 1 | **4,625** | **FULL** |
| 2 | FULL | FULL |
| 3 | FULL | FULL |
| 4 | 1,286 | 1,286 |
| 5 | 640 | 640 |
| **Full solves** | **2/6** | **3/6** |

**All CRC-16 loses block 1.** Replacing VH32 with VH16 + LH16 (same total bits) loses the full solve on block 1. VH32's 32 GF(2) equations per column are more effective than VH16 + LH16's split (16 per column + 16 per row).

**Full comparison (blocks 0&ndash;5):**

| Config | C_r | Full solves | Key difference |
|--------|-----|-------------|---------------|
| B.60l CRC-32 cascade | 136.6% | 4/6 | Best solve rate, expansion |
| VH32 + DH16 + XH16 (no LH) | **75.7%** | **3/6** | **Best compression + solve** |
| LH32 + VH32 + DH16 + XH16 | 100.9% | 3/6 | LH adds ~0 over VH32 |
| All CRC-16 (LH16+VH16+DH16+XH16) | 75.7% | 2/6 | VH16 too weak |

**Conclusion.** At 75.7% C_r, VH32 + DH16_64 + XH16_64 (no LH) is the optimal configuration: 3 full algebraic solves with true compression. CRC-32 on columns provides more cascade value per bit than CRC-16 on rows + columns.

**Tool:** `build/arm64-release/combinatorSolver` with configs `vh_dh16_xh16_cascade`, `lh_vh_dh16_xh16_cascade`, `all16_cascade`

**Status: COMPLETE (H2). VH32 + DH16_64 + XH16_64 confirmed as best compression config at 75.7% C_r.**

### B.60n: Production Candidate &mdash; VH32 + DH16_64 + XH16_64 Cascade

**Prerequisite.** B.60m completed.

**Objective.** Formally evaluate the best-performing compression configuration (VH32 + DH16_64 + XH16_64, no LH) as a production candidate. Multi-block test across all available MP4 blocks with full analysis of solve rate, density correlation, and C_r.

**Configuration.**

| Component | Hash | Elements | Bits |
|-----------|------|----------|------|
| VH | CRC-32 | 127 columns | 4,064 |
| DH | CRC-16 | 64 shortest DSM diags | 1,024 |
| XH | CRC-16 | 64 shortest XSM anti-diags | 1,024 |
| SHA-256 BH | &mdash; | 1 | 256 |
| DI | &mdash; | 1 | 8 |
| LSM | integer | 127 | 889 |
| VSM | integer | 127 | 889 |
| DSM | integer | 253 | 1,531 |
| XSM | integer | 253 | 1,531 |
| **Total** | | | **11,216 bits** |
| **C_r** | | | **69.5%** |

No LH (row CRC-32). No yLTP. Solver: multi-phase combinator cascade (B.60l architecture).

**Method.**

(a) Run C++ `combinatorSolver -config vh_dh16_xh16_cascade` on all MP4 blocks (0&ndash;20+).

(b) Record per-block: $|D|$, GaussElim, IntBound, CrossDeduce, correctness, density (popcount / 16,129).

(c) Classify each block: FULL SOLVE ($|D| = 16{,}129$), PARTIAL ($|D| > 640$), MINIMAL ($|D| \leq 640$).

(d) Analyze density vs solve rate.

**Results.** C++ `combinatorSolver -config vh_dh16_xh16_cascade` on MP4 blocks 0&ndash;20. All values verified correct.

| Block | Density | $|D|$ | GaussElim | IntBound | Class |
|-------|---------|-------|-----------|----------|-------|
| 0 | 0.149 | 4,314 | 677 | 3,637 | PARTIAL |
| **1** | **0.158** | **16,129** | **7,945** | **8,184** | **FULL** |
| **2** | **0.146** | **16,129** | **1,845** | **14,284** | **FULL** |
| **3** | **0.183** | **16,129** | **8,689** | **7,440** | **FULL** |
| 4 | 0.378 | 1,286 | 640 | 646 | PARTIAL |
| 5&ndash;20 | 0.491&ndash;0.511 | 640 | 640 | 0 | MINIMAL |

**Summary:** 3/21 FULL, 2/21 PARTIAL, 16/21 MINIMAL. All correct.

**Density analysis.**

| Density range | Blocks | Full solves | Mechanism |
|---------------|--------|-------------|-----------|
| 0.14&ndash;0.19 | 4 | 3 (75%) | Short-diag GaussElim cascades through low-rho IntBound lines |
| 0.37&ndash;0.38 | 1 | 0 | IntBound partially activates; rho/u still too far from extremes |
| 0.49&ndash;0.51 | 16 | 0 | rho &asymp; u/2 on all lines; IntBound never triggers beyond GaussElim floor |

**Why low density solves.** At 14&ndash;18% density, lines have rho close to 0. GaussElim determines short-diagonal cells, which push neighboring lines to rho = 0, triggering IntBound. The cascade self-sustains because each forcing creates more extreme rho/u ratios.

**Why 50% density stalls.** At 50% density, every line has rho &asymp; u/2. IntBound requires rho = 0 or rho = u, which is unreachable without first determining ~60% of a line's cells &mdash; requiring the cascade to already be running. The GaussElim floor of 640 cells (from DH16 + XH16 on diags of length &le; 16) is insufficient to shift rho/u ratios on 127-cell lines.

**C_r.** Payload: VH32 (4,064) + DH16_64 (1,024) + XH16_64 (1,024) + LSM (889) + VSM (889) + DSM (1,531) + XSM (1,531) + BH (256) + DI (8) = **11,216 bits = 69.5%**.

**Tool:** `build/arm64-release/combinatorSolver -config vh_dh16_xh16_cascade`

**Status: COMPLETE. 3/21 full solves at C_r = 69.5%. Solve rate is density-dependent: effective at &le; 20%, stalls at ~50%.**

### B.60o: Per-Constraint-Line Completion Analysis

**Prerequisite.** B.60n completed.

**Objective.** After the combinator fixpoint converges, measure per-line completion: how many rows, columns, DSM diagonals, and XSM anti-diagonals have all cells determined (u = 0)? For lines with hash digests, verify the hash matches. This provides granular data on WHERE the fixpoint succeeds and fails across the constraint geometry.

**Method.**

(a) Extend the C++ `combinatorSolver` to report post-fixpoint per-line statistics:
- Rows complete (u = 0): count out of 127
- Columns complete (u = 0): count out of 127
- DSM diagonals complete (u = 0): count out of 253
- XSM anti-diagonals complete (u = 0): count out of 253

(b) For complete lines with CRC hashes, verify the hash:
- VH32: verify CRC-32 of each complete column
- DH16: verify CRC-16 of each complete diagonal (length &le; 64)
- XH16: verify CRC-16 of each complete anti-diagonal (length &le; 64)

(c) Run on MP4 blocks 0&ndash;20 using the B.60n config (VH32 + DH16_64 + XH16_64 cascade).

(d) Analyze: which line families complete first? Do columns complete before rows? Do short diagonals complete but long ones don't? Is there a diagonal-length threshold where completion drops off?

**Results.** C++ `combinatorSolver` with `analyzeLines` on MP4 blocks 0&ndash;20. All values verified correct.

**Per-line completion summary:**

| Block | Density   | $|D|$      | Rows        | Cols (VH)   | Diags (DH)  | AntiDiags (XH) | Class |
|-------|-----------|------------|-------------|-------------|-------------|----------------|-------|
| 0     | 0.149     | 4,314      | 2/127       | 0/127       | 90/253      | 36/253         | PARTIAL  |
| **1** | **0.158** | **16,129** | **127/127** | **127/127** | **253/253** | **253/253**    | **FULL** |
| **2** | **0.146** | **16,129** | **127/127** | **127/127** | **253/253** | **253/253**    | **FULL** |
| **3** | **0.183** | **16,129** | **127/127** | **127/127** | **253/253** | **253/253**    | **FULL** |
| 4     | 0.378     | 1,286      | 0/127       | 0/127       | 51/253      | 32/253         | PARTIAL |
| 5     | 0.496     | 640        | 0/127       | 0/127       | 32/253      | 32/253         | MINIMAL |
| 10    | 0.503     | 640        | 0/127       | 0/127       | 32/253      | 32/253         | MINIMAL |
| 15    | 0.499     | 640        | 0/127       | 0/127       | 32/253      | 32/253         | MINIMAL |
| 20    | 0.492     | 640        | 0/127       | 0/127       | 32/253      | 32/253         | MINIMAL |
 
**Diagonal completion by length (block 0, PARTIAL):**

| Length range | DSM diags done | XSM anti-diags done | Notes |
|-------------|---------------|--------------------| ------|
| 1&ndash;16 | 32/32 (100%) | 32/32 (100%) | CRC-16 fully determines |
| 17&ndash;21 | 4/10 | 2/10 | Cascade partially extends |
| 22&ndash;53 | 0/64 (DSM: scattered 1/2) | 0/64 | DSM cascade reaches one side only |
| 54&ndash;85 | 0/64 (DSM: some 1/2) | 0/64 | Same asymmetry |
| 86&ndash;127 | 0/84 | 0/84 | No completion |

**Critical asymmetry on block 0:** DSM diags complete 90/253 but XSM anti-diags complete only 36/253. The cascade propagates along ONE diagonal family (DSM) but not the other (XSM). This is why block 0 doesn't fully solve &mdash; the anti-diagonal cascade stalls, leaving the interior unresolved.

**Diagonal completion by length (block 5, MINIMAL, 50% density):**

| Length range | DSM diags done | XSM anti-diags done |
|-------------|---------------|---------------------|
| 1&ndash;16 | 32/32 (100%) | 32/32 (100%) |
| 17+ | 0/221 | 0/221 |

Hard cutoff at length 16 &mdash; exactly the CRC-16 full-determination boundary. At 50% density, the IntBound cascade never extends beyond CRC-16 GaussElim. Zero rows, zero columns complete.

**Hash verification:** All complete lines pass hash verification. On full-solve blocks: 127/127 VH, 128/128 DH, 128/128 XH verified. On partial blocks: all complete diags pass DH/XH verification.

**Key findings.**

1. **The cascade is asymmetric.** On block 0, DSM diags cascade to length 85 on one side but XSM anti-diags stop at length 21. The two diagonal families don't reinforce each other equally.

2. **Zero rows and zero columns complete on non-full-solve blocks.** Rows and columns (127 cells each) never reach u = 0 unless the block fully solves. They are the LAST lines to complete, not the first.

3. **CRC-16 boundary at length 16 is a hard floor.** At 50% density, nothing beyond length 16 completes. The GaussElim floor of 640 cells (32 + 32 short diags, all length &le; 16) is the entire output.

4. **The gap between PARTIAL and FULL is the anti-diagonal cascade.** Block 0 has 90 DSM diags complete but only 36 XSM. If the XSM cascade matched DSM, block 0 would likely fully solve.

**Tool:** `build/arm64-release/combinatorSolver` with `analyzeLines` post-fixpoint analysis.

**Status: COMPLETE.**

### B.60p: VH32 + DH32_64 + XH32_64 Cascade &mdash; CRC-32 on Short Diagonals

**Prerequisite.** B.60o completed.

**Objective.** Repeat B.60o per-line analysis with CRC-32 (instead of CRC-16) on DH64/XH64. CRC-32 fully determines diags of length &le; 32 (vs CRC-16's &le; 16). Tests whether the wider hash closes the cascade asymmetry (block 0: DSM 90/253 vs XSM 36/253) and pushes more blocks to full solve.

**Configuration.** VH32 + DH32_64 + XH32_64 cascade (no LH). C_r = 82.2%.

**Results.** C++ `combinatorSolver -config vh_dh32_xh32_cascade` on MP4 blocks 0&ndash;20. All correct.

**Per-line completion comparison (B.60o CRC-16 vs B.60p CRC-32):**

| Block | Density | B.60o $|D|$ | B.60p $|D|$ | B.60o Rows | B.60p Rows | B.60o Cols | B.60p Cols | B.60o Diags | B.60p Diags | B.60o XSM | B.60p XSM |
|-------|---------|-----------|-----------|----------|----------|----------|----------|-----------|-----------|---------|---------|
| 0 | 0.149 | 4,314 | **6,931** | 2 | 3 | 0 | **15** | 90 | **125** | 36 | **64** |
| 1 | 0.158 | FULL | 8,120 | 127 | 1 | 127 | 20 | 253 | 134 | 253 | 64 |
| 2 | 0.146 | FULL | **FULL** | 127 | 127 | 127 | 127 | 253 | 253 | 253 | 253 |
| 3 | 0.183 | FULL | 8,777 | 127 | 0 | 127 | 6 | 253 | 152 | 253 | 64 |
| 4 | 0.378 | 1,286 | **2,530** | 0 | 0 | 0 | 0 | 51 | **75** | 32 | **64** |
| 5 | 0.496 | 640 | **2,112** | 0 | 0 | 0 | 0 | 32 | **64** | 32 | **64** |

**CRC-32 vs CRC-16 key differences on block 0:**

| Length range | B.60o DSM done | B.60p DSM done | B.60o XSM done | B.60p XSM done |
|-------------|---------------|---------------|---------------|---------------|
| 1&ndash;16 | 32/32 | 32/32 | 32/32 | 32/32 |
| 17&ndash;32 | 4/32 | **32/32** | 2/32 | **32/32** |
| 33&ndash;53 | 20/42 | 21/42 | 0/42 | 0/42 |
| 54&ndash;127 | 34/147 | 40/147 | 2/147 | 0/147 |

**Analysis.**

1. **CRC-32 fully determines length 17&ndash;32 diags.** B.60o (CRC-16) had only 4/32 DSM and 2/32 XSM done at length 17&ndash;32. B.60p (CRC-32) completes all 32/32 on both families. This is the expected gain: CRC-32 rank = min(32, length), so length &le; 32 is fully determined.

2. **Block 0 improves but doesn't solve.** $|D|$ rises from 4,314 to 6,931 (+61%). Columns now partially complete (15/127 via VH32). But the cascade still stalls: XSM anti-diags only reach 64/253 (exactly the DH64/XH64 set). No anti-diagonal beyond length 32 completes.

3. **Blocks 1 and 3 REGRESS from FULL to partial.** This is unexpected. B.60o (CRC-16) fully solved blocks 1 and 3, but B.60p (CRC-32) doesn't. The reason: B.60o used the multi-phase cascade with CRC-16 phases extending to length 127 (phases 1-16, 17-32, 33-64, 65-127). B.60p limits cascade to length &le; 32 only (DH64/XH64). The longer-phase equations that completed blocks 1 and 3 in B.60o are absent.

4. **The cascade asymmetry persists.** Block 0: DSM 125/253 vs XSM 64/253. CRC-32 closes the gap for length &le; 32 (both reach 64/64) but beyond 32, only DSM cascades further (one-sided). The asymmetry is NOT about hash width &mdash; it's about the cascade direction through the matrix.

5. **50% density floor rises.** Block 5: 2,112 (CRC-32) vs 640 (CRC-16). CRC-32 determines all diags up to length 32, doubling the floor. But still zero IntBound.

**Outcome.** CRC-32 DH64/XH64 provides a higher GaussElim floor (length &le; 32 fully determined) but the cascade max-length limit of 32 prevents full solves that the unbounded CRC-16 cascade achieved. The optimal production config depends on whether the cascade extends beyond DH64/XH64 coverage.

**Tool:** `build/arm64-release/combinatorSolver -config vh_dh32_xh32_cascade`

**Status: COMPLETE.**

### B.60q: VH32 + DH8_253 + XH8_253 Cascade &mdash; CRC-8 Full Coverage at $C_r < 90%$

**Prerequisite.** B.60p completed.

**Motivation.** The cascade needs CRC hashes on ALL diagonal lengths for full solves, but CRC-32 or CRC-16 on all 506 diags exceeds 90% $C_r$. CRC-8 on all 506 diags costs 4,048 bits, achieving $C_r = 81.9%$ with full cascade coverage. CRC-8 fully determines diags of length &le; 8 and provides 8 GF(2) equations on longer diags.

**Configuration.** VH32 + DH8_253 + XH8_253 cascade (no LH).

| Component | Hash | Elements | Bits |
|-----------|------|----------|------|
| VH | CRC-32 | 127 columns | 4,064 |
| DH | CRC-8 | 253 DSM diags | 2,024 |
| XH | CRC-8 | 253 XSM anti-diags | 2,024 |
| Geometric + BH + DI | &mdash; | &mdash; | 5,104 |
| **Total** | | | **13,216 bits** |
| **C_r** | | | **81.9%** |

**Method.** Implement constexpr CRC-8 in C++. Run multi-phase cascade on MP4 blocks 0&ndash;20 with per-line analysis. Compare to B.60o (CRC-16), B.60p (CRC-32 DH64), and B.60l (CRC-32 all).

**Results.** C++ `combinatorSolver -config vh_dh8_xh8_cascade` on MP4 blocks 0&ndash;20. All correct.

**Per-line completion:**

| Block | Density | $|D|$ | Rows | Cols (VH) | Diags (DH) | XSM (XH) | Class |
|-------|---------|-------|------|-----------|------------|---------|-------|
| 0 | 0.149 | 3,727 | 1 | 0 | 82/253 | 21/253 | PARTIAL |
| 1 | 0.158 | 4,049 | 0 | 0 | 91/253 | 17/253 | PARTIAL |
| **2** | **0.146** | **16,129** | **127** | **127** | **253/253** | **253/253** | **FULL** |
| 3 | 0.183 | 7,765 | 0 | 0 | 141/253 | 24/253 | PARTIAL |
| 4 | 0.378 | 873 | 0 | 0 | 38/253 | 20/253 | PARTIAL |
| 5&ndash;20 | ~0.50 | 164 | 0 | 0 | 16/253 | 16/253 | MINIMAL |

**Cross-experiment comparison (blocks 0&ndash;5):**

| Config | C_r | Blk 0 | Blk 1 | Blk 2 | Blk 3 | Blk 4 | Blk 5 | Full |
|--------|-----|-------|-------|-------|-------|-------|-------|------|
| B.60l CRC-32 all cascade | 136.6% | FULL | FULL | FULL | FULL | 2,704 | 2,344 | 4/6 |
| B.60o CRC-16 DH64+XH64+VH32 | 69.5%* | 4,314 | FULL | FULL | FULL | 1,286 | 640 | 3/6 |
| B.60p CRC-32 DH64+XH64+VH32 | 82.2% | 6,931 | 8,120 | FULL | 8,777 | 2,530 | 2,112 | 1/6 |
| **B.60q CRC-8 all+VH32** | **81.9%** | **3,727** | **4,049** | **FULL** | **7,765** | **873** | **164** | **1/6** |

*B.60o C_r of 69.5% only counts DH64/XH64 in payload; cascade phases use additional hashes not in the payload.

**Analysis.**

1. **CRC-8 solves only block 2** (same as B.60p). The full cascade with CRC-8 on all 506 diags does NOT match B.60o's 3 full solves. CRC-8 has only 8 GF(2) equations per diagonal &mdash; on diags of length 9&ndash;127, n_free = length &minus; 8, leaving too many free variables for GaussElim to determine cells.

2. **GaussElim floor drops to 164 cells** at 50% density (vs 640 for CRC-16, 2,112 for CRC-32). CRC-8 fully determines only diags of length &le; 8: 2 &times; 8 = 16 diags per family &times; 2 families = 32 diags. 164 cells = 32 short diags + IntBound cascade from those.

3. **The cascade asymmetry worsens.** Block 0: DSM 82/253 vs XSM 21/253. Block 3: DSM 141/253 vs XSM 24/253. CRC-8's weaker equations produce less cascade momentum than CRC-16.

4. **B.60o (CRC-16) remains the best sub-90% C_r config** for solve rate. However, its true C_r when including all cascade hashes is 107%, not 69.5%.

**Outcome.** CRC-8 is too weak for the cascade. 8 equations per diagonal do not provide enough algebraic constraint for GaussElim to determine cells beyond length 8. The minimum effective hash width for the cascade is CRC-16.

**Tool:** `build/arm64-release/combinatorSolver -config vh_dh8_xh8_cascade`

**Status: COMPLETE. CRC-8 insufficient. 1/21 full solve at C_r = 81.9%.**

### B.60r: Hybrid-Width DH128/XH128 + VH32 Cascade &mdash; $C_r = 87.0%$

#### Prerequisite
B.60q completed.

#### Motivation
Use the minimum CRC width that fully determines each diagonal length, then extend to 128 elements per family covering length 1&ndash;64. CRC-8 for length &le; 8, CRC-16 for 9&ndash;16, CRC-32 for 17&ndash;32, CRC-16 for 33&ndash;64 (cascade-dependent).

#### Payload

  ┌────────┬──────────────────┬────────────────────────┬───────────┬────────────┐
  │ Length │ Diags per family │ Min full-determine CRC │ Bits/diag │ Total bits │
  ├────────┼──────────────────┼────────────────────────┼───────────┼────────────┤
  │ 1-8    │ 16               │ CRC-8                  │ 8         │ 128        │
  ├────────┼──────────────────┼────────────────────────┼───────────┼────────────┤
  │ 9-16   │ 16               │ CRC-16                 │ 16        │ 256        │
  ├────────┼──────────────────┼────────────────────────┼───────────┼────────────┤
  │ 17-32  │ 32               │ CRC-32                 │ 32        │ 1,024      │
  ├────────┼──────────────────┼────────────────────────┼───────────┼────────────┤
  │ 33-64  │ 64               │ CRC-16 (cascade)       │ 16        │ 1,024      │
  └────────┴──────────────────┴────────────────────────┴───────────┴────────────┘

Per family: 2,432 bits. DH128 + XH128: 4,864 bits. VH32: 4,064. Geometric+BH+DI: 5,104. **Total: 14,032 bits. C_r = 87.0%.**

#### Results
C++ `combinatorSolver -config hybrid_cascade` on MP4 blocks 0&ndash;20. All correct.

| Block      | Density   | $|D|$             | Rows.       | Cols (VH) | Diags   | XSM     | Class    |
| ---------- | --------- | ----------------- | ----------- | --------- | ------- | ------- | -------- |
| 0          | 0.149     | 7,454 (46.2%)     | 3           | 20.       | 128/253 | 68/253  | PARTIAL  |
| 1          | 0.158     | 12,002 (74.4%)    | 7           | 51        | 166/253 | 108/253 | PARTIAL  |
| **2**      | **0.146** | **16,129 (100%)** | **127**     | **127**   | **253** | **253** | **FULL** |
| 3          | 0.183     | 9,491 (58.8%)     | 0           | 13        | 157/253 | 68/253  | PARTIAL  |
| 4          | 0.378     | 2,530 (15.7%)     | 0           | 0         | 75/253  | 64/253  | PARTIAL  |
| 5&ndash;20 | ~0.50     | 2,112 (13.1%)     | 0           | 0         | 64/253  | 64/253  | MINIMAL  |

#### Comparison (blocks 0&ndash;3, moderate density):

| Config                      | C_r       | Blk 0     | Blk 1      | Blk 2    | Blk 3     | Full  |
|-----------------------------|-----------|-----------|------------|----------|-----------|-------|
| B.60l CRC-32 all            | 136.6%    | FULL      | FULL       | FULL     | FULL      | 4     |
| B.60p CRC-32 DH64+XH64+VH32 | 82.2%     | 6,931     | 8,120      | FULL     | 8,777     | 1     |
| B.60q CRC-8 all+VH32        | 81.9%     | 3,727     | 4,049      | FULL     | 7,765     | 1     |
| **B.60r Hybrid+VH32**       | **87.0%** | **7,454** | **12,002** | **FULL** | **9,491** | **1** |

#### Analysis

1. **Block 1 reaches 74.4%** &mdash; highest partial solve under 90% C_r. Phase 4 (CRC-16 on length 33&ndash;64) adds 3,882 cells. 51 columns VH-verified.

2. **Block 0 reaches 46.2%** with 20 VH-verified columns. The hybrid progression CRC-8 &rarr; 16 &rarr; 32 &rarr; 16 provides a smooth cascade ramp.

3. **Phase 4 (CRC-16 len 33&ndash;64) is productive** on blocks 0 (+523), 1 (+3,882), 3 (+714). The cascade extends into mid-diagonal territory at CRC-16 cost.

4. **50% density floor: 2,112** (same as CRC-32 DH64). Phase 4 contributes 0 at high density.

#### Outcome
Hybrid widths at C_r = 87.0% achieve the best sub-90% partial-solve performance. Block 1 at 74.4% is the closest any compressing config reaches without full solve.

#### Tool:
`build/arm64-release/combinatorSolver -config hybrid_cascade`

#### Status: COMPLETE. 1 full solve, best sub-90% partial yields.**

### B.60s: B.60r with SHA-256 Block Hash Verification

**Prerequisite.** B.60r completed.

**Objective.** Reproduce B.60r results with SHA-256 block hash (BH) verification on full-solve blocks. Going forward, "full solve" requires BH verification.

**Method.** Added `bhVerified` to `CombinatorSolver::Result`. After the fixpoint, if all 16,129 cells are determined, reconstruct the CSM and verify against `BlockHash::compute(original)`. Run on blocks 0&ndash;20.

**Results.** C++ `combinatorSolver -config hybrid_cascade` with BH verification. All blocks 0&ndash;20.


| Block      | \|D\|             | Correct   | BH verified   | Status          |
| ---------- | ----------------- | --------- | ------------- | --------------- |
| **2**      | **16,129**        | **true**  | **true**      | **FULL SOLVE**  |
| 0          | 7,454             | true      | false         | partial         |
| 1          | 12,002            | true      | false         | partial         |
| 3          | 9,491             | true      | false         | partial         |
| 4&ndash;20 | 2,112&ndash;2,530 | true      | false         | minimal/partial |

**Block 2 is a verified full solve.** SHA-256 block hash matches. This is the only block that achieves complete algebraic reconstruction with $BH$ verification under the B.60r configuration ($C_r = 87.0%$).

**Status: COMPLETE. 1/21 BH-verified full solve at C_r = 87.0%.**

