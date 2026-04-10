## B.58 Combinator-Based Symbolic Solver at $S=127$ (Proposed)

### B.58.1 Motivation

Every solver architecture tested by the CRSCE research program&mdash;DFS with constraint propagation (B.1&ndash;B.52), SAT/CDCL (B.21, B.54), ILP (B.54), LP relaxation (B.54/B.55), and SMT (B.54)&mdash;has failed to reconstruct the CSM beyond approximately 37% of the matrix at S=511. The root cause, established algebraically by B.39, is that the cross-sum constraint system has only 2% constraint density (5,097 independent equations over 261,121 cells), leaving a GF(2) null space of 256,024 dimensions. All solver architectures face the same information-theoretic barrier: the stored constraints carry insufficient information to determine the matrix.

B.57 proposed reducing S from 511 to 127 to increase constraint density from 2% to 6.2%. B.58 goes further: it eliminates the DFS solver entirely and replaces it with a **purely algebraic pipeline** of composable constraint-reduction operators (combinators). The key enabler is a previously unexploited property of CRC-32 lateral hashes.

**CRC-32 is a linear function over GF(2).** Unlike SHA-1 (a cryptographic hash with no useful algebraic structure), CRC-32 computes the polynomial remainder of the input treated as an element of GF(2)[x] modulo the CRC-32 generator polynomial. This is a linear map: $\text{CRC32}(a \oplus b) = \text{CRC32}(a) \oplus \text{CRC32}(b)$. Each of the 32 output bits is a linear function of the input bits over GF(2).

**Consequence:** The 127 CRC-32 lateral hashes provide $127 \times 32 = 4{,}064$ linear equations over GF(2) that can be incorporated directly into the constraint matrix *before* any row is complete. This eliminates the **row-completion barrier**&mdash;the fundamental circular dependency identified by B.54 that stymied all prior solver architectures:

> *"The CRC-32 oracle can only fire when a row is fully assigned ($u = 0$). Completing a meeting-band row requires assigning ~511 cells. The cross-sum constraints alone cannot determine those cells. The oracle's guidance is needed to identify the correct row assignment, but the oracle requires the row to be complete before it can evaluate."* &mdash; B.54.11

With CRC-32 as an algebraic (not verification) constraint, the oracle's information is available immediately, not after row completion. This is the architectural distinction between B.58 and all prior experiments: CRC-32 participates in the constraint system from the start, not as a post-hoc verification step.

### B.58.2 Key Innovation: CRC-32 as Algebraic Constraint

#### B.58.2.1 CRC-32 Polynomial Specification

CRSCE uses standard CRC-32 (ISO 3309/ITU-T V.42) with generator polynomial:

$$
    g(z) = z^{32} + z^{26} + z^{23} + z^{22} + z^{16} + z^{12} + z^{11} + z^{10} + z^8 + z^7 + z^5 + z^4 + z^2 + z + 1
$$

Hexadecimal representation: `0x04C11DB7` (normal form), `0xEDB88320` (bit-reversed).

**Padding convention.** Each row message is 127 data bits followed by 1 trailing zero bit (128 bits = 16 bytes), matching the B.57 row storage format. The trailing zero is a fixed constant and does not affect the linear relationship between data bits and CRC output.

**Reflection and inversion.** Standard CRC-32 applies bit-reflection on input bytes, bit-reflection on output, and XOR of the output with `0xFFFFFFFF`. These transformations are all affine over GF(2) (linear map plus constant). Bit-reflection permutes columns of the generator matrix. The output inversion adds a constant vector to the target. Both are absorbed into the generator matrix and target vector construction described below.

#### B.58.2.2 CRC-32 Generator Matrix Construction

For row $r$ with cells $x_{r,0}, x_{r,1}, \ldots, x_{r,126}$, the CRC-32 digest $h_r$ is computed as:

$$
    h_r = \text{CRC32}\!\left(\text{pack}(x_{r,0}, \ldots, x_{r,126}, 0)\right)
$$

where $\text{pack}$ serializes the 128 bits into 16 bytes MSB-first (matching CSM row storage). Because CRC-32 with fixed initialization and fixed finalization is an affine function of the input bits:

$$
    h_r = G_{\text{CRC}} \cdot \mathbf{x}_r \oplus \mathbf{c}
$$

where:

- $G_{\text{CRC}} \in \text{GF}(2)^{32 \times 127}$ is the CRC-32 generator matrix for 127-bit data
- $\mathbf{x}_r = (x_{r,0}, \ldots, x_{r,126})^T \in \text{GF}(2)^{127}$ is the row data vector
- $\mathbf{c} = \text{CRC32}(\mathbf{0}_{128}) \in \text{GF}(2)^{32}$ is the constant offset (CRC of the all-zeros row, absorbing initialization and finalization)

**Algorithm to compute $G_{\text{CRC}}$:**

```
def build_crc32_generator_matrix(n_data_bits=127, n_pad_bits=1):
    """Build the 32 x n_data_bits GF(2) generator matrix for CRC-32."""
    G = zeros(32, n_data_bits, dtype=GF2)

    # CRC-32 of the all-zeros message (absorbs init/finalize constants)
    c = crc32(bytes(ceil((n_data_bits + n_pad_bits) / 8)))

    for col in range(n_data_bits):
        # Create a message with only bit `col` set to 1
        msg = bytearray(ceil((n_data_bits + n_pad_bits) / 8))
        byte_idx = col // 8
        bit_idx  = 7 - (col % 8)           # MSB-first within each byte
        msg[byte_idx] |= (1 << bit_idx)

        # CRC of single-bit message XOR CRC of zero message = column of G
        crc_one = crc32(bytes(msg))
        G[:, col] = to_gf2_vector(crc_one ^ c)  # XOR removes the constant

    return G, to_gf2_vector(c)
```

This produces a $32 \times 127$ binary matrix $G_{\text{CRC}}$ and a 32-bit constant vector $\mathbf{c}$. Each row of $G_{\text{CRC}}$ gives one GF(2) linear equation in the 127 cell variables.

**Verification.** For any row vector $\mathbf{x}_r$:

$$
    \text{CRC32}(\text{pack}(\mathbf{x}_r, 0)) = G_{\text{CRC}} \cdot \mathbf{x}_r \oplus \mathbf{c}
$$

This identity must hold for all $2^{127}$ possible row values. It can be verified exhaustively for small $n$ (e.g., 8 or 16 bits) and by linearity for arbitrary $n$: if it holds for the zero vector and all unit vectors, it holds for all vectors by superposition.

#### B.58.2.3 Integration into the GF(2) Constraint System

For each row $r \in \{0, \ldots, 126\}$, the stored CRC-32 digest $h_r$ yields 32 linear equations:

$$
    G_{\text{CRC}} \cdot \mathbf{x}_r = h_r \oplus \mathbf{c} \quad \Longleftrightarrow \quad \forall i \in \{0, \ldots, 31\}: \bigoplus_{c=0}^{126} G_{\text{CRC},i,c} \cdot x_{r,c} = (h_r \oplus \mathbf{c})_i
$$

Each equation involves only the 127 cells of row $r$. In the global constraint matrix (16,129 columns, one per cell), each CRC-32 equation for row $r$ has non-zero entries only in columns $127r$ through $127r + 126$.

**Row-disjoint structure.** CRC-32 equations for different rows share no variables. This block-diagonal structure is critical for the row-grouped residual search (&sect;B.58.12).

**Independence from cross-sum parity.** The cross-sum parity for row $r$ is $\bigoplus_{c=0}^{126} x_{r,c} = \text{LSM}[r] \bmod 2$. This is the all-ones linear combination. The CRC-32 generator matrix $G_{\text{CRC}}$ has 32 rows; at most one of these is the all-ones vector (it would correspond to $g(1) = 0$, i.e., $z = 1$ being a root of the CRC polynomial). For CRC-32 with polynomial `0x04C11DB7`, evaluation at $z = 1$ gives $g(1) = 1$ (odd number of non-zero coefficients), so $z = 1$ is NOT a root. Therefore the all-ones vector is NOT a row of $G_{\text{CRC}}$, and **all 32 CRC-32 equations per row are linearly independent of the row parity constraint**.

**Net new GF(2) equations from CRC-32:** $127 \times 32 = 4{,}064$ (full independence from cross-sum parity, not $127 \times 31 = 3{,}937$ as conservatively estimated in the Motivation).

### B.58.3 Information-Theoretic Analysis

**GF(2) constraint budget at S=127.**

| Source | Equations | Est. Independent | Notes |
|--------|-----------|-----------------|-------|
| Cross-sum parity (6 families) | 1,014 | ~1,006 | Conservation axiom: ~8 dependent |
| CRC-32 (127 rows $\times$ 32 bits) | 4,064 | ~4,064 | Row-disjoint; fully independent of cross-sum parity (&sect;B.58.2.3) |
| **Total GF(2)** | **5,078** | **~5,070** | |

| Metric | Value |
|--------|-------|
| Total cells (variables) | 16,129 |
| GF(2) rank (cross-sums only) | ~1,006 |
| GF(2) null-space (cross-sums only) | ~15,123 (93.8%) |
| GF(2) rank (cross-sums + CRC-32) | ~5,070 |
| **GF(2) null-space (cross-sums + CRC-32)** | **~11,059 (68.6%)** |

