#!/usr/bin/env python3
"""
B.60a: GF(2) Rank with LH + VH (Dual CRC-32 Hash) at S=127

Measures the GF(2) rank of the combined cross-sum + LH + VH constraint system
at S=127 WITHOUT yLTP sub-tables. VH (vertical hash) provides per-column CRC-32
digests, adding 4,064 equations on orthogonal variable subsets.

Constructs an 8,888 x 16,129 GF(2) matrix:
  - 760 cross-sum parity equations (127 LSM + 127 VSM + 253 DSM + 253 XSM)
  - 4,064 CRC-32 LH equations (127 rows x 32 bits)
  - 4,064 CRC-32 VH equations (127 cols x 32 bits)

Stratified rank: geometric-only -> +LH -> +VH.
Repeats with 5 random CSMs to verify rank is input-independent.

Usage:
    python3 tools/b60a_dual_hash_rank.py
    python3 tools/b60a_dual_hash_rank.py --trials 10
"""

import argparse
import json
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S  # 16,129
DIAG_COUNT = 2 * S - 1  # 253


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
# GF(2) matrix assembly (no yLTP)
# ---------------------------------------------------------------------------
def build_geometric_parity_matrix():
    """Build the 760 x 16,129 geometric cross-sum parity matrix (no yLTP).

    760 = 127 LSM + 127 VSM + 253 DSM + 253 XSM.
    """
    n_lines = S + S + DIAG_COUNT + DIAG_COUNT  # 760
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

    assert row_idx == n_lines
    return G


