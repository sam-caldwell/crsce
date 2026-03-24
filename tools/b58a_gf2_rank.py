#!/usr/bin/env python3
"""
B.58a: GF(2) Rank and Null-Space at S=127 with CRC-32

Determines the exact GF(2) rank of the combined cross-sum + CRC-32 constraint
system at S=127 with 2 yLTP sub-tables. This is the foundational measurement
that determines whether the B.58 combinator approach is viable.

Constructs a 5,078 x 16,129 GF(2) matrix:
  - 1,014 cross-sum parity equations (127 LSM + 127 VSM + 253 DSM + 253 XSM + 2x127 yLTP)
  - 4,064 CRC-32 equations (127 rows x 32 bits)

Measures: GF(2) rank, null-space dimension, stratified rank by family,
CRC-32 independent contribution, and minimum null-space vector weight.

Usage:
    python3 tools/b58a_gf2_rank.py
    python3 tools/b58a_gf2_rank.py --seeds 10   # test 10 random yLTP seed pairs
"""

import argparse
import json
import os
import struct
import sys
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S  # 16,129
DIAG_COUNT = 2 * S - 1  # 253
WORDS_PER_ROW = (N + 63) // 64  # 253 uint64 words per GF(2) row


# ---------------------------------------------------------------------------
# CRC-32 generator matrix construction (Section B.58.2.2)
# ---------------------------------------------------------------------------
def build_crc32_generator_matrix(n_data_bits=127, n_pad_bits=1):
    """Build the 32 x n_data_bits GF(2) generator matrix for CRC-32.

    Returns (G_crc, c_vec) where:
      G_crc: 32 x n_data_bits numpy array (uint8, 0/1)
      c_vec: 32-element numpy array (uint8) = CRC-32 of all-zero message
    """
    total_bytes = (n_data_bits + n_pad_bits + 7) // 8
    zero_msg = bytes(total_bytes)
    c = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = np.array([(c >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)

    G_crc = np.zeros((32, n_data_bits), dtype=np.uint8)
    for col in range(n_data_bits):
        msg = bytearray(total_bytes)
        byte_idx = col // 8
        bit_idx = 7 - (col % 8)  # MSB-first
        msg[byte_idx] |= (1 << bit_idx)
        crc_one = zlib.crc32(bytes(msg)) & 0xFFFFFFFF
        col_val = crc_one ^ c  # XOR removes the constant
        for i in range(32):
            G_crc[i, col] = (col_val >> (31 - i)) & 1

    return G_crc, c_vec


# ---------------------------------------------------------------------------
# yLTP partition construction (Fisher-Yates with LCG)
# ---------------------------------------------------------------------------
LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64


def build_yltp_membership(seed, s=S):
    """Build yLTP membership: membership[flat_cell] = line_index."""
    n = s * s
    pool = list(range(n))
    membership = [0] * n
    state = seed

    for i in range(n - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        j = state % (i + 1)
        pool[i], pool[j] = pool[j], pool[i]

    # Assign consecutive s-cell chunks to lines
    for line_idx in range(s):
        for slot in range(s):
            flat = pool[line_idx * s + slot]
            membership[flat] = line_idx

    return membership


# ---------------------------------------------------------------------------
# GF(2) matrix assembly
# ---------------------------------------------------------------------------
def build_crosssum_parity_matrix(yltp1_membership, yltp2_membership):
    """Build the 1,014 x 16,129 cross-sum parity matrix.

    Returns G_cs (1014 x N uint8 array).
    """
    n_lines = S + S + DIAG_COUNT + DIAG_COUNT + S + S  # 1,014
    G = np.zeros((n_lines, N), dtype=np.uint8)
    row_idx = 0

    # LSM: row r contains cells (r, 0..126)
    for r in range(S):
        for c in range(S):
            G[row_idx, r * S + c] = 1
        row_idx += 1

    # VSM: column c contains cells (0..126, c)
    for c in range(S):
        for r in range(S):
            G[row_idx, r * S + c] = 1
        row_idx += 1

    # DSM: diagonal d has cells (r, c) where c - r = d - (S-1)
    for d in range(DIAG_COUNT):
        offset = d - (S - 1)
        for r in range(S):
            c = r + offset
            if 0 <= c < S:
                G[row_idx, r * S + c] = 1
        row_idx += 1

    # XSM: anti-diagonal x has cells (r, c) where r + c = x
    for x in range(DIAG_COUNT):
        for r in range(S):
            c = x - r
            if 0 <= c < S:
                G[row_idx, r * S + c] = 1
        row_idx += 1

    # yLTP1
    for line in range(S):
        for flat in range(N):
            if yltp1_membership[flat] == line:
                G[row_idx, flat] = 1
        row_idx += 1

    # yLTP2
    for line in range(S):
        for flat in range(N):
            if yltp2_membership[flat] == line:
                G[row_idx, flat] = 1
        row_idx += 1

    assert row_idx == n_lines
    return G


def build_crc32_global_matrix(G_crc):
    """Build the 4,064 x 16,129 CRC-32 equation matrix.

    G_crc is 32 x 127 (per-row generator). Each row r produces 32 equations
    on columns 127*r .. 127*r+126.
    """
    n_eq = S * 32  # 4,064
    G = np.zeros((n_eq, N), dtype=np.uint8)

    for r in range(S):
        base_col = r * S
        base_row = r * 32
        for i in range(32):
            for c in range(S):
                G[base_row + i, base_col + c] = G_crc[i, c]

    return G


# ---------------------------------------------------------------------------
# GF(2) Gaussian elimination (packed uint64 for speed)
# ---------------------------------------------------------------------------
def gf2_rank_packed(G_uint8):
    """Compute GF(2) rank of a binary matrix using packed uint64 rows.

    Args:
        G_uint8: m x n numpy array (uint8, 0/1 entries)

    Returns:
        rank: int
    """
    m, n = G_uint8.shape
    words = (n + 63) // 64

    # Pack each row into uint64 words
    rows = []
    for i in range(m):
        packed = np.zeros(words, dtype=np.uint64)
        for w in range(words):
            val = np.uint64(0)
            for b in range(64):
                col = w * 64 + b
                if col < n and G_uint8[i, col]:
                    val |= np.uint64(1) << np.uint64(b)
            packed[w] = val
        rows.append(packed)

    pivot_row = 0
    for col in range(n):
        w = col // 64
        b = np.uint64(col % 64)
        mask = np.uint64(1) << b

        # Find pivot
        found = -1
        for r in range(pivot_row, m):
            if rows[r][w] & mask:
                found = r
                break
        if found == -1:
            continue

        # Swap
        rows[pivot_row], rows[found] = rows[found], rows[pivot_row]

        # Eliminate
        for r in range(m):
            if r != pivot_row and (rows[r][w] & mask):
                rows[r] ^= rows[pivot_row]

        pivot_row += 1

    return pivot_row


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.58a: GF(2) rank measurement at S=127")
    parser.add_argument("--seeds", type=int, default=3, help="Number of yLTP seed pairs to test")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b58a_results.json"))
    args = parser.parse_args()

    print("B.58a: GF(2) Rank and Null-Space at S=127")
    print(f"  S={S}, N={N}, diag_count={DIAG_COUNT}")
    print(f"  Cross-sum equations: 1,014")
    print(f"  CRC-32 equations: {S * 32} = 4,064")
    print(f"  Total equations: 5,078")
    print(f"  yLTP seed pairs to test: {args.seeds}")
    print()

    # Build CRC-32 generator matrix (same for all seed pairs)
    print("Building CRC-32 generator matrix (32 x 127)...", flush=True)
    t0 = time.time()
    G_crc, c_vec = build_crc32_generator_matrix()
    print(f"  Done in {time.time() - t0:.2f}s. G_crc shape: {G_crc.shape}")
    print(f"  CRC-32 of zero row (c_vec): {c_vec}")
    print(f"  G_crc rank (should be 32): ", end="", flush=True)
    crc_rank = gf2_rank_packed(G_crc.copy())
    print(f"{crc_rank}")
    print()

    # Build CRC-32 global matrix (4,064 x 16,129)
    print("Building CRC-32 global matrix (4064 x 16129)...", flush=True)
    t0 = time.time()
    G_crc_global = build_crc32_global_matrix(G_crc)
    print(f"  Done in {time.time() - t0:.2f}s")
    print()

    # Default seeds (B.26c winners)
    PREFIX = b"CRSCLTP"
    default_seeds = [
        (int.from_bytes(PREFIX + b"V", "big"), int.from_bytes(PREFIX + b"P", "big")),
    ]
    # Add random seed pairs
    rng = np.random.default_rng(42)
    for _ in range(args.seeds - 1):
        s1 = int(rng.integers(1, 2**63))
        s2 = int(rng.integers(1, 2**63))
        default_seeds.append((s1, s2))

    results = []

    for seed_idx, (seed1, seed2) in enumerate(default_seeds):
        print(f"{'='*72}")
        print(f"Seed pair {seed_idx + 1}/{len(default_seeds)}: seed1=0x{seed1:016x}, seed2=0x{seed2:016x}")
        print(f"{'='*72}")

        # Build yLTP memberships
        print("  Building yLTP1...", end="", flush=True)
        t0 = time.time()
        yltp1 = build_yltp_membership(seed1)
        print(f" {time.time() - t0:.2f}s")
        print("  Building yLTP2...", end="", flush=True)
        t0 = time.time()
        yltp2 = build_yltp_membership(seed2)
        print(f" {time.time() - t0:.2f}s")

        # Build cross-sum parity matrix
        print("  Building cross-sum parity matrix (1014 x 16129)...", end="", flush=True)
        t0 = time.time()
        G_cs = build_crosssum_parity_matrix(yltp1, yltp2)
        print(f" {time.time() - t0:.2f}s")

        # Stratified rank analysis
        print("  Stratified rank analysis:")
        stratified = {}
        configs = [
            ("LSM", S),
            ("LSM+VSM", 2 * S),
            ("LSM+VSM+DSM", 2 * S + DIAG_COUNT),
            ("LSM+VSM+DSM+XSM", 2 * S + 2 * DIAG_COUNT),
            ("+yLTP1", 2 * S + 2 * DIAG_COUNT + S),
            ("+yLTP2 (all cross-sums)", 2 * S + 2 * DIAG_COUNT + 2 * S),
        ]

        for label, n_rows in configs:
            t0 = time.time()
            rank = gf2_rank_packed(G_cs[:n_rows].copy())
            elapsed = time.time() - t0
            null_dim = N - rank
            stratified[label] = {"rank": rank, "null_dim": null_dim}
            print(f"    {label:40s} rank={rank:5d}  null={null_dim:5d}  ({elapsed:.1f}s)")

        # Full system: cross-sums + CRC-32
        print("  Full system (cross-sums + CRC-32):", flush=True)
        G_full = np.vstack([G_cs, G_crc_global])
        print(f"    Matrix shape: {G_full.shape}")
        t0 = time.time()
        full_rank = gf2_rank_packed(G_full.copy())
        elapsed = time.time() - t0
        full_null = N - full_rank

        cs_rank = stratified["+yLTP2 (all cross-sums)"]["rank"]
        crc_contribution = full_rank - cs_rank

        print(f"    Full rank:           {full_rank}")
        print(f"    Null-space:          {full_null}")
        print(f"    Cross-sum rank:      {cs_rank}")
        print(f"    CRC-32 contribution: +{crc_contribution} independent equations")
        print(f"    Time:                {elapsed:.1f}s")
        print()

        result = {
            "seed1": hex(seed1),
            "seed2": hex(seed2),
            "stratified": stratified,
            "full_rank": full_rank,
            "full_null_dim": full_null,
            "crosssum_rank": cs_rank,
            "crc32_contribution": crc_contribution,
            "crc32_max_possible": S * 32,
        }
        results.append(result)

    # Summary
    print(f"\n{'='*72}")
    print("B.58a Summary")
    print(f"{'='*72}")
    for r in results:
        print(f"  {r['seed1']}: rank={r['full_rank']}, null={r['full_null_dim']}, "
              f"CRC-32 contrib=+{r['crc32_contribution']}/{r['crc32_max_possible']}")

    avg_rank = sum(r["full_rank"] for r in results) / len(results)
    avg_crc = sum(r["crc32_contribution"] for r in results) / len(results)
    print(f"\n  Average rank:           {avg_rank:.0f}")
    print(f"  Average CRC-32 contrib: {avg_crc:.0f} / {S * 32}")
    print(f"  Average null-space:     {N - avg_rank:.0f}")

    if avg_rank >= 5000:
        print(f"\n  OUTCOME: H1 (Near-maximum rank) — CRC-32 contributing nearly all equations")
    elif avg_rank >= 3000:
        print(f"\n  OUTCOME: H2 (Partial CRC contribution)")
    else:
        print(f"\n  OUTCOME: H3 (Minimal CRC contribution)")

    # Write results
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "experiment": "B.58a",
        "config": {"S": S, "N": N, "diag_count": DIAG_COUNT},
        "results": results,
    }
    out_path.write_text(json.dumps(summary, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