CRC-32 reduces the GF(2) null-space from 93.8% to 68.6% of the matrix&mdash;a reduction of 4,064 dimensions. This is significant but insufficient for direct enumeration. However, the GF(2) analysis undercounts the total constraint information.

**Integer cross-sum information.** Each cardinality constraint $\sum x_j = k$ on a line of length $n$ eliminates all binary vectors with the wrong Hamming weight. For a line of length 127 at 50% density ($k \approx 63$):

$$
    I_{\text{cardinality}} = n - \log_2 \binom{n}{k} \approx 127 - 126 \approx 1 \text{ bit}
$$

At 50% density, the cardinality constraint provides approximately 1 bit of information per line&mdash;essentially just the parity. However, for lines near the matrix edge (short diagonals/anti-diagonals with length 1 to 126), the information content is higher:

- Length 1, sum 0 or 1: 0 bits remaining (fully determined)
- Length 5, sum 2: $5 - \log_2 \binom{5}{2} = 5 - 3.32 = 1.68$ bits of information
- Length 20, sum 10: $20 - \log_2 \binom{20}{10} = 20 - 18.0 = 2.0$ bits
- Length 127, sum 63: $\approx 1.0$ bit

**Short-diagonal cell count at S=127.** Diagonals and anti-diagonals with length $\leq 20$ provide significantly more than 1 bit of information each. At S=127:

| Diagonal lengths | Count (DSM + XSM) | Cells covered | Information/line |
|-----|-----|-----|-----|
| 1 | 4 | 4 | 0 bits remaining (fully determined) |
| 2&ndash;5 | 16 | 56 | 1.0&ndash;1.7 bits each |
| 6&ndash;20 | 60 | 780 | 1.5&ndash;2.0 bits each |
| 21&ndash;126 | 424 | ~27,884 | 1.0&ndash;1.5 bits each |
| 127 | 2 | 254 | 1.0 bit each |
| **Total** | **506** | ~28,978 | |

The 4 length-1 diagonals (corners) determine 4 cells immediately. The 16 length-2&ndash;5 diagonals provide strong constraints on 56 cells. This "edge effect" is proportionally larger at S=127 than at S=511 because the short diagonals constitute a larger fraction of the total constraint set.

**Combined information estimate.** The total information from all sources:

| Source | Bits | Nature |
|--------|------|--------|
| Integer cross-sums (1,014 lines) | ~7,098 | Integer (subsumes GF(2) parity) |
| CRC-32 (127 rows $\times$ 32 bits) | ~4,064 net | GF(2) (fully independent of cross-sum parity) |
| SHA-256 BH | 256 | Non-linear (final verification only) |
| **Total exploitable** | **~11,162** | |
| Cell entropy | 16,129 | |
| **Residual free dimensions** | **~4,967** | |

The ~4,967 residual dimensions are an optimistic lower bound (the integer and GF(2) constraints are not fully independent). The true number of free variables lies between 4,967 and 11,059. At S=511, the residual was ~256,024 dimensions&mdash;the S=127 system with CRC-32 represents a 23&ndash;52$\times$ reduction.

**Comparison with S=511.**

| Metric | S=511 (SHA-1) | S=127 (CRC-32) | Improvement |
|--------|--------------|-----------------|-------------|
| GF(2) null-space | 256,024 (98%) | ~11,059 (69%) | 23$\times$ smaller |
| Integer information | ~7,098 bits | ~7,098 bits | (same families) |
| Exploitable hash info | 0 (SHA-1 non-linear) | 4,064 bits (CRC-32 linear) | $\infty$ (qualitative) |
| Residual dimensions | ~256,024 | ~4,967&ndash;11,059 | 23&ndash;52$\times$ smaller |

### B.58.4 Combinator Architecture

B.58 replaces the DFS solver with a pipeline of pure, composable constraint-reduction operators. Each combinator is a function $\mathcal{C}: \mathcal{S} \to \mathcal{S}$ that produces a strictly more determined (or equally determined) system. The solver is their composition.

#### B.58.4.1 Data Structures

**Constraint system state.** The state $\mathcal{S}$ is a tuple:

$$
    \mathcal{S} = (G, b_G, A, b_A, D, F, \text{val}, \text{cell\_lines})
$$

where:

- $G \in \text{GF}(2)^{m \times n}$: the GF(2) constraint matrix (cross-sum parity + CRC-32 equations), packed as arrays of 64-bit words. Each row is $\lceil 16{,}129 / 64 \rceil = 253$ words.
- $b_G \in \text{GF}(2)^m$: the GF(2) target vector (one bit per equation).
- $A$: the integer constraint structure. For each line $L_i$, store:
  - $\text{target}[i] \in \mathbb{Z}$: the cross-sum value (0 to 127).
  - $\text{members}[i] \subseteq \{0, \ldots, n-1\}$: the set of cell indices on this line.
  - $\rho[i] \in \mathbb{Z}$: the residual sum (target minus sum of determined cells on this line).
  - $u[i] \in \mathbb{Z}$: the count of undetermined cells on this line.
- $b_A \in \mathbb{Z}^p$: the integer target vector (same as $\text{target}$; kept for notational clarity).
- $D \subseteq \{0, \ldots, n-1\}$: set of determined cells, with $\text{val}[j] \in \{0, 1\}$ for each $j \in D$.
- $F \subseteq \{0, \ldots, n-1\}$: set of undetermined cells ($F = \{0, \ldots, n-1\} \setminus D$).
- $\text{val}[j]$: known value of cell $j$ (defined only for $j \in D$).
- $\text{cell\_lines}[j]$: the list of line indices (integer constraint lines) containing cell $j$. Pre-computed from $A$. Each cell appears in exactly 6 lines (1 row + 1 column + 1 diagonal + 1 anti-diagonal + 2 yLTP).

Initially, $|D| = 0$, $|F| = 16{,}129$, $m = 5{,}078$, $p = 1{,}014$.

**Memory layout.** The GF(2) matrix $G$ is stored as a dense binary matrix: $5{,}078 \times 253$ uint64 words = $5{,}078 \times 2{,}024$ bytes $\approx 10.3$ MB. The integer structures ($\rho$, $u$, $\text{members}$) are stored as arrays with total size $< 1$ MB. Total memory: ~12 MB.

**Cell index convention.** Cell $(r, c)$ maps to global index $j = 127r + c$. This row-major ordering is canonical for DI enumeration.

#### B.58.4.2 Line Index Convention

| Family | Line index range | Count | Length |
|--------|-----------------|-------|--------|
| LSM (rows) | 0&ndash;126 | 127 | 127 |
| VSM (columns) | 127&ndash;253 | 127 | 127 |
| DSM (diagonals) | 254&ndash;506 | 253 | 1&ndash;127 |
| XSM (anti-diagonals) | 507&ndash;759 | 253 | 1&ndash;127 |
| yLTP1 | 760&ndash;886 | 127 | 127 |
| yLTP2 | 887&ndash;1,013 | 127 | 127 |
| **Total** | | **1,014** | |

Line membership (which cells belong to each line):

- LSM line $r$: cells $\{127r + c : c \in \{0, \ldots, 126\}\}$
- VSM line $c$: cells $\{127r + c : r \in \{0, \ldots, 126\}\}$
- DSM line $d$ (index $d \in \{0, \ldots, 252\}$): cells $(r, c)$ where $c - r = d - 126$, $0 \leq r, c \leq 126$
- XSM line $x$ (index $x \in \{0, \ldots, 252\}$): cells $(r, c)$ where $r + c = x$, $0 \leq r, c \leq 126$
- yLTP$t$ line $k$: cells $\{(r, c) : \text{LTP}_{t}(r, c) = k\}$, determined by Fisher-Yates permutation

#### B.58.4.3 yLTP Construction at S=127

Each yLTP sub-table partitions the 16,129 cells into 127 lines of exactly 127 cells each via the pool-chained Fisher-Yates construction from B.22:

```
def build_yltp_subtable(seed, s=127):
    """Fisher-Yates partition of s*s cells into s lines of s cells each."""
    rng = LCG(seed)                          # 64-bit LCG, same constants as production
    pool = list(range(s * s))                 # flat cell indices 0..16128
    membership = [0] * (s * s)               # membership[cell] = line_index

    for line in range(s):
        for slot in range(s):
            idx = line * s + slot
            swap_idx = idx + (rng.next() % (s * s - idx))
            pool[idx], pool[swap_idx] = pool[swap_idx], pool[idx]
            membership[pool[idx]] = line

    return membership
```

**Seed selection.** B.58 uses 2 yLTP sub-tables. The seeds are parameters of the experiment. B.58a tests 10 random seeds per sub-table to characterize rank variance. The optimal seeds from B.58a are fixed for B.58b and B.58c.

**Constraint on yLTP seeds.** The Fisher-Yates construction guarantees Axioms 1&ndash;3 (&sect;2.2.1) by construction: every cell is assigned to exactly one line per sub-table, every line has exactly $s$ cells, and no cell appears twice in any line.

#### B.58.4.4 Core Combinators &mdash; Detailed Specifications

**Combinator 1: GaussElim**

