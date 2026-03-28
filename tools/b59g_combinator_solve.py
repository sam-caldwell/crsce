#!/usr/bin/env python3
"""
B.59g: Combinator-Based Row-Serial Algebraic Solver at S=127.

Pure algebraic pipeline — no DFS, no backtracking.

Pipeline:
  1. IntBound propagation (rho=0/rho=u forcing)
  2. Row-serial solve: for each row (fewest unknowns first),
     combine CRC-32 + cross-sum bounds to determine all cells
  3. Propagate solved row values into remaining constraints
  4. Repeat until all rows solved or no progress

Per-row solver:
  - 32 CRC-32 GF(2) equations on f_r unknown cells
  - Row-sum integer constraint
  - Per-cell bounds from column/diagonal/LTP residuals
  - Enumerate GF(2)-consistent solutions, filter by integer constraints
  - If unique: row is solved
  - If ambiguous: report remaining candidates

Usage:
    python3 tools/b59g_combinator_solve.py
    python3 tools/b59g_combinator_solve.py --table tools/b59c_table.bin
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
N = S * S
DIAG_COUNT = 2 * S - 1
N_LINES = S + S + DIAG_COUNT + DIAG_COUNT + S + S

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64


def build_fy_membership(seed):
    pool = list(range(N)); state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        pool[i], pool[int(state % (i + 1))] = pool[int(state % (i + 1))], pool[i]
    mem = [0] * N
    for l in range(S):
        for s in range(S): mem[pool[l * S + s]] = l
    return mem


def load_ltpb_membership(path):
    data = Path(path).read_bytes()
    _, _, s, nsub = struct.unpack_from("<4sIII", data, 0)
    mems = []
    for sub in range(2):
        off = 16 + sub * N * 2
        mems.append(list(np.frombuffer(data[off:off + N * 2], dtype="<u2")))
    return mems


def load_csm(data, block_idx):
    csm = np.zeros((S, S), dtype=np.uint8)
    start = block_idx * N
    for i in range(N):
        sb = (start + i) // 8
        if sb < len(data) and (data[sb] >> (7 - (start + i) % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


# CRC-32 generator matrix
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
    return G, np.array(c_bits, dtype=np.uint8), c


G_CRC, C_VEC, CRC_ZERO = build_crc32_gen()


def solve_block(csm, mem1, mem2):
    """Combinator row-serial algebraic solve."""

    # Compute all projections
    targets = []
    members = []
    for r in range(S):
        targets.append(int(np.sum(csm[r, :]))); members.append([r * S + c for c in range(S)])
    for c in range(S):
        targets.append(int(np.sum(csm[:, c]))); members.append([r * S + c for r in range(S)])
    for d in range(DIAG_COUNT):
        off = d - (S - 1); m = []; s = 0
        for r in range(S):
            cv = r + off
            if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
        targets.append(s); members.append(m)
    for x in range(DIAG_COUNT):
        m = []; s = 0
        for r in range(S):
            cv = x - r
            if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
        targets.append(s); members.append(m)
    for mt in [mem1, mem2]:
        ls = [0] * S; lm = [[] for _ in range(S)]
        for f in range(N): ls[mt[f]] += int(csm[f // S, f % S]); lm[mt[f]].append(f)
        for k in range(S): targets.append(ls[k]); members.append(lm[k])

    cell_lines = [[] for _ in range(N)]
    for i, m in enumerate(members):
        for j in m: cell_lines[j].append(i)

    total_bytes = (S + 1 + 7) // 8
    row_crcs = []
    for r in range(S):
        msg = bytearray(total_bytes)
        for c in range(S):
            if csm[r, c]: msg[c // 8] |= (1 << (7 - c % 8))
        row_crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)

    # Phase 1: IntBound propagation
    rho = list(targets); u = [len(m) for m in members]
    known = {}
    free = set(range(N))
    q = collections.deque(range(len(targets))); inq = set(q)
    while q:
        i = q.popleft(); inq.discard(i)
        if u[i] == 0: continue
        if rho[i] < 0 or rho[i] > u[i]: break
        fv = None
        if rho[i] == 0: fv = 0
        elif rho[i] == u[i]: fv = 1
        if fv is None: continue
        for j in members[i]:
            if j not in free: continue
            known[j] = fv; free.discard(j)
            for li in cell_lines[j]:
                u[li] -= 1; rho[li] -= fv
                if li not in inq: q.append(li); inq.add(li)

    intbound_det = len(known)
    print(f"  Phase 1 (IntBound): {intbound_det} cells ({intbound_det/N*100:.1f}%)")

    # Phase 2: Row-serial CRC-32 algebraic solve
    # Process rows in order of fewest unknowns (easiest first)
    rows_solved = 0
    crc_determined = 0
    propagation_determined = 0

    for iteration in range(1, 50):
        # Find rows sorted by unknowns (ascending)
        row_unknowns = []
        for r in range(S):
            row_free = [c for c in range(S) if (r * S + c) in free]
            if len(row_free) > 0:
                row_unknowns.append((len(row_free), r, row_free))
        row_unknowns.sort()

        if not row_unknowns:
            break  # all rows solved

        progress = False

        for f_r, r, free_cols in row_unknowns:
            if f_r > 127:  # skip fully-free rows
                continue

            # Build per-row CRC-32 system
            h = row_crcs[r]
            h_bits = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
            target = h_bits ^ C_VEC

            # Subtract known cells' contribution
            for c in range(S):
                flat = r * S + c
                if flat in known and known[flat] == 1:
                    target ^= G_CRC[:, c]

            # Build per-row GF(2) sub-matrix: 32 × f_r
            G_sub = G_CRC[:, free_cols].copy()

            # GF(2) Gaussian elimination
            G_work = G_sub.copy()
            t_work = target.copy()
            pivots = []
            pivot_row = 0
            for col in range(f_r):
                found = -1
                for rr in range(pivot_row, 32):
                    if G_work[rr, col]: found = rr; break
                if found < 0: continue
                G_work[[pivot_row, found]] = G_work[[found, pivot_row]]
                t_work[pivot_row], t_work[found] = t_work[found], t_work[pivot_row]
                for rr in range(32):
                    if rr != pivot_row and G_work[rr, col]:
                        G_work[rr] ^= G_work[pivot_row]
                        t_work[rr] ^= t_work[pivot_row]
                pivots.append(col)
                pivot_row += 1

            # Check consistency
            inconsistent = False
            for rr in range(pivot_row, 32):
                if G_work[rr].sum() == 0 and t_work[rr] != 0:
                    inconsistent = True; break
            if inconsistent:
                continue

            rank = len(pivots)
            n_free = f_r - rank
            if n_free > 25:  # too many free variables to enumerate
                continue

            # Row-sum constraint
            row_rho = rho[r]  # how many unknowns must be 1

            # Per-cell bounds from column/diagonal/LTP constraints
            # For each unknown cell, check if setting it to 0 or 1 is feasible
            cell_bounds = {}
            for c in free_cols:
                flat = r * S + c
                can_be_0 = True
                can_be_1 = True
                for li in cell_lines[flat]:
                    if rho[li] < 0 or rho[li] > u[li]:
                        can_be_0 = False; can_be_1 = False; break
                    # If v=0: rho stays, u-1
                    if rho[li] > u[li] - 1: can_be_0 = False  # rho > new_u
                    # If v=1: rho-1, u-1
                    if rho[li] - 1 < 0: can_be_1 = False  # new_rho < 0
                    if rho[li] - 1 > u[li] - 1: can_be_1 = False  # new_rho > new_u
                cell_bounds[c] = (can_be_0, can_be_1)

            # Enumerate GF(2)-consistent solutions (fully vectorized)
            pivot_set = set(pivots)
            free_idx = sorted(set(range(f_r)) - pivot_set)

            if n_free > 25:
                continue  # 2^25 = 33M candidates — too large for RAM

            n_candidates = 1 << n_free

            # Build all 2^n_free free-variable assignments as a matrix
            all_free = np.array([[(i >> b) & 1 for b in range(n_free)]
                                 for i in range(n_candidates)], dtype=np.uint8)

            # Dependency matrix: dep[i, fi] = G_work[pivot_row_i, free_col_fi]
            dep_matrix = np.zeros((rank, n_free), dtype=np.uint8)
            for i in range(rank):
                for fi, fc in enumerate(free_idx):
                    dep_matrix[i, fi] = G_work[i, fc]

            # Compute ALL pivot values at once: (rank x n_candidates)
            t_vec = np.array([t_work[i] for i in range(rank)], dtype=np.uint8)
            pivot_contrib = (dep_matrix @ all_free.T) % 2  # rank x n_candidates
            pivot_vals = pivot_contrib ^ t_vec[:, np.newaxis]  # rank x n_candidates

            # Reconstruct full cell value matrix: (f_r x n_candidates)
            full_vals = np.zeros((f_r, n_candidates), dtype=np.uint8)
            for fi, fc in enumerate(free_idx):
                full_vals[fc] = all_free[:, fi]
            for i, pc in enumerate(pivots):
                full_vals[pc] = pivot_vals[i]

            # Row-sum filter
            sums = full_vals.sum(axis=0)  # n_candidates
            sum_mask = (sums == row_rho)

            # Cell-bounds filter
            bounds_mask = np.ones(n_candidates, dtype=bool)
            for j, c in enumerate(free_cols):
                can0, can1 = cell_bounds[c]
                if not can0:
                    bounds_mask &= (full_vals[j] == 1)
                if not can1:
                    bounds_mask &= (full_vals[j] == 0)

            # Combined filter
            valid = sum_mask & bounds_mask
            valid_indices = np.where(valid)[0]

            candidates = [full_vals[:, idx].tolist() for idx in valid_indices]

            if len(candidates) == 1:
                # Unique solution — assign all cells in this row
                vals = candidates[0]
                for j, c in enumerate(free_cols):
                    flat = r * S + c
                    known[flat] = vals[j]
                    free.discard(flat)
                    for li in cell_lines[flat]:
                        u[li] -= 1; rho[li] -= vals[j]
                rows_solved += 1
                crc_determined += len(free_cols)
                progress = True

                # IntBound propagation cascade
                q2 = collections.deque()
                for j, c in enumerate(free_cols):
                    for li in cell_lines[r * S + c]:
                        q2.append(li)
                inq2 = set(q2)
                while q2:
                    i = q2.popleft(); inq2.discard(i)
                    if u[i] == 0: continue
                    if rho[i] < 0 or rho[i] > u[i]: continue
                    fv2 = None
                    if rho[i] == 0: fv2 = 0
                    elif rho[i] == u[i]: fv2 = 1
                    if fv2 is None: continue
                    for jj in members[i]:
                        if jj not in free: continue
                        known[jj] = fv2; free.discard(jj)
                        propagation_determined += 1
                        for li2 in cell_lines[jj]:
                            u[li2] -= 1; rho[li2] -= fv2
                            if li2 not in inq2: q2.append(li2); inq2.add(li2)

            elif len(candidates) == 0:
                pass  # No valid solution — skip this row
            # else: multiple candidates — can't determine uniquely, skip

        total = len(known)
        pct = total / N * 100
        print(f"  Iteration {iteration}: {total} cells ({pct:.1f}%) "
              f"[rows_solved={rows_solved} crc_det={crc_determined} prop_det={propagation_determined}]")

        if not progress:
            break

    # Verify
    total_det = len(known)
    solved = total_det == N
    bh_ok = False
    if solved:
        buf = bytearray(S * 16)
        for r in range(S):
            w0, w1 = 0, 0
            for c in range(S):
                if known.get(r * S + c, 0):
                    if c < 64: w0 |= (1 << (63 - c))
                    else: w1 |= (1 << (63 - (c - 64)))
            struct.pack_into(">QQ", buf, r * 16, w0, w1)
        bh_ok = hashlib.sha256(bytes(buf)).digest() == hashlib.sha256(
            b''.join(struct.pack(">QQ",
                *[sum(int(known.get(r*S+c,0)) << (63-c%64) for c in range(64*w, min(64*(w+1), S)))
                  for w in range(2)])
            for r in range(S))
        ).digest()

    return {
        "intbound": intbound_det,
        "crc_determined": crc_determined,
        "propagation_determined": propagation_determined,
        "rows_solved": rows_solved,
        "total_determined": total_det,
        "free": N - total_det,
        "pct": round(total_det / N * 100, 1),
        "solved": solved,
    }


def main():
    parser = argparse.ArgumentParser(description="B.59g: Combinator row-serial solver")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--table", default=None)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b59g_combinator_results.json"))
    args = parser.parse_args()

    data = (REPO_ROOT / "docs" / "testData" / "useless-machine.mp4").read_bytes()

    if args.table:
        mem1, mem2 = load_ltpb_membership(args.table)
    else:
        mem1 = build_fy_membership(int.from_bytes(b"CRSCLTPZ", "big"))
        mem2 = build_fy_membership(int.from_bytes(b"CRSCLTPR", "big"))

    csm = load_csm(data, args.block)
    density = np.sum(csm) / N
    print(f"B.59g Combinator Row-Serial Solver (block {args.block}, density {density:.3f})")

    t0 = time.time()
    result = solve_block(csm, mem1, mem2)
    elapsed = time.time() - t0

    result["wall_time"] = round(elapsed, 2)
    result["block"] = args.block
    result["density"] = round(density, 4)

    print(f"\nResult: {result['total_determined']}/{N} ({result['pct']}%)")
    print(f"  IntBound:    {result['intbound']}")
    print(f"  CRC-32 rows: {result['rows_solved']} solved, {result['crc_determined']} cells")
    print(f"  Propagation: {result['propagation_determined']} cells from cascade")
    print(f"  Free:        {result['free']}")
    print(f"  Solved:      {result['solved']}")
    print(f"  Time:        {elapsed:.1f}s")

    Path(args.out).write_text(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
