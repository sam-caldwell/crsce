## B.56 Hierarchical Multi-Resolution Reconstruction (Proposed)

### B.56.1 Motivation

The CRSCE constraint system's 2% constraint density is computed over the full $511 \times 511$ matrix (5,097 independent constraints / 261,121 cells). For a smaller sub-matrix, the effective constraint density is HIGHER because the geometric constraint families (rows, columns, diagonals) scale linearly with matrix dimension while the cell count scales quadratically:

| Matrix size | Cells | Geometric constraints | Density |
|------------|-------|----------------------|---------|
| $63 \times 63$ | 3,969 | 374 | 9.4% |
| $127 \times 127$ | 16,129 | 758 | 4.7% |
| $255 \times 255$ | 65,025 | 1,526 | 2.3% |
| $511 \times 511$ | 261,121 | 3,064 | 1.2% |

At $63 \times 63$, the constraint density is 9.4%&mdash;nearly 5$\times$ higher than the full matrix. If the DFS solver's depth ceiling scales with constraint density, a $63 \times 63$ sub-matrix may be fully solvable.

### B.56.2 Method

**Stage 1: Solve the central 63$\times$63 sub-matrix.** Extract rows 224-286, columns 224-286 (the center of the CSM). Compute cross-sums for this sub-matrix from the known full-matrix cross-sums (each sub-row sum is derivable from the full row sum minus the cells outside the sub-matrix, which are unknowns&mdash;so the sub-matrix cross-sums are NOT directly available).

**Challenge:** The sub-matrix cross-sums cannot be computed independently because they depend on cells outside the sub-matrix. The full-matrix row sum $\text{LSM}[r]$ equals the sum of all 511 cells in row $r$, not just the 63 cells in the sub-matrix.

**Resolution:** Instead of decomposing the problem spatially, decompose it ALGEBRAICALLY. Use Gaussian elimination over the integers to reduce the 5,108-equation system to echelon form, identifying which cells are determined by the constraints and which are free. The determined cells can be assigned without branching; the free cells require search.

**Alternative hierarchical approach:** Process the matrix in row bands rather than sub-matrices. Solve rows 0-63 first (high constraint density from diagonal/anti-diagonal short lines at the matrix edge), then rows 64-127 (using the solved rows 0-63 as additional constraints), then 128-191, etc. This is similar to what the DFS solver already does but with the addition of integer Gaussian elimination at each stage to extract more information than singleton propagation.

### B.56.3 Algebraic Decomposition Variant

Instead of spatial hierarchy, perform **integer Gaussian elimination** on the full constraint matrix $A$ ($5{,}108 \times 261{,}121$, integer entries). This reduces the system to:

- **Determined cells:** Cells whose values are uniquely determined by the integer constraints (not just the GF(2) parity constraints). The integer rank may be higher than the GF(2) rank of 5,097.
- **Free cells:** Cells that can take either value without violating any cross-sum constraint.
- **Constrained cells:** Cells that are not fully determined but are constrained by integer relationships (e.g., "if cell A = 1 then cell B must = 0").

The determined cells can be assigned immediately. The constrained cells can be solved via SAT/ILP (B.54) or iterative methods (B.55). The free cells require SHA-1 disambiguation.

### B.56.4 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Integer rank $>$ GF(2) rank) | Integer Gaussian elimination determines more cells than GF(2) analysis predicts | The integer constraints carry information beyond parity; the effective solution space is smaller than $2^{256,024}$ |
| H2 (Integer rank = GF(2) rank) | No additional cells determined | The integer and GF(2) ranks coincide; the constraint system's information content is accurately captured by the GF(2) analysis |
| H3 (Hierarchical solve succeeds) | Row-band processing with integer elimination reaches full reconstruction | Hierarchical decomposition with stronger per-stage reasoning overcomes the DFS solver's plateau |

### B.56.5 Implementation

**Tool:** `tools/b56_hierarchical.py`&mdash;integer Gaussian elimination on the constraint matrix, followed by staged reconstruction.

**Dependencies:** NumPy for matrix operations, SciPy for sparse linear algebra, SymPy for exact integer arithmetic (avoiding floating-point rounding errors in Gaussian elimination).

**Computational cost.** Integer Gaussian elimination on a $5{,}108 \times 261{,}121$ matrix is memory-intensive (~2.6 GB for a dense integer matrix) but feasible. Sparse representations and modular arithmetic can reduce memory and computation time.

**Status: PROPOSED.**

---