Performs GF(2) Gaussian elimination on the binary constraint matrix $G$ using partial pivoting. Produces the reduced row echelon form (RREF) and identifies pivot/free variables.

```
def gauss_elim(G, b_G):
    """GF(2) Gaussian elimination with partial pivoting.

    Args:
        G: m x n binary matrix (packed as uint64 arrays)
        b_G: m-element GF(2) target vector

    Returns:
        pivot_cols: list of column indices that are pivots (length = rank)
        free_cols:  list of column indices that are free (length = n - rank)
        rref_G:     reduced row echelon form of [G | b_G]
        rank:       GF(2) rank of G
    """
    m, n = G.shape
    pivot_row = 0
    pivot_cols = []

    for col in range(n):
        # Find a row with a 1 in this column at or below pivot_row
        found = -1
        for row in range(pivot_row, m):
            if G[row, col] == 1:
                found = row
                break
        if found == -1:
            continue                        # col is free

        # Swap rows
        G[pivot_row], G[found] = G[found], G[pivot_row]
        b_G[pivot_row], b_G[found] = b_G[found], b_G[pivot_row]

        # Eliminate all other rows with a 1 in this column
        for row in range(m):
            if row != pivot_row and G[row, col] == 1:
                G[row] ^= G[pivot_row]      # GF(2) row addition = XOR
                b_G[row] ^= b_G[pivot_row]

        pivot_cols.append(col)
        pivot_row += 1

    free_cols = [c for c in range(n) if c not in set(pivot_cols)]
    rank = len(pivot_cols)

    return pivot_cols, free_cols, G[:rank], b_G[:rank], rank
```

**Cost.** $O(r \cdot m \cdot \lceil n / 64 \rceil)$ where $r$ is the rank, $m$ is the row count, and $n$ is the column count. For the B.58 system: $O(5{,}070 \times 5{,}078 \times 253) \approx 6.5 \times 10^9$ word operations. At ~1 ns/word-XOR, this takes ~6.5 seconds. In practice, cache effects and SIMD (NumPy vectorized XOR) can reduce this to 1&ndash;3 seconds.

**Pivot selection and DI determinism.** The leftmost-column-first pivot strategy is mandatory for DI determinism: it produces a unique RREF for any given matrix, ensuring that the compressor and decompressor agree on which variables are pivots and which are free. The free variables in leftmost-first RREF are exactly those columns that have no leading 1 in the RREF&mdash;a deterministic function of the constraint system, not of the solver implementation.

**Determined cells from GaussElim.** After RREF, a cell $j$ is GF(2)-determined if column $j$ is a pivot column AND its RREF row has no non-zero entries in any free column. In that case, the cell's value equals the target bit directly. If the RREF row has non-zero free-column entries, the cell's value depends on free-variable assignments and is not yet determined.

```
def extract_determined_from_rref(rref_G, b_G, pivot_cols, free_cols, n):
    """Identify cells whose values are determined by GF(2) constraints alone.

    A pivot cell is determined iff its RREF row has zero weight in all free columns.
    """
    free_set = set(free_cols)
    determined = {}  # cell_index -> value

    for i, pcol in enumerate(pivot_cols):
        row = rref_G[i]
        has_free_dependency = any(row[fc] == 1 for fc in free_cols)
        if not has_free_dependency:
            determined[pcol] = b_G[i]

    return determined
```

**Combinator 2: IntBound**

Applies integer cardinality bounds to all constraint lines. For each line $L_i$ with residual $\rho_i$ and undetermined count $u_i$:

- If $\rho_i = 0$: all undetermined cells on $L_i$ must be 0.
- If $\rho_i = u_i$: all undetermined cells on $L_i$ must be 1.
- If $\rho_i < 0$ or $\rho_i > u_i$: the system is **inconsistent** (corrupted payload or hash collision).

Additionally, IntBound applies **interval tightening** beyond the $\rho = 0$ / $\rho = u$ forcing:

For each undetermined cell $j$ on line $L_i$:

- If $j = 1$ and $\rho_i = 0$: contradiction (cell contributes 1 but residual is 0).
- If $j = 0$ and $\rho_i = u_i$: contradiction (cell contributes 0 but all unknowns must be 1).

These per-cell checks enable the **cell-line interaction** deduction: if cell $j$ is on multiple lines, and setting $j = 0$ would make some line $L_i$ infeasible ($\rho_i > u_i - 1$, which means $\rho_i = u_i$ after removing $j$ with value 0, so the remaining $u_i - 1$ unknowns must sum to $\rho_i$; infeasible if $\rho_i > u_i - 1$), then $j$ must be 1.

```
def int_bound(lines, D, F, val):
    """Integer bound propagation.

    Args:
        lines: list of Line objects with .members, .rho, .u
        D: set of determined cell indices
        F: set of free cell indices
        val: dict of cell_index -> value for determined cells

    Returns:
        newly_determined: dict of cell_index -> value (newly forced cells)
        inconsistent: bool (True if system is contradictory)
    """
    newly_determined = {}
    queue = list(range(len(lines)))  # all lines to check

    while queue:
        i = queue.pop()
        L = lines[i]

        if L.u == 0:
            continue    # fully determined, nothing to do

        # Check feasibility
        if L.rho < 0 or L.rho > L.u:
            return {}, True  # inconsistent

        forced_value = None
        if L.rho == 0:
            forced_value = 0
        elif L.rho == L.u:
            forced_value = 1

        if forced_value is not None:
            for j in L.members:
                if j in F:
                    newly_determined[j] = forced_value
                    F.remove(j)
                    D.add(j)
                    val[j] = forced_value
                    # Update all lines containing j
                    for li in cell_lines[j]:
                        lines[li].u -= 1
                        lines[li].rho -= forced_value
                        if li not in queue:
                            queue.append(li)

    # Cell-line interaction: check if any free cell is forced by multi-line reasoning
    for j in list(F):
        forced = None
        for v in (0, 1):
            feasible = True
            for li in cell_lines[j]:
                L = lines[li]
                new_rho = L.rho - v
                new_u = L.u - 1
                if new_rho < 0 or new_rho > new_u:
                    feasible = False
                    break
            if not feasible:
                if forced is not None:
                    return {}, True  # both values infeasible
                forced = 1 - v      # the other value must hold
        if forced is not None:
            newly_determined[j] = forced
            F.remove(j)
            D.add(j)
            val[j] = forced
            for li in cell_lines[j]:
                lines[li].u -= 1
                lines[li].rho -= forced

    return newly_determined, False
```

**Cost.** The basic $\rho = 0$ / $\rho = u$ pass is $O(p)$ where $p = 1{,}014$ is the number of lines. The cell-line interaction pass is $O(|F| \times 6 \times 2)$ per iteration. Total: $O(|F| \times 12 + p) \approx O(16{,}129 \times 12) \approx 194$K operations per iteration. Negligible compared to GaussElim.

**Combinator 3: Propagate**

When GaussElim or IntBound determines new cells, Propagate substitutes their values into the GF(2) system and the integer constraint structures.

```
def propagate(G, b_G, lines, newly_determined, D, F, val, cell_lines):
    """Substitute determined cell values into all constraint systems.

    For GF(2): for each determined cell j with value v,
      - For each equation row with G[row, j] == 1: b_G[row] ^= v
      - Set column j to all zeros in G (variable eliminated)

    For integer: update rho and u on each line containing the cell.
    """
    for j, v in newly_determined.items():
        # GF(2) substitution
        for row in range(len(G)):
            if G[row, j] == 1:
                b_G[row] ^= v
                G[row, j] = 0

        # Integer substitution (already done in IntBound, but needed if
        # the cell was determined by GaussElim rather than IntBound)
        if j not in int_bound_determined:
            for li in cell_lines[j]:
                lines[li].u -= 1
                lines[li].rho -= v
```

**Cost.** $O(|\text{newly\_determined}| \times m)$ for GF(2) substitution. Worst case: $O(16{,}129 \times 5{,}078) \approx 82$M operations, but in practice the number of newly determined cells per fixpoint iteration is small.

**Combinator 4: CrossDeduce**

Derives new cell values from pairwise interactions between integer constraint lines. This combinator exploits the fact that at S=127, many pairs of constraint lines from different families share exactly 1 cell (row $r$ and column $c$ share cell $(r, c)$; row $r$ and diagonal $d$ share 0 or 1 cells).

For each pair of lines $(L_i, L_j)$ from different families sharing a set of undetermined cells $S_{ij} = \text{members}(L_i) \cap \text{members}(L_j) \cap F$:

1. Compute bounds on $\sigma_{ij} = \sum_{k \in S_{ij}} x_k$:
   - From $L_i$: $\max(0, \rho_i - (u_i - |S_{ij}|)) \leq \sigma_{ij} \leq \min(|S_{ij}|, \rho_i)$
   - From $L_j$: $\max(0, \rho_j - (u_j - |S_{ij}|)) \leq \sigma_{ij} \leq \min(|S_{ij}|, \rho_j)$
   - Intersect the two intervals.
2. If $|S_{ij}| = 1$: the intersection directly determines the single shared cell.
3. If $|S_{ij}| > 1$ and the interval collapses to a single value: the sum is determined, enabling further deduction on subsequent fixpoint passes.

