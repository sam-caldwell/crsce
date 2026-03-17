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

Key outputs:
  - Rank of A
  - Null-space dimension (= 261121 - rank = degrees of freedom for constraint-preserving swaps)
  - Stratified rank contribution of each constraint family
  - Minimum L1-norm null-space vector (minimum swap size)

Usage:
    python3 tools/b39a_ndim_swap.py                              # use default FY seeds
    python3 tools/b39a_ndim_swap.py --ltp-file tools/b38e_t31000000_best_s137.bin  # use LTPB file
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
    from scipy.sparse.linalg import svds
    from scipy.optimize import linprog
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


# ── Rank Computation ────────────────────────────────────────────────────────

def compute_rank_gf2(A: csr_matrix) -> int:
    """
    Compute the GF(2) rank of a binary matrix using Gaussian elimination.

    Since we are working over GF(2) (not the reals), we cannot use SVD or
    standard scipy rank routines. Instead we perform row-echelon reduction
    mod 2 on a dense copy.

    For a 5108 x 261121 matrix this is memory-intensive (~1.3 GB for uint8
    dense) but feasible on modern machines.

    Returns the GF(2) rank.
    """
    M, Ncols = A.shape
    print(f"    Computing GF(2) rank of {M} x {Ncols} matrix ...", flush=True)

    # Convert to dense uint8 for bitwise GF(2) elimination.
    # To save memory, represent each row as a numpy array of uint64 words
    # (bit-packed), enabling fast XOR operations.
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

    # GF(2) Gaussian elimination (partial pivoting)
    print(f"    Running GF(2) Gaussian elimination ...", flush=True)
    rank = 0
    pivot_col = 0

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

        rank += 1

        # Progress reporting (every 1000 pivots)
        if rank % 1000 == 0:
            elapsed = time.time() - t1
            print(f"      rank so far: {rank} / {M}  (elapsed {elapsed:.1f}s)", flush=True)

    elapsed = time.time() - t1
    print(f"    GF(2) elimination done in {elapsed:.1f}s, rank = {rank}", flush=True)
    return rank


# ── Minimum-Weight Null Vector (L1 relaxation) ──────────────────────────────

