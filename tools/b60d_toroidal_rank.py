#!/usr/bin/env python3
"""
B.60d: GF(2) Rank with Toroidal Diagonals + LH + VH at S=127.

B.60a repeated on the B.60c toroidal constraint geometry.
tDSM slope=1: k = (c - r) mod 127
tXSM slope=126: k = (c + r) mod 127

Usage:
    python3 tools/b60d_toroidal_rank.py
    python3 tools/b60d_toroidal_rank.py --trials 5
"""

import argparse
import json
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
N_GEO = S + S + S + S  # 508 (LSM + VSM + tDSM + tXSM)


def build_crc32_gen():
    total_bytes = (S + 1 + 7) // 8
    c0 = zlib.crc32(bytes(total_bytes)) & 0xFFFFFFFF
    c_vec = np.array([(c0 >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
    G = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(total_bytes)
        msg[col // 8] |= (1 << (7 - (col % 8)))
        col_val = (zlib.crc32(bytes(msg)) & 0xFFFFFFFF) ^ c0
        for i in range(32):
            G[i, col] = (col_val >> (31 - i)) & 1
    return G, c_vec


def build_toroidal_geo_matrix():
    """Build 508 x 16,129 toroidal geometric parity matrix."""
    G = np.zeros((N_GEO, N), dtype=np.uint8)
    idx = 0

    # LSM
    for r in range(S):
        for c in range(S):
            G[idx, r * S + c] = 1
        idx += 1

    # VSM
    for c in range(S):
        for r in range(S):
            G[idx, r * S + c] = 1
        idx += 1

    # tDSM slope=1: k = (c - r) mod S
    for k in range(S):
        for r in range(S):
            c = (k + r) % S
            G[idx, r * S + c] = 1
        idx += 1

    # tXSM slope=126: k = (c + r) mod S  (since (c - 126*r) mod 127 = (c + r) mod 127)
    for k in range(S):
        for r in range(S):
            c = (k - r) % S
            if c < 0:
                c += S
            G[idx, r * S + c] = 1
        idx += 1

    assert idx == N_GEO
    return G


def build_lh_matrix(G_crc):
    n_eq = S * 32
    G = np.zeros((n_eq, N), dtype=np.uint8)
    for r in range(S):
        base_col = r * S
        base_row = r * 32
        for i in range(32):
            for c in range(S):
                G[base_row + i, base_col + c] = G_crc[i, c]
    return G


def build_vh_matrix(G_crc):
    n_eq = S * 32
    G = np.zeros((n_eq, N), dtype=np.uint8)
    for c in range(S):
        base_row = c * 32
        for i in range(32):
            for r in range(S):
                G[base_row + i, r * S + c] = G_crc[i, r]
    return G


def gf2_rank(G_uint8):
    """GF(2) rank via Gaussian elimination."""
    m, n = G_uint8.shape
    words = (n + 63) // 64
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
        found = -1
        for r in range(pivot_row, m):
            if rows[r][w] & mask:
                found = r
                break
        if found == -1:
            continue
        rows[pivot_row], rows[found] = rows[found], rows[pivot_row]
        for r in range(m):
            if r != pivot_row and (rows[r][w] & mask):
                rows[r] ^= rows[pivot_row]
        pivot_row += 1
    return pivot_row


def main():
    parser = argparse.ArgumentParser(description="B.60d: GF(2) rank with toroidal diags")
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60d_results.json"))
    args = parser.parse_args()

    print(f"B.60d: GF(2) Rank with Toroidal Diagonals + LH + VH (S={S})")
    print(f"  Geometric: {N_GEO} equations (127 LSM + 127 VSM + 127 tDSM + 127 tXSM)")
    print(f"  LH: {S * 32} equations")
    print(f"  VH: {S * 32} equations")
    print(f"  Total: {N_GEO + 2 * S * 32} equations")
    print(f"  Trials: {args.trials}")
    print()

    G_crc, c_vec = build_crc32_gen()

    print("Building toroidal geometric matrix...", flush=True)
    t0 = time.time()
    G_geo = build_toroidal_geo_matrix()
    print(f"  Done ({time.time() - t0:.1f}s)")

    print("Building LH matrix...", flush=True)
    G_lh = build_lh_matrix(G_crc)

    print("Building VH matrix...", flush=True)
    G_vh = build_vh_matrix(G_crc)
    print()

    results = []
    for trial in range(1, args.trials + 1):
        print(f"{'=' * 60}")
        print(f"Trial {trial}/{args.trials}")
        print(f"{'=' * 60}")

        configs = [
            ("LSM only", G_geo[:S]),
            ("LSM + VSM", G_geo[:2 * S]),
            ("4 geometric (toroidal)", G_geo),
            ("+ LH", np.vstack([G_geo, G_lh])),
            ("+ VH (full system)", np.vstack([G_geo, G_lh, G_vh])),
        ]

        trial_data = {}
        for label, G_sub in configs:
            t0 = time.time()
            r = gf2_rank(G_sub.copy())
            elapsed = time.time() - t0
            null_dim = N - r
            density = r / N
            trial_data[label] = {"rank": r, "null_dim": null_dim, "density": round(density, 4)}
            print(f"  {label:40s} rank={r:5d}  null={null_dim:5d}  density={density:.4f}  ({elapsed:.1f}s)")

        geo_rank = trial_data["4 geometric (toroidal)"]["rank"]
        lh_rank = trial_data["+ LH"]["rank"]
        full_rank = trial_data["+ VH (full system)"]["rank"]
        print(f"\n  Geometric: {geo_rank}")
        print(f"  LH contribution: +{lh_rank - geo_rank}")
        print(f"  VH contribution: +{full_rank - lh_rank}")
        print(f"  Full rank: {full_rank}")

        results.append(trial_data)

        if trial > 1 and trial_data["+ VH (full system)"]["rank"] == results[0]["+ VH (full system)"]["rank"]:
            print(f"  (rank confirmed identical — skipping remaining trials)")
            break
        print()

    # Comparison to B.60a
    full_rank = results[0]["+ VH (full system)"]["rank"]
    print(f"\n{'=' * 60}")
    print(f"B.60d Summary")
    print(f"{'=' * 60}")
    print(f"  B.60a (non-toroidal): rank=7,793, geo=753")
    print(f"  B.60d (toroidal):     rank={full_rank}, geo={results[0]['4 geometric (toroidal)']['rank']}")
    print(f"  Delta rank: {full_rank - 7793:+d}")

    if full_rank > 7793:
        outcome = "H1 (Higher rank)"
    elif abs(full_rank - 7793) <= 200:
        outcome = "H2 (Comparable rank)"
    else:
        outcome = "H3 (Lower rank)"
    print(f"  Outcome: {outcome}")

    summary = {
        "experiment": "B.60d",
        "config": {"S": S, "N": N, "geo_lines": N_GEO, "total_eq": N_GEO + 2 * S * 32},
        "results": results,
        "outcome": outcome,
        "comparison": {"b60a_rank": 7793, "b60d_rank": full_rank, "delta": full_rank - 7793},
    }
    Path(args.out).write_text(json.dumps(summary, indent=2))
    print(f"\n  Results: {args.out}")


if __name__ == "__main__":
    main()