```
def cross_deduce(lines, D, F, val, cell_lines):
    """Pairwise cross-line deduction.

    For each cell j in F, check all pairs of lines containing j.
    If any pair forces j's value, determine it.
    """
    newly_determined = {}

    # Pre-compute intersections for all (cell, pair_of_lines) triples
    for j in list(F):
        j_lines = cell_lines[j]
        for a in range(len(j_lines)):
            for b in range(a + 1, len(j_lines)):
                Li = lines[j_lines[a]]
                Lj = lines[j_lines[b]]

                # Shared undetermined cells between Li and Lj
                shared = set(Li.members) & set(Lj.members) & F
                if len(shared) != 1 or j not in shared:
                    continue  # Only handle |S| = 1 for now

                # Bounds on x_j from Li: rho_i - (u_i - 1) <= x_j <= rho_i
                lo_i = max(0, Li.rho - (Li.u - 1))
                hi_i = min(1, Li.rho)

                # Bounds on x_j from Lj: rho_j - (u_j - 1) <= x_j <= rho_j
                lo_j = max(0, Lj.rho - (Lj.u - 1))
                hi_j = min(1, Lj.rho)

                lo = max(lo_i, lo_j)
                hi = min(hi_i, hi_j)

                if lo > hi:
                    return {}, True  # inconsistent
                if lo == hi:
                    newly_determined[j] = lo
                    break
            if j in newly_determined:
                break

    # Apply determinations
    for j, v in newly_determined.items():
        F.remove(j)
        D.add(j)
        val[j] = v
        for li in cell_lines[j]:
            lines[li].u -= 1
            lines[li].rho -= v

    return newly_determined, False
```

**Cost.** $O(|F| \times \binom{6}{2}) = O(|F| \times 15)$ per iteration. The intersection computation ($|S_{ij}| = 1$ test) requires pre-computed per-line membership sets.

**Efficiency note.** The pairwise intersection test is the bottleneck when $|S_{ij}| > 1$. For S=127, the expected intersection size between two constraint lines from different families of length $n_1$ and $n_2$ is $n_1 \cdot n_2 / 16{,}129$. For two full-length lines ($n_1 = n_2 = 127$): $127^2 / 16{,}129 \approx 1.0$ cell. So most intersections have $|S_{ij}| \leq 2$, and the $|S_{ij}| = 1$ case dominates.

**Extension: multi-cell intersection deduction.** When $|S_{ij}| = k > 1$ and the intersected bounds determine $\sigma_{ij}$ to a single value, the $k$ shared cells satisfy $\sum_{c \in S} x_c = \sigma_{ij}$. If $\sigma_{ij} = 0$ or $\sigma_{ij} = k$, all cells are forced. For $0 < \sigma_{ij} < k$, the constraint is stored as a new cardinality constraint on the $k$-cell subset, which may participate in future CrossDeduce rounds. This creates derived constraints of increasingly small scope, potentially cascading to full determination.

**Combinator 5: Fixpoint**

Iterates all combinators until convergence (no new cells determined in a full round).

```
def fixpoint(S):
    """Iterate combinators to convergence.

    Returns the final constraint system state with maximal determined cells.
    """
    iteration = 0
    while True:
        prev_D_size = len(S.D)

        # Phase A: GF(2) reduction
        det_gf2 = extract_determined_from_rref(S.G, S.b_G, S.pivot_cols, S.free_cols, S.n)
        if det_gf2:
            propagate(S, det_gf2)

        # Phase B: Integer bound propagation
        det_int, inconsistent = int_bound(S.lines, S.D, S.F, S.val)
        if inconsistent:
            return S, False  # infeasible
        if det_int:
            propagate(S, det_int)
            # Re-run GaussElim on reduced system
            S.pivot_cols, S.free_cols, _, _, S.rank = gauss_elim(S.G, S.b_G)
            det_gf2_2 = extract_determined_from_rref(S.G, S.b_G, S.pivot_cols, S.free_cols, S.n)
            if det_gf2_2:
                propagate(S, det_gf2_2)

        # Phase C: Cross-deduction
        det_cross, inconsistent = cross_deduce(S.lines, S.D, S.F, S.val, S.cell_lines)
        if inconsistent:
            return S, False
        if det_cross:
            propagate(S, det_cross)
            S.pivot_cols, S.free_cols, _, _, S.rank = gauss_elim(S.G, S.b_G)

        iteration += 1
        new_D_size = len(S.D)

        print(f"  Fixpoint iteration {iteration}: "
              f"|D|={new_D_size} (+{new_D_size - prev_D_size}), "
              f"|F|={len(S.F)}, rank={S.rank}")

        if new_D_size == prev_D_size:
            break  # Convergence

        if new_D_size == S.n:
            break  # Fully determined

    return S, True
```

**Convergence guarantee.** Each iteration either increases $|D|$ (strict progress) or terminates. Since $|D| \leq n = 16{,}129$ and each combinator is monotone, the loop terminates in at most $n$ iterations. In practice, convergence is expected in 5&ndash;20 iterations (each GaussElim pass may determine hundreds or thousands of cells via chain reactions between GF(2) and integer reasoning).

**Iteration cap.** As a safety measure, the loop terminates after 200 iterations regardless. If 200 iterations are insufficient, the system is effectively at fixpoint (progress is less than 1 cell per iteration).

**Combinator 6: EnumerateFree**

After fixpoint, the free variables form the null-space basis. To enumerate solutions in DI-consistent order:

```
def enumerate_free(S, max_solutions=256):
    """Enumerate solutions over the free variables in lexicographic order.

    Free variables are ordered by cell index (row-major).
    Values are tried 0 before 1 (canonical lex order).

    Each free-variable assignment determines all pivot variables via RREF.
    The integer constraints are checked for consistency.
    SHA-256 block hash is checked for final verification.

    Yields solutions in DI order (0-indexed).
    """
    free_cols = sorted(S.free_cols)  # row-major order for DI determinism
    n_free = len(free_cols)

    if n_free == 0:
        # Unique solution: all cells determined
        csm = reconstruct_csm(S.val, S.n)
        if verify_bh(csm, S.bh):
            yield csm
        return

    if n_free > 64:
        raise TooManyFreeVariables(n_free)  # fallback to row-grouped search

    # RREF gives pivot[i] = b_G[i] XOR sum of (rref_G[i, fc] * free_val[fc])
    # Pre-extract the dependency matrix
    dep = {}  # pivot_col -> list of (free_col_index, coefficient) pairs
    for i, pcol in enumerate(S.pivot_cols):
        deps = [(fi, S.rref_G[i, fc]) for fi, fc in enumerate(free_cols)
                if S.rref_G[i, fc] == 1]
        dep[pcol] = (S.b_G_rref[i], deps)

    solutions_found = 0
    for assignment in range(2 ** n_free):
        # Decode assignment: bit 0 = free_cols[0], bit 1 = free_cols[1], ...
        free_vals = [(assignment >> i) & 1 for i in range(n_free)]

        # Compute all pivot values
        candidate_val = dict(S.val)  # start with already-determined cells
        for fc_idx, fc in enumerate(free_cols):
            candidate_val[fc] = free_vals[fc_idx]

        for pcol, (base, deps) in dep.items():
            pval = base
            for fc_idx, coeff in deps:
                pval ^= (free_vals[fc_idx] & coeff)
            candidate_val[pcol] = pval

        # Check integer constraints
        if not check_integer_constraints(candidate_val, S.lines):
            continue

        # Reconstruct CSM and verify block hash
        csm = reconstruct_csm(candidate_val, S.n)
        if verify_bh(csm, S.bh):
            yield csm
            solutions_found += 1
            if solutions_found >= max_solutions:
                return
```

**DI determinism.** The enumeration order is canonical:

1. Free variables are sorted by cell index $(r, c)$ in row-major order (ascending $j = 127r + c$).
2. The binary counter iterates from 0 to $2^{n_{\text{free}}} - 1$, assigning bit $i$ to free variable $i$ in order.
3. This produces solutions in lexicographic order over the free variables, matching the DFS enumeration order.

**Cost.** $O(2^{n_{\text{free}}} \times (n_{\text{free}} \cdot r + p))$ where $r$ is the GF(2) rank (for pivot computation) and $p = 1{,}014$ (for integer constraint checking). For $n_{\text{free}} \leq 40$: $\leq 2^{40} \times (40 \times 5{,}070 + 1{,}014) \approx 2^{40} \times 204$K $\approx 2.2 \times 10^{17}$ operations. At $10^9$ ops/sec, this is ~$2.2 \times 10^8$ seconds (infeasible). **Therefore, $n_{\text{free}} > 30$ requires the row-grouped search (B.58c) rather than brute-force enumeration.**

Revised tractability threshold: **$n_{\text{free}} \leq 25$** for exhaustive enumeration within 1 hour ($2^{25} \times 204$K $\approx 6.8 \times 10^{12}$ $\approx 6{,}800$ seconds at $10^9$ ops/sec; optimizable to minutes with bitwise tricks).

**Combinator 7: VerifyBH**

Computes SHA-256 over the reconstructed CSM (row-major, 127 rows $\times$ 2 uint64 words = 2,032 bytes) and compares to the stored block hash.

