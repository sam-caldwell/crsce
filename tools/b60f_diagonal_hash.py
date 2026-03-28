#!/usr/bin/env python3
"""
B.60f: Diagonal Hash (DH) — CRC-32 per DSM Diagonal.

Replaces VH (column CRC-32) with DH (diagonal CRC-32). Each of the 253
non-toroidal DSM diagonals gets a CRC-32 digest. Short diagonals (length
1-10) have CRC rank = length, fully determining all cells on that diagonal.

Runs: stratified GF(2) rank + combinator fixpoint.
Compares to B.60b (LH + VH, |D|=3,600).

Usage:
    python3 tools/b60f_diagonal_hash.py --block 0
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
DIAG_COUNT = 2 * S - 1  # 253
N_GEO = S + S + DIAG_COUNT + DIAG_COUNT  # 760


def load_csm(path, block_idx=0):
    data = Path(path).read_bytes()
    csm = np.zeros((S, S), dtype=np.int8)
    start_bit = block_idx * N
    for i in range(N):
        sb = start_bit + i
        by = sb // 8
        if by < len(data):
            csm[i // S, i % S] = (data[by] >> (7 - (sb % 8))) & 1
    return csm


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
# CRC-32 for variable-length messages
# ---------------------------------------------------------------------------
def crc32_bits(bits):
    """CRC-32 of a bit vector: len(bits) data bits + 1 zero pad bit, padded to bytes."""
    n = len(bits)
    total_bits = n + 1  # data + 1 pad
    total_bytes = (total_bits + 7) // 8
    msg = bytearray(total_bytes)
    for k in range(n):
        if bits[k]:
            msg[k // 8] |= (1 << (7 - (k % 8)))
    return zlib.crc32(bytes(msg)) & 0xFFFFFFFF


def build_crc32_gen_for_length(length):
    """Build 32 x length GF(2) generator matrix for CRC-32 of a message with `length` data bits.

    Message format: `length` data bits + 1 zero pad bit, padded to ceil((length+1)/8) bytes.
    Returns (G_crc, c_vec) where G_crc is 32 x length and c_vec is the affine constant.
    """
    total_bits = length + 1
    total_bytes = (total_bits + 7) // 8

    zero_msg = bytes(total_bytes)
    c0 = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = np.array([(c0 >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)

    G = np.zeros((32, length), dtype=np.uint8)
    for col in range(length):
        msg = bytearray(total_bytes)
        msg[col // 8] |= (1 << (7 - (col % 8)))
        crc_one = zlib.crc32(bytes(msg)) & 0xFFFFFFFF
        col_val = crc_one ^ c0
        for i in range(32):
            G[i, col] = (col_val >> (31 - i)) & 1

    return G, c_vec


# Row LH generator (length 127, same as B.60a/b)
def build_row_crc32_gen():
    return build_crc32_gen_for_length(S)


# ---------------------------------------------------------------------------
# Diagonal geometry
# ---------------------------------------------------------------------------
def get_diag_cells(d):
    """Return list of (r, c) for DSM diagonal d. d = c - r + (S-1)."""
    offset = d - (S - 1)
    cells = []
    for r in range(S):
        c = r + offset
        if 0 <= c < S:
            cells.append((r, c))
    return cells


def diag_length(d):
    return len(get_diag_cells(d))


# ---------------------------------------------------------------------------
# Geometric cross-sums (non-toroidal, same as B.60b)
# ---------------------------------------------------------------------------
def compute_geometric_sums(csm):
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
    for d in range(DIAG_COUNT):
        offset = d - (S - 1)
        mems = []
        s = 0
        for r in range(S):
            c = r + offset
            if 0 <= c < S:
                s += int(csm[r, c])
                mems.append((r, c))
        targets[idx] = s
        members[idx] = mems
        idx += 1
    for x in range(DIAG_COUNT):
        mems = []
        s = 0
        for r in range(S):
            c = x - r
            if 0 <= c < S:
                s += int(csm[r, c])
                mems.append((r, c))
        targets[idx] = s
        members[idx] = mems
        idx += 1

    assert idx == N_GEO
    return targets, members


# ---------------------------------------------------------------------------
# GF(2) system: geometric + LH + DH
# ---------------------------------------------------------------------------
def build_gf2_system(csm, geo_targets, geo_members, row_crcs, G_row_crc, c_row_vec):
    """Build GF(2) system with geometric parity + LH + DH.

    DH: for each DSM diagonal d, 32 CRC-32 equations on that diagonal's cells.
    """
    n_geo = N_GEO  # 760
    n_lh = S * 32  # 4,064
    n_dh = DIAG_COUNT * 32  # 253 * 32 = 8,096
    m = n_geo + n_lh + n_dh  # 12,920

    G = np.zeros((m, N), dtype=np.uint8)
    b = np.zeros(m, dtype=np.uint8)

    # Geometric parity
    for i in range(n_geo):
        for (r, c) in geo_members[i]:
            G[i, r * S + c] = 1
        b[i] = geo_targets[i] % 2

    # LH (row CRC-32)
    for r in range(S):
        h = row_crcs[r]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        tv = h_vec ^ c_row_vec
        base = n_geo + r * 32
        for i in range(32):
            for c in range(S):
                G[base + i, r * S + c] = G_row_crc[i, c]
            b[base + i] = tv[i]

    # DH (diagonal CRC-32)
    # Cache generator matrices by length to avoid recomputation
    gen_cache = {}
    base_dh = n_geo + n_lh

    for d in range(DIAG_COUNT):
        cells = get_diag_cells(d)
        dlen = len(cells)

        if dlen not in gen_cache:
            gen_cache[dlen] = build_crc32_gen_for_length(dlen)
        G_diag, c_diag_vec = gen_cache[dlen]

        # Compute diagonal CRC-32
        diag_bits = [int(csm[r, c]) for (r, c) in cells]
        diag_crc = crc32_bits(diag_bits)

        h_vec = np.array([(diag_crc >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        tv = h_vec ^ c_diag_vec

        base = base_dh + d * 32
        for i in range(32):
            for j, (r, c) in enumerate(cells):
                G[base + i, r * S + c] = G_diag[i, j]
            b[base + i] = tv[i]

    return G, b, n_geo, n_lh, n_dh


# ---------------------------------------------------------------------------
# GF(2) RREF and combinators (same as B.60b)
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
            "t_gauss": round(t_gauss, 3),
        })

        pct = new_D / N * 100
        print(f"  Iter {iteration}: |D|={new_D} ({pct:.1f}%) |F|={len(F)} "
              f"[+{len(det_gf2)} GF2, +{len(det_int)} IB, +{len(det_cross)} CD] "
              f"rank={rank} ({t_gauss:.1f}s)")

        if gain == 0 or new_D == N:
            break

    return D, F, val, stats, True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.60f: Diagonal Hash (DH)")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60f_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    csm = load_csm(mp4_path, args.block)
    density = float(np.sum(csm)) / N

    print(f"B.60f: Diagonal Hash (DH) — CRC-32 per DSM Diagonal (S={S})")
    print(f"  Block: {args.block}, density: {density:.3f}")
    print(f"  Geometric lines: {N_GEO} (non-toroidal)")
    print(f"  LH: {S} rows x 32 = {S * 32} equations")
    print(f"  DH: {DIAG_COUNT} diags x 32 = {DIAG_COUNT * 32} equations")
    print(f"  Total GF(2): {N_GEO + S * 32 + DIAG_COUNT * 32}")
    print()

    # Diagonal length distribution
    lengths = [diag_length(d) for d in range(DIAG_COUNT)]
    short = sum(1 for l in lengths if l <= 32)
    print(f"  Diagonal lengths: min={min(lengths)}, max={max(lengths)}")
    print(f"  Short diags (length <= 32): {short} / {DIAG_COUNT}")
    print(f"  Short diags fully determined by CRC-32: {short}")
    print()

    G_row_crc, c_row_vec = build_row_crc32_gen()
    row_crcs = [crc32_bits(list(csm[r])) for r in range(S)]

    geo_targets, geo_members = compute_geometric_sums(csm)

    # Build GF(2) system
    print("Building GF(2) system (geo + LH + DH)...", flush=True)
    t0 = time.time()
    G, b_vec, n_geo, n_lh, n_dh = build_gf2_system(
        csm, geo_targets, geo_members, row_crcs, G_row_crc, c_row_vec)
    print(f"  Shape: {G.shape} ({time.time() - t0:.1f}s)")

    # Stratified rank
    print("\nStratified rank:")
    G_geo = G[:n_geo]
    G_lh = G[n_geo:n_geo + n_lh]
    G_dh = G[n_geo + n_lh:]

    for label, G_sub in [
        ("4 geometric (non-toroidal)", G_geo),
        ("+ LH", np.vstack([G_geo, G_lh])),
        ("+ DH (full system)", np.vstack([G_geo, G_lh, G_dh])),
    ]:
        t0 = time.time()
        _, _, rank, _, _ = gauss_elim_rref(G_sub.copy(), np.zeros(G_sub.shape[0], dtype=np.uint8))
        elapsed = time.time() - t0
        print(f"  {label:40s} rank={rank:5d}  null={N - rank:5d}  density={rank/N:.4f}  ({elapsed:.1f}s)")

    # Full rank for comparison
    _, _, full_rank, _, _ = gauss_elim_rref(G.copy(), np.zeros(G.shape[0], dtype=np.uint8))

    # Fixpoint
    ic = IntConstraints(geo_targets, geo_members)

    print(f"\n{'=' * 60}")
    print("Combinator fixpoint")
    print(f"{'=' * 60}")
    t_start = time.time()
    D, F, val, stats, feasible = fixpoint(G, b_vec, ic)
    t_total = time.time() - t_start

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
    print(f"  GF(2) rank:  {full_rank}")
    print(f"  Time:        {t_total:.1f}s")

    print(f"\nComparison:")
    print(f"  B.60b (LH + VH): |D|=3,600, rank=7,793")
    print(f"  B.60f (LH + DH): |D|={n_det}, rank={full_rank}")
    print(f"  Delta |D|: {n_det - 3600:+d}")
    print(f"  Delta rank: {full_rank - 7793:+d}")

    if n_det > 3600:
        outcome = "H1 (More cells)"
    elif n_det == 3600:
        outcome = "H2 (Same cells)"
    else:
        outcome = "H3 (Fewer cells)"
    print(f"  Outcome: {outcome}")

    result = {
        "experiment": "B.60f",
        "block": args.block,
        "density": round(density, 4),
        "n_determined": n_det,
        "n_free": n_free,
        "from_gauss": total_gauss,
        "from_intbound": total_int,
        "from_crossdeduce": total_cross,
        "correct": correct,
        "full_rank": full_rank,
        "time_s": round(t_total, 2),
        "outcome": outcome,
        "stats": stats,
    }
    Path(args.out).write_text(json.dumps(result, indent=2))
    print(f"\n  Results: {args.out}")


if __name__ == "__main__":
    main()
