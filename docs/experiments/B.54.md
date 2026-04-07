## B.54 SMT Hybrid Solver with CRC-32 Row Verification (Proposed)

### B.54.1 Motivation

The DFS + propagation solver exhausts at ~row 189, having assigned approximately 96,000 of 261,121 cells. The remaining ~165,000 cells lie in the meeting band where the DFS solver's singleton propagation ($\rho = 0$ or $\rho = u$) extracts zero constraint information. However, the constraint system carries substantial INTEGER information in this region that the DFS solver ignores: each line's unknown cells must sum to exactly $\rho(L)$, a cardinality constraint that modern SAT/ILP solvers exploit through joint reasoning (CDCL, cutting planes, watched literals) fundamentally different from singleton propagation.

B.54 proposes a **format change** (SHA-1 $\to$ CRC-32 row hashes) combined with a **solver architecture change** (DFS $\to$ SMT hybrid) to break through the depth ceiling.

### B.54.2 Format Change: CRC-32 Row Hashes

Replace the 511 per-row SHA-1 lateral hashes (160 bits each) with CRC-32 (32 bits each). The SHA-256 block hash (BH) is retained as the cryptographic correctness guarantee.

| Component | SHA-1 (current) | CRC-32 (B.54) |
|-----------|-----------------|----------------|
| Row hash bits | 511 $\times$ 160 = 81,760 | 511 $\times$ 32 = 16,352 |
| Block hash (SHA-256) | 256 | 256 |
| DI | 8 | 8 |
| Cross-sums | 43,964 | 43,964 |
| **Total payload** | **125,988 bits (15,749 bytes)** | **60,580 bits (7,573 bytes)** |
| **Compression ratio** | **48.2%** | **23.2%** |

**Compression improvement:** The payload drops from 125,988 to 60,580 bits&mdash;a 52% reduction&mdash;improving the compression ratio from 48.2% to **23.2%**.

**Collision resistance:** CRC-32 is linear over GF(2) and provides zero adversarial collision resistance on its own. However, the SHA-256 block hash provides full $2^{256}$ second-preimage resistance over the entire CSM. For the minimum constraint-preserving swap (1,528 cells, 11 rows), the combined evasion probability is:

$$P_{\text{collision}} = 2^{-32 \times 11 - 256} = 2^{-608}$$

This is weaker than the current $2^{-2,016}$ (SHA-1) but still astronomically beyond any foreseeable computational capability. The security rests on the SHA-256 BH, which is unaffected by the row hash change.

**Solver pruning:** CRC-32 provides the same pass/fail signal as SHA-1 when a row reaches $u = 0$. The $2^{-32}$ false positive rate per row means the solver encounters a spurious pass approximately once per $4 \times 10^9$ row verifications&mdash;effectively never. The BH catches any false positive at final verification.

### B.54.3 Prior Art: Why Previous CDCL/SAT Attempts Failed

**B.21 CDCL implementation (&sect;B.21.12).** CDCL was implemented within the DFS solver and produced catastrophic depth regression (88,503 $\to$ 52,042, &minus;41%). The failure was caused by applying 1-UIP conflict analysis to SHA-1 hash failures&mdash;a global non-linear constraint over 511 cells. The conflict analysis identified irrelevant decision variables as culprits, producing wrong-direction backjumps that undid correct work. The CDCL learned clauses were too long (predicted 111+ literals) to provide meaningful pruning.

**Why B.54 differs from B.21.** B.21 embedded CDCL INSIDE the DFS solver and applied it to the COMBINED problem (cross-sums + SHA-1). B.54 separates the concerns:

- **Cross-sum constraints** are linear cardinality constraints&mdash;perfectly suited for SAT/ILP solving. The conflict analysis operates on cardinality constraint violations, not SHA-1 hash failures. This eliminates the global-constraint mismatch that destroyed B.21's CDCL.
- **Row hash verification** is handled as an **SMT theory oracle**: a black-box function called when a row becomes fully assigned. The oracle returns pass/fail and, on failure, generates a blocking clause. This is SAT Modulo Theories (SMT), not CDCL on bit-blasted hashes.

### B.54.4 SMT Formulation

The solver operates in two layers:

**Boolean core (SAT solver).** For each unassigned cell $(r,c)$, a Boolean variable $x_{r,c}$. For each constraint line $L$ with residual $\rho(L)$ and $u(L)$ unknowns, a cardinality constraint:

$$\sum_{\substack{(r,c) \in L \\ \text{unassigned}}} x_{r,c} = \rho(L)$$