```
def verify_bh(csm, expected_bh):
    """Verify SHA-256 block hash of reconstructed CSM."""
    import hashlib
    raw = csm.tobytes()              # row-major, 127 * 16 = 2032 bytes
    actual = hashlib.sha256(raw).digest()
    return actual == expected_bh
```

**Cost.** SHA-256 on 2,032 bytes: ~1 &mu;s.

#### B.58.4.5 Solver Pipeline

$$
    \text{solve} = \text{VerifyBH} \circ \text{EnumerateFree} \circ \text{Fixpoint}(\text{CrossDeduce} \circ \text{Propagate} \circ \text{IntBound} \circ \text{GaussElim}) \circ \text{BuildSystem}
$$

where BuildSystem constructs $\mathcal{S}$ from the compressed payload (cross-sums, CRC-32 hashes, yLTP tables).

#### B.58.4.6 DI Discovery (Compressor Side)

The compressor knows the original CSM. It runs the combinator pipeline to determine which DI index the original CSM occupies:

```
def discover_di(original_csm, payload):
    """Find the DI such that EnumerateFree produces original_csm as the DI-th solution."""
    S = build_system(payload)
    S, feasible = fixpoint(S)

    if not feasible:
        raise CompressionError("Constraint system infeasible")

    di = 0
    for candidate in enumerate_free(S, max_solutions=256):
        if candidate == original_csm:
            return di
        di += 1

    raise CompressionError(f"Original CSM not found within 256 solutions")
```

If the fixpoint fully determines the CSM ($|F| = 0$), the solution is unique and DI = 0. This is the ideal case: compression requires only the combinator fixpoint (milliseconds) and produces DI = 0 for every block.

#### B.58.4.7 Error Handling

**Inconsistent system.** If IntBound or CrossDeduce detects $\rho < 0$ or $\rho > u$ on any line, the payload is corrupted (transmission error or hash collision). The decompressor reports a decompression failure. No attempt at partial recovery.

**No solution passes SHA-256.** If EnumerateFree exhausts all $2^{n_{\text{free}}}$ assignments without finding a SHA-256 match, the payload is corrupted. Report decompression failure.

**DI overflow.** If the compressor finds $\geq 256$ solutions before matching the original CSM, compression fails (DI cannot be represented in 8 bits). This is the same semantics as S=511; the combinator approach does not change the DI representation.

**GF(2) inconsistency.** If GaussElim produces a row of the form $[0, 0, \ldots, 0 | 1]$ (zero left-hand side, non-zero right-hand side), the GF(2) system is inconsistent. This indicates a corrupted CRC-32 or cross-sum value. Report decompression failure.

### B.58.5 Worked Example at S=7

To illustrate the combinator pipeline, consider a minimal example with $S = 7$ (49 cells, $b = 3$).

**Setup.** CSM is a $7 \times 7$ binary matrix. Constraint families: LSM (7 rows), VSM (7 columns), DSM (13 diagonals), XSM (13 anti-diagonals), yLTP1 (7 lines), yLTP2 (7 lines). Total: 54 lines. CRC-32 contributes $7 \times 32 = 224$ GF(2) equations. Total GF(2) equations: $54 + 224 = 278$ over 49 variables.

**GF(2) rank estimate.** With 278 equations and 49 variables, the rank is at most 49. If the system has full column rank (rank = 49), every cell is GF(2)-determined and the fixpoint produces a unique solution without search.

**Expected outcome at S=7.** The CRC-32 alone provides $7 \times 32 = 224$ equations on 49 variables. Even accounting for dependencies, this is massively overconstrained in GF(2). The system is almost certainly rank 49. **At S=7, the combinator approach is expected to achieve full determination in a single GaussElim pass.**

**Scaling question.** The ratio of CRC-32 equations to variables is $7 \times 32 / 49 = 4.57$ at S=7, but $127 \times 32 / 16{,}129 = 0.252$ at S=127. At S=127, CRC-32 provides only 0.25 equations per variable (underdetermined in GF(2)). The cross-sum parity adds another $1{,}006 / 16{,}129 = 0.062$ equations per variable. Total: 0.31 equations per variable. This is well below 1.0, so the system is underdetermined. The question is *how* underdetermined&mdash;which is precisely what B.58a measures.

### B.58.6 Method

**Phase 1: Build the GF(2) system.**

(a) **Construct the cross-sum parity matrix.** For each of the 1,014 constraint lines, create a GF(2) row vector of length 16,129 indicating which cells belong to that line (1 if the cell is a member, 0 otherwise). The target bit is the cross-sum value modulo 2. This produces a $1{,}014 \times 16{,}129$ binary matrix.

(b) **Construct the CRC-32 matrix.** Using the algorithm from &sect;B.58.2.2, compute $G_{\text{CRC}}$ (a $32 \times 127$ binary matrix) and $\mathbf{c}$ (the 32-bit constant). For each row $r \in \{0, \ldots, 126\}$:

- Create 32 GF(2) equations, each of length 16,129: non-zero only in columns $127r$ through $127r + 126$, with the bit pattern from the corresponding row of $G_{\text{CRC}}$.
- The target bit for equation $i$ of row $r$ is $(h_r \oplus \mathbf{c})_i$, where $h_r$ is the stored CRC-32 digest.

This produces a $4{,}064 \times 16{,}129$ binary matrix with block-diagonal structure (each $32 \times 127$ block is non-zero only in the columns of its row).

(c) **Assemble the full GF(2) system.** Stack the cross-sum parity matrix ($1{,}014 \times 16{,}129$) and the CRC-32 matrix ($4{,}064 \times 16{,}129$) vertically to produce the $5{,}078 \times 16{,}129$ matrix $G$ with target vector $b_G$ of length 5,078.

(d) **Construct the integer constraint structures.** For each of the 1,014 lines, store the membership set, target sum, and initialize $\rho = \text{target}$, $u = |\text{members}|$.

(e) **Construct the cell-to-line index.** For each cell $j$, pre-compute the list of line indices $\text{cell\_lines}[j]$ (exactly 6 entries per cell).

**Phase 2: GF(2) Gaussian elimination.**

Apply GaussElim (&sect;B.58.4.4, Combinator 1) to obtain RREF. Record:

- GF(2) rank $r$ (this is the primary measurement for B.58a)
- Pivot columns (cells that are pivot variables)
- Free columns (cells that are free variables)
- Null-space dimension $n - r = 16{,}129 - r$
- Immediately determined cells (pivots with no free-column dependencies)

**Phase 3: Fixpoint iteration.**

Execute the Fixpoint combinator (&sect;B.58.4.4, Combinator 5). Record per-iteration telemetry:

- $|D|$ (determined cells) after each iteration
- $|F|$ (free cells) after each iteration
- Source of each determination (GaussElim, IntBound, or CrossDeduce)
- Wall time per iteration

**Phase 4: Residual analysis.**

After fixpoint convergence:

- If $|F| = 0$: verify CSM against SHA-256 and report success.
- If $0 < |F| \leq 25$: execute EnumerateFree to find the DI-indexed solution.
- If $25 < |F| \leq 1{,}000$: proceed to row-grouped search (B.58c).
- If $|F| > 1{,}000$: report the free-variable distribution and declare the approach insufficient.

**Phase 5: Output.**

For each block, report:

| Metric | Value |
|--------|-------|
| GF(2) rank | (from Phase 2) |
| Null-space dimension | $16{,}129 - r$ |
| Fixpoint $\|D\|$ | (after Phase 3) |
| Fixpoint $\|F\|$ | (after Phase 3) |
| Fixpoint iterations | (count) |
| Fixpoint wall time | (seconds) |
| Cells from GaussElim | (count) |
| Cells from IntBound | (count) |
| Cells from CrossDeduce | (count) |
| DI (if solved) | (0&ndash;255) |
| SHA-256 verified | (yes/no) |

### B.58.7 Density Sensitivity Analysis

The combinator approach's effectiveness depends critically on the input density (fraction of 1s in the CSM). This section analyzes the expected behavior across densities.

**Information content vs. density.** For a line of length $n$ with sum $k$, the information content is $n - \log_2 \binom{n}{k}$. Aggregated over all lines:

| Density | Avg. sum per line (len 127) | Info/line (bits) | Total integer info (1,014 lines) |
|---------|---------------------------|------------------|----------------------------------|
| 10% | 12.7 | ~5.6 | ~5,678 |
| 20% | 25.4 | ~3.2 | ~3,245 |
| 30% | 38.1 | ~1.8 | ~1,825 |
| 40% | 50.8 | ~1.2 | ~1,217 |
| 50% | 63.5 | ~1.0 | ~1,014 |

At 10% density, the integer constraints carry 5.6$\times$ more information than at 50% density. Combined with CRC-32's fixed 4,064 GF(2) bits, the total exploitable information at 10% density is ~9,742 bits vs. ~5,078 bits at 50% density. With 16,129 cell bits total, this gives:

| Density | Total exploitable bits | Residual free dimensions | Expected outcome |
|---------|----------------------|--------------------------|------------------|
| 10% | ~9,742 | ~6,387 | Likely solvable (GF(2) dominant) |
| 30% | ~5,889 | ~10,240 | Possibly solvable with search |
| 50% | ~5,078 | ~11,051 | Hardest case; most free variables |

