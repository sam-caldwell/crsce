#!/usr/bin/env python3
"""
B.39a: N-Dimensional Constraint Geometry — Minimum Swap Characterization.

Constructs the full 5108 x 261121 binary constraint matrix A over GF(2) for the
CRSCE 511x511 CSM and performs null-space analysis to determine the exact dimension
of the constraint-preserving swap space.

Constraint families (8 total, 5108 lines):
  - 511 row constraints
  - 511 column constraints
  - 1021 diagonal constraints (d = c - r + 510)
  - 1021 anti-diagonal constraints (x = r + c)
  - 4 x 511 LTP sub-table constraints (Fisher-Yates partition)

Phase 1: Stratified GF(2) rank analysis (null-space dimension).
Phase 2: Minimum-weight null-space vector search via basis extraction + sampling.
Phase 3: Swap structure analysis (row span, column span, cells per row).

Key outputs:
  - Rank of A and null-space dimension per family configuration
  - Minimum-weight null-space vector (upper bound on minimum swap size)
  - Swap structure of the lightest vectors found

Usage:
    python3 tools/b39a_ndim_swap.py                              # use default FY seeds
    python3 tools/b39a_ndim_swap.py --ltp-file tools/b38e_t31000000_best_s137.bin
    python3 tools/b39a_ndim_swap.py --skip-rank   # skip rank (reuse prior), do min-weight only
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import argparse
import json
import pathlib
import struct
import sys
import time

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required. Install with: pip install numpy")

try:
    from scipy.sparse import csr_matrix, vstack
    from scipy.spatial.distance import pdist
except ImportError:
    sys.exit("ERROR: scipy required. Install with: pip install scipy")


# ── Constants ────────────────────────────────────────────────────────────────

S = 511
N = S * S  # 261,121 cells

# LCG parameters (must match C++ LtpTable.cpp)
LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

# Default LTP seeds (B.26c winners + B.27 extensions)
SEEDS = [
    0x435253434C545056,  # kSeed1 = CRSCLTPV
    0x435253434C545050,  # kSeed2 = CRSCLTPP
    0x435253434C545033,  # kSeed3 = CRSCLTP3
    0x435253434C545034,  # kSeed4 = CRSCLTP4
    0x435253434C545035,  # kSeed5 = CRSCLTP5
    0x435253434C545036,  # kSeed6 = CRSCLTP6
]

# Number of constraint lines per family
N_ROWS = S           # 511
N_COLS = S           # 511
N_DIAGS = 2 * S - 1  # 1021
N_ANTIS = 2 * S - 1  # 1021
N_LTP_PER_SUB = S    # 511
NUM_LTP_SUBS = 4     # production uses 4

# LTPB file constants
LTPB_MAGIC = b"LTPB"
LTPB_HEADER_SIZE = 16  # 4 magic + 4 version + 4 S + 4 num_subtables


# ── LTP Construction ────────────────────────────────────────────────────────

def lcg_next(state: int) -> int:
    """Advance one step of the Knuth LCG (matches C++ kLcgA/kLcgC)."""
    return (state * LCG_A + LCG_C) & (LCG_MOD - 1)


def build_fy_tables(num_sub: int = NUM_LTP_SUBS) -> list[np.ndarray]:
    """
    Build LTP sub-table assignments via pool-chained Fisher-Yates shuffle.

    The pool is initialized once and NOT reset between sub-tables (pool-chained),
    exactly matching the C++ buildAllPartitions() implementation.

    Returns a list of num_sub arrays, each of shape (N,) with dtype uint16,
    where assignments[sub][flat] = line index (0..510) for cell `flat`.
    """
    pool = list(range(N))
    assignments = []

    for seed_idx in range(num_sub):
        state = SEEDS[seed_idx]

        # Fisher-Yates shuffle (descending, matching C++ loop)
        for i in range(N - 1, 0, -1):
            state = lcg_next(state)
            j = int(state % (i + 1))
            pool[i], pool[j] = pool[j], pool[i]

        # Assign consecutive S-cell chunks to lines 0..510
        a = np.empty(N, dtype=np.uint16)
        for line in range(S):
            base = line * S
            for pos in range(S):
                a[pool[base + pos]] = line
        assignments.append(a)

    return assignments


def load_ltpb(path: pathlib.Path, num_sub: int = NUM_LTP_SUBS) -> list[np.ndarray]:
    """
    Load LTP sub-table assignments from an LTPB binary file.

    LTPB format (all integers little-endian):
      Offset  Size       Field
      0       4          magic = "LTPB"
      4       4 uint32   version = 1
      8       4 uint32   S (must equal 511)
      12      4 uint32   num_subtables (1..6)
      16      num_subtables * N * 2  uint16[] assignment arrays, sub-major

    If the file has fewer sub-tables than num_sub, the remaining are built
    from Fisher-Yates seeds.

    Returns a list of num_sub arrays, each of shape (N,) with dtype uint16.
    """
    data = path.read_bytes()
    if len(data) < LTPB_HEADER_SIZE:
        sys.exit(f"ERROR: {path} too small ({len(data)} bytes)")

    magic, version, s_field, file_num_sub = struct.unpack_from("<4sIII", data, 0)
    if magic != LTPB_MAGIC:
        sys.exit(f"ERROR: bad magic in {path}: {magic!r} (expected {LTPB_MAGIC!r})")
    if version != 1:
        sys.exit(f"ERROR: unsupported LTPB version {version} in {path}")
    if s_field != S:
        sys.exit(f"ERROR: S={s_field} in {path}, expected {S}")
    if file_num_sub < 1 or file_num_sub > 6:
        sys.exit(f"ERROR: num_subtables={file_num_sub} in {path}, expected 1..6")

    expected_size = LTPB_HEADER_SIZE + file_num_sub * N * 2
    if len(data) != expected_size:
        sys.exit(f"ERROR: {path} size mismatch: {len(data)} vs expected {expected_size}")

    assignments = []
    offset = LTPB_HEADER_SIZE
    subs_to_read = min(file_num_sub, num_sub)
    for _ in range(subs_to_read):
        a = np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy()
        assignments.append(a)
        offset += N * 2

    # If file had fewer sub-tables than requested, fill from FY seeds
    if len(assignments) < num_sub:
        fy = build_fy_tables(num_sub)
        for i in range(len(assignments), num_sub):
            assignments.append(fy[i])

    return assignments[:num_sub]


# ── Constraint Matrix Construction ──────────────────────────────────────────

def build_constraint_matrix(
    ltp_assignments: list[np.ndarray],
    families: str = "all",
) -> csr_matrix:
    """
    Build the sparse binary constraint matrix A (M x N) where:
      - Each row corresponds to one constraint line
      - Each column corresponds to one cell (flat index r*S+c)
      - A[i, flat] = 1 iff cell `flat` participates in constraint line i

    The `families` parameter controls which constraint families to include:
      "geo"        = rows + cols + diags + anti-diags (4 geometric)
      "geo+ltp1"   = geometric + LTP sub-table 0
      "geo+ltp12"  = geometric + LTP sub-tables 0,1
      "all"        = geometric + all 4 LTP sub-tables

    Returns a scipy csr_matrix of shape (M, N) with dtype uint8.
    """
    rows_list = []  # row indices into A
    cols_list = []  # column indices into A (flat cell index)
    row_offset = 0

    # ── Row constraints (511 lines) ──
    for r in range(S):
        for c in range(S):
            rows_list.append(row_offset + r)
            cols_list.append(r * S + c)
    row_offset += N_ROWS

    # ── Column constraints (511 lines) ──
    for c in range(S):
        for r in range(S):
            rows_list.append(row_offset + c)
            cols_list.append(r * S + c)
    row_offset += N_COLS

    # ── Diagonal constraints (1021 lines, d = c - r + S - 1) ──
    for r in range(S):
        for c in range(S):
            d = c - r + S - 1
            rows_list.append(row_offset + d)
            cols_list.append(r * S + c)
    row_offset += N_DIAGS

    # ── Anti-diagonal constraints (1021 lines, x = r + c) ──
    for r in range(S):
        for c in range(S):
            x = r + c
            rows_list.append(row_offset + x)
            cols_list.append(r * S + c)
    row_offset += N_ANTIS

    # ── LTP constraints ──
    if families == "geo":
        num_ltp = 0
    elif families == "geo+ltp1":
        num_ltp = 1
    elif families == "geo+ltp12":
        num_ltp = 2
    elif families == "all":
        num_ltp = len(ltp_assignments)
    else:
        sys.exit(f"ERROR: unknown families={families!r}")

    for sub in range(num_ltp):
        asn = ltp_assignments[sub]
        for flat in range(N):
            line = int(asn[flat])
            rows_list.append(row_offset + line)
            cols_list.append(flat)
        row_offset += N_LTP_PER_SUB

    total_rows = row_offset
    data = np.ones(len(rows_list), dtype=np.uint8)
    rows_arr = np.array(rows_list, dtype=np.int32)
    cols_arr = np.array(cols_list, dtype=np.int32)

    A = csr_matrix((data, (rows_arr, cols_arr)), shape=(total_rows, N), dtype=np.uint8)
    return A


# ── GF(2) Rank Computation + RREF Extraction ────────────────────────────────

def compute_rank_gf2(A: csr_matrix) -> tuple:
    """
    Compute the GF(2) rank of a binary matrix using Gaussian elimination.

    Returns (rank, packed_rref, pivot_cols, words_per_row) where:
      - rank: the GF(2) rank
      - packed_rref: numpy array (rank x words_per_row) of uint64, the RREF rows
      - pivot_cols: list of pivot column indices (length = rank)
      - words_per_row: number of uint64 words per packed row
    """
    M, Ncols = A.shape
    print(f"    Computing GF(2) rank of {M} x {Ncols} matrix ...", flush=True)

    words_per_row = (Ncols + 63) // 64

    # Pack rows into uint64 bit arrays
    print(f"    Packing {M} rows into {words_per_row}-word bit vectors ...", flush=True)
    t0 = time.time()
    packed = np.zeros((M, words_per_row), dtype=np.uint64)

    A_csr = A.tocsr()
    for i in range(M):
        row_start = A_csr.indptr[i]
        row_end = A_csr.indptr[i + 1]
        col_indices = A_csr.indices[row_start:row_end]
        for c in col_indices:
            word = c // 64
            bit = c % 64
            packed[i, word] ^= np.uint64(1) << np.uint64(bit)

    t1 = time.time()
    print(f"    Packed in {t1 - t0:.1f}s", flush=True)

    # GF(2) Gaussian elimination (full RREF — eliminates above and below pivot)
    print(f"    Running GF(2) Gaussian elimination ...", flush=True)
    rank = 0
    pivot_cols = []

    for pivot_col in range(Ncols):
        if rank >= M:
            break

        word = pivot_col // 64
        bit = np.uint64(pivot_col % 64)
        mask = np.uint64(1) << bit

        # Find a row with a 1 in this column (starting from current rank)
        found = -1
        for i in range(rank, M):
            if packed[i, word] & mask:
                found = i
                break

        if found == -1:
            continue  # Column is all-zero below pivot; skip

        # Swap found row with current rank row
        if found != rank:
            packed[[rank, found]] = packed[[found, rank]]

        # Eliminate all other rows with a 1 in this column
        pivot_row = packed[rank]
        for i in range(M):
            if i != rank and (packed[i, word] & mask):
                packed[i] ^= pivot_row

        pivot_cols.append(pivot_col)
        rank += 1

        # Progress reporting (every 1000 pivots)
        if rank % 1000 == 0:
            elapsed = time.time() - t1
            print(f"      rank so far: {rank} / {M}  (elapsed {elapsed:.1f}s)", flush=True)

    elapsed = time.time() - t1
    print(f"    GF(2) elimination done in {elapsed:.1f}s, rank = {rank}", flush=True)

    return rank, packed[:rank].copy(), pivot_cols, words_per_row


# ── Null-Space Basis Weight Analysis ─────────────────────────────────────────

def unpack_packed_row(packed_row: np.ndarray, n_cols: int) -> np.ndarray:
    """
    Unpack a single packed uint64 row into a dense binary array.

    Converts the uint64 word array to uint8 bytes, then uses np.unpackbits
    with little-endian bit order to reconstruct individual bits.
    """
    row_bytes = packed_row.view(np.uint8)
    bits = np.unpackbits(row_bytes, bitorder='little')
    return bits[:n_cols].astype(np.uint32)


def compute_column_counts(packed_rref: np.ndarray, rank: int, n_cols: int) -> np.ndarray:
    """
    For each column j, count how many pivot rows have a 1 in column j.

    This is the key statistic for basis vector weights: the weight of the
    null-space basis vector for free column j is 1 + col_counts[j].
    """
    print(f"    Computing column popcount across {rank} pivot rows ...", flush=True)
    t0 = time.time()

    col_counts = np.zeros(n_cols, dtype=np.uint32)

    for i in range(rank):
        row_bits = unpack_packed_row(packed_rref[i], n_cols)
        col_counts += row_bits

        if (i + 1) % 1000 == 0:
            elapsed = time.time() - t0
            print(f"      processed {i + 1} / {rank} rows ({elapsed:.1f}s)", flush=True)

    elapsed = time.time() - t0
    print(f"    Column popcount done in {elapsed:.1f}s", flush=True)
    return col_counts


def find_minimum_weight_null_vectors(
    packed_rref: np.ndarray,
    rank: int,
    pivot_cols: list[int],
    n_cols: int,
    top_k: int = 2000,
    n_random_samples: int = 200_000,
) -> dict:
    """
    Find minimum-weight null-space vectors using basis analysis + sampling.

    Phase 1: Compute individual basis vector weights (exact, fast).
    Phase 2: Pairwise XOR of lightest K basis vectors.
    Phase 3: Random k-way XOR combinations (k=2..6) for 100K+ samples.

    Returns a dict with minimum weights found, distribution statistics,
    and structure analysis of the lightest vector.
    """
    # ── Step 1: Identify free columns ────────────────────────────────────────
    pivot_set = set(pivot_cols)
    free_cols = np.array([j for j in range(n_cols) if j not in pivot_set], dtype=np.int32)
    n_free = len(free_cols)
    print(f"\n    Free columns: {n_free} (null-space dimension)", flush=True)

    # ── Step 2: Column popcount → individual basis vector weights ────────────
    col_counts = compute_column_counts(packed_rref, rank, n_cols)

    # Weight of basis vector for free column j = 1 (the free col itself) + col_counts[j]
    # col_counts[j] = number of pivot rows that have a 1 in column j
    # (which means the basis vector has a 1 in those pivot columns)
    basis_weights = col_counts[free_cols] + 1

    min_individual_weight = int(np.min(basis_weights))
    min_individual_idx = int(np.argmin(basis_weights))
    min_individual_free_col = int(free_cols[min_individual_idx])

    print(f"    Individual basis vector weights:", flush=True)
    print(f"      min = {min_individual_weight}", flush=True)
    print(f"      max = {int(np.max(basis_weights))}", flush=True)
    print(f"      mean = {np.mean(basis_weights):.1f}", flush=True)
    print(f"      median = {np.median(basis_weights):.1f}", flush=True)
    print(f"      p5 = {np.percentile(basis_weights, 5):.0f}", flush=True)
    print(f"      p95 = {np.percentile(basis_weights, 95):.0f}", flush=True)

    # ── Step 3: Extract lightest K basis vectors for pairwise analysis ───────
    K = min(top_k, n_free)
    lightest_order = np.argsort(basis_weights)[:K]
    lightest_free_cols = free_cols[lightest_order]
    lightest_weights = basis_weights[lightest_order]

    print(f"\n    Lightest {min(10, K)} individual basis vectors:", flush=True)
    for i in range(min(10, K)):
        fc = int(lightest_free_cols[i])
        w = int(lightest_weights[i])
        r, c = fc // S, fc % S
        print(f"      free_col={fc} (row={r}, col={c}), weight={w}", flush=True)

    # Extract the pivot-column part of each lightest basis vector
    # cols[k, i] = bit of free column lightest_free_cols[k] in pivot row i
    print(f"\n    Extracting {K} lightest basis vectors' pivot-column parts ...", flush=True)
    t0 = time.time()
    cols = np.zeros((K, rank), dtype=np.uint8)
    for k in range(K):
        j = int(lightest_free_cols[k])
        word_idx = j // 64
        bit_pos = np.uint64(j % 64)
        mask = np.uint64(1) << bit_pos
        cols[k] = ((packed_rref[:, word_idx] & mask) != 0).astype(np.uint8)

    elapsed = time.time() - t0
    print(f"    Extraction done in {elapsed:.1f}s", flush=True)

    # ── Step 4: Pairwise XOR weights ────────────────────────────────────────
    print(f"    Computing pairwise Hamming distances for {K} vectors ...", flush=True)
    t0 = time.time()

    # pdist returns condensed distance matrix; hamming metric gives fraction
    dists_frac = pdist(cols, metric='hamming')
    # Convert fraction to integer count of differing positions
    dists_int = np.round(dists_frac * rank).astype(np.int32)
    # XOR of two basis vectors: 2 free-column 1s + Hamming dist of pivot parts
    pairwise_weights = dists_int + 2

    min_pairwise_weight = int(np.min(pairwise_weights))

    # Find the indices of the minimum pair
    min_pair_idx = int(np.argmin(pairwise_weights))
    # Convert condensed index to (i, j) pair
    # For condensed distance matrix from pdist, index k corresponds to pair (i, j)
    # where i < j, using the formula: k = K*i - i*(i+1)//2 + j - i - 1
    i_min = 0
    remaining = min_pair_idx
    for i_min in range(K):
        row_len = K - i_min - 1
        if remaining < row_len:
            break
        remaining -= row_len
    j_min = i_min + remaining + 1

    elapsed = time.time() - t0
    print(f"    Pairwise search done in {elapsed:.1f}s", flush=True)
    print(f"    Min pairwise weight: {min_pairwise_weight} "
          f"(vectors {i_min}, {j_min})", flush=True)
    print(f"    Min pair free cols: {int(lightest_free_cols[i_min])}, "
          f"{int(lightest_free_cols[j_min])}", flush=True)

    # ── Step 5: Random k-way XOR sampling ────────────────────────────────────
    print(f"\n    Random k-way XOR sampling ({n_random_samples:,} samples) ...", flush=True)
    t0 = time.time()

    rng = np.random.default_rng(42)
    min_random_weight = min_individual_weight
    min_random_k = 1
    min_random_indices = [min_individual_idx]

    sample_counts = {2: 0, 3: 0, 4: 0, 5: 0, 6: 0}
    sample_mins = {2: 999999, 3: 999999, 4: 999999, 5: 999999, 6: 999999}

    for sample_i in range(n_random_samples):
        k = int(rng.integers(2, 7))  # 2 to 6 vectors
        indices = rng.choice(K, size=k, replace=False)

        # XOR the pivot parts
        xor_vec = cols[indices[0]].copy()
        for idx in indices[1:]:
            xor_vec ^= cols[idx]

        # Weight = k (free-column 1s) + popcount of XOR'd pivot part
        weight = k + int(np.sum(xor_vec))

        sample_counts[k] += 1
        if weight < sample_mins[k]:
            sample_mins[k] = weight

        if weight < min_random_weight:
            min_random_weight = weight
            min_random_k = k
            min_random_indices = [int(idx) for idx in indices]

        if (sample_i + 1) % 50000 == 0:
            elapsed = time.time() - t0
            print(f"      {sample_i + 1:,} samples, "
                  f"best so far: {min_random_weight} (k={min_random_k}), "
                  f"elapsed {elapsed:.1f}s", flush=True)

    elapsed = time.time() - t0
    print(f"    Random sampling done in {elapsed:.1f}s", flush=True)
    print(f"    Best random: weight={min_random_weight}, k={min_random_k}", flush=True)
    for k in sorted(sample_mins.keys()):
        print(f"      k={k}: {sample_counts[k]:,} samples, "
              f"min weight={sample_mins[k]}", flush=True)

    # ── Step 6: Overall minimum ──────────────────────────────────────────────
    overall_min = min(min_individual_weight, min_pairwise_weight, min_random_weight)
    print(f"\n    *** OVERALL MINIMUM WEIGHT (upper bound on min swap): {overall_min} ***",
          flush=True)

    # ── Step 7: Construct and analyze the lightest vector ────────────────────
    swap_analysis = None
    if overall_min == min_individual_weight:
        # Reconstruct the lightest individual basis vector
        swap_analysis = analyze_basis_vector(
            packed_rref, rank, pivot_cols,
            int(lightest_free_cols[min_individual_idx]),
            n_cols, "individual"
        )
    elif overall_min == min_pairwise_weight:
        # Reconstruct the lightest pairwise XOR
        swap_analysis = analyze_pairwise_xor(
            packed_rref, rank, pivot_cols,
            int(lightest_free_cols[i_min]),
            int(lightest_free_cols[j_min]),
            n_cols, "pairwise"
        )
    else:
        # Reconstruct the lightest random combination
        free_col_list = [int(lightest_free_cols[idx]) for idx in min_random_indices]
        swap_analysis = analyze_kway_xor(
            packed_rref, rank, pivot_cols,
            free_col_list, n_cols, f"{min_random_k}-way"
        )

    result = {
        "overall_minimum_weight": overall_min,
        "min_individual": {
            "weight": min_individual_weight,
            "free_col": min_individual_free_col,
            "row": min_individual_free_col // S,
            "col": min_individual_free_col % S,
        },
        "min_pairwise": {
            "weight": min_pairwise_weight,
            "free_cols": [int(lightest_free_cols[i_min]),
                          int(lightest_free_cols[j_min])],
        },
        "min_random": {
            "weight": min_random_weight,
            "k": min_random_k,
            "n_samples": n_random_samples,
        },
        "basis_weight_distribution": {
            "min": int(np.min(basis_weights)),
            "max": int(np.max(basis_weights)),
            "mean": float(np.mean(basis_weights)),
            "median": float(np.median(basis_weights)),
            "p5": float(np.percentile(basis_weights, 5)),
            "p10": float(np.percentile(basis_weights, 10)),
            "p25": float(np.percentile(basis_weights, 25)),
            "p75": float(np.percentile(basis_weights, 75)),
            "p90": float(np.percentile(basis_weights, 90)),
            "p95": float(np.percentile(basis_weights, 95)),
        },
        "lightest_10_weights": [int(w) for w in lightest_weights[:10]],
        "per_k_minimums": {str(k): v for k, v in sample_mins.items()},
        "swap_analysis": swap_analysis,
    }

    return result


# ── Swap Structure Analysis ──────────────────────────────────────────────────

def construct_null_vector(
    packed_rref: np.ndarray,
    rank: int,
    pivot_cols: list[int],
    free_col_indices: list[int],
    n_cols: int,
) -> np.ndarray:
    """
    Construct a null-space vector as the XOR of basis vectors for the given free columns.

    Returns a dense binary array of shape (n_cols,).
    """
    vec = np.zeros(n_cols, dtype=np.uint8)

    # Set the free columns to 1
    for fc in free_col_indices:
        vec[fc] = 1

    # For each free column, XOR in its pivot-column contributions
    for fc in free_col_indices:
        word_idx = fc // 64
        bit_pos = np.uint64(fc % 64)
        mask = np.uint64(1) << bit_pos
        for i in range(rank):
            if packed_rref[i, word_idx] & mask:
                pc = pivot_cols[i]
                vec[pc] ^= 1

    return vec


def analyze_vector_structure(vec: np.ndarray, n_cols: int, label: str) -> dict:
    """
    Analyze the structure of a null-space vector in CSM coordinates.
    """
    weight = int(np.sum(vec))
    nonzero_flat = np.where(vec != 0)[0]

    rows_touched = set()
    cols_touched = set()
    cells_per_row = {}

    for flat in nonzero_flat:
        r = int(flat) // S
        c = int(flat) % S
        rows_touched.add(r)
        cols_touched.add(c)
        cells_per_row[r] = cells_per_row.get(r, 0) + 1

    row_span = max(rows_touched) - min(rows_touched) + 1 if rows_touched else 0
    col_span = max(cols_touched) - min(cols_touched) + 1 if cols_touched else 0

    cells_per_row_vals = list(cells_per_row.values())

    result = {
        "label": label,
        "weight": weight,
        "rows_touched": len(rows_touched),
        "cols_touched": len(cols_touched),
        "row_span": row_span,
        "col_span": col_span,
        "min_row": min(rows_touched) if rows_touched else None,
        "max_row": max(rows_touched) if rows_touched else None,
        "cells_per_row_min": min(cells_per_row_vals) if cells_per_row_vals else 0,
        "cells_per_row_max": max(cells_per_row_vals) if cells_per_row_vals else 0,
        "cells_per_row_mean": (
            sum(cells_per_row_vals) / len(cells_per_row_vals)
            if cells_per_row_vals else 0
        ),
    }

    print(f"\n    Swap structure ({label}):", flush=True)
    print(f"      Weight (swap size): {weight}", flush=True)
    print(f"      Rows touched: {len(rows_touched)} (span {row_span})", flush=True)
    print(f"      Cols touched: {len(cols_touched)} (span {col_span})", flush=True)
    print(f"      Row range: [{result['min_row']}, {result['max_row']}]", flush=True)
    print(f"      Cells/row: min={result['cells_per_row_min']}, "
          f"max={result['cells_per_row_max']}, "
          f"mean={result['cells_per_row_mean']:.1f}", flush=True)

    return result


def analyze_basis_vector(
    packed_rref, rank, pivot_cols, free_col, n_cols, label
) -> dict:
    """Construct and analyze a single basis vector."""
    vec = construct_null_vector(packed_rref, rank, pivot_cols, [free_col], n_cols)
    return analyze_vector_structure(vec, n_cols, label)


def analyze_pairwise_xor(
    packed_rref, rank, pivot_cols, fc1, fc2, n_cols, label
) -> dict:
    """Construct and analyze a pairwise XOR of two basis vectors."""
    vec = construct_null_vector(packed_rref, rank, pivot_cols, [fc1, fc2], n_cols)
    return analyze_vector_structure(vec, n_cols, label)


def analyze_kway_xor(
    packed_rref, rank, pivot_cols, free_col_list, n_cols, label
) -> dict:
    """Construct and analyze a k-way XOR of basis vectors."""
    vec = construct_null_vector(packed_rref, rank, pivot_cols, free_col_list, n_cols)
    return analyze_vector_structure(vec, n_cols, label)


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--ltp-file", type=pathlib.Path, default=None,
        help="Path to LTPB binary file (default: use B.26c FY seeds)",
    )
    parser.add_argument(
        "--output", type=pathlib.Path, default=pathlib.Path("tools/b39a_results.json"),
        help="Path to write JSON results (default: tools/b39a_results.json)",
    )
    parser.add_argument(
        "--num-ltp", type=int, default=NUM_LTP_SUBS,
        help=f"Number of LTP sub-tables to use (default: {NUM_LTP_SUBS})",
    )
    parser.add_argument(
        "--top-k", type=int, default=2000,
        help="Number of lightest basis vectors for pairwise search (default: 2000)",
    )
    parser.add_argument(
        "--n-samples", type=int, default=200_000,
        help="Number of random k-way XOR samples (default: 200000)",
    )
    args = parser.parse_args()

    results = {
        "experiment": "B.39a",
        "description": "N-Dimensional Constraint Geometry — Null-Space Analysis "
                       "with Minimum-Weight Vector Search",
        "S": S,
        "N": N,
        "ltp_source": None,
        "num_ltp": args.num_ltp,
        "stratified": {},
        "minimum_weight_search": None,
    }

    # ── Load / build LTP ─────────────────────────────────────────────────────
    if args.ltp_file is not None:
        print(f"Loading LTPB from {args.ltp_file} ...", flush=True)
        ltp = load_ltpb(args.ltp_file, num_sub=args.num_ltp)
        results["ltp_source"] = str(args.ltp_file)
    else:
        print(f"Building FY assignments from B.26c seeds ({args.num_ltp} sub-tables) ...",
              flush=True)
        ltp = build_fy_tables(num_sub=args.num_ltp)
        results["ltp_source"] = "FY seeds (B.26c)"

    # Validate LTP: each sub-table should have exactly S cells per line
    for sub_idx, asn in enumerate(ltp):
        counts = np.bincount(asn, minlength=S)
        if not np.all(counts == S):
            bad_lines = np.where(counts != S)[0]
            print(f"  WARNING: LTP sub-table {sub_idx} has non-uniform lines: "
                  f"{bad_lines[:5]}... (counts: {counts[bad_lines[:5]]})", flush=True)
        else:
            print(f"  LTP sub-table {sub_idx}: validated (511 lines x 511 cells)", flush=True)

    # ── Stratified rank analysis ─────────────────────────────────────────────
    family_configs = [
        ("geo", "Geometric only (rows+cols+diags+anti-diags)"),
    ]
    if args.num_ltp >= 1:
        family_configs.append(("geo+ltp1", "Geometric + LTP1"))
    if args.num_ltp >= 2:
        family_configs.append(("geo+ltp12", "Geometric + LTP1 + LTP2"))
    family_configs.append(("all", f"All 8 families (geo + {args.num_ltp} LTP)"))

    total_constraints = {
        "geo": N_ROWS + N_COLS + N_DIAGS + N_ANTIS,
        "geo+ltp1": N_ROWS + N_COLS + N_DIAGS + N_ANTIS + N_LTP_PER_SUB,
        "geo+ltp12": N_ROWS + N_COLS + N_DIAGS + N_ANTIS + 2 * N_LTP_PER_SUB,
        "all": N_ROWS + N_COLS + N_DIAGS + N_ANTIS + args.num_ltp * N_LTP_PER_SUB,
    }

    print("\n" + "=" * 72, flush=True)
    print("PHASE 1: STRATIFIED RANK ANALYSIS (GF(2))", flush=True)
    print("=" * 72, flush=True)

    # We need the full-system RREF for the minimum-weight search
    full_packed_rref = None
    full_pivot_cols = None
    full_words_per_row = None

    for families, desc in family_configs:
        print(f"\n--- {desc} ---", flush=True)
        n_constraints = total_constraints[families]
        print(f"  Constraint lines: {n_constraints}", flush=True)
        print(f"  Cells (columns):  {N}", flush=True)

        t0 = time.time()
        A = build_constraint_matrix(ltp, families=families)
        t_build = time.time() - t0
        print(f"  Matrix built in {t_build:.1f}s, shape={A.shape}, nnz={A.nnz:,}", flush=True)

        rank, packed_rref, pivot_cols, words_per_row = compute_rank_gf2(A)
        null_dim = N - rank

        print(f"  GF(2) rank:       {rank}", flush=True)
        print(f"  Null-space dim:   {null_dim}", flush=True)
        print(f"  Constraint efficiency: {rank}/{n_constraints} "
              f"({100.0 * rank / n_constraints:.1f}% of lines are independent)", flush=True)

        results["stratified"][families] = {
            "description": desc,
            "constraint_lines": n_constraints,
            "rank": rank,
            "null_dim": null_dim,
            "independent_pct": round(100.0 * rank / n_constraints, 2),
        }

        # Keep the full-system RREF for minimum-weight search
        if families == "all":
            full_packed_rref = packed_rref
            full_pivot_cols = pivot_cols
            full_words_per_row = words_per_row

    # ── Summary ──────────────────────────────────────────────────────────────
    print("\n" + "=" * 72, flush=True)
    print("RANK SUMMARY", flush=True)
    print("=" * 72, flush=True)

    for families, desc in family_configs:
        r = results["stratified"][families]
        print(f"  {desc}:", flush=True)
        print(f"    rank={r['rank']}, null_dim={r['null_dim']}, "
              f"independent={r['independent_pct']}%", flush=True)

    full = results["stratified"]["all"]
    print(f"\n  Full system: {full['null_dim']} degrees of freedom for "
          f"constraint-preserving swaps.", flush=True)

    if full["null_dim"] == 0:
        print("  The CSM is UNIQUELY determined by the 8-family constraint system.", flush=True)
        print("  No constraint-preserving swaps exist (DI always 0).", flush=True)
    else:
        print(f"  The constraint system has a {full['null_dim']}-dimensional null space.", flush=True)
        print(f"  SHA-1 lateral hashes are needed to disambiguate.", flush=True)

    # ── Phase 2: Minimum-weight null-space vector search ─────────────────────
    if full["null_dim"] > 0 and full_packed_rref is not None:
        print("\n" + "=" * 72, flush=True)
        print("PHASE 2: MINIMUM-WEIGHT NULL-SPACE VECTOR SEARCH", flush=True)
        print("=" * 72, flush=True)

        min_weight_results = find_minimum_weight_null_vectors(
            full_packed_rref,
            full["rank"],
            full_pivot_cols,
            N,
            top_k=args.top_k,
            n_random_samples=args.n_samples,
        )
        results["minimum_weight_search"] = min_weight_results

        # ── Interpretation ───────────────────────────────────────────────────
        overall_min = min_weight_results["overall_minimum_weight"]

        print("\n" + "=" * 72, flush=True)
        print("INTERPRETATION", flush=True)
        print("=" * 72, flush=True)

        if overall_min <= 50:
            print(f"  OPTIMISTIC: Minimum swap <= {overall_min} cells.", flush=True)
            print(f"  B.33 Phase 3 may be viable. Proceed to B.39b.", flush=True)
        elif overall_min <= 200:
            print(f"  MODERATE: Minimum swap <= {overall_min} cells.", flush=True)
            print(f"  B.39b marginally tractable. Strategy C (compound) may help.", flush=True)
        else:
            print(f"  PESSIMISTIC: Minimum swap >= {overall_min} cells.", flush=True)
            print(f"  B.33 Phase 3 infeasible. Cascade model confirmed.", flush=True)

        swap = min_weight_results.get("swap_analysis", {})
        if swap:
            rows_touched = swap.get("rows_touched", 0)
            print(f"  Lightest vector touches {rows_touched} rows.", flush=True)
            if rows_touched <= 20:
                print(f"  Row span manageable for B.39b Phase 3.", flush=True)
            else:
                print(f"  Row span too large for row-targeted Phase 3.", flush=True)

    # ── Write results ────────────────────────────────────────────────────────
    print(f"\nWriting results to {args.output} ...", flush=True)
    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)
    print("Done.", flush=True)


if __name__ == "__main__":
    main()
