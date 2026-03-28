#!/usr/bin/env python3
"""
B.59f: Minimum Swap Size at S=127 (from B.33/B.39)

At S=511: minimum swap = 1,528 cells (11 rows, 492 columns).
At S=127: predicted ~254-381 cells (scaling model: ~3*S for 6 families).

Method:
  1. Build GF(2) cross-sum parity matrix (1,014 x 16,129) for 6 families
  2. GF(2) Gaussian elimination → null-space basis
  3. Minimum-weight null-space vector via:
     (a) Individual basis vectors
     (b) Pairwise XOR of lightest 2,000
     (c) 3-way through 6-way random XOR (40K samples each)
  4. Report: min weight, row span, column span

Usage:
    python3 tools/b59f_min_swap.py
"""

import json
import numpy as np
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S  # 16,129
DIAG_COUNT = 2 * S - 1  # 253

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64
PREFIX = b"CRSCLTP"
SEED1 = int.from_bytes(PREFIX + b"V", "big")
SEED2 = int.from_bytes(PREFIX + b"P", "big")


def build_yltp_membership(seed):
    n = S * S
    pool = list(range(n))
    state = seed
    for i in range(n - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        pool[i], pool[int(state % (i + 1))] = pool[int(state % (i + 1))], pool[i]
    mem = [0] * n
    for line in range(S):
        for slot in range(S):
            mem[pool[line * S + slot]] = line
    return mem


def build_crosssum_parity_matrix():
    """Build 1,014 x 16,129 GF(2) matrix packed as uint64 rows."""
    n_lines = S + S + DIAG_COUNT + DIAG_COUNT + S + S
    words = (N + 63) // 64

    # Use list of numpy uint64 arrays for efficiency
    rows = []
    idx = 0

    # LSM
    for r in range(S):
        row = np.zeros(words, dtype=np.uint64)
        for c in range(S):
            flat = r * S + c
            row[flat // 64] |= np.uint64(1) << np.uint64(flat % 64)
        rows.append(row)

    # VSM
    for c in range(S):
        row = np.zeros(words, dtype=np.uint64)
        for r in range(S):
            flat = r * S + c
            row[flat // 64] |= np.uint64(1) << np.uint64(flat % 64)
        rows.append(row)

    # DSM
    for d in range(DIAG_COUNT):
        row = np.zeros(words, dtype=np.uint64)
        offset = d - (S - 1)
        for r in range(S):
            c = r + offset
            if 0 <= c < S:
                flat = r * S + c
                row[flat // 64] |= np.uint64(1) << np.uint64(flat % 64)
        rows.append(row)

    # XSM
    for x in range(DIAG_COUNT):
        row = np.zeros(words, dtype=np.uint64)
        for r in range(S):
            c = x - r
            if 0 <= c < S:
                flat = r * S + c
                row[flat // 64] |= np.uint64(1) << np.uint64(flat % 64)
        rows.append(row)

    # yLTP1, yLTP2
    for seed in [SEED1, SEED2]:
        mem = build_yltp_membership(seed)
        for line in range(S):
            row = np.zeros(words, dtype=np.uint64)
            for flat in range(N):
                if mem[flat] == line:
                    row[flat // 64] |= np.uint64(1) << np.uint64(flat % 64)
            rows.append(row)

    assert len(rows) == n_lines
    return rows


def popcount_packed(row):
    """Count set bits in a packed uint64 row."""
    total = 0
    for w in row:
        total += bin(int(w)).count('1')
    return total


def row_span(row):
    """Count distinct rows touched by a packed null-space vector."""
    touched = set()
    for flat in range(N):
        w = flat // 64
        b = flat % 64
        if int(row[w]) & (1 << b):
            touched.add(flat // S)
    return len(touched)


def col_span(row):
    """Count distinct columns touched."""
    touched = set()
    for flat in range(N):
        w = flat // 64
        b = flat % 64
        if int(row[w]) & (1 << b):
            touched.add(flat % S)
    return len(touched)


def gauss_elim_nullspace(rows):
    """GF(2) Gaussian elimination → null-space basis.

    Returns (rank, null_basis) where null_basis is a list of packed uint64 vectors.
    """
    m = len(rows)
    words = len(rows[0])
    n = N

    # Augment each row with an identity column for tracking
    # Instead, track row operations to extract null-space
    # Use the standard approach: RREF on the transpose gives null-space

    # For efficiency, compute rank first, then extract null-space basis
    # from the RREF of the original matrix

    # Copy rows
    G = [r.copy() for r in rows]

    pivot_cols = []
    pivot_row = 0

    for col in range(n):
        w = col // 64
        b = np.uint64(col % 64)
        mask = np.uint64(1) << b

        found = -1
        for r in range(pivot_row, m):
            if G[r][w] & mask:
                found = r
                break
        if found == -1:
            continue

        G[pivot_row], G[found] = G[found], G[pivot_row]

        for r in range(m):
            if r != pivot_row and (G[r][w] & mask):
                G[r] ^= G[pivot_row]

        pivot_cols.append(col)
        pivot_row += 1

    rank = len(pivot_cols)
    pivot_set = set(pivot_cols)
    free_cols = [c for c in range(n) if c not in pivot_set]
    null_dim = len(free_cols)

    print(f"  Rank: {rank}, Null-space: {null_dim}, Free cols: {len(free_cols)}")

    # Extract null-space basis: for each free column f, construct a vector
    # with 1 at position f, and for each pivot column p in row i,
    # set bit p if G_rref[i, f] == 1
    null_basis = []
    for f in free_cols:
        vec = np.zeros(words, dtype=np.uint64)
        # Set the free variable bit
        vec[f // 64] |= np.uint64(1) << np.uint64(f % 64)
        # Set pivot bits from RREF
        for i, p in enumerate(pivot_cols):
            fw = f // 64
            fb = np.uint64(f % 64)
            if G[i][fw] & (np.uint64(1) << fb):
                vec[p // 64] |= np.uint64(1) << np.uint64(p % 64)
        null_basis.append(vec)

    return rank, null_basis


def main():
    print("B.59f: Minimum Swap Size at S=127")
    print(f"  S={S}, N={N}")
    print(f"  Prediction: ~{2*S}-{3*S} cells (2-3 * S scaling)")
    print()

    # Build cross-sum parity matrix
    print("Building cross-sum parity matrix (1014 x 16129)...", flush=True)
    t0 = time.time()
    rows = build_crosssum_parity_matrix()
    print(f"  Done in {time.time() - t0:.1f}s")

    # GF(2) elimination → null-space basis
    print("Computing GF(2) null-space basis...", flush=True)
    t0 = time.time()
    rank, null_basis = gauss_elim_nullspace(rows)
    print(f"  Done in {time.time() - t0:.1f}s")
    print(f"  Null-space basis: {len(null_basis)} vectors")

    # (a) Individual basis vector weights
    print("\n(a) Individual basis vector weights...", flush=True)
    weights = []
    for i, vec in enumerate(null_basis):
        w = popcount_packed(vec)
        weights.append((w, i))
    weights.sort()

    print(f"  Min: {weights[0][0]}")
    print(f"  p5:  {weights[len(weights)//20][0]}")
    print(f"  p25: {weights[len(weights)//4][0]}")
    print(f"  Median: {weights[len(weights)//2][0]}")
    print(f"  Mean: {sum(w for w,_ in weights) / len(weights):.1f}")
    print(f"  p75: {weights[3*len(weights)//4][0]}")
    print(f"  p95: {weights[19*len(weights)//20][0]}")
    print(f"  Max: {weights[-1][0]}")

    min_weight = weights[0][0]
    min_idx = weights[0][1]
    min_vec = null_basis[min_idx]
    print(f"\n  Lightest basis vector: weight={min_weight}, "
          f"row_span={row_span(min_vec)}, col_span={col_span(min_vec)}")

    # (b) Pairwise XOR of lightest 2,000
    print("\n(b) Pairwise XOR of lightest 2,000 basis vectors...", flush=True)
    t0 = time.time()
    top_k = min(2000, len(null_basis))
    lightest_indices = [idx for _, idx in weights[:top_k]]
    best_pair_weight = min_weight
    best_pair_vec = min_vec

    pairs_tested = 0
    for i in range(top_k):
        for j in range(i + 1, top_k):
            xor_vec = null_basis[lightest_indices[i]] ^ null_basis[lightest_indices[j]]
            w = popcount_packed(xor_vec)
            if w > 0 and w < best_pair_weight:
                best_pair_weight = w
                best_pair_vec = xor_vec
            pairs_tested += 1

    print(f"  Pairs tested: {pairs_tested:,}")
    print(f"  Best pairwise weight: {best_pair_weight}")
    if best_pair_weight < min_weight:
        print(f"  Improvement: {min_weight} → {best_pair_weight}")
        print(f"  Row span: {row_span(best_pair_vec)}, Col span: {col_span(best_pair_vec)}")
    print(f"  Time: {time.time() - t0:.1f}s")

    # (c) k-way random XOR for k=3..6
    best_overall_weight = best_pair_weight
    best_overall_vec = best_pair_vec
    rng = np.random.default_rng(42)

    for k in range(3, 7):
        print(f"\n({chr(ord('a') + k - 1)}) {k}-way random XOR (40K samples)...", flush=True)
        t0 = time.time()
        best_k_weight = best_overall_weight

        for _ in range(40000):
            indices = rng.choice(len(null_basis), size=k, replace=False)
            vec = null_basis[indices[0]].copy()
            for idx in indices[1:]:
                vec ^= null_basis[idx]
            w = popcount_packed(vec)
            if w > 0 and w < best_k_weight:
                best_k_weight = w
                best_overall_weight = w
                best_overall_vec = vec.copy()

        print(f"  Best {k}-way weight: {best_k_weight}")
        print(f"  Time: {time.time() - t0:.1f}s")

    # Final results
    print(f"\n{'='*72}")
    print(f"B.59f Results")
    print(f"{'='*72}")
    print(f"  GF(2) rank (cross-sums only): {rank}")
    print(f"  Null-space dimension: {len(null_basis)}")
    print(f"  Minimum swap size: {best_overall_weight} cells")
    print(f"  Row span: {row_span(best_overall_vec)}")
    print(f"  Col span: {col_span(best_overall_vec)}")

    # Basis weight distribution
    print(f"\n  Basis weight distribution:")
    print(f"    min={weights[0][0]}, p5={weights[len(weights)//20][0]}, "
          f"median={weights[len(weights)//2][0]}, "
          f"mean={sum(w for w,_ in weights)/len(weights):.0f}, "
          f"p95={weights[19*len(weights)//20][0]}, max={weights[-1][0]}")

    # Scaling comparison
    print(f"\n  Scaling comparison:")
    print(f"    S=511 (8 families): min swap = 1,528 cells (3.0 * S)")
    print(f"    S=127 (6 families): min swap = {best_overall_weight} cells "
          f"({best_overall_weight/S:.1f} * S)")

    if best_overall_weight <= 100:
        print(f"\n  *** Min swap ≤ 100 — Complete-Then-Verify (B.59s) may be viable ***")
    elif best_overall_weight <= S:
        print(f"\n  Min swap ≤ S — moderate; CTV requires further analysis")
    else:
        print(f"\n  Min swap > S — CTV likely infeasible (same conclusion as B.33/B.39)")

    # Save results
    result = {
        "experiment": "B.59f",
        "S": S, "N": N,
        "rank": rank,
        "null_dim": len(null_basis),
        "min_swap_weight": best_overall_weight,
        "min_swap_row_span": row_span(best_overall_vec),
        "min_swap_col_span": col_span(best_overall_vec),
        "basis_weights": {
            "min": weights[0][0],
            "p5": weights[len(weights)//20][0],
            "p25": weights[len(weights)//4][0],
            "median": weights[len(weights)//2][0],
            "mean": round(sum(w for w,_ in weights)/len(weights), 1),
            "p75": weights[3*len(weights)//4][0],
            "p95": weights[19*len(weights)//20][0],
            "max": weights[-1][0],
        },
    }
    out_path = REPO_ROOT / "tools" / "b59f_results.json"
    out_path.write_text(json.dumps(result, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