**Prediction.** The combinator approach is most likely to succeed on low-density ($\leq 30\%$) and high-density ($\geq 70\%$) blocks, where the integer constraints carry more information. At 50% density (worst case), the approach depends entirely on the CRC-32 GF(2) equations and the cross-deduction cascade.

**Experimental design.** B.58b should test blocks at multiple densities (10%, 30%, 50%, 70%, 90%) to characterize the density-viability curve.

### B.58.8 Sub-experiment B.58a: GF(2) Rank and Null-Space at S=127 with CRC-32

**Objective.** Determine the exact GF(2) rank of the combined cross-sum + CRC-32 constraint system at S=127 with 2 yLTP sub-tables. This is the foundational measurement that determines whether the combinator approach is viable.

**Method.**

(a) Generate a random $127 \times 127$ binary CSM at 50% density.

(b) Compute cross-sums for all 6 families (LSM, VSM, DSM, XSM, yLTP1, yLTP2).

(c) Compute CRC-32 for all 127 rows.

(d) Construct $G_{\text{CRC}}$ using the algorithm from &sect;B.58.2.2.

(e) Assemble the full GF(2) constraint matrix ($5{,}078 \times 16{,}129$).

(f) Compute GF(2) rank via Gaussian elimination (leftmost-column-first).

(g) Repeat (a)&ndash;(f) with 10 different yLTP seed pairs to characterize seed-dependent variance.

(h) Repeat (a)&ndash;(f) without CRC-32 (cross-sums only, $1{,}014 \times 16{,}129$) to measure the CRC-32 contribution independently.

(i) Perform stratified rank analysis, adding constraint families one at a time:

| Configuration | Expected rank |
|---------------|---------------|
| LSM only (127 rows) | 126 |
| LSM + VSM (254) | ~253 |
| LSM + VSM + DSM (507) | ~503 |
| LSM + VSM + DSM + XSM (760) | ~753 |
| + yLTP1 (887) | ~879 |
| + yLTP2 (1,014) | ~1,006 |
| + CRC-32 for all rows (5,078) | **MEASURE** |

The stratified analysis reveals how many independent GF(2) constraints CRC-32 actually adds after the cross-sum system has been established.

**Report.** For each seed pair and configuration:

- GF(2) rank
- Null-space dimension
- CRC-32 contribution: rank(cross-sums + CRC-32) $-$ rank(cross-sums only)
- Basis weight distribution (min, p5, p25, median, mean, p75, p95, max)
- Minimum-weight null-space vector (via pairwise XOR of lightest basis vectors, as in B.39a)

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (High rank) | GF(2) rank $\geq$ 14,000 | Null-space $\leq$ 2,129; combinators likely achieve full solve |
| H2 (Moderate rank) | GF(2) rank 8,000&ndash;14,000 | Null-space 2,129&ndash;8,129; residual may be tractable with row-grouped search |
| H3 (Low rank) | GF(2) rank $\leq$ 5,000 | Null-space $\geq$ 11,129; CRC-32 equations largely redundant; approach unlikely viable |

**Note on H1 plausibility.** Achieving rank $\geq$ 14,000 requires the CRC-32 equations to contribute $\geq$ 12,994 independent GF(2) equations. This would mean 12,994 of the 4,064 CRC-32 equations are independent&mdash;which is impossible (only 4,064 exist). **Therefore, H1 is unachievable from GF(2) rank alone.** The maximum possible GF(2) rank is $\min(5{,}078, 16{,}129) = 5{,}078$. The practical maximum is ~5,070 (accounting for conservation dependencies). This gives a null-space of ~$16{,}129 - 5{,}070 = 11{,}059$.

**Revised expected outcomes (corrected).**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Near-maximum rank) | GF(2) rank $\geq$ 5,000 | CRC-32 is contributing nearly all its 4,064 equations independently; maximum GF(2) exploitation achieved; null-space ~11,059&ndash;11,129 |
| H2 (Partial CRC contribution) | GF(2) rank 3,000&ndash;5,000 | CRC-32 contributes 2,000&ndash;4,000 independent equations; partial but significant |
| H3 (Minimal CRC contribution) | GF(2) rank $\leq$ 2,000 | CRC-32 equations mostly redundant with cross-sum parity; approach not viable from GF(2) alone |

**Critical realization.** The GF(2) null-space will remain ~11,000 dimensions regardless of outcome. The combinator approach's viability therefore depends not on GF(2) rank alone but on the **interaction between GF(2) and integer constraints** during the fixpoint loop. The GF(2) rank determines the starting point; IntBound and CrossDeduce must close the remaining gap. This makes B.58b (the full pipeline test) essential regardless of B.58a outcome.

### B.58.9 Sub-experiment B.58b: Full Symbolic Solve Pipeline

**Prerequisite.** B.58a completed (any outcome). B.58b proceeds regardless of B.58a outcome because the GF(2)-integer interaction is the core hypothesis.

**Objective.** Implement the full combinator pipeline and measure the fixpoint: how many cells does the symbolic approach determine without any search?

**Method.**

(a) Select the best yLTP seed pair from B.58a (highest GF(2) rank).

(b) Generate a test CSM. Two sources:

- **Synthetic:** random $127 \times 127$ binary matrix at 50% density (repeat for 10%, 30%, 70%, 90%).
- **Real data:** compress the first 2,016 bytes of `useless-machine.mp4` into a $127 \times 127$ CSM. Record the actual density.

(c) Compute all projections (cross-sums, CRC-32, SHA-256) and construct the full payload.

(d) Run the combinator pipeline from the decompressor's perspective: given only the payload (not the original CSM), reconstruct.

(e) Execute the fixpoint loop: GaussElim $\to$ IntBound $\to$ Propagate $\to$ CrossDeduce, iterated until stable.

(f) Record per-iteration telemetry (&sect;B.58.6, Phase 5 output table).

(g) If fixpoint does not fully determine the CSM, record the free-variable distribution:

| Row $r$ | Free cells $f_r$ | CRC-32 residual equations | Effective per-row free vars ($f_r - \text{CRC residual}$) |
|---------|-------------------|--------------------------|------------------------------------------------------|
| 0 | ... | ... | ... |
| ... | ... | ... | ... |
| 126 | ... | ... | ... |

(h) Verify: compare the reconstructed CSM (or the best candidate from EnumerateFree) against the original CSM to confirm correctness.

**Expected outcomes.**

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Full solve) | $|F| = 0$ after fixpoint | CSM fully determined by combinators; no search required; CRSCE is viable at S=127 |
| H2 (Near-full solve) | $|F| \leq 25$ after fixpoint | Residual enumeration ($\leq 2^{25}$) is tractable; CRSCE viable with seconds of search |
| H3 (Partial solve) | $25 < |F| \leq 1{,}000$ | Row-grouped search needed; viability depends on per-row distribution (B.58c) |
| H4 (Minimal solve) | $|F| > 1{,}000$ | Combinator approach does not reduce free variables below DFS-equivalent at S=127 |

**Decision gate.** If H4 on 50% density AND H4 on all other densities: B.58 is not viable. If H3 or better on any density: proceed to B.58c for that density.

### B.58.10 Sub-experiment B.58c: Row-Grouped Residual Search

**Prerequisite.** B.58b reports H3 ($25 < |F| \leq 1{,}000$) on at least one density.

**Objective.** Exploit the row-local structure of CRC-32 constraints to decompose the residual enumeration into per-row sub-problems, then combine via cross-row constraint propagation.

#### B.58.10.1 Per-Row Candidate Generation

After the fixpoint, each row $r$ has $f_r$ free cells. The CRC-32 equations for row $r$ have been partially consumed by GaussElim; let $c_r$ be the number of remaining independent CRC-32 equations that constrain only the free cells of row $r$. The per-row search space is $2^{f_r - c_r}$ (the CRC-32 residual equations reduce the dimension).

For each row $r$:

```
def generate_row_candidates(r, free_cells_in_row, crc_residual_eqns, row_sum_residual):
    """Generate all assignments to free cells in row r consistent with
    CRC-32 residual equations and integer row sum.

    Args:
        r: row index
        free_cells_in_row: list of cell indices that are free in row r
        crc_residual_eqns: GF(2) equations on the free cells (after fixpoint)
        row_sum_residual: rho for the row's integer constraint

    Returns:
        candidates: list of assignment dicts {cell_index: value}
    """
    f_r = len(free_cells_in_row)
    c_r = len(crc_residual_eqns)

    # GF(2) null-space of CRC residual equations
    # (free cells of row r, reduced by c_r equations)
    row_free_basis = gf2_null_space(crc_residual_eqns)
    n_row_free = len(row_free_basis)  # = f_r - c_r

    candidates = []
    for bits in range(2 ** n_row_free):
        # Construct assignment from null-space basis
        assignment = base_assignment(crc_residual_eqns)
        for i in range(n_row_free):
            if (bits >> i) & 1:
                assignment ^= row_free_basis[i]

        # Check integer row sum
        if sum(assignment[c] for c in free_cells_in_row) != row_sum_residual:
            continue

        candidates.append(assignment)

    return candidates
```

