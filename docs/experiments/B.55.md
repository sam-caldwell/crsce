## B.55 LP Relaxation with Iterative Rounding (Answered by B.54)

### B.55.1 Motivation

The CRSCE constraint system is a system of 5,108 linear equations over 261,121 binary variables. The DFS solver treats this as a combinatorial search problem, exploring the binary solution space one variable at a time. An alternative approach treats it as a continuous optimization problem by relaxing the binary constraints to $x_{r,c} \in [0, 1]$ and solving the resulting linear program (LP).

The LP relaxation provides fractional cell values that indicate the "confidence" of each cell's assignment: a cell with LP value 0.01 is almost certainly 0, while a cell with LP value 0.99 is almost certainly 1. Cells with LP value near 0.5 are undetermined by the linear constraints alone. By rounding the high-confidence cells and iterating, the LP relaxation can progressively reduce the number of free variables.

### B.55.2 Method

1. Construct the constraint matrix $A$ ($5{,}108 \times 261{,}121$) and target vector $b$ (cross-sum values).
2. Solve the LP relaxation: minimize $\mathbf{0}^T x$ subject to $Ax = b$, $0 \leq x \leq 1$. (The objective is a feasibility problem; any feasible point suffices.)
3. Identify cells with LP value $< \epsilon$ or $> 1 - \epsilon$ (for threshold $\epsilon = 0.01$). Round these cells to 0 or 1 respectively.
4. Fix the rounded cells, update $A$ and $b$, and re-solve the reduced LP.
5. Repeat until no more cells can be rounded or all cells are assigned.
6. Verify the rounded assignment against SHA-1 lateral hashes.

### B.55.3 Analysis

The LP relaxation has $5{,}108$ equality constraints and $261{,}121$ variables. The system is heavily under-determined ($261{,}121 - 5{,}097 = 256{,}024$ degrees of freedom in the null space). A basic feasible solution (vertex of the polytope) would have at most 5,108 non-zero variables, but the LP relaxation finds an INTERIOR point where most variables are fractional.

The key question is whether the LP fractional values are informative. If most cells have LP value near 0.5, the relaxation provides no guidance. If many cells have LP values near 0 or 1, the relaxation identifies cells that the integer constraints determine even though the DFS propagation engine cannot detect them.

**Interaction with SHA-1 hashes.** The SHA-1 constraints are non-linear and cannot be included in the LP directly. They serve as post-rounding verification: after iterative rounding, check whether the assignment satisfies all SHA-1 hashes. If not, the rounded assignment is a candidate for refinement via local search or SAT solving on the remaining mismatched rows.

### B.55.4 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Many cells rounded) | $\geq 50\%$ of cells have LP value $< 0.01$ or $> 0.99$ | LP relaxation extracts substantial information from the integer constraints; iterative rounding may converge |
| H2 (Few cells rounded) | $< 10\%$ of cells have extreme LP values | The integer constraints are too loose for LP relaxation to provide guidance; the under-determined system admits too many feasible fractional solutions |
| H3 (Full solve) | Iterative rounding converges to a binary solution satisfying all constraints + SHA-1 | CRSCE is viable with LP-based reconstruction |

### B.55.5 Implementation

**Tool:** `tools/b55_lp_relaxation.py`&mdash;builds the constraint matrix, solves the LP relaxation using SciPy's `linprog` or HiGHS, and performs iterative rounding.

**Computational cost.** Solving a 5,108-constraint LP with 261,121 variables is feasible with modern solvers (HiGHS handles millions of variables). The iterative rounding adds $O(k)$ LP solves where $k$ is the number of rounding iterations.

**Status: ANSWERED BY B.54.** B.54's LP relaxation experiment (row-only, 322 constraints, 164K variables) solved in 0.5 seconds but achieved only 49.9% cell accuracy&mdash;equivalent to random guessing. The constraint system is too under-determined for LP relaxation to provide informative fractional values. All cells land at LP vertices (0 or 1), but the vertex chosen is unrelated to the correct CSM. Iterative rounding cannot converge because the starting point carries no signal. **B.55 is not viable.**

---

