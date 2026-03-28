#!/usr/bin/env python3
"""
B.59g: Algebraic Solve — CRC-32 + Cross-Sum GF(2) Combinator Pipeline.

Full algebraic representation and solution of the CRSCE constraint system:
  - 1,014 cross-sum parity equations (GF(2))
  - 4,064 CRC-32 equations (127 rows × 32 bits, GF(2))
  - 1,014 integer cardinality constraints (rho=0/rho=u forcing)
  - Cross-deduction (joint multi-line feasibility)

Fixpoint loop: GaussElim → IntBound → CrossDeduce → Propagate → repeat.

Uses packed uint64 GF(2) arithmetic for performance.

Usage:
    python3 tools/b59g_algebraic_solve.py                         # MP4 block 0, 2 yLTPs
    python3 tools/b59g_algebraic_solve.py --table tools/b59c_table.bin   # 2 rLTPs
    python3 tools/b59g_algebraic_solve.py --blocks 20             # first 20 blocks
"""

import argparse
import collections
import hashlib
import json
import struct
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S  # 16,129
DIAG_COUNT = 2 * S - 1  # 253
N_LINES = S + S + DIAG_COUNT + DIAG_COUNT + S + S  # 1,014
WORDS = (N + 63) // 64  # 253 uint64 words per GF(2) row

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64


# ── LTP membership ──────────────────────────────────────────────────────────

def build_fy_membership(seed):
    pool = list(range(N)); state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        pool[i], pool[int(state % (i + 1))] = pool[int(state % (i + 1))], pool[i]
    mem = [0] * N
    for l in range(S):
        for s in range(S):
            mem[pool[l * S + s]] = l
    return mem


def load_ltpb_membership(path):
    """Load LTPB and return 2 membership arrays."""
    data = Path(path).read_bytes()
    _, _, s, nsub = struct.unpack_from("<4sIII", data, 0)
    assert s == S and nsub >= 2
    mems = []
    for sub in range(2):
        off = 16 + sub * N * 2
        a = np.frombuffer(data[off:off + N * 2], dtype="<u2")
        mems.append(list(a))
    return mems


# ── CSM and projections ─────────────────────────────────────────────────────