**Per-row cost.** $O(2^{n_{\text{row\_free}}})$ per row. If $f_r \leq 50$ and $c_r \geq 25$, then $n_{\text{row\_free}} \leq 25$ and per-row search is $\leq 2^{25} \approx 33$M&mdash;tractable in seconds.

**Expected per-row candidates.** The integer row-sum constraint eliminates most GF(2)-consistent assignments. For a row with $f_r = 40$ free cells, row sum residual $\rho = 20$, and $c_r = 30$ CRC-32 equations: the GF(2) search space is $2^{10} = 1{,}024$, and the integer filter retains $\binom{10}{?} / 2^{10}$ fraction (approximately $\binom{40}{20} / 2^{40} \approx 13.2\%$). Expected candidates per row: $\sim 135$.

#### B.58.10.2 Cross-Row Consistency via Arc Consistency

After generating per-row candidates, the cross-row problem is: select one candidate per row such that all inter-row constraints (column sums, diagonal sums, anti-diagonal sums, yLTP sums) are satisfied.

This is a constraint satisfaction problem (CSP) with:

- **Variables:** 127 rows, each with a domain of per-row candidates
- **Constraints:** ~887 inter-row constraints (127 columns + 253 diagonals + 253 anti-diagonals + 127 yLTP1 + 127 yLTP2)

**Arc consistency propagation.**

```
def arc_consistency(row_candidates, inter_row_constraints):
    """Reduce per-row candidate sets via arc consistency (AC-3).

    For each inter-row constraint (e.g., column c with sum target k):
      For each row r that has cells in column c:
        Remove candidates for row r that are inconsistent with ALL
        remaining candidates for all other rows touching column c.

    Iterate until no more candidates are removed.
    """
    changed = True
    while changed:
        changed = False
        for constraint in inter_row_constraints:
            affected_rows = constraint.rows  # rows that have cells in this constraint
            target = constraint.rho          # residual sum target

            for r in affected_rows:
                new_candidates = []
                for cand in row_candidates[r]:
                    # Check if this candidate is consistent with at least one
                    # combination of candidates from other affected rows
                    cand_contribution = constraint.contribution(r, cand)
                    remaining_target = target - cand_contribution
                    remaining_rows = [r2 for r2 in affected_rows if r2 != r]

                    if can_achieve_target(remaining_target, remaining_rows,
                                         row_candidates, constraint):
                        new_candidates.append(cand)

                if len(new_candidates) < len(row_candidates[r]):
                    row_candidates[r] = new_candidates
                    changed = True

                if len(new_candidates) == 0:
                    return None  # inconsistent

    return row_candidates
```

**Cost.** Arc consistency on the cross-row CSP has worst-case cost $O(e \cdot d^2)$ where $e$ is the number of constraints and $d$ is the maximum domain size. With $e = 887$ and $d = 135$ (estimated per-row candidates): $O(887 \times 135^2) \approx 16$M operations per pass. With 10&ndash;50 passes for convergence: $\leq$ 800M operations, completing in $< 1$ second.

**If arc consistency reduces all domains to size 1:** the solution is unique (verify with SHA-256).

**If some domains remain $> 1$:** apply DFS over the remaining multi-valued rows. With arc consistency, the residual typically has very small domains (2&ndash;10 candidates per row), making exhaustive search tractable.

#### B.58.10.3 Fallback: DFS on Row-Candidate CSP

If the arc-consistent domain sizes are still too large for direct enumeration:

```
def dfs_row_csp(row_candidates, inter_row_constraints, row_order):
    """DFS over per-row candidates with forward-checking.

    row_order: rows sorted by domain size (smallest first = fail-first heuristic)
    """
    assignment = {}  # row -> selected candidate

    def backtrack(idx):
        if idx == len(row_order):
            # All rows assigned; verify SHA-256
            csm = reconstruct_csm_from_row_assignments(assignment)
            return verify_bh(csm, expected_bh)

        r = row_order[idx]
        for cand in row_candidates[r]:
            assignment[r] = cand

            # Forward check: does this candidate violate any inter-row constraint?
            if forward_check(r, cand, assignment, inter_row_constraints, row_candidates):
                if backtrack(idx + 1):
                    return True

        del assignment[r]
        return False

    return backtrack(0)
```

The DFS over the row-candidate CSP is fundamentally different from the original cell-level DFS: each "branching decision" assigns an entire row (127 cells) simultaneously, and the domain per row is pre-filtered by CRC-32 + integer sum. This is a search over ~$100^{127}$ in the worst case but typically collapses to a small tree after arc consistency.

### B.58.11 Expected Outcomes

| Outcome | Criteria | Interpretation |
|---------|----------|----------------|
| H1 (Full algebraic solve) | B.58b H1: $|F| = 0$ | The combinator approach fully solves S=127 CSMs. CRSCE is viable at S=127 with CRC-32 LH. The DFS solver is unnecessary. |
| H2 (Tractable residual) | B.58b H2 or B.58c succeeds | CRSCE is viable at S=127 with a hybrid approach: symbolic reduction + bounded search. Decompression time dominated by GF(2) Gaussian elimination ($\approx$ seconds). |
| H3 (Row-decomposable) | B.58c partially succeeds | Some blocks solvable, others not. Viability depends on input density and yLTP table. The approach is density-sensitive: low-entropy blocks (density far from 50%) may solve fully while 50%-density blocks require deeper search. |
| H4 (Not viable) | B.58a H3 AND B.58b H4 on all densities | CRC-32 equations are insufficiently independent of cross-sum constraints. The GF(2) rank does not increase enough, and integer/cross-deduction reasoning cannot close the gap. |

**H4 implication.** If the CRC-32 equations are largely redundant with cross-sum parity at S=127, then algebraic hash exploitation is not a viable path for CRSCE at any matrix dimension. This would confirm that the hash information (needed to break the null-space ambiguity) is fundamentally inaccessible to polynomial-time algebraic methods, and that the only route to full reconstruction is exponential-time search over the null space&mdash;which is infeasible at all practical matrix sizes.

**H1 implication.** If the combinator approach fully solves S=127 CSMs, the production C++ solver should be rewritten from DFS to the combinator pipeline. Decompression becomes a deterministic algebraic computation (GF(2) Gaussian elimination + integer propagation) with no backtracking. Compression discovers DI = 0 for every block (if the fixpoint is unique) or small DI via bounded enumeration. The format achieves 36.1% compression with provably efficient decompression.

### B.58.13 Results

#### B.58a: GF(2) Rank (COMPLETE &mdash; H1)

| Configuration | Rank | Null-Space | CRC-32 Contribution |
|---------------|------|-----------|---------------------|
| LSM only | 127 | 16,002 | &mdash; |
| +VSM | 253 | 15,876 | &mdash; |
| +DSM | 504 | 15,625 | &mdash; |
| +XSM | 753 | 15,376 | &mdash; |
| +yLTP1 | 879 | 15,250 | &mdash; |
| +yLTP2 (all cross-sums) | 1,005 | 15,124 | &mdash; |
| **+CRC-32 (full system)** | **5,037** | **11,092** | **+4,032 / 4,064 (99.2%)** |

CRC-32 contributes 4,032 of its theoretical 4,064 equations independently. Only 32 are redundant with cross-sum parity (exactly 1 per row). This is the best possible GF(2) outcome.

#### B.58b: Full Symbolic Solve Pipeline (COMPLETE &mdash; H4)

| Source | Density | Determined | GF(2) | IntBound | CrossDeduce | Free | Outcome |
|--------|---------|-----------|-------|----------|-------------|------|---------|
| MP4 block 0 | 14.9% | 3,600 (22.3%) | 4 | 3,596 | 0 | 12,529 | INSUFFICIENT |
| Synthetic | 9.7% | 302 (1.9%) | 4 | 298 | 0 | 15,827 | INSUFFICIENT |
| Synthetic | 30.2% | 35 (0.2%) | 4 | 31 | 0 | 16,094 | INSUFFICIENT |
| Synthetic | 49.9% | 17 (0.1%) | 4 | 13 | 0 | 16,112 | INSUFFICIENT |

Despite 5,037 GF(2) rank, the Gaussian elimination determines only 4 cells (the length-1 corner diagonals). The null-space of 11,092 dimensions means almost all pivot columns have free-variable dependencies. IntBound produces the same result as the DFS propagation engine. CrossDeduce contributes zero across all densities.

**Root cause:** The CRC-32 equations are informationally correct but operationally useless for cell determination. The information is distributed across 11,092 undetermined dimensions &mdash; no individual cell's value is uniquely determined by the GF(2) system. The row-completion barrier has been transformed from "can't evaluate oracle until row complete" to "can't determine individual cell from underdetermined linear system."

#### B.58c: Row-Grouped Residual Search (COMPLETE &mdash; H4)

Analyzed all 1,331 blocks of the MP4 test file:

| Metric | Value |
|--------|-------|
| Blocks analyzed | 1,331 |
| Blocks solvable (all rows &le; 2^25) | **0** |
| Density range | 14.6% &ndash; 51.4% |
| Determined range | 0.0% &ndash; 55.7% |
| Free cells range | 7,145 &ndash; 16,125 |
| Per-row max log2 search | 42 &ndash; 95 |
| Best block | Block 2: 55.7% det (density 14.6%, 14 fully-det rows) |