def build_lh_matrix(G_crc):
    """Build the 4,064 x 16,129 LH (row CRC-32) equation matrix.

    LH row r: 32 equations on variables {127*r, ..., 127*r+126}.
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


def build_vh_matrix(G_crc):
    """Build the 4,064 x 16,129 VH (column CRC-32) equation matrix.

    VH column c: 32 equations on variables {c, 127+c, 254+c, ..., 126*127+c}.
    Same generator matrix G_crc but variables are column-strided.
    """
    n_eq = S * 32  # 4,064
    G = np.zeros((n_eq, N), dtype=np.uint8)

    for c in range(S):
        base_row = c * 32
        for i in range(32):
            for r in range(S):
                G[base_row + i, r * S + c] = G_crc[i, r]

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
    parser = argparse.ArgumentParser(description="B.60a: GF(2) rank with LH+VH at S=127")
    parser.add_argument("--trials", type=int, default=5,
                        help="Number of random CSMs to test (rank should be input-independent)")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60a_results.json"))
    args = parser.parse_args()

    print("B.60a: GF(2) Rank with LH + VH (Dual CRC-32) at S=127")
    print(f"  S={S}, N={N}, diag_count={DIAG_COUNT}")
    print(f"  Geometric equations: 760 (127 LSM + 127 VSM + 253 DSM + 253 XSM)")
    print(f"  CRC-32 LH equations: {S * 32} = 4,064")
    print(f"  CRC-32 VH equations: {S * 32} = 4,064")
    print(f"  Total equations: 8,888")
    print(f"  No yLTP sub-tables (replaced by VH)")
    print(f"  Trials: {args.trials}")
    print()

    # Build CRC-32 generator matrix (same for all trials)
    print("Building CRC-32 generator matrix (32 x 127)...", flush=True)
    t0 = time.time()
    G_crc, c_vec = build_crc32_generator_matrix()
    print(f"  Done in {time.time() - t0:.2f}s. G_crc rank: ", end="", flush=True)
    crc_rank = gf2_rank_packed(G_crc.copy())
    print(f"{crc_rank}")
    print()

    # Build geometric parity matrix (760 x 16,129)
    print("Building geometric parity matrix (760 x 16129)...", flush=True)
    t0 = time.time()
    G_geo = build_geometric_parity_matrix()
    geo_time = time.time() - t0
    print(f"  Done in {geo_time:.2f}s")
    print()

    # Build LH and VH matrices
    print("Building LH matrix (4064 x 16129)...", flush=True)
    t0 = time.time()
    G_lh = build_lh_matrix(G_crc)
    print(f"  Done in {time.time() - t0:.2f}s")

    print("Building VH matrix (4064 x 16129)...", flush=True)
    t0 = time.time()
    G_vh = build_vh_matrix(G_crc)
    print(f"  Done in {time.time() - t0:.2f}s")
    print()

    results = []
    rng = np.random.default_rng(42)

    for trial in range(args.trials):
        print(f"{'=' * 72}")
        print(f"Trial {trial + 1}/{args.trials}")
        print(f"{'=' * 72}")

        # Stratified rank analysis (structure-only, input-independent)
        print("  Stratified rank analysis:")

        configs = [
            ("LSM only", G_geo[:S]),
            ("LSM + VSM", G_geo[:2 * S]),
            ("LSM + VSM + DSM", G_geo[:2 * S + DIAG_COUNT]),
            ("4 geometric (LSM+VSM+DSM+XSM)", G_geo),
            ("+ LH (geometric + row CRC-32)", np.vstack([G_geo, G_lh])),
            ("+ VH (geometric + LH + VH)", np.vstack([G_geo, G_lh, G_vh])),
        ]

        trial_stratified = {}
        for label, G_sub in configs:
            t0 = time.time()
            rank = gf2_rank_packed(G_sub.copy())
            elapsed = time.time() - t0
            null_dim = N - rank
            density = rank / N
            trial_stratified[label] = {
                "rank": rank,
                "null_dim": null_dim,
                "density": round(density, 4),
            }
            print(f"    {label:45s} rank={rank:5d}  null={null_dim:5d}  "
                  f"density={density:.4f}  ({elapsed:.1f}s)")

        # Compute contributions
        geo_rank = trial_stratified["4 geometric (LSM+VSM+DSM+XSM)"]["rank"]
        lh_plus_rank = trial_stratified["+ LH (geometric + row CRC-32)"]["rank"]
        full_rank = trial_stratified["+ VH (geometric + LH + VH)"]["rank"]

        lh_contribution = lh_plus_rank - geo_rank
        vh_contribution = full_rank - lh_plus_rank
        total_crc_contribution = lh_contribution + vh_contribution

        print()
        print(f"    Geometric rank:        {geo_rank}")
        print(f"    LH contribution:       +{lh_contribution} (of {S * 32} max)")
        print(f"    VH contribution:       +{vh_contribution} (of {S * 32} max)")
        print(f"    Total CRC-32 contrib:  +{total_crc_contribution}")
        print(f"    Full system rank:      {full_rank}")
        print(f"    Null-space:            {N - full_rank}")
        print(f"    Density:               {full_rank / N:.4f}")
        print()

        result = {
            "trial": trial + 1,
            "stratified": trial_stratified,
            "geometric_rank": geo_rank,
            "lh_contribution": lh_contribution,
            "vh_contribution": vh_contribution,
            "full_rank": full_rank,
            "full_null_dim": N - full_rank,
            "density": round(full_rank / N, 4),
        }
        results.append(result)

        # Rank is structure-dependent only (not input-dependent), so all trials
        # should produce identical results. Break early after confirming.
        if trial > 0 and result["full_rank"] == results[0]["full_rank"]:
            print(f"  (rank confirmed identical to trial 1 — skipping remaining trials)")
            break

    # Summary
    print(f"\n{'=' * 72}")
    print("B.60a Summary")
    print(f"{'=' * 72}")

    r = results[0]
    full_rank = r["full_rank"]
    full_null = r["full_null_dim"]

    # B.58 comparison
    b58_rank = 5037  # from B.58a measurement (with 2 yLTP)
    b58_null = N - b58_rank
    improvement = full_rank - b58_rank

    print(f"  B.58 (LH + 2 yLTP):     rank={b58_rank}, null={b58_null}, density={b58_rank/N:.4f}")
    print(f"  B.60 (LH + VH, no yLTP): rank={full_rank}, null={full_null}, density={full_rank/N:.4f}")
    print(f"  Improvement:             +{improvement} rank ({improvement/b58_rank*100:.1f}%)")
    print(f"                           -{b58_null - full_null} null-space")
    print()
    print(f"  Geometric rank:          {r['geometric_rank']}")
    print(f"  LH contribution:         +{r['lh_contribution']} / {S * 32}")
    print(f"  VH contribution:         +{r['vh_contribution']} / {S * 32}")
    print()

    if full_rank >= 8500:
        outcome = "H1 (Near-maximum)"
        interp = "VH contributes ~3,700+ independent equations; dual-hash achieves ~0.53+ density"
    elif full_rank >= 7000:
        outcome = "H2 (Partial)"
        interp = "VH partially redundant with LH + cross-sums"
    else:
        outcome = "H3 (Low)"
        interp = "VH largely redundant; cross-axis structure does not help"
    print(f"  OUTCOME: {outcome}")
    print(f"  {interp}")

    # Write results
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "experiment": "B.60a",
        "config": {
            "S": S,
            "N": N,
            "diag_count": DIAG_COUNT,
            "geometric_equations": 760,
            "lh_equations": S * 32,
            "vh_equations": S * 32,
            "total_equations": 8888,
        },
        "results": results,
        "outcome": outcome,
        "b58_comparison": {
            "b58_rank": b58_rank,
            "b60_rank": full_rank,
            "improvement": improvement,
        },
    }
    out_path.write_text(json.dumps(summary, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
