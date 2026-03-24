#!/usr/bin/env python3
"""
B.58b: Full Symbolic Solve Pipeline at S=127

Implements the combinator architecture from B.58 spec:
  BuildSystem → Fixpoint(GaussElim, IntBound, CrossDeduce, Propagate) → EnumerateFree → VerifyBH

Tests on block 0 of useless-machine.mp4 and synthetic blocks at various densities.

Usage:
    python3 tools/b58b_symbolic_solve.py
    python3 tools/b58b_symbolic_solve.py --block 5
    python3 tools/b58b_symbolic_solve.py --density 0.3
"""

import argparse
import hashlib
import json
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
N_LINES = S + S + DIAG_COUNT + DIAG_COUNT + S + S  # 1,014

# LCG constants (match C++ LtpTable.cpp)
LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

# Default seeds (B.26c winners)
PREFIX = b"CRSCLTP"
SEED1 = int.from_bytes(PREFIX + b"V", "big")
SEED2 = int.from_bytes(PREFIX + b"P", "big")


# ---------------------------------------------------------------------------
# CSM loading
# ---------------------------------------------------------------------------
def load_csm_from_file(path, block_idx=0):
    """Load block_idx-th 127x127 CSM from a raw file."""
    data = Path(path).read_bytes()
    bits_per_block = S * S
    start_bit = block_idx * bits_per_block
    csm = np.zeros((S, S), dtype=np.uint8)

    for i in range(bits_per_block):
        src_bit = start_bit + i
        src_byte = src_bit // 8
        src_bit_in_byte = 7 - (src_bit % 8)
        if src_byte < len(data):
            val = (data[src_byte] >> src_bit_in_byte) & 1
            r = i // S
            c = i % S
            csm[r, c] = val

    density = np.sum(csm) / N
    return csm, density


def make_random_csm(density, rng=None):
    """Generate a random S x S CSM at approximately the given density."""
    if rng is None:
        rng = np.random.default_rng(42)
    csm = (rng.random((S, S)) < density).astype(np.uint8)
    actual_density = np.sum(csm) / N
    return csm, actual_density


# ---------------------------------------------------------------------------
# Cross-sum computation
# ---------------------------------------------------------------------------
def compute_cross_sums(csm):
    """Compute all 1,014 cross-sum targets from a CSM."""
    targets = np.zeros(N_LINES, dtype=np.int32)
    line_members = [[] for _ in range(N_LINES)]
    idx = 0

    # LSM (rows)
    for r in range(S):
        targets[idx] = int(np.sum(csm[r, :]))
        line_members[idx] = [r * S + c for c in range(S)]
        idx += 1

    # VSM (columns)
    for c in range(S):
        targets[idx] = int(np.sum(csm[:, c]))
        line_members[idx] = [r * S + c for r in range(S)]
        idx += 1

    # DSM (diagonals): d = c - r + (S-1)
    for d in range(DIAG_COUNT):
        offset = d - (S - 1)
        members = []
        s = 0
        for r in range(S):
            c = r + offset
            if 0 <= c < S:
                s += int(csm[r, c])
                members.append(r * S + c)
        targets[idx] = s
        line_members[idx] = members
        idx += 1

    # XSM (anti-diagonals): x = r + c
    for x in range(DIAG_COUNT):
        members = []
        s = 0
        for r in range(S):
            c = x - r
            if 0 <= c < S:
                s += int(csm[r, c])
                members.append(r * S + c)
        targets[idx] = s
        line_members[idx] = members
        idx += 1

    # yLTP1 and yLTP2
    for seed in [SEED1, SEED2]:
        membership = build_yltp_membership(seed)
        line_sums = [0] * S
        line_mems = [[] for _ in range(S)]
        for flat in range(N):
            line_idx = membership[flat]
            r = flat // S
            c = flat % S
            line_sums[line_idx] += int(csm[r, c])
            line_mems[line_idx].append(flat)
        for k in range(S):
            targets[idx] = line_sums[k]
            line_members[idx] = line_mems[k]
            idx += 1

    assert idx == N_LINES
    return targets, line_members