Even the best block has per-row search spaces of 2^77 after CRC-32 reduction. The 32 CRC-32 equations per row reduce the GF(2) dimension but leave 42&ndash;95 free variables per row &mdash; far beyond tractable enumeration.

**B.58 conclusion.** The combinator-based algebraic approach does not solve CRSCE at S=127. The fundamental issue is information-theoretic: 5,078 GF(2) equations cannot determine 16,129 binary variables regardless of how they are exploited (Gaussian elimination, integer propagation, or cross-deduction). The CRC-32 lateral hashes provide genuine algebraic information (+4,032 independent GF(2) equations), but the information density (0.31 equations per variable) is far below the 1.0 threshold needed for unique determination.

**Status: COMPLETE. H4 &mdash; combinator approach is not viable at S=127.**

### B.58.12 Performance Analysis

**Decompression time (per block).**

| Phase | Operation | Cost estimate |
|-------|-----------|--------------|
| BuildSystem | Construct GF(2) matrix + integer structures | ~10 ms |
| GaussElim | GF(2) RREF on $5{,}078 \times 16{,}129$ | 1&ndash;3 seconds |
| IntBound (per fixpoint iteration) | Scan 1,014 lines + 16,129 cells | ~0.2 ms |
| CrossDeduce (per fixpoint iteration) | Scan $|F| \times 15$ intersections | ~0.5 ms |
| Fixpoint (total, ~10 iterations) | GaussElim re-run dominates | 5&ndash;15 seconds |
| EnumerateFree ($|F| \leq 25$) | $2^{25} \times$ pivot computation | 10&ndash;100 seconds |
| Row-grouped search ($|F| \leq 1{,}000$) | Per-row CRC + arc consistency | 1&ndash;60 seconds |
| VerifyBH | SHA-256 on 2,032 bytes | ~1 &mu;s |

**Total decompression time estimate:**

- $|F| = 0$ (H1): ~5&ndash;15 seconds per block (GaussElim-dominated)
- $|F| \leq 25$ (H2): ~15&ndash;120 seconds per block
- $25 < |F| \leq 1{,}000$ (H3): ~10&ndash;75 seconds per block (row-grouped search)

**Memory.** ~12 MB per block (dominated by the $5{,}078 \times 253$-word GF(2) matrix).

**Compression time (per block).** Same as decompression plus DI discovery overhead. If $|F| = 0$, DI = 0 for all blocks and compression time equals decompression time. If $|F| > 0$, the compressor must verify that the original CSM matches a specific enumerated solution; this adds one SHA-256 comparison per candidate.

**Comparison with DFS at S=511.** The current DFS solver runs for 1,800 seconds per block (max compression time) and reaches ~37% depth. At S=127 with the combinator approach, even the H3 outcome (~75 seconds per block) would be 24$\times$ faster per block. However, S=127 blocks are 16.2$\times$ smaller, so the per-byte cost is comparable. The critical advantage is that the combinator approach is expected to achieve *full reconstruction* (100% depth), not 37%.

### B.58.13 Implementation

**Language:** Python 3.10+ with NumPy for GF(2) matrix operations. The S=127 system is small enough ($5{,}078 \times 16{,}129$ binary matrix $\approx$ 10 MB) for in-memory computation. No C++ dependency.

**Tool:** `tools/b58a_gf2_rank.py`

Implements:

- `build_crc32_generator_matrix(n_data_bits=127)`: returns $G_{\text{CRC}}$ and $\mathbf{c}$
- `build_cross_sum_parity_matrix(csm, yltp_tables)`: returns the 1,014-row GF(2) matrix
- `build_full_gf2_system(csm, yltp_tables)`: assembles the $5{,}078 \times 16{,}129$ system
- `gf2_gauss_elim(G, b)`: GF(2) Gaussian elimination with leftmost-column-first pivoting
- `stratified_rank_analysis(csm, yltp_tables)`: ranks for each family configuration
- `null_space_weight_analysis(rref, pivot_cols, free_cols)`: basis weight statistics
- Main: runs 10 yLTP seed pairs, reports rank statistics

**Tool:** `tools/b58b_symbolic_solve.py`

Implements:

- `class ConstraintSystem`: holds $\mathcal{S}$, all data structures from &sect;B.58.4.1
- `gauss_elim(S)`: Combinator 1
- `int_bound(S)`: Combinator 2
- `propagate(S, newly_determined)`: Combinator 3
- `cross_deduce(S)`: Combinator 4
- `fixpoint(S)`: Combinator 5
- `enumerate_free(S, max_solutions=256)`: Combinator 6
- `verify_bh(csm, expected_bh)`: Combinator 7
- `solve(payload)`: full pipeline
- Main: runs on synthetic 50% block + MP4 block 0 + multiple densities; reports telemetry

**Tool:** `tools/b58c_row_grouped.py`

Implements:

- `generate_row_candidates(r, ...)`: per-row candidate generation (&sect;B.58.10.1)
- `arc_consistency(row_candidates, constraints)`: AC-3 on cross-row CSP (&sect;B.58.10.2)
- `dfs_row_csp(row_candidates, constraints)`: fallback DFS (&sect;B.58.10.3)
- Main: runs on blocks where B.58b reports H3

**Test plan.**

| Test | Verification |
|------|-------------|
| CRC-32 generator matrix | Verify $G_{\text{CRC}} \cdot \mathbf{x}_r \oplus \mathbf{c} = \text{CRC32}(\text{pack}(\mathbf{x}_r, 0))$ for 1,000 random rows |
| GF(2) Gaussian elimination | Verify RREF satisfies $G \cdot \mathbf{x} = b_G$ for all determined cells |
| IntBound correctness | Verify all forced cells satisfy integer constraints on all 6 lines |
| Fixpoint monotonicity | Verify $|D|$ is non-decreasing across iterations |
| Reconstruction correctness | Compare reconstructed CSM to original (compressor has access to both) |
| DI determinism | Run pipeline twice on same input; verify identical DI |
| SHA-256 verification | Verify reconstructed CSM matches stored BH |
| Inconsistency detection | Corrupt one cross-sum value; verify the pipeline reports infeasibility |
| Small-scale exhaustive | At S=7: verify the pipeline produces the unique correct CSM for 100 random matrices |

### B.58.14 Relationship to Prior Work

**B.54 (SMT Hybrid Solver).** B.54 used CRC-32 as a **verification oracle**: the solver called CRC-32 only when a row was fully assigned ($u = 0$), obtaining a binary pass/fail signal. B.58 uses CRC-32 as an **algebraic constraint**: the 32 linear equations per row are incorporated into the GF(2) system at initialization, participating in Gaussian elimination alongside cross-sum parity. This is the fundamental architectural distinction. B.54's failure was due to the row-completion barrier; B.58 eliminates that barrier entirely.

**B.55 (LP Relaxation).** B.55 solved the cross-sum system as a continuous LP and found that the LP values were uninformative (49.9% accuracy). B.58's GF(2) approach is fundamentally different: it operates over a finite field, not the reals, and CRC-32 adds ~4,064 *new* constraints that the LP did not have. The LP failure was caused by an underdetermined system; B.58 attacks the underdetermination directly by adding algebraically exploitable constraints.

**B.56 (Hierarchical Multi-Resolution).** B.56 proposed integer Gaussian elimination on the constraint matrix. B.58 subsumes B.56's algebraic decomposition variant but operates primarily over GF(2) (where CRC-32 is natively representable) rather than the integers. Integer reasoning is retained via the IntBound combinator for cardinality constraints that carry information beyond parity.

**B.39 (N-Dimensional Constraint Geometry).** B.39 established the GF(2) null-space dimension at S=511 as 256,024. B.58a will compute the corresponding quantity at S=127 *with CRC-32 included*. The CRC-32 contribution (~4,064 new GF(2) equations) was not considered in B.39 because SHA-1 lateral hashes have no useful GF(2) structure.

**B.44 (Constraint Density Breakthrough).** B.44 concluded that no viable path exists to increase constraint density at S=511 without counterproductive side effects (deep exploration paradox). B.58 operates in a fundamentally different regime: the "constraint density" is not the DFS propagation density (singleton forcing) but the GF(2) rank as a fraction of cell count. CRC-32 increases GF(2) constraint density from ~6.2% (cross-sums only) to ~31.4% (cross-sums + CRC-32) at S=127. The deep exploration paradox does not apply because there is no DFS.

**B.57 (S=127 Format).** B.58 adopts B.57's format specification but proposes a completely different solver. B.57 assumed the existing DFS solver would be tested at S=127 to measure depth improvement from higher constraint density. B.58 abandons DFS altogether. If B.58 succeeds, B.57's DFS depth measurement (B.57a) becomes moot.

**B.42 (Pre-Branch Pruning Spectrum).** B.42c's both-value probing is the DFS equivalent of B.58's IntBound cell-line interaction check: testing both values of a cell before committing. B.42c achieved H2 (neutral: no depth improvement despite eliminating all wasted branches). B.58 generalizes this idea from single-cell probing to full algebraic reasoning across the entire constraint system simultaneously.

**Status: PROPOSED.**

---

