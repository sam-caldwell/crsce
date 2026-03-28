#!/usr/bin/env python3
"""
B.59h: Arc Consistency over Row-Candidate CSP (Phase 3 of B.59g).

Pipeline:
  1. IntBound propagation (22.3%)
  2. Per-row CRC-32 candidate generation (for tractable rows, n_free <= MAX_FREE)
  3. AC-3 bounds consistency on cross-row constraints (columns, diags, anti-diags, LTPs)
  4. Solve rows with domain size 1, propagate via IntBound
  5. Re-generate candidates for newly tractable rows
  6. Repeat until all rows solved or no progress

Usage:
    python3 tools/b59h_arc_consistency.py
    python3 tools/b59h_arc_consistency.py --block 0
    python3 tools/b59h_arc_consistency.py --max-free 25
"""

import argparse
import collections
import hashlib
import json
import struct
import time
import zlib
import numpy as np
from math import comb
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
DIAG = 2 * S - 1
MAX_FREE_DEFAULT = 22

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64


def build_yltp(seed):
    pool = list(range(N)); state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        pool[i], pool[int(state % (i + 1))] = pool[int(state % (i + 1))], pool[i]
    mem = [0] * N
    for l in range(S):
        for s in range(S): mem[pool[l * S + s]] = l
    return mem


YLTP1 = build_yltp(int.from_bytes(b"CRSCLTPZ", "big"))
YLTP2 = build_yltp(int.from_bytes(b"CRSCLTPR", "big"))