def find_min_weight_null_vector(A: csr_matrix, rank: int) -> int | None:
    """
    Attempt to find the minimum-weight (L1-norm) vector in the null space of A
    over the reals, as a lower bound on the minimum swap size over GF(2).

    We solve: minimize sum(t_j) subject to A @ x = 0, -t <= x <= t, t >= 0,
    and sum(t) >= 1 (to exclude the trivial zero solution).

    The LP relaxation gives a lower bound; the actual GF(2) minimum may be higher.

    Returns the minimum weight (number of non-zero entries) or None on failure.
    """
    M, Ncols = A.shape
    null_dim = Ncols - rank

    if null_dim <= 0:
        print("    Null space is trivial (dim=0). No constraint-preserving swaps exist.", flush=True)
        return 0

    print(f"    Attempting L1 minimization (LP relaxation) ...", flush=True)
    print(f"    WARNING: This LP has {2 * Ncols} variables and {M + 2 * Ncols + 1} constraints.",
          flush=True)
    print(f"    This may take a very long time or run out of memory for N={Ncols}.", flush=True)

    # Variables: x (Ncols), t (Ncols) where t_j >= |x_j|
    # minimize sum(t)
    # subject to:
    #   A @ x = 0
    #   x_j - t_j <= 0   (i.e., x <= t)
    #   -x_j - t_j <= 0  (i.e., -x <= t, equivalently x >= -t)
    #   sum(t) >= 1       (non-trivial)
    #   t >= 0

    try:
        c_obj = np.zeros(2 * Ncols)
        c_obj[Ncols:] = 1.0  # minimize sum(t)

        # Equality constraint: A @ x = 0
        A_dense_eq_x = A.toarray().astype(np.float64)
        A_eq = np.hstack([A_dense_eq_x, np.zeros((M, Ncols))])
        b_eq = np.zeros(M)

        # Inequality constraints (as upper bounds):
        # x_j - t_j <= 0  =>  [I | -I] @ [x; t] <= 0
        # -x_j - t_j <= 0 => [-I | -I] @ [x; t] <= 0
        # -sum(t) <= -1    => [0 | -1^T] @ [x; t] <= -1
        I_n = np.eye(Ncols)
        A_ub_1 = np.hstack([I_n, -I_n])       # x - t <= 0
        A_ub_2 = np.hstack([-I_n, -I_n])      # -x - t <= 0
        A_ub_3 = np.zeros((1, 2 * Ncols))
        A_ub_3[0, Ncols:] = -1.0              # -sum(t) <= -1
        A_ub = np.vstack([A_ub_1, A_ub_2, A_ub_3])
        b_ub = np.zeros(2 * Ncols + 1)
        b_ub[-1] = -1.0

        bounds_x = [(None, None)] * Ncols
        bounds_t = [(0, None)] * Ncols
        bounds = bounds_x + bounds_t

        print("    Solving LP (this may take a while) ...", flush=True)
        t0 = time.time()
        result = linprog(c_obj, A_ub=A_ub, b_ub=b_ub, A_eq=A_eq, b_eq=b_eq,
                         bounds=bounds, method="highs")
        elapsed = time.time() - t0
        print(f"    LP solved in {elapsed:.1f}s", flush=True)

        if result.success:
            min_l1 = result.fun
            print(f"    LP optimal L1 norm = {min_l1:.6f}", flush=True)
            # Count non-zero entries (threshold at 1e-6)
            x_vals = result.x[:Ncols]
            nz = np.sum(np.abs(x_vals) > 1e-6)
            print(f"    LP solution has {nz} non-zero entries", flush=True)
            return int(np.ceil(min_l1))
        else:
            print(f"    LP failed: {result.message}", flush=True)
            return None

    except MemoryError:
        print("    LP aborted: out of memory (matrix too large for dense LP).", flush=True)
        print("    Use the GF(2) null-space dimension as the structural characterization.", flush=True)
        return None
    except Exception as e:
        print(f"    LP aborted: {e}", flush=True)
        return None


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
        "--skip-lp", action="store_true",
        help="Skip the L1 LP relaxation (very memory-intensive for full N=261121)",
    )
    parser.add_argument(
        "--num-ltp", type=int, default=NUM_LTP_SUBS,
        help=f"Number of LTP sub-tables to use (default: {NUM_LTP_SUBS})",
    )
    args = parser.parse_args()

    results = {
        "experiment": "B.39a",
        "description": "N-Dimensional Constraint Geometry — Null-Space Analysis",
        "S": S,
        "N": N,
        "ltp_source": None,
        "num_ltp": args.num_ltp,
        "stratified": {},
        "min_swap_lp": None,
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
    print("STRATIFIED RANK ANALYSIS (GF(2))", flush=True)
    print("=" * 72, flush=True)

    for families, desc in family_configs:
        print(f"\n--- {desc} ---", flush=True)
        n_constraints = total_constraints[families]
        print(f"  Constraint lines: {n_constraints}", flush=True)
        print(f"  Cells (columns):  {N}", flush=True)

        t0 = time.time()
        A = build_constraint_matrix(ltp, families=families)
        t_build = time.time() - t0
        print(f"  Matrix built in {t_build:.1f}s, shape={A.shape}, nnz={A.nnz:,}", flush=True)

        rank = compute_rank_gf2(A)
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

    # ── Summary ──────────────────────────────────────────────────────────────
    print("\n" + "=" * 72, flush=True)
    print("SUMMARY", flush=True)
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
        print(f"  This means 2^{full['null_dim']} distinct solutions share the same cross-sums.", flush=True)
        print(f"  SHA-1 lateral hashes are needed to disambiguate.", flush=True)

    # ── L1 minimization (optional) ───────────────────────────────────────────
    if not args.skip_lp and full["null_dim"] > 0:
        print("\n" + "=" * 72, flush=True)
        print("MINIMUM-WEIGHT NULL VECTOR (L1 LP RELAXATION)", flush=True)
        print("=" * 72, flush=True)
        A_full = build_constraint_matrix(ltp, families="all")
        min_weight = find_min_weight_null_vector(A_full, full["rank"])
        results["min_swap_lp"] = min_weight
        if min_weight is not None:
            print(f"\n  LP lower bound on minimum swap size: {min_weight} cells", flush=True)
    elif args.skip_lp:
        print("\n  (LP relaxation skipped via --skip-lp)", flush=True)

    # ── Write results ────────────────────────────────────────────────────────
    print(f"\nWriting results to {args.output} ...", flush=True)
    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)
    print("Done.", flush=True)


if __name__ == "__main__":
    main()
