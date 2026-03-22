#!/usr/bin/env python3
"""
B.44a: Constraint Family Discovery — Rank Analysis.

Extends the B.39a constraint matrix with candidate new families and measures
the GF(2) rank increase (new independent constraints) and information efficiency
(independent constraints per payload bit) for each.

Candidate families:
  C1a: Sub-row block sums (B=2, 2 blocks per row)
  C1b: Sub-row block sums (B=4, 4 blocks per row)
  C2a: 1 parity partition (FY, new seed)
  C2b: 8 parity partitions (FY, 8 new seeds)
  C4a: 1 MOLS(511) partition
  C4b: 6 MOLS(511) partitions

Usage:
    python3 tools/b44a_constraint_sim.py
    python3 tools/b44a_constraint_sim.py --ltp-file tools/b38e_t31000000_best_s137.bin
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
    sys.exit("ERROR: numpy required.")

try:
    from scipy.sparse import csr_matrix
except ImportError:
    sys.exit("ERROR: scipy required.")

S = 511
N = S * S

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

SEEDS = [
    0x435253434C545056, 0x435253434C545050,
    0x435253434C545033, 0x435253434C545034,
    0x435253434C545035, 0x435253434C545036,
]

# New seeds for parity partitions (chosen to be distinct from existing LTP seeds)
PARITY_SEEDS = [
    0x4352534350415231,  # CRSCPAR1
    0x4352534350415232,  # CRSCPAR2
    0x4352534350415233,  # CRSCPAR3
    0x4352534350415234,  # CRSCPAR4
    0x4352534350415235,  # CRSCPAR5
    0x4352534350415236,  # CRSCPAR6
    0x4352534350415237,  # CRSCPAR7
    0x4352534350415238,  # CRSCPAR8
]


def lcg_next(state: int) -> int:
    return (state * LCG_A + LCG_C) & (LCG_MOD - 1)


def build_fy_partition(seed: int) -> np.ndarray:
    """Build a single FY partition assignment array (N cells -> 511 lines)."""
    pool = list(range(N))
    state = seed
    for i in range(N - 1, 0, -1):
        state = lcg_next(state)
        j = int(state % (i + 1))
        pool[i], pool[j] = pool[j], pool[i]
    a = np.empty(N, dtype=np.uint16)
    for line in range(S):
        base = line * S
        for pos in range(S):
            a[pool[base + pos]] = line
    return a


def build_baseline_rows(ltp_assignments: list[np.ndarray]) -> tuple:
    """Build row/col indices for the baseline 8-family constraint matrix."""
    rows_list = []
    cols_list = []
    row_offset = 0

    for r in range(S):
        for c in range(S):
            rows_list.append(row_offset + r)
            cols_list.append(r * S + c)
    row_offset += S

    for c in range(S):
        for r in range(S):
            rows_list.append(row_offset + c)
            cols_list.append(r * S + c)
    row_offset += S

    for r in range(S):
        for c in range(S):
            d = c - r + S - 1
            rows_list.append(row_offset + d)
            cols_list.append(r * S + c)
    row_offset += 2 * S - 1

    for r in range(S):
        for c in range(S):
            x = r + c
            rows_list.append(row_offset + x)
            cols_list.append(r * S + c)
    row_offset += 2 * S - 1

    for sub in range(len(ltp_assignments)):
        asn = ltp_assignments[sub]
        for flat in range(N):
            rows_list.append(row_offset + int(asn[flat]))
            cols_list.append(flat)
        row_offset += S

    return rows_list, cols_list, row_offset


def add_subrow_blocks(rows_list, cols_list, row_offset, B):
    """Add sub-row block sum constraint lines."""
    block_size = (S + B - 1) // B
    added = 0
    for r in range(S):
        for b in range(B):
            c_start = b * block_size
            c_end = min(c_start + block_size, S)
            for c in range(c_start, c_end):
                rows_list.append(row_offset + added)
                cols_list.append(r * S + c)
            added += 1
    return row_offset + added, added


def add_parity_partitions(rows_list, cols_list, row_offset, seeds):
    """Add parity partition constraint lines (same structure as LTP but 1-bit info)."""
    added = 0
    for seed in seeds:
        asn = build_fy_partition(seed)
        for flat in range(N):
            rows_list.append(row_offset + added + int(asn[flat]))
            cols_list.append(flat)
        added += S
    return row_offset + added, added


def build_mols_partition(square_idx: int) -> np.ndarray:
    """
    Build a MOLS(511)-based partition using Galois field construction.
    GF(511) exists since 511 is prime. MOLS square k maps cell (r,c)
    to line (k*r + c) mod 511.
    """
    k = square_idx + 1  # k=1,2,...,6 (k=0 would be rows, k=inf would be columns)
    a = np.empty(N, dtype=np.uint16)
    for r in range(S):
        for c in range(S):
            a[r * S + c] = (k * r + c) % S
    return a


def add_mols_partitions(rows_list, cols_list, row_offset, num_squares):
    """Add MOLS-based partition constraint lines."""
    added = 0
    for sq in range(num_squares):
        asn = build_mols_partition(sq)
        for flat in range(N):
            rows_list.append(row_offset + added + int(asn[flat]))
            cols_list.append(flat)
        added += S
    return row_offset + added, added


def compute_rank_gf2(rows_list, cols_list, total_rows, n_cols):
    """GF(2) rank via packed Gaussian elimination."""
    data = np.ones(len(rows_list), dtype=np.uint8)
    rows_arr = np.array(rows_list, dtype=np.int32)
    cols_arr = np.array(cols_list, dtype=np.int32)
    A = csr_matrix((data, (rows_arr, cols_arr)), shape=(total_rows, n_cols), dtype=np.uint8)

    M = A.shape[0]
    words_per_row = (n_cols + 63) // 64
    packed = np.zeros((M, words_per_row), dtype=np.uint64)

    A_csr = A.tocsr()
    for i in range(M):
        for c in A_csr.indices[A_csr.indptr[i]:A_csr.indptr[i + 1]]:
            word = c // 64
            bit = c % 64
            packed[i, word] ^= np.uint64(1) << np.uint64(bit)

    rank = 0
    t0 = time.time()
    for pivot_col in range(n_cols):
        if rank >= M:
            break
        word = pivot_col // 64
        bit = np.uint64(pivot_col % 64)
        mask = np.uint64(1) << bit
        found = -1
        for i in range(rank, M):
            if packed[i, word] & mask:
                found = i
                break
        if found == -1:
            continue
        if found != rank:
            packed[[rank, found]] = packed[[found, rank]]
        pivot_row = packed[rank]
        for i in range(M):
            if i != rank and (packed[i, word] & mask):
                packed[i] ^= pivot_row
        rank += 1
        if rank % 1000 == 0:
            print(f"    rank {rank}/{M} ({time.time()-t0:.1f}s)", flush=True)

    print(f"    Final rank: {rank} ({time.time()-t0:.1f}s)", flush=True)
    return rank


def load_ltpb(path, num_sub=4):
    data = path.read_bytes()
    magic, version, s_field, file_num_sub = struct.unpack_from("<4sIII", data, 0)
    assert magic == b"LTPB" and s_field == S
    assignments = []
    offset = 16
    for _ in range(min(file_num_sub, num_sub)):
        a = np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy()
        assignments.append(a)
        offset += N * 2
    return assignments


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ltp-file", type=pathlib.Path, default=None)
    parser.add_argument("--output", type=pathlib.Path,
                        default=pathlib.Path("tools/b44a_results.json"))
    args = parser.parse_args()

    # Build LTP assignments
    if args.ltp_file:
        print(f"Loading LTPB from {args.ltp_file}...", flush=True)
        ltp = load_ltpb(args.ltp_file, num_sub=4)
    else:
        print("Building FY assignments from seeds...", flush=True)
        ltp = [build_fy_partition(SEEDS[i]) for i in range(4)]

    # Baseline rank (8 families: row+col+diag+anti+4 LTP)
    print("\n" + "=" * 60, flush=True)
    print("BASELINE: 8 families (row+col+diag+anti+4 LTP)", flush=True)
    print("=" * 60, flush=True)
    base_rows, base_cols, base_offset = build_baseline_rows(ltp)
    baseline_rank = compute_rank_gf2(base_rows, base_cols, base_offset, N)
    baseline_null = N - baseline_rank
    print(f"  Rank: {baseline_rank}, Null dim: {baseline_null}", flush=True)

    results = {
        "baseline": {"rank": baseline_rank, "null_dim": baseline_null,
                      "lines": base_offset, "payload_bits": 43964},
        "candidates": {},
    }

    # Test each candidate family
    candidates = [
        ("C1a_subrow_B2", "Sub-row blocks B=2", lambda r, c, o: add_subrow_blocks(r, c, o, 2),
         S * 2, S * 2 * 8),
        ("C1b_subrow_B4", "Sub-row blocks B=4", lambda r, c, o: add_subrow_blocks(r, c, o, 4),
         S * 4, S * 3 * 7),
        ("C2a_parity_1", "1 parity partition", lambda r, c, o: add_parity_partitions(r, c, o, PARITY_SEEDS[:1]),
         S, S * 1),
        ("C2b_parity_8", "8 parity partitions", lambda r, c, o: add_parity_partitions(r, c, o, PARITY_SEEDS),
         S * 8, S * 8),
        ("C4a_mols_1", "1 MOLS(511) partition", lambda r, c, o: add_mols_partitions(r, c, o, 1),
         S, S * 9),
        ("C4b_mols_6", "6 MOLS(511) partitions", lambda r, c, o: add_mols_partitions(r, c, o, 6),
         S * 6, S * 6 * 9),
    ]

    for cid, desc, builder, expected_lines, payload_bits in candidates:
        print(f"\n{'=' * 60}", flush=True)
        print(f"CANDIDATE: {desc} ({cid})", flush=True)
        print(f"{'=' * 60}", flush=True)

        ext_rows = list(base_rows)
        ext_cols = list(base_cols)
        new_offset, added = builder(ext_rows, ext_cols, base_offset)

        print(f"  New lines: {added}, Total lines: {new_offset}", flush=True)
        print(f"  Payload cost: {payload_bits} bits ({payload_bits / 8:.0f} bytes)", flush=True)

        rank = compute_rank_gf2(ext_rows, ext_cols, new_offset, N)
        rank_gain = rank - baseline_rank
        null_dim = N - rank
        efficiency = rank_gain / payload_bits if payload_bits > 0 else 0

        print(f"  Rank: {rank} (+{rank_gain} over baseline)", flush=True)
        print(f"  Null dim: {null_dim} (was {baseline_null})", flush=True)
        print(f"  Efficiency: {efficiency:.4f} constraints/bit", flush=True)

        results["candidates"][cid] = {
            "description": desc,
            "lines_added": added,
            "payload_bits": payload_bits,
            "rank": rank,
            "rank_gain": rank_gain,
            "null_dim": null_dim,
            "efficiency": round(efficiency, 6),
        }

    # Summary
    print(f"\n{'=' * 60}", flush=True)
    print("SUMMARY", flush=True)
    print(f"{'=' * 60}", flush=True)
    print(f"{'Family':<25} {'Rank gain':>10} {'Payload bits':>13} {'Efficiency':>12} {'New null dim':>12}")
    for cid, info in results["candidates"].items():
        print(f"{info['description']:<25} {info['rank_gain']:>10} {info['payload_bits']:>13} "
              f"{info['efficiency']:>12.4f} {info['null_dim']:>12}")

    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults written to {args.output}", flush=True)


if __name__ == "__main__":
    main()