encoded via sequential counter or totalizer (standard cardinality-to-CNF encodings). The SAT solver applies CDCL, restarts, and watched-literal propagation to the cardinality constraints. This is the core search mechanism.

**Theory oracle (CRC-32 verification).** When all cells in a row $r$ are assigned ($u(\text{row}_r) = 0$), the theory oracle computes CRC-32 of the 511-bit row and compares to the stored value. On failure, it generates a blocking clause:

$$\neg(x_{r,0} = v_0 \wedge x_{r,1} = v_1 \wedge \ldots \wedge x_{r,510} = v_{510})$$

This is a 511-literal clause blocking the specific wrong row assignment. While long, the CDCL engine propagates implications from this clause through the cardinality constraints to other cells, potentially pruning large portions of the search space.

**Key architectural difference from B.21.** The CDCL engine operates on cardinality constraints (linear, local, well-suited for 1-UIP analysis). The CRC-32 oracle generates blocking clauses WITHOUT invoking conflict analysis on the hash function itself. The hash is a black box; only its pass/fail result enters the SAT solver.

### B.54.5 The Deep Exploration Paradox and SAT Solvers

B.44d established the deep exploration paradox: the DFS solver's depth depends on tolerating wrong tentative assignments for constraint cascade feedback. Sub-block CRC-32 verification (64-cell boundaries) regressed DFS depth by 36% because it pruned the informative deep exploration.

**SAT solvers do not rely on deep exploration.** They use CDCL + restarts, restarting frequently and relying on learned clauses (accumulated from all previous failures) to prune the search space globally. The deep exploration paradox is specific to the DFS architecture's dependence on tentative wrong assignments for constraint cascade feedback. SAT solvers generate this feedback through clause learning instead.

This means the B.44d regression may not apply to B.54's SMT formulation. The CRC-32 row verification (or even sub-block verification) may be productive in the SAT context where it was counterproductive in the DFS context.

### B.54.6 Residual Problem Sizing

After the DFS solver assigns ~96,000 cells (rows 0-~189):

| Parameter | Full problem | Residual problem |
|-----------|-------------|-----------------|
| Variables | 261,121 | ~165,000 |
| Cardinality constraints | 5,108 | 5,108 (tightened) |
| Row hash verifications | 511 | ~322 (meeting-band rows) |
| Constraint density | 2.0% | 3.1% |

The residual problem has 55% higher constraint density than the full problem because the same number of constraint lines covers fewer variables. Each line's unknown count is reduced (from 511 to ~300), and the residuals are tighter.

### B.54.7 DI Determinism

CRSCE requires deterministic enumeration: the compressor and decompressor must follow identical search trajectories to agree on DI. Classical SAT solvers use non-deterministic restarts and variable ordering.

**Resolution:** The SMT solver operates on the residual problem AFTER the DFS solver has determined the partial assignment deterministically. The residual problem asks: does a solution exist that extends this partial assignment to satisfy all constraints + CRC-32 hashes? If the answer is yes, that solution IS the DI=0 CSM (with overwhelming probability, since CRC-32 + SHA-256 BH provide $2^{-16,608}$ false positive rate across all rows). The SAT solver does not enumerate solutions; it finds one satisfying assignment. Determinism is preserved because the DFS solver deterministically produces the same partial assignment, and the residual problem has a unique solution (the correct CSM).

If the residual problem has multiple solutions (CRC-32 false positives), the SHA-256 BH distinguishes them. The compressor tries each candidate and selects the one matching BH. With $2^{-32}$ false positive rate per row and 322 meeting-band rows, the expected number of false positives across ALL rows is $2^{-32 \times 322} = 2^{-10,304}$&mdash;essentially zero.

### B.54.8 Method

1. **Format change:** Replace SHA-1 lateral hashes with CRC-32 (32 bits per row). Retain SHA-256 BH.
2. **DFS phase:** Run the existing DFS solver on the test block. Record the partial assignment and per-line residuals at the plateau.
3. **Export:** Write the residual problem as a cardinality-constrained SAT instance (PB format) or ILP instance (LP/MPS format).
4. **SMT solve:** Invoke an SMT solver (or SAT solver + CRC-32 oracle callback) on the residual problem. Time limit: 1 hour.
5. **Verify:** Check the completed CSM against the SHA-256 block hash.
6. **Measure:** Solve time, number of CRC-32 oracle calls, number of learned clauses.