def load_csm(data, block_idx):
    csm = np.zeros((S, S), dtype=np.uint8)
    start = block_idx * N
    for i in range(N):
        sb = (start + i) // 8
        if sb < len(data) and (data[sb] >> (7 - (start + i) % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


# CRC-32 generator matrix
def _build_crc():
    tb = (S + 1 + 7) // 8
    cz = zlib.crc32(bytes(tb)) & 0xFFFFFFFF
    cv = np.array([(cz >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
    G = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(tb)
        msg[col // 8] |= (1 << (7 - col % 8))
        val = (zlib.crc32(bytes(msg)) & 0xFFFFFFFF) ^ cz
        for i in range(32): G[i, col] = (val >> (31 - i)) & 1
    return G, cv, cz


G_CRC, C_VEC, CRC_ZERO = _build_crc()


class ConstraintSystem:
    """Manages the constraint state and IntBound propagation."""

    def __init__(self, csm):
        self.csm = csm
        self.targets = []
        self.members = []
        self._build(csm)
        self.cell_lines = [[] for _ in range(N)]
        for i, m in enumerate(self.members):
            for j in m: self.cell_lines[j].append(i)
        self.rho = list(self.targets)
        self.u = [len(m) for m in self.members]
        self.known = {}
        self.free = set(range(N))

        # CRC-32 per row
        tb = (S + 1 + 7) // 8
        self.row_crcs = []
        for r in range(S):
            msg = bytearray(tb)
            for c in range(S):
                if csm[r, c]: msg[c // 8] |= (1 << (7 - c % 8))
            self.row_crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)

    def _build(self, csm):
        for r in range(S):
            self.targets.append(int(np.sum(csm[r, :]))); self.members.append([r * S + c for c in range(S)])
        for c in range(S):
            self.targets.append(int(np.sum(csm[:, c]))); self.members.append([r * S + c for r in range(S)])
        for d in range(DIAG):
            off = d - (S - 1); m = []; s = 0
            for r in range(S):
                cv = r + off
                if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
            self.targets.append(s); self.members.append(m)
        for x in range(DIAG):
            m = []; s = 0
            for r in range(S):
                cv = x - r
                if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
            self.targets.append(s); self.members.append(m)
        for mt in [YLTP1, YLTP2]:
            ls = [0] * S; lm = [[] for _ in range(S)]
            for f in range(N): ls[mt[f]] += int(csm[f // S, f % S]); lm[mt[f]].append(f)
            for k in range(S): self.targets.append(ls[k]); self.members.append(lm[k])

    def intbound(self):
        """Full IntBound propagation. Returns number of newly forced cells."""
        newly = 0
        q = collections.deque(range(len(self.targets))); inq = set(q)
        while q:
            i = q.popleft(); inq.discard(i)
            if self.u[i] == 0: continue
            if self.rho[i] < 0 or self.rho[i] > self.u[i]: continue
            fv = None
            if self.rho[i] == 0: fv = 0
            elif self.rho[i] == self.u[i]: fv = 1
            if fv is None: continue
            for j in self.members[i]:
                if j not in self.free: continue
                self.known[j] = fv; self.free.discard(j); newly += 1
                for li in self.cell_lines[j]:
                    self.u[li] -= 1; self.rho[li] -= fv
                    if li not in inq: q.append(li); inq.add(li)
        return newly

    def solve_row(self, r, values):
        """Assign known values for all free cells in row r. Returns cascade count."""
        free_cols = [c for c in range(S) if (r * S + c) in self.free]
        for j, c in enumerate(free_cols):
            flat = r * S + c
            v = values[j]
            self.known[flat] = v; self.free.discard(flat)
            for li in self.cell_lines[flat]:
                self.u[li] -= 1; self.rho[li] -= v
        return self.intbound()

    def row_free_cols(self, r):
        return [c for c in range(S) if (r * S + c) in self.free]

    def row_n_free(self, r):
        return sum(1 for c in range(S) if (r * S + c) in self.free)


def generate_candidates(cs, r, max_free):
    """Generate CRC-32 + row-sum filtered candidates for row r.

    Returns numpy array of shape (n_candidates, f_r) or None if intractable.
    """
    free_cols = cs.row_free_cols(r)
    f_r = len(free_cols)
    if f_r == 0: return None

    # Build CRC-32 + row parity GF(2) system (33 x f_r)
    h = cs.row_crcs[r]
    h_bits = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
    target = h_bits ^ C_VEC
    for c in range(S):
        if (r * S + c) in cs.known and cs.known[r * S + c] == 1:
            target ^= G_CRC[:, c]

    G_sub = G_CRC[:, free_cols].copy()
    rp_target = cs.targets[r] % 2
    for c in range(S):
        if (r * S + c) in cs.known and cs.known[r * S + c] == 1:
            rp_target ^= 1
    G_full = np.vstack([G_sub, np.ones((1, f_r), dtype=np.uint8)])
    t_full = np.append(target, rp_target)

    # GaussElim
    G_w = G_full.copy(); t_w = t_full.copy()
    pivots = []; pr = 0
    for col in range(f_r):
        found = -1
        for rr in range(pr, 33):
            if G_w[rr, col]: found = rr; break
        if found < 0: continue
        G_w[[pr, found]] = G_w[[found, pr]]
        t_w[pr], t_w[found] = t_w[found], t_w[pr]
        for rr in range(33):
            if rr != pr and G_w[rr, col]:
                G_w[rr] ^= G_w[pr]; t_w[rr] ^= t_w[pr]
        pivots.append(col); pr += 1

    rank = len(pivots)
    n_free = f_r - rank
    if n_free > max_free:
        return None  # intractable

    # Vectorized enumeration
    free_idx = sorted(set(range(f_r)) - set(pivots))
    n_cand = 1 << n_free

    all_free = np.zeros((n_cand, n_free), dtype=np.uint8)
    for i in range(n_cand):
        for b in range(n_free):
            all_free[i, b] = (i >> b) & 1

    # Dependency matrix
    dep = np.zeros((rank, n_free), dtype=np.uint8)
    for i in range(rank):
        for fi, fc in enumerate(free_idx):
            dep[i, fi] = G_w[i, fc]

    # Compute all solutions
    t_vec = np.array([t_w[i] for i in range(rank)], dtype=np.uint8)
    pivot_contrib = (dep @ all_free.T) % 2
    pivot_vals = pivot_contrib ^ t_vec[:, np.newaxis]

    full_vals = np.zeros((f_r, n_cand), dtype=np.uint8)
    for fi, fc in enumerate(free_idx): full_vals[fc] = all_free[:, fi]
    for i, pc in enumerate(pivots): full_vals[pc] = pivot_vals[i]

    # Row-sum filter
    row_rho = cs.rho[r]
    sums = full_vals.sum(axis=0)
    valid = (sums == row_rho)

    # Per-cell bounds filter
    for j, c in enumerate(free_cols):
        flat = r * S + c
        can0, can1 = True, True
        for li in cs.cell_lines[flat]:
            if cs.rho[li] > cs.u[li] - 1: can0 = False
            if cs.rho[li] - 1 < 0 or cs.rho[li] - 1 > cs.u[li] - 1: can1 = False
        if not can0: valid &= (full_vals[j] == 1)
        if not can1: valid &= (full_vals[j] == 0)

    candidates = full_vals[:, valid].T  # (n_valid, f_r)
    return candidates


def arc_consistency_pass(cs, row_candidates, row_free_cols_map):
    """One pass of bounds-consistency on cross-row constraints.

    For each cross-row constraint line, compute min/max contribution from ALL
    free cells (tracked + untracked rows), then prune tracked rows' candidates.

    Returns total number of candidates pruned.
    """
    total_pruned = 0

    for li in range(len(cs.targets)):
        if cs.u[li] == 0: continue
        target_residual = cs.rho[li]

        # Collect all free cells on this line, split into tracked and untracked
        tracked = []   # (row, local_col_idx, min_val, max_val)
        untracked_count = 0  # free cells on rows NOT in row_candidates

        for flat in cs.members[li]:
            if flat not in cs.free: continue
            r = flat // S; c = flat % S

            if r in row_candidates and c in row_free_cols_map.get(r, []):
                cands = row_candidates[r]
                fc_list = row_free_cols_map[r]
                local_idx = fc_list.index(c)
                col_vals = cands[:, local_idx]
                tracked.append((r, local_idx, int(col_vals.min()), int(col_vals.max())))
            else:
                # Untracked free cell: could be 0 or 1
                untracked_count += 1

        if not tracked: continue

        # Total bounds including untracked cells (each contributes [0, 1])
        tracked_min = sum(mn for _, _, mn, _ in tracked)
        tracked_max = sum(mx for _, _, _, mx in tracked)
        global_min = tracked_min + 0  # untracked cells contribute min 0
        global_max = tracked_max + untracked_count  # untracked contribute max 1 each

        for r, local_idx, mn, mx in tracked:
            # Other = everything except this row's cell
            other_min = global_min - mn
            other_max = global_max - mx

            cands = row_candidates[r]
            col_vals = cands[:, local_idx]

            # v is valid if other_min <= target_residual - v <= other_max
            v_lo = max(0, target_residual - other_max)
            v_hi = min(1, target_residual - other_min)

            if v_lo > v_hi:
                continue  # constraint is unsatisfiable — don't prune (untracked may resolve)

            keep = np.ones(len(cands), dtype=bool)
            if v_lo == 1: keep &= (col_vals == 1)
            if v_hi == 0: keep &= (col_vals == 0)

            pruned = len(cands) - keep.sum()
            if pruned > 0:
                row_candidates[r] = cands[keep]
                total_pruned += pruned
                if len(row_candidates[r]) == 0:
                    return -1  # infeasible

    return total_pruned


def main():
    parser = argparse.ArgumentParser(description="B.59h: Arc consistency row-CSP solver")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--max-free", type=int, default=MAX_FREE_DEFAULT)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b59h_results.json"))
    args = parser.parse_args()

    data = (REPO_ROOT / "docs" / "testData" / "useless-machine.mp4").read_bytes()
    csm = load_csm(data, args.block)
    density = np.sum(csm) / N

    print(f"B.59h: Arc Consistency Row-CSP Solver (block {args.block}, density {density:.3f})")
    print(f"  max_free: {args.max_free}")
    print()

    cs = ConstraintSystem(csm)
    t0_total = time.time()

    # Phase 1: IntBound
    n_intbound = cs.intbound()
    print(f"Phase 1 (IntBound): {len(cs.known)} cells ({len(cs.known)/N*100:.1f}%)")

    rows_solved = 0
    total_crc_candidates = 0

    # Phase 2+3: Sequential row solve with forward checking
    # Find the tractable row with fewest candidates, try each candidate,
    # propagate, and recurse on the next tractable row.

    def snapshot(cs_obj):
        """Save constraint state for backtracking."""
        return (dict(cs_obj.known), set(cs_obj.free), list(cs_obj.rho), list(cs_obj.u))

    def restore(cs_obj, snap):
        """Restore constraint state."""
        cs_obj.known, cs_obj.free, cs_obj.rho, cs_obj.u = snap[0], snap[1], snap[2], snap[3]

    max_depth = 0

    def solve_sequential(depth=0):
        nonlocal rows_solved, max_depth
        max_depth = max(max_depth, depth)

        if len(cs.free) == 0:
            return True  # genuinely all rows solved

        # Find the tractable row with fewest ACTUAL candidates (not just estimated)
        # Try rows in order of estimated n_free, generate candidates, pick the one
        # with fewest actual candidates
        candidates_by_row = []
        for r in range(S):
            nf = cs.row_n_free(r)
            if nf == 0: continue
            est = max(0, nf - 33)
            if est <= args.max_free:
                candidates_by_row.append((est, r))
        candidates_by_row.sort()

        best_r = -1
        best_cands = None
        best_count = float('inf')
        for est, r in candidates_by_row[:5]:  # check top 5 easiest rows
            cands = generate_candidates(cs, r, args.max_free)
            if cands is not None and 0 < len(cands) < best_count:
                best_r = r; best_cands = cands; best_count = len(cands)
            if best_count <= 10: break  # good enough

        if best_r < 0 or best_cands is None:
            return False  # no tractable row

        cands = best_cands

        f_r = cs.row_n_free(best_r)
        print(f"  {'  '*depth}Depth {depth}: row {best_r} f_r={f_r} "
              f"candidates={len(cands)}")

        # Try each candidate
        for ci, cand in enumerate(cands):
            snap = snapshot(cs)
            cascade = cs.solve_row(best_r, cand.tolist())
            rows_solved += 1

            if depth < 3 or ci % 500 == 0:
                pct = len(cs.known) / N * 100
                if depth < 3:
                    print(f"  {'  '*depth}  trying {ci+1}/{len(cands)}: "
                          f"known={len(cs.known)} ({pct:.1f}%) cascade=+{cascade}")

            # Check feasibility: any constraint violated?
            feasible = True
            for li in range(len(cs.targets)):
                if cs.rho[li] < 0 or cs.rho[li] > cs.u[li]:
                    feasible = False; break

            if feasible and len(cs.free) == 0:
                return True  # fully solved!

            if feasible:
                # Forward check: verify all constraint lines remain feasible
                for li in range(len(cs.targets)):
                    if cs.rho[li] < 0 or cs.rho[li] > cs.u[li]:
                        feasible = False; break

            if feasible:
                # Recurse
                if solve_sequential(depth + 1):
                    return True

            # Backtrack
            rows_solved -= 1
            restore(cs, snap)

        return False  # all candidates exhausted

    print(f"\nPhase 2+3: Sequential row solve with forward checking")
    t_solve = time.time()
    solved_flag = solve_sequential()
    solve_time = time.time() - t_solve
    print(f"\nSolve {'SUCCEEDED' if solved_flag else 'FAILED'} in {solve_time:.1f}s")
    print(f"Max recursion depth: {max_depth}")

    # Final result
    elapsed = time.time() - t0_total
    total_known = len(cs.known)
    solved = total_known == N

    print(f"\n{'='*60}")
    print(f"B.59h Result (block {args.block})")
    print(f"  Determined: {total_known}/{N} ({total_known/N*100:.1f}%)")
    print(f"  Rows solved: {rows_solved}")
    print(f"  Free: {len(cs.free)}")
    print(f"  Solved: {solved}")
    print(f"  Time: {elapsed:.1f}s")

    if solved:
        # Verify SHA-256
        buf = bytearray(S * 16)
        for r in range(S):
            w0, w1 = 0, 0
            for c in range(S):
                if cs.known.get(r * S + c, 0):
                    if c < 64: w0 |= (1 << (63 - c))
                    else: w1 |= (1 << (63 - (c - 64)))
            struct.pack_into(">QQ", buf, r * 16, w0, w1)
        actual_bh = hashlib.sha256(bytes(buf)).digest()

        # Compute expected BH from original CSM
        buf2 = bytearray(S * 16)
        for r in range(S):
            w0, w1 = 0, 0
            for c in range(S):
                if csm[r, c]:
                    if c < 64: w0 |= (1 << (63 - c))
                    else: w1 |= (1 << (63 - (c - 64)))
            struct.pack_into(">QQ", buf2, r * 16, w0, w1)
        expected_bh = hashlib.sha256(bytes(buf2)).digest()

        bh_ok = actual_bh == expected_bh
        print(f"  SHA-256 verified: {bh_ok}")

    result = {
        "experiment": "B.59h",
        "block": args.block,
        "density": round(density, 4),
        "determined": total_known,
        "pct": round(total_known / N * 100, 1),
        "rows_solved": rows_solved,
        "free": len(cs.free),
        "solved": solved,
        "wall_time": round(elapsed, 2),
    }
    Path(args.out).write_text(json.dumps(result, indent=2))
    print(f"  Results: {args.out}")


if __name__ == "__main__":
    main()
