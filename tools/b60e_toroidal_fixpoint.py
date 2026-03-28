#!/usr/bin/env python3
"""
B.60e: Combinator Fixpoint with Toroidal Diagonals + LH + VH at S=127.

B.60b repeated on the B.60c toroidal constraint geometry.
Runs: GaussElim + IntBound + CrossDeduce + Propagate fixpoint.

Usage:
    python3 tools/b60e_toroidal_fixpoint.py --block 0
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
N_GEO = S + S + S + S  # 508


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


def compute_toroidal_sums(csm):
    """Compute 508 toroidal geometric cross-sum targets and members."""
    targets = np.zeros(N_GEO, dtype=np.int32)
    members = [[] for _ in range(N_GEO)]
    idx = 0

    for r in range(S):
        targets[idx] = int(np.sum(csm[r, :]))
        members[idx] = [(r, c) for c in range(S)]
        idx += 1
    for c in range(S):
        targets[idx] = int(np.sum(csm[:, c]))
        members[idx] = [(r, c) for r in range(S)]
        idx += 1
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

    assert idx == N_GEO
    return targets, members


def build_gf2_system(geo_targets, geo_members, row_crcs, col_crcs, G_crc, c_vec):
    n_geo = N_GEO
    n_lh = S * 32
    n_vh = S * 32
    m = n_geo + n_lh + n_vh

    G = np.zeros((m, N), dtype=np.uint8)
    b = np.zeros(m, dtype=np.uint8)

    for i in range(n_geo):
        for (r, c) in geo_members[i]:
            G[i, r * S + c] = 1
        b[i] = geo_targets[i] % 2

    for r in range(S):
        h = row_crcs[r]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        tv = h_vec ^ c_vec
        base = n_geo + r * 32
        for i in range(32):
            for c in range(S):
                G[base + i, r * S + c] = G_crc[i, c]
            b[base + i] = tv[i]

    for c in range(S):
        h = col_crcs[c]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        tv = h_vec ^ c_vec
        base = n_geo + n_lh + c * 32
        for i in range(32):
            for r in range(S):
                G[base + i, r * S + c] = G_crc[i, r]
            b[base + i] = tv[i]

    return G, b


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
    queue = list(range(N_GEO))
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


def fixpoint(G, b, ic, max_iter=200):
    D = set()
    F = set(range(N))
    val = {}
    stats = []

    for iteration in range(1, max_iter + 1):
        prev_D = len(D)

        t0 = time.time()
        pc, fc, rank, Gr, br = gauss_elim_rref(G.copy(), b.copy())
        t_gauss = time.time() - t0

        det_gf2 = extract_determined(Gr, br, pc, fc)
        det_gf2 = {j: v for j, v in det_gf2.items() if j in F}
        if det_gf2:
            for j, v in det_gf2.items():
                F.discard(j); D.add(j); val[j] = v; ic.determine(j, v)
            propagate_gf2(G, b, det_gf2)

        t0 = time.time()
        det_int, incon = int_bound(ic, D, F, val)
        t_int = time.time() - t0
        if incon:
            return D, F, val, stats, False
        if det_int:
            propagate_gf2(G, b, det_int)

        t0 = time.time()
        det_cross, incon = cross_deduce(ic, D, F, val)
        t_cross = time.time() - t0
        if incon:
            return D, F, val, stats, False
        if det_cross:
            propagate_gf2(G, b, det_cross)

        new_D = len(D)
        gain = new_D - prev_D

        stats.append({
            "iteration": iteration, "D": new_D, "F": len(F), "gain": gain,
            "from_gauss": len(det_gf2), "from_intbound": len(det_int),
            "from_crossdeduce": len(det_cross), "rank": rank,
        })

        pct = new_D / N * 100
        print(f"  Iter {iteration}: |D|={new_D} ({pct:.1f}%) |F|={len(F)} "
              f"[+{len(det_gf2)} GF2, +{len(det_int)} IB, +{len(det_cross)} CD] rank={rank}")

        if gain == 0 or new_D == N:
            break

    return D, F, val, stats, True


def main():
    parser = argparse.ArgumentParser(description="B.60e: Toroidal fixpoint")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60e_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    csm = load_csm(mp4_path, args.block)
    density = float(np.sum(csm)) / N

    print(f"B.60e: Combinator Fixpoint with Toroidal Diags (S={S})")
    print(f"  Block: {args.block}, density: {density:.3f}")
    print(f"  Geometric lines: {N_GEO} (all uniform-length {S})")
    print()

    G_crc, c_vec = build_crc32_gen()
    row_crcs = [compute_crc32(csm[r]) for r in range(S)]
    col_crcs = [compute_crc32(csm[:, c]) for c in range(S)]
    bh = compute_block_hash(csm)

    geo_targets, geo_members = compute_toroidal_sums(csm)

    print("Building GF(2) system...", flush=True)
    G, b_vec = build_gf2_system(geo_targets, geo_members, row_crcs, col_crcs, G_crc, c_vec)
    print(f"  Shape: {G.shape}")

    ic = IntConstraints(geo_targets, geo_members)

    print(f"\n{'=' * 60}")
    print("Fixpoint")
    print(f"{'=' * 60}")
    t0 = time.time()
    D, F, val, stats, feasible = fixpoint(G, b_vec, ic)
    t_total = time.time() - t0

    n_det = len(D)
    n_free = len(F)
    pct = n_det / N * 100

    total_gauss = sum(s["from_gauss"] for s in stats)
    total_int = sum(s["from_intbound"] for s in stats)
    total_cross = sum(s["from_crossdeduce"] for s in stats)

    correct = all(val[j] == csm[j // S, j % S] for j in D)

    print(f"\nResults:")
    print(f"  Determined:  {n_det} / {N} ({pct:.1f}%)")
    print(f"  Free:        {n_free}")
    print(f"  GaussElim:   {total_gauss}")
    print(f"  IntBound:    {total_int}")
    print(f"  CrossDeduce: {total_cross}")
    print(f"  Correct:     {correct}")
    print(f"  Time:        {t_total:.1f}s")
    print()
    print(f"Comparison to B.60b (non-toroidal):")
    print(f"  B.60b: |D|=3,600 (GF2=4, IB=3,596, CD=0)")
    print(f"  B.60e: |D|={n_det} (GF2={total_gauss}, IB={total_int}, CD={total_cross})")
    print(f"  Delta |D|: {n_det - 3600:+d}")

    if n_det > 3600:
        outcome = "H1 (More cells)"
    elif n_det == 3600:
        outcome = "H2 (Same cells)"
    else:
        outcome = "H3 (Fewer cells)"
    print(f"  Outcome: {outcome}")

    result = {
        "experiment": "B.60e",
        "block": args.block,
        "density": round(density, 4),
        "n_determined": n_det,
        "n_free": n_free,
        "from_gauss": total_gauss,
        "from_intbound": total_int,
        "from_crossdeduce": total_cross,
        "correct": correct,
        "time_s": round(t_total, 2),
        "outcome": outcome,
        "stats": stats,
    }
    Path(args.out).write_text(json.dumps(result, indent=2))
    print(f"\n  Results: {args.out}")


if __name__ == "__main__":
    main()