### B.54.9 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Full solve) | SMT finds the correct CSM within 1 hour | CRSCE is viable with a hybrid DFS+SMT solver at 23.2% compression ratio |
| H2 (Partial progress) | SMT assigns additional cells beyond DFS plateau but does not complete | Cardinality constraint reasoning provides value beyond singleton propagation; further solver tuning warranted |
| H3 (Timeout) | SMT cannot solve within 1 hour | The residual problem (~165K variables, ~5K cardinality constraints) is too under-constrained for CDCL/branch-and-cut |
| H4 (Immediate solve) | SMT solves in $< 1$ second | The cross-sum cardinality constraints alone (without hash verification) suffice to determine the CSM; the problem was always tractable, the DFS solver was simply the wrong architecture |

### B.54.10 Implementation

**Tools:**
- `tools/b54_export_residual.py`&mdash;runs the DFS solver, captures the plateau state, exports as PB/LP format.
- `tools/b54_smt_solve.py`&mdash;invokes PySAT or HiGHS on the exported problem with CRC-32 oracle callback.

**Dependencies:**
- PySAT (`python-sat`) for cardinality constraint encoding and SAT solving with theory oracle callbacks.
- HiGHS (`highspy`) for ILP solving with lazy constraint callbacks.
- The C++ DFS solver (existing) for the propagation-zone phase.

### B.54.11 Results (Completed &mdash; Not Viable in Current Form)

Four solver approaches were tested on the residual problem (164,542 unassigned cells in the meeting band, rows 189-510, with rows 0-188 pre-assigned from the correct CSM):

| Approach | Variables | Constraints | Result | Accuracy | Time |
|----------|-----------|-------------|--------|----------|------|
| ILP (all constraints) | 164,526 | 4,533 | Timeout | N/A | $>$600s |
| LP relaxation (row-only) | 164,542 | 322 | Solved | 49.9% | 0.5s |
| SAT (all constraints upfront) | 164,542 + 17M aux | 395M clauses | OOM killed | N/A | N/A |
| Lazy SMT (incremental) | 164,542 + growing | 40 iter $\times$ 20 constraints | 50.4% at iter 40 | 50.4% | 67s (28GB RAM) |

**Key findings:**

**1. LP relaxation is uninformative.** The row-only LP solves in 0.5 seconds and returns an all-integer solution&mdash;but it matches only 49.9% of meeting-band cells (essentially random). The constraint system is so under-determined (322 row equations, 164K variables, 164K free dimensions) that the LP finds an arbitrary vertex of the feasible polytope unrelated to the correct CSM. This also answers B.55: LP relaxation with iterative rounding will not converge because the fractional values carry no information.

**2. Cardinality constraint encoding is too large for SAT.** Each cardinality constraint on ~300 unknowns produces ~54K-95K CNF clauses (depending on encoding). With 2,877 constraint lines, the total is 155M-395M clauses and 5M-17M auxiliary variables. This exceeds available memory. Pseudo-Boolean (PB) solvers that handle cardinality constraints natively would avoid this encoding overhead but were unavailable in the test environment.

**3. Lazy SMT does not converge.** Adding violated constraints incrementally reduces violations (2,877 $\to$ 2,013 in 40 iterations) but accuracy stays at ~50%. Each added constraint locally fixes its line but does not propagate to other lines because the constraints are weakly coupled in the meeting band. After 40 iterations the solver consumed 28GB of RAM from accumulated cardinality encodings.

**4. The fundamental circular dependency persists.** The CRC-32 oracle can only fire when a row is fully assigned ($u = 0$). Completing a meeting-band row requires assigning ~511 cells. The cross-sum constraints alone cannot determine those cells (49.9% accuracy = random). The oracle's guidance is needed to identify the correct row assignment, but the oracle requires the row to be complete before it can evaluate. This is the same barrier the DFS solver faces, and no alternative solver architecture tested here breaks it.

**Implication for the CRC-32 format change.** The CRC-32 row hash improvement (48.2% $\to$ 23.2% compression ratio) remains valid independently of the solver question. It reduces payload size without affecting solver behavior. However, the SMT hybrid solver that motivated the CRC-32 change does not appear viable&mdash;the residual problem is too under-constrained for cross-sum reasoning alone, and the hash oracle faces the same row-completion barrier as the DFS solver.

**Tool:** `tools/b54_residual_ilp.py`

**Status: COMPLETED &mdash; NOT VIABLE. Cross-sum constraints alone are insufficient; CRC-32 oracle faces same row-completion barrier as DFS.**

---

