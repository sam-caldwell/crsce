#!/usr/bin/env python3
"""
B.60c: Combinator fixpoint with toroidal DSM/XSM + LH + VH.

Replaces non-toroidal diagonals (253+253 = 506 lines, length 1-127) with
toroidal diagonals (127+127 = 254 lines, all length 127).

tDSM slope=1:   k = (c - r) mod 127
tXSM slope=126: k = (c + r) mod 127  (126 ≡ -1 mod 127)

Runs: GaussElim + IntBound + CrossDeduce + Propagate fixpoint.
Compares to B.60b baseline (non-toroidal, |D|=3,600).

Usage:
    python3 tools/b60c_toroidal_baseline.py --block 0
"""

import argparse
import hashlib
import json
import struct
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
DIAG_COUNT_TOROIDAL = S  # 127 toroidal lines per slope
N_GEO_LINES = S + S + DIAG_COUNT_TOROIDAL + DIAG_COUNT_TOROIDAL  # 508


# ---------------------------------------------------------------------------
# CSM loading & CRC-32 (reused from b60b)
# ---------------------------------------------------------------------------
def load_csm(path, block_idx=0):
    data = Path(path).read_bytes()
    csm = np.zeros((S, S), dtype=np.int8)
    start_bit = block_idx * N
    for i in range(N):
        src_bit = start_bit + i
        src_byte = src_bit // 8
        if src_byte < len(data):
            csm[i // S, i % S] = (data[src_byte] >> (7 - (src_bit % 8))) & 1
    return csm


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


def compute_crc32(bits):
    total_bytes = (S + 1 + 7) // 8
    msg = bytearray(total_bytes)
    for k in range(S):
        if bits[k]:
            msg[k // 8] |= (1 << (7 - (k % 8)))
    return zlib.crc32(bytes(msg)) & 0xFFFFFFFF


def compute_block_hash(csm):
    buf = bytearray(S * 16)
    for r in range(S):
        w0, w1 = 0, 0
        for c in range(S):
            if csm[r, c]:
                if c < 64:
                    w0 |= (1 << (63 - c))
                else:
                    w1 |= (1 << (63 - (c - 64)))
        struct.pack_into(">QQ", buf, r * 16, w0, w1)
    return hashlib.sha256(bytes(buf)).digest()


# ---------------------------------------------------------------------------
# Toroidal geometric cross-sums
# ---------------------------------------------------------------------------
def compute_toroidal_sums(csm):
    """Compute 508 toroidal geometric cross-sum targets and line memberships.

    Lines 0..126:   LSM (rows)
    Lines 127..253: VSM (columns)
    Lines 254..380: tDSM slope=1, k = (c - r) mod 127
    Lines 381..507: tXSM slope=126, k = (c + r) mod 127
    """
    targets = np.zeros(N_GEO_LINES, dtype=np.int32)
    members = [[] for _ in range(N_GEO_LINES)]
    idx = 0

    # LSM
    for r in range(S):
        targets[idx] = int(np.sum(csm[r, :]))
        members[idx] = [(r, c) for c in range(S)]
        idx += 1

    # VSM
    for c in range(S):
        targets[idx] = int(np.sum(csm[:, c]))
        members[idx] = [(r, c) for r in range(S)]
        idx += 1

    # tDSM: slope=1, k = (c - r) mod S
    for k in range(S):
        mems = []
        s = 0
        for r in range(S):
            c = (k + r) % S
            s += int(csm[r, c])
            mems.append((r, c))
        targets[idx] = s
        members[idx] = mems
        idx += 1

    # tXSM: slope=126 (≡ -1 mod 127), k = (c + r) mod S
    for k in range(S):
        mems = []
        s = 0
        for r in range(S):
            c = (k - r) % S
            if c < 0:
                c += S
            s += int(csm[r, c])
            mems.append((r, c))
        targets[idx] = s
        members[idx] = mems
        idx += 1

    assert idx == N_GEO_LINES

    # Validate: every toroidal line should have exactly S cells
    for li in range(2 * S, N_GEO_LINES):
        assert len(members[li]) == S, f"Line {li} has {len(members[li])} cells, expected {S}"

    return targets, members


# ---------------------------------------------------------------------------
# GF(2) system (toroidal geometric + LH + VH)
# ---------------------------------------------------------------------------
def build_gf2_system(geo_targets, geo_members, row_crcs, col_crcs, G_crc, c_vec):
    n_geo = N_GEO_LINES       # 508
    n_lh = S * 32              # 4,064
    n_vh = S * 32              # 4,064
    m = n_geo + n_lh + n_vh    # 8,636

    G = np.zeros((m, N), dtype=np.uint8)
    b = np.zeros(m, dtype=np.uint8)

    # Geometric parity
    for i in range(n_geo):
        for (r, c) in geo_members[i]:
            G[i, r * S + c] = 1
        b[i] = geo_targets[i] % 2

    # LH
    for r in range(S):
        h = row_crcs[r]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target_vec = h_vec ^ c_vec
        base_row = n_geo + r * 32
        base_col = r * S
        for i in range(32):
            for c in range(S):
                G[base_row + i, base_col + c] = G_crc[i, c]
            b[base_row + i] = target_vec[i]

    # VH
    for c in range(S):
        h = col_crcs[c]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target_vec = h_vec ^ c_vec
        base_row = n_geo + n_lh + c * 32
        for i in range(32):
            for r in range(S):
                G[base_row + i, r * S + c] = G_crc[i, r]
            b[base_row + i] = target_vec[i]

    return G, b


# ---------------------------------------------------------------------------
# GF(2) RREF
# ---------------------------------------------------------------------------
def gauss_elim_rref(G, b):
    m, n = G.shape
    pivot_cols = []
    pivot_row = 0
    for col in range(n):
        found = -1
        for r in range(pivot_row, m):
            if G[r, col]:
                found = r
                break
        if found == -1:
            continue
        if found != pivot_row:
            G[[pivot_row, found]] = G[[found, pivot_row]]
            b[pivot_row], b[found] = b[found], b[pivot_row]
        mask = G[:, col].astype(bool)
        mask[pivot_row] = False
        G[mask] ^= G[pivot_row]
        b[mask] ^= b[pivot_row]
        pivot_cols.append(col)
        pivot_row += 1
    rank = len(pivot_cols)
    free_cols = sorted(set(range(n)) - set(pivot_cols))
    return pivot_cols, free_cols, rank, G[:rank], b[:rank]


def extract_determined(G_rref, b_rref, pivot_cols, free_cols):
    determined = {}
    for i, pcol in enumerate(pivot_cols):
        has_free = False
        for fc in free_cols:
            if G_rref[i, fc]:
                has_free = True
                break
        if not has_free:
            determined[pcol] = int(b_rref[i])
    return determined


# ---------------------------------------------------------------------------
# IntBound + CrossDeduce
# ---------------------------------------------------------------------------
class IntConstraints:
    def __init__(self, targets, members):
        self.targets = list(targets)
        self.members = [list(m) for m in members]
        self.rho = list(targets)
        self.u = [len(m) for m in members]
        self.cell_lines = [[] for _ in range(N)]
        for i, mems in enumerate(self.members):
            for (r, c) in mems:
                self.cell_lines[r * S + c].append(i)

    def determine(self, j, v):
        for li in self.cell_lines[j]:
            self.u[li] -= 1
            self.rho[li] -= v


def int_bound(ic, D, F, val):
    newly = {}
    queue = list(range(N_GEO_LINES))
    in_queue = set(queue)
    while queue:
        i = queue.pop(0)
        in_queue.discard(i)
        if ic.u[i] == 0:
            continue
        if ic.rho[i] < 0 or ic.rho[i] > ic.u[i]:
            return {}, True
        fv = None
        if ic.rho[i] == 0:
            fv = 0
        elif ic.rho[i] == ic.u[i]:
            fv = 1
        if fv is None:
            continue
        for (r, c) in ic.members[i]:
            j = r * S + c
            if j not in F:
                continue
            newly[j] = fv
            F.discard(j)
            D.add(j)
            val[j] = fv
            ic.determine(j, fv)
            for li in ic.cell_lines[j]:
                if li not in in_queue:
                    queue.append(li)
                    in_queue.add(li)
    return newly, False


def cross_deduce(ic, D, F, val):
    newly = {}
    for j in list(F):
        if j in newly:
            continue
        forced = None
        for v in (0, 1):
            feasible = True
            for li in ic.cell_lines[j]:
                new_rho = ic.rho[li] - v
                new_u = ic.u[li] - 1
                if new_rho < 0 or new_rho > new_u:
                    feasible = False
                    break
            if not feasible:
                if forced is not None:
                    return {}, True
                forced = 1 - v
        if forced is not None:
            newly[j] = forced
    for j, v in newly.items():
        F.discard(j)
        D.add(j)
        val[j] = v
        ic.determine(j, v)
    return newly, False


def propagate_gf2(G, b, det):
    for j, v in det.items():
        if v == 1:
            mask = G[:, j].astype(bool)
            b[mask] ^= 1
        G[:, j] = 0


# ---------------------------------------------------------------------------
# Fixpoint
# ---------------------------------------------------------------------------
def fixpoint(G, b, ic, max_iter=200):
    D = set()
    F = set(range(N))
    val = {}
    stats = []

    for iteration in range(1, max_iter + 1):
        prev_D = len(D)

        t0 = time.time()
        pivot_cols, free_cols, rank, G_rref, b_rref = gauss_elim_rref(G.copy(), b.copy())
        t_gauss = time.time() - t0

        det_gf2 = extract_determined(G_rref, b_rref, pivot_cols, free_cols)
        det_gf2 = {j: v for j, v in det_gf2.items() if j in F}
        if det_gf2:
            for j, v in det_gf2.items():
                F.discard(j); D.add(j); val[j] = v; ic.determine(j, v)
            propagate_gf2(G, b, det_gf2)

        t0 = time.time()
        det_int, incon = int_bound(ic, D, F, val)
        t_int = time.time() - t0
        if incon:
            print(f"  Iter {iteration}: INCONSISTENT in IntBound")
            return D, F, val, stats, False
        if det_int:
            propagate_gf2(G, b, det_int)

        t0 = time.time()
        det_cross, incon = cross_deduce(ic, D, F, val)
        t_cross = time.time() - t0
        if incon:
            print(f"  Iter {iteration}: INCONSISTENT in CrossDeduce")
            return D, F, val, stats, False
        if det_cross:
            propagate_gf2(G, b, det_cross)

        new_D = len(D)
        gain = new_D - prev_D
        pct = new_D / N * 100

        s = {
            "iteration": iteration, "D": new_D, "F": len(F), "gain": gain,
            "from_gauss": len(det_gf2), "from_intbound": len(det_int),
            "from_crossdeduce": len(det_cross), "rank": rank,
            "t_gauss": round(t_gauss, 3), "t_int": round(t_int, 3),
            "t_cross": round(t_cross, 3),
        }
        stats.append(s)

        print(f"  Iter {iteration}: |D|={new_D} ({pct:.1f}%) |F|={len(F)} "
              f"[+{len(det_gf2)} GF2, +{len(det_int)} IB, +{len(det_cross)} CD] "
              f"rank={rank} ({t_gauss:.1f}s)")

        if gain == 0 or new_D == N:
            break

    return D, F, val, stats, True


# ---------------------------------------------------------------------------
# Stratified rank (for B.60d data)
# ---------------------------------------------------------------------------
def stratified_rank(G_geo, G_lh, G_vh):
    """Measure rank at each stratum."""
    print("  Stratified rank:")
    configs = [
        ("LSM only", G_geo[:S]),
        ("LSM + VSM", G_geo[:2 * S]),
        ("4 geometric (toroidal)", G_geo),
        ("+ LH", np.vstack([G_geo, G_lh])),
        ("+ VH (full system)", np.vstack([G_geo, G_lh, G_vh])),
    ]
    results = {}
    for label, G_sub in configs:
        m, n = G_sub.shape
        # Quick rank via RREF
        _, _, rank, _, _ = gauss_elim_rref(G_sub.copy(), np.zeros(m, dtype=np.uint8))
        null_dim = N - rank
        density = rank / N
        results[label] = {"rank": rank, "null_dim": null_dim, "density": round(density, 4)}
        print(f"    {label:40s} rank={rank:5d}  null={null_dim:5d}  density={density:.4f}")
    return results


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.60c: Toroidal baseline")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60c_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    csm = load_csm(mp4_path, args.block)
    density = float(np.sum(csm)) / N

    print(f"B.60c: Toroidal Diagonal Baseline (S={S})")
    print(f"  Block: {args.block}, density: {density:.3f}")
    print(f"  Geometric lines: {N_GEO_LINES} (127 LSM + 127 VSM + 127 tDSM + 127 tXSM)")
    print(f"  All lines uniform-length {S}")
    print(f"  GF(2) equations: {N_GEO_LINES} + {S*32} LH + {S*32} VH = {N_GEO_LINES + 2*S*32}")
    print()

    G_crc, c_vec = build_crc32_gen()
    row_crcs = [compute_crc32(csm[r]) for r in range(S)]
    col_crcs = [compute_crc32(csm[:, c]) for c in range(S)]
    bh = compute_block_hash(csm)

    # Compute toroidal cross-sums
    print("Computing toroidal cross-sums...", flush=True)
    geo_targets, geo_members = compute_toroidal_sums(csm)

    # Build GF(2) system
    print("Building GF(2) system...", flush=True)
    t0 = time.time()
    G, b_vec = build_gf2_system(geo_targets, geo_members, row_crcs, col_crcs, G_crc, c_vec)
    print(f"  Shape: {G.shape} ({time.time() - t0:.1f}s)")

    # Build separate matrices for stratified rank
    G_geo = G[:N_GEO_LINES].copy()
    G_lh = G[N_GEO_LINES:N_GEO_LINES + S*32].copy()
    G_vh = G[N_GEO_LINES + S*32:].copy()

    # B.60d: Stratified rank
    print()
    rank_results = stratified_rank(G_geo, G_lh, G_vh)

    # B.60c/e: Combinator fixpoint
    print()
    ic = IntConstraints(geo_targets, geo_members)

    print(f"{'=' * 60}")
    print("Combinator fixpoint")
    print(f"{'=' * 60}")
    t_start = time.time()
    D, F, val, stats, feasible = fixpoint(G, b_vec, ic)
    t_total = time.time() - t_start

    n_det = len(D)
    n_free = len(F)
    pct = n_det / N * 100

    print(f"\nResults:")
    print(f"  Determined: {n_det} / {N} ({pct:.1f}%)")
    print(f"  Free:       {n_free}")
    print(f"  Feasible:   {feasible}")
    print(f"  Time:       {t_total:.1f}s")

    if stats:
        total_gauss = sum(s["from_gauss"] for s in stats)
        total_int = sum(s["from_intbound"] for s in stats)
        total_cross = sum(s["from_crossdeduce"] for s in stats)
        print(f"  GaussElim:  {total_gauss}")
        print(f"  IntBound:   {total_int}")
        print(f"  CrossDeduce:{total_cross}")

    # Verify correctness
    correct = all(val[j] == csm[j // S, j % S] for j in D)
    print(f"  Values correct: {correct}")

    # Comparison to B.60b
    print(f"\nComparison to B.60b (non-toroidal):")
    print(f"  B.60b: |D|=3,600, rank=7,793, geo_lines=760")
    print(f"  B.60c: |D|={n_det}, rank={rank_results.get('+ VH (full system)', {}).get('rank', '?')}, geo_lines={N_GEO_LINES}")

    full_rank = rank_results.get("+ VH (full system)", {}).get("rank", 0)
    delta_d = n_det - 3600
    delta_r = full_rank - 7793
    print(f"  Delta |D|: {delta_d:+d}")
    print(f"  Delta rank: {delta_r:+d}")

    # Save
    result = {
        "experiment": "B.60c",
        "block": args.block,
        "density": round(density, 4),
        "toroidal": True,
        "geo_lines": N_GEO_LINES,
        "total_gf2_equations": N_GEO_LINES + 2 * S * 32,
        "n_determined": n_det,
        "n_free": n_free,
        "pct_determined": round(pct, 2),
        "feasible": feasible,
        "values_correct": correct,
        "total_time_s": round(t_total, 2),
        "rank_stratified": rank_results,
        "fixpoint_stats": stats,
    }
    Path(args.out).write_text(json.dumps(result, indent=2))
    print(f"\nResults: {args.out}")


if __name__ == "__main__":
    main()