def load_csm(data, block_idx):
    csm = np.zeros((S, S), dtype=np.uint8)
    start = block_idx * N
    for i in range(N):
        sb = (start + i) // 8
        if sb < len(data) and (data[sb] >> (7 - (start + i) % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


def compute_projections(csm, mem1, mem2):
    """Compute all cross-sum targets and CRC-32 per row."""
    targets = []
    members = []

    # LSM
    for r in range(S):
        targets.append(int(np.sum(csm[r, :]))); members.append([r * S + c for c in range(S)])
    # VSM
    for c in range(S):
        targets.append(int(np.sum(csm[:, c]))); members.append([r * S + c for r in range(S)])
    # DSM
    for d in range(DIAG_COUNT):
        off = d - (S - 1); m = []; s = 0
        for r in range(S):
            cv = r + off
            if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
        targets.append(s); members.append(m)
    # XSM
    for x in range(DIAG_COUNT):
        m = []; s = 0
        for r in range(S):
            cv = x - r
            if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
        targets.append(s); members.append(m)
    # LTP1, LTP2
    for mem_t in [mem1, mem2]:
        ls = [0] * S; lm = [[] for _ in range(S)]
        for f in range(N):
            ls[mem_t[f]] += int(csm[f // S, f % S]); lm[mem_t[f]].append(f)
        for k in range(S):
            targets.append(ls[k]); members.append(lm[k])

    # CRC-32 per row
    total_bytes = (S + 1 + 7) // 8
    crcs = []
    for r in range(S):
        msg = bytearray(total_bytes)
        for c in range(S):
            if csm[r, c]: msg[c // 8] |= (1 << (7 - c % 8))
        crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)

    # SHA-256 block hash
    buf = bytearray(S * 16)
    for r in range(S):
        w0, w1 = 0, 0
        for c in range(S):
            if csm[r, c]:
                if c < 64: w0 |= (1 << (63 - c))
                else: w1 |= (1 << (63 - (c - 64)))
        struct.pack_into(">QQ", buf, r * 16, w0, w1)
    bh = hashlib.sha256(bytes(buf)).digest()

    return targets, members, crcs, bh


# ── CRC-32 generator matrix ─────────────────────────────────────────────────

def build_crc32_gen():
    total_bytes = (S + 1 + 7) // 8
    c = zlib.crc32(bytes(total_bytes)) & 0xFFFFFFFF
    c_bits = [(c >> (31 - i)) & 1 for i in range(32)]
    G = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(total_bytes)
        msg[col // 8] |= (1 << (7 - col % 8))
        val = (zlib.crc32(bytes(msg)) & 0xFFFFFFFF) ^ c
        for i in range(32): G[i, col] = (val >> (31 - i)) & 1
    return G, np.array(c_bits, dtype=np.uint8)


G_CRC, C_VEC = build_crc32_gen()


# ── GF(2) packed matrix operations ──────────────────────────────────────────

def pack_row(bits):
    """Pack N-element uint8 array into WORDS uint64 words."""
    row = np.zeros(WORDS, dtype=np.uint64)
    for i in range(len(bits)):
        if bits[i]: row[i // 64] |= np.uint64(1) << np.uint64(i % 64)
    return row


def unpack_row(row):
    """Unpack WORDS uint64 words to N-element bool list."""
    bits = [False] * N
    for i in range(N):
        if int(row[i // 64]) & (1 << (i % 64)): bits[i] = True
    return bits


def build_gf2_system(targets, members, crcs):
    """Build packed GF(2) matrix: cross-sum parity + CRC-32."""
    n_cs = N_LINES
    n_crc = S * 32
    m = n_cs + n_crc

    # Rows as packed uint64 arrays + 1 target bit each
    rows = []
    target_bits = []

    # Cross-sum parity
    for i in range(n_cs):
        bits = np.zeros(N, dtype=np.uint8)
        for j in members[i]: bits[j] = 1
        rows.append(pack_row(bits))
        target_bits.append(targets[i] % 2)

    # CRC-32 equations
    for r in range(S):
        h = crcs[r]
        h_bits = [(h >> (31 - i)) & 1 for i in range(32)]
        for bit in range(32):
            bits = np.zeros(N, dtype=np.uint8)
            for c in range(S):
                bits[r * S + c] = G_CRC[bit, c]
            rows.append(pack_row(bits))
            target_bits.append(h_bits[bit] ^ int(C_VEC[bit]))

    return rows, target_bits


def gauss_elim(rows, target_bits):
    """GF(2) Gaussian elimination with RREF. Returns (pivot_cols, free_cols, rank)."""
    m = len(rows)
    t = list(target_bits)
    pivot_cols = []
    pivot_row = 0

    for col in range(N):
        w = col // 64
        b = np.uint64(col % 64)
        mask = np.uint64(1) << b

        found = -1
        for r in range(pivot_row, m):
            if rows[r][w] & mask:
                found = r; break
        if found == -1: continue

        rows[pivot_row], rows[found] = rows[found], rows[pivot_row]
        t[pivot_row], t[found] = t[found], t[pivot_row]

        for r in range(m):
            if r != pivot_row and (rows[r][w] & mask):
                rows[r] = rows[r] ^ rows[pivot_row]
                t[r] ^= t[pivot_row]

        pivot_cols.append(col)
        pivot_row += 1

    rank = len(pivot_cols)
    free_cols = sorted(set(range(N)) - set(pivot_cols))
    return pivot_cols, free_cols, rank, rows[:rank], t[:rank]


def extract_gf2_determined(rref_rows, rref_targets, pivot_cols, free_cols, known):
    """Find cells determined by GF(2) alone (pivots with no free dependencies)."""
    free_set = set(free_cols)
    determined = {}
    for i, pcol in enumerate(pivot_cols):
        if pcol in known: continue
        row = rref_rows[i]
        has_free = False
        for fc in free_cols:
            if fc in known: continue
            w = fc // 64; b = fc % 64
            if int(row[w]) & (1 << b):
                has_free = True; break
        if not has_free:
            determined[pcol] = rref_targets[i]
    return determined


# ── Integer constraint propagation ──────────────────────────────────────────

def int_bound(targets, members, cell_lines, known):
    """IntBound: rho=0 → force 0, rho=u → force 1."""
    rho = list(targets)
    u = [len(m) for m in members]
    free = set(range(N))

    # Subtract known values
    for j, v in known.items():
        free.discard(j)
        for li in cell_lines[j]:
            u[li] -= 1; rho[li] -= v

    newly = {}
    q = collections.deque(range(len(targets))); inq = set(q)
    while q:
        i = q.popleft(); inq.discard(i)
        if u[i] == 0: continue
        if rho[i] < 0 or rho[i] > u[i]: return {}, True
        fv = None
        if rho[i] == 0: fv = 0
        elif rho[i] == u[i]: fv = 1
        if fv is None: continue
        for j in members[i]:
            if j not in free: continue
            newly[j] = fv; free.discard(j)
            for li in cell_lines[j]:
                u[li] -= 1; rho[li] -= fv
                if li not in inq: q.append(li); inq.add(li)

    return newly, False


def crc32_row_resolve(crcs, known):
    """Per-row CRC-32 resolution: for each row, solve 32 GF(2) equations on free cells.

    Rows with f_r <= 32 may become fully determined.
    Rows with f_r > 32 still yield (f_r - rank) free variables per row.

    Returns (newly_determined, inconsistent).
    """
    newly = {}

    for r in range(S):
        # Identify free cells in this row
        free_cols = [c for c in range(S) if (r * S + c) not in known]
        f_r = len(free_cols)
        if f_r == 0:
            continue  # row fully determined

        # Build CRC-32 target residual: h_r XOR c XOR contribution of known cells
        h = crcs[r]
        h_bits = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target = h_bits ^ C_VEC

        # Subtract known cells' contribution
        for c in range(S):
            flat = r * S + c
            if flat in known and known[flat] == 1:
                target ^= G_CRC[:, c]

        # Build per-row sub-matrix: 32 × f_r
        G_sub = np.zeros((32, f_r), dtype=np.uint8)
        for j, c in enumerate(free_cols):
            G_sub[:, j] = G_CRC[:, c]

        # GF(2) Gaussian elimination on sub-matrix
        G = G_sub.copy()
        t = target.copy()
        pivots = []
        pivot_row = 0

        for col in range(f_r):
            found = -1
            for rr in range(pivot_row, 32):
                if G[rr, col]:
                    found = rr; break
            if found == -1:
                continue
            G[[pivot_row, found]] = G[[found, pivot_row]]
            t[pivot_row], t[found] = t[found], t[pivot_row]
            for rr in range(32):
                if rr != pivot_row and G[rr, col]:
                    G[rr] ^= G[pivot_row]
                    t[rr] ^= t[pivot_row]
            pivots.append(col)
            pivot_row += 1

        # Check consistency: any 0=1 rows?
        for rr in range(pivot_row, 32):
            if t[rr] != 0:
                return {}, True  # inconsistent

        # Extract determined cells: pivots with no free-column dependencies
        free_in_row = sorted(set(range(f_r)) - set(pivots))
        for i, pcol in enumerate(pivots):
            has_free_dep = False
            for fc in free_in_row:
                if G[i, fc]:
                    has_free_dep = True; break
            if not has_free_dep:
                # This cell is determined by CRC-32!
                flat = r * S + free_cols[pcol]
                newly[flat] = int(t[i])

    return newly, False


def cross_deduce(targets, members, cell_lines, known):
    """CrossDeduce: if v=0 makes any line infeasible, cell must be 1."""
    rho = list(targets)
    u = [len(m) for m in members]
    free = set(range(N))
    for j, v in known.items():
        free.discard(j)
        for li in cell_lines[j]:
            u[li] -= 1; rho[li] -= v

    newly = {}
    for j in sorted(free):
        forced = None
        for v in (0, 1):
            feasible = True
            for li in cell_lines[j]:
                nr = rho[li] - v; nu = u[li] - 1
                if nr < 0 or nr > nu: feasible = False; break
            if not feasible:
                if forced is not None: return {}, True
                forced = 1 - v
        if forced is not None: newly[j] = forced
    return newly, False


def propagate_gf2(rows, target_bits, newly):
    """Substitute known cells into GF(2) system."""
    for j, v in newly.items():
        w = j // 64; b = np.uint64(j % 64); mask = np.uint64(1) << b
        for i in range(len(rows)):
            if rows[i][w] & mask:
                rows[i][w] &= ~mask
                if v: target_bits[i] ^= 1


# ── Fixpoint ────────────────────────────────────────────────────────────────

def solve_block(csm, mem1, mem2):
    """Full algebraic solve of one block. Returns result dict."""
    targets, members, crcs, bh = compute_projections(csm, mem1, mem2)

    cell_lines = [[] for _ in range(N)]
    for i, m in enumerate(members):
        for j in m: cell_lines[j].append(i)

    # Build GF(2) system
    t0 = time.time()
    rows, tbits = build_gf2_system(targets, members, crcs)
    t_build = time.time() - t0

    known = {}
    stats = []

    for iteration in range(1, 50):
        prev = len(known)

        # Phase A: GF(2) Gaussian elimination
        t0 = time.time()
        rows_copy = [r.copy() for r in rows]
        tbits_copy = list(tbits)
        pivots, frees, rank, rref, rref_t = gauss_elim(rows_copy, tbits_copy)
        t_gauss = time.time() - t0

        det_gf2 = extract_gf2_determined(rref, rref_t, pivots, frees, known)
        if det_gf2:
            known.update(det_gf2)
            propagate_gf2(rows, tbits, det_gf2)

        # Phase B: IntBound
        t0 = time.time()
        det_int, inconsistent = int_bound(targets, members, cell_lines, known)
        t_int = time.time() - t0
        if inconsistent:
            return {"error": "inconsistent", "iteration": iteration}
        if det_int:
            known.update(det_int)
            propagate_gf2(rows, tbits, det_int)

        # Phase C: Per-row CRC-32 resolution
        t0 = time.time()
        det_crc, inconsistent = crc32_row_resolve(crcs, known)
        t_crc = time.time() - t0
        if inconsistent:
            return {"error": "crc32_inconsistent", "iteration": iteration}
        if det_crc:
            known.update(det_crc)
            propagate_gf2(rows, tbits, det_crc)

        # Phase D: IntBound again (CRC-32 determinations cascade)
        if det_crc:
            t0 = time.time()
            det_int2, inconsistent = int_bound(targets, members, cell_lines, known)
            t_int += time.time() - t0
            if inconsistent:
                return {"error": "inconsistent_post_crc", "iteration": iteration}
            if det_int2:
                known.update(det_int2)
                propagate_gf2(rows, tbits, det_int2)
                det_int.update(det_int2)

        # Phase E: CrossDeduce
        t0 = time.time()
        det_cross, inconsistent = cross_deduce(targets, members, cell_lines, known)
        t_cross = time.time() - t0
        if inconsistent:
            return {"error": "inconsistent", "iteration": iteration}
        if det_cross:
            known.update(det_cross)
            propagate_gf2(rows, tbits, det_cross)

        gain = len(known) - prev
        pct = len(known) / N * 100

        stat = {
            "iter": iteration, "determined": len(known), "pct": round(pct, 1),
            "free": N - len(known), "gain": gain,
            "gf2": len(det_gf2), "intbound": len(det_int),
            "crc32_row": len(det_crc), "crossdeduce": len(det_cross),
            "rank": rank, "t_gauss": round(t_gauss, 2), "t_int": round(t_int, 2),
            "t_crc": round(t_crc, 2), "t_cross": round(t_cross, 2),
        }
        stats.append(stat)

        print(f"    Iter {iteration}: |D|={len(known)} ({pct:.1f}%) "
              f"[+{len(det_gf2)} GF2, +{len(det_int)} Int, +{len(det_crc)} CRC32row, +{len(det_cross)} Cross] "
              f"rank={rank} ({t_gauss:.1f}s)")

        if gain == 0 or len(known) == N: break

    # Verify if fully solved
    solved = False
    bh_ok = False
    if len(known) == N:
        recon = np.zeros((S, S), dtype=np.uint8)
        for j, v in known.items(): recon[j // S, j % S] = v
        buf = bytearray(S * 16)
        for r in range(S):
            w0, w1 = 0, 0
            for c in range(S):
                if recon[r, c]:
                    if c < 64: w0 |= (1 << (63 - c))
                    else: w1 |= (1 << (63 - (c - 64)))
            struct.pack_into(">QQ", buf, r * 16, w0, w1)
        bh_ok = hashlib.sha256(bytes(buf)).digest() == bh
        solved = bh_ok

    return {
        "determined": len(known), "free": N - len(known),
        "pct": round(len(known) / N * 100, 1),
        "solved": solved, "bh_verified": bh_ok,
        "iterations": len(stats), "stats": stats,
        "total_gf2": sum(s["gf2"] for s in stats),
        "total_int": sum(s["intbound"] for s in stats),
        "total_crc32_row": sum(s.get("crc32_row", 0) for s in stats),
        "total_cross": sum(s["crossdeduce"] for s in stats),
        "build_time": round(t_build, 2),
    }


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="B.59g: Algebraic solve at S=127")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--blocks", type=int, default=None, help="Run on first N blocks")
    parser.add_argument("--table", default=None, help="LTPB file (default: from seeds)")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b59g_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    data = mp4_path.read_bytes()

    # Build LTP memberships
    if args.table:
        print(f"Loading LTP table: {args.table}")
        mem1, mem2 = load_ltpb_membership(args.table)
    else:
        seed1 = int.from_bytes(b"CRSCLTPZ", "big")
        seed2 = int.from_bytes(b"CRSCLTPR", "big")
        print(f"Building yLTP from seeds")
        mem1 = build_fy_membership(seed1)
        mem2 = build_fy_membership(seed2)

    block_list = list(range(args.blocks)) if args.blocks else [args.block]

    print(f"\nB.59g: Algebraic Solve (S={S}, N={N})")
    print(f"  CRC-32 equations: {S * 32} = {S * 32}")
    print(f"  Cross-sum equations: {N_LINES}")
    print(f"  Total GF(2) equations: {N_LINES + S * 32}")
    print(f"  Blocks: {len(block_list)}")
    print()

    results = []
    for bidx in block_list:
        csm = load_csm(data, bidx)
        density = np.sum(csm) / N
        print(f"  Block {bidx}: density={density:.3f}")

        t0 = time.time()
        result = solve_block(csm, mem1, mem2)
        elapsed = time.time() - t0

        result["block"] = bidx
        result["density"] = round(density, 4)
        result["wall_time"] = round(elapsed, 1)
        results.append(result)

        print(f"    Result: {result['determined']}/{N} ({result['pct']}%) "
              f"free={result['free']} solved={result['solved']} "
              f"[GF2={result['total_gf2']} Int={result['total_int']} Cross={result['total_cross']}] "
              f"({elapsed:.1f}s)\n")

    # Summary
    print(f"{'='*72}")
    print(f"B.59g Summary — {len(results)} blocks")
    for r in results:
        print(f"  Block {r['block']}: {r['pct']}% determined, free={r['free']}, solved={r['solved']}")

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps({"experiment": "B.59g", "results": results}, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