def build_yltp_membership(seed, s=S):
    """Fisher-Yates LCG partition."""
    n = s * s
    pool = list(range(n))
    state = seed
    for i in range(n - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        j = int(state % (i + 1))
        pool[i], pool[j] = pool[j], pool[i]
    membership = [0] * n
    for line_idx in range(s):
        for slot in range(s):
            membership[pool[line_idx * s + slot]] = line_idx
    return membership


# ---------------------------------------------------------------------------
# CRC-32
# ---------------------------------------------------------------------------
def compute_row_crc32(csm):
    """Compute CRC-32 for each row of the CSM."""
    crcs = []
    total_bytes = (S + 1 + 7) // 8  # 127 data bits + 1 pad = 128 bits = 16 bytes
    for r in range(S):
        msg = bytearray(total_bytes)
        for c in range(S):
            if csm[r, c]:
                byte_idx = c // 8
                bit_idx = 7 - (c % 8)
                msg[byte_idx] |= (1 << bit_idx)
        crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)
    return crcs


def build_crc32_generator_matrix():
    """Build 32 x 127 GF(2) generator matrix for CRC-32."""
    total_bytes = (S + 1 + 7) // 8
    zero_msg = bytes(total_bytes)
    c = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = np.array([(c >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)

    G_crc = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(total_bytes)
        byte_idx = col // 8
        bit_idx = 7 - (col % 8)
        msg[byte_idx] |= (1 << bit_idx)
        crc_one = zlib.crc32(bytes(msg)) & 0xFFFFFFFF
        col_val = crc_one ^ c
        for i in range(32):
            G_crc[i, col] = (col_val >> (31 - i)) & 1

    return G_crc, c_vec


def compute_block_hash(csm):
    """Compute SHA-256 block hash of CSM (row-major, 2 uint64 words per row)."""
    buf = bytearray(S * 16)  # 127 * 16 = 2,032 bytes
    for r in range(S):
        # Pack row into 2 uint64 words, big-endian
        word0 = 0
        word1 = 0
        for c in range(S):
            if csm[r, c]:
                if c < 64:
                    word0 |= (1 << (63 - c))
                else:
                    word1 |= (1 << (63 - (c - 64)))
        offset = r * 16
        struct.pack_into(">QQ", buf, offset, word0, word1)
    return hashlib.sha256(bytes(buf)).digest()


# ---------------------------------------------------------------------------
# GF(2) system with RREF
# ---------------------------------------------------------------------------
def build_gf2_system(targets, line_members, row_crcs, G_crc, c_vec):
    """Build the full GF(2) constraint matrix and target vector.

    Returns G (m x N uint8), b (m uint8).
    """
    n_cs = N_LINES
    n_crc = S * 32
    m = n_cs + n_crc

    G = np.zeros((m, N), dtype=np.uint8)
    b = np.zeros(m, dtype=np.uint8)

    # Cross-sum parity equations
    for i in range(n_cs):
        for j in line_members[i]:
            G[i, j] = 1
        b[i] = targets[i] % 2

    # CRC-32 equations
    for r in range(S):
        h = row_crcs[r]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target_vec = h_vec ^ c_vec  # h XOR c

        base_row = n_cs + r * 32
        base_col = r * S
        for i in range(32):
            for c in range(S):
                G[base_row + i, base_col + c] = G_crc[i, c]
            b[base_row + i] = target_vec[i]

    return G, b


def gauss_elim_rref(G, b):
    """GF(2) Gaussian elimination producing RREF.

    Returns (pivot_cols, free_cols, rank, G_rref, b_rref).
    Modifies G and b in-place.
    """
    m, n = G.shape
    pivot_cols = []
    pivot_row = 0

    for col in range(n):
        # Find pivot
        found = -1
        for r in range(pivot_row, m):
            if G[r, col]:
                found = r
                break
        if found == -1:
            continue

        # Swap rows
        if found != pivot_row:
            G[[pivot_row, found]] = G[[found, pivot_row]]
            b[pivot_row], b[found] = b[found], b[pivot_row]

        # Eliminate all other rows
        mask = G[:, col].astype(bool)
        mask[pivot_row] = False
        G[mask] ^= G[pivot_row]
        b[mask] ^= b[pivot_row]

        pivot_cols.append(col)
        pivot_row += 1

    rank = len(pivot_cols)
    free_cols = sorted(set(range(n)) - set(pivot_cols))

    return pivot_cols, free_cols, rank, G[:rank], b[:rank]


def extract_determined_from_rref(G_rref, b_rref, pivot_cols, free_cols):
    """Find cells determined by GF(2) alone (pivots with no free dependencies)."""
    free_set = set(free_cols)
    determined = {}

    for i, pcol in enumerate(pivot_cols):
        row = G_rref[i]
        has_free_dep = False
        for fc in free_cols:
            if row[fc]:
                has_free_dep = True
                break
        if not has_free_dep:
            determined[pcol] = int(b_rref[i])

    return determined


# ---------------------------------------------------------------------------
# Integer constraint state
# ---------------------------------------------------------------------------
class IntConstraints:
    def __init__(self, targets, line_members):
        self.targets = list(targets)
        self.members = [list(m) for m in line_members]
        self.rho = list(targets)  # residual = target - assigned_ones
        self.u = [len(m) for m in line_members]  # undetermined count

        # cell_lines: for each cell, list of line indices
        self.cell_lines = [[] for _ in range(N)]
        for i, members in enumerate(self.members):
            for j in members:
                self.cell_lines[j].append(i)

    def determine(self, j, v):
        """Mark cell j as determined with value v."""
        for li in self.cell_lines[j]:
            self.u[li] -= 1
            self.rho[li] -= v


# ---------------------------------------------------------------------------
# Combinators
# ---------------------------------------------------------------------------
def int_bound(ic, D, F, val):
    """IntBound combinator: rho=0 → force 0, rho=u → force 1.

    Returns (newly_determined dict, inconsistent bool).
    """
    newly = {}
    queue = list(range(N_LINES))
    in_queue = set(queue)

    while queue:
        i = queue.pop(0)
        in_queue.discard(i)

        if ic.u[i] == 0:
            continue
        if ic.rho[i] < 0 or ic.rho[i] > ic.u[i]:
            return {}, True  # inconsistent

        forced_value = None
        if ic.rho[i] == 0:
            forced_value = 0
        elif ic.rho[i] == ic.u[i]:
            forced_value = 1

        if forced_value is None:
            continue

        for j in ic.members[i]:
            if j not in F:
                continue
            newly[j] = forced_value
            F.discard(j)
            D.add(j)
            val[j] = forced_value
            ic.determine(j, forced_value)
            for li in ic.cell_lines[j]:
                if li not in in_queue:
                    queue.append(li)
                    in_queue.add(li)

    return newly, False


def cross_deduce(ic, D, F, val):
    """CrossDeduce: pairwise cross-line deduction for single-cell intersections."""
    newly = {}

    for j in list(F):
        if j in newly:
            continue
        j_lines = ic.cell_lines[j]
        forced = None

        for v in (0, 1):
            feasible = True
            for li in j_lines:
                new_rho = ic.rho[li] - v
                new_u = ic.u[li] - 1
                if new_rho < 0 or new_rho > new_u:
                    feasible = False
                    break
            if not feasible:
                if forced is not None:
                    return {}, True  # both infeasible
                forced = 1 - v

        if forced is not None:
            newly[j] = forced

    # Apply
    for j, v in newly.items():
        F.discard(j)
        D.add(j)
        val[j] = v
        ic.determine(j, v)

    return newly, False


def propagate_gf2(G, b, newly_determined):
    """Substitute determined cells into GF(2) system."""
    for j, v in newly_determined.items():
        if v == 1:
            mask = G[:, j].astype(bool)
            b[mask] ^= 1
        G[:, j] = 0


# ---------------------------------------------------------------------------
# Fixpoint
# ---------------------------------------------------------------------------
def fixpoint(G, b, ic, max_iter=200):
    """Iterate combinators to convergence.

    Returns (D, F, val, stats_per_iteration, feasible).
    """
    D = set()
    F = set(range(N))
    val = {}
    stats = []

    for iteration in range(1, max_iter + 1):
        prev_D = len(D)

        # Phase A: GF(2) Gaussian elimination
        t0 = time.time()
        pivot_cols, free_cols, rank, G_rref, b_rref = gauss_elim_rref(G.copy(), b.copy())
        t_gauss = time.time() - t0

        det_gf2 = extract_determined_from_rref(G_rref, b_rref, pivot_cols, free_cols)
        # Filter out already-determined cells
        det_gf2 = {j: v for j, v in det_gf2.items() if j in F}

        if det_gf2:
            for j, v in det_gf2.items():
                F.discard(j)
                D.add(j)
                val[j] = v
                ic.determine(j, v)
            propagate_gf2(G, b, det_gf2)

        # Phase B: IntBound
        t0 = time.time()
        det_int, inconsistent = int_bound(ic, D, F, val)
        t_int = time.time() - t0
        if inconsistent:
            print(f"  Iter {iteration}: INCONSISTENT in IntBound")
            return D, F, val, stats, False
        if det_int:
            propagate_gf2(G, b, det_int)

        # Phase C: CrossDeduce
        t0 = time.time()
        det_cross, inconsistent = cross_deduce(ic, D, F, val)
        t_cross = time.time() - t0
        if inconsistent:
            print(f"  Iter {iteration}: INCONSISTENT in CrossDeduce")
            return D, F, val, stats, False
        if det_cross:
            propagate_gf2(G, b, det_cross)

        new_D = len(D)
        gain = new_D - prev_D

        iter_stats = {
            "iteration": iteration,
            "D": new_D, "F": len(F),
            "gain": gain,
            "from_gauss": len(det_gf2),
            "from_intbound": len(det_int),
            "from_crossdeduce": len(det_cross),
            "rank": rank,
            "t_gauss": round(t_gauss, 3),
            "t_int": round(t_int, 3),
            "t_cross": round(t_cross, 3),
        }
        stats.append(iter_stats)

        pct = new_D / N * 100
        print(f"  Iter {iteration}: |D|={new_D} ({pct:.1f}%) |F|={len(F)} "
              f"[+{len(det_gf2)} GF2, +{len(det_int)} IntBound, +{len(det_cross)} CrossDeduce] "
              f"rank={rank} ({t_gauss:.1f}s)")

        if gain == 0:
            break
        if new_D == N:
            break

    return D, F, val, stats, True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.58b: Full symbolic solve pipeline at S=127")
    parser.add_argument("--block", type=int, default=0, help="Block index from MP4")
    parser.add_argument("--density", type=float, default=None, help="Use synthetic block at this density")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b58b_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"

    print("B.58b: Full Symbolic Solve Pipeline (S=127)")
    print(f"  S={S}, N={N}, lines={N_LINES}")
    print()

    # Load or generate CSM
    if args.density is not None:
        print(f"Generating synthetic block at density={args.density}...")
        csm, actual_density = make_random_csm(args.density)
        source_label = f"synthetic_{args.density}"
    else:
        print(f"Loading block {args.block} from {mp4_path.name}...")
        csm, actual_density = load_csm_from_file(mp4_path, args.block)
        source_label = f"mp4_block_{args.block}"

    popcount = int(np.sum(csm))
    print(f"  Density: {actual_density:.3f} ({popcount} ones / {N} cells)")
    print()

    # Compute projections
    print("Computing cross-sums...", flush=True)
    t0 = time.time()
    targets, line_members = compute_cross_sums(csm)
    print(f"  Done in {time.time() - t0:.2f}s")

    print("Computing CRC-32 per row...", flush=True)
    row_crcs = compute_row_crc32(csm)

    print("Computing SHA-256 block hash...", flush=True)
    bh = compute_block_hash(csm)
    print(f"  BH: {bh.hex()[:16]}...")

    print("Building CRC-32 generator matrix...", flush=True)
    G_crc, c_vec = build_crc32_generator_matrix()

    # Build GF(2) system
    print("Building GF(2) constraint matrix (5078 x 16129)...", flush=True)
    t0 = time.time()
    G, b_vec = build_gf2_system(targets, line_members, row_crcs, G_crc, c_vec)
    print(f"  Done in {time.time() - t0:.2f}s. Shape: {G.shape}")

    # Build integer constraints
    print("Building integer constraint state...", flush=True)
    ic = IntConstraints(targets, line_members)

    # Run fixpoint
    print(f"\n{'='*72}")
    print("Fixpoint iteration")
    print(f"{'='*72}")
    t_start = time.time()
    D, F, val, stats, feasible = fixpoint(G, b_vec, ic)
    t_total = time.time() - t_start

    # Results
    n_determined = len(D)
    n_free = len(F)
    pct_determined = n_determined / N * 100
    print(f"\n{'='*72}")
    print(f"B.58b Results — {source_label}")
    print(f"{'='*72}")
    print(f"  Determined:    {n_determined} / {N} ({pct_determined:.1f}%)")
    print(f"  Free:          {n_free}")
    print(f"  Feasible:      {feasible}")
    print(f"  Total time:    {t_total:.1f}s")
    print(f"  Iterations:    {len(stats)}")

    if stats:
        total_gauss = sum(s["from_gauss"] for s in stats)
        total_int = sum(s["from_intbound"] for s in stats)
        total_cross = sum(s["from_crossdeduce"] for s in stats)
        print(f"  From GaussElim:   {total_gauss}")
        print(f"  From IntBound:    {total_int}")
        print(f"  From CrossDeduce: {total_cross}")

    # Verify if fully solved
    if n_free == 0 and feasible:
        print("\n  *** FULLY SOLVED — verifying SHA-256... ***")
        # Reconstruct CSM
        recon = np.zeros((S, S), dtype=np.uint8)
        for j, v in val.items():
            recon[j // S, j % S] = v
        recon_bh = compute_block_hash(recon)
        if recon_bh == bh:
            print("  SHA-256 VERIFIED — perfect reconstruction!")
            match_original = np.array_equal(recon, csm)
            print(f"  Matches original CSM: {match_original}")
        else:
            print("  SHA-256 MISMATCH — reconstruction error!")
    elif n_free <= 25 and feasible:
        print(f"\n  {n_free} free variables — exhaustive search feasible (2^{n_free} = {2**n_free})")
    elif n_free <= 1000 and feasible:
        print(f"\n  {n_free} free variables — row-grouped search needed (B.58c)")
    else:
        print(f"\n  {n_free} free variables — approach insufficient at this density")

    # Outcome classification
    if n_free == 0:
        outcome = "FULL_SOLVE"
    elif n_free <= 25:
        outcome = "TRACTABLE_SEARCH"
    elif n_free <= 1000:
        outcome = "ROW_GROUPED_SEARCH"
    else:
        outcome = "INSUFFICIENT"

    print(f"\n  Outcome: {outcome}")

    # Save results
    result = {
        "experiment": "B.58b",
        "source": source_label,
        "density": round(actual_density, 4),
        "popcount": popcount,
        "n_determined": n_determined,
        "n_free": n_free,
        "pct_determined": round(pct_determined, 2),
        "feasible": feasible,
        "total_time_s": round(t_total, 2),
        "iterations": len(stats),
        "outcome": outcome,
        "stats": stats,
    }
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(result, indent=2))
    print(f"  Results: {out_path}")


if __name__ == "__main__":
    main()
