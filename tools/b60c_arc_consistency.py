#!/usr/bin/env python3
"""
B.60c: Iterative Arc Consistency with Dual CRC-32

Progressive domain narrowing:
1. Generate candidate domains for tractable rows (LH) and columns (VH)
2. Extract cell-level determinations (cells where ALL candidates agree)
3. Propagate determinations via IntBound → more rows/cols become tractable
4. AC-3 cross-filtering between row and column domains
5. Repeat until convergence

No known input used in solver decisions. Known input for verification only.

Usage:
    python3 tools/b60c_arc_consistency.py --block 0
    python3 tools/b60c_arc_consistency.py --block 0 --max-free 25
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
DIAG_COUNT = 2 * S - 1
UNKNOWN = -1


# ---------------------------------------------------------------------------
# CSM loading & CRC-32
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
# Constraint state with IntBound
# ---------------------------------------------------------------------------
class ConstraintState:
    def __init__(self, csm):
        self.cell = np.full((S, S), UNKNOWN, dtype=np.int8)
        self.original = csm.copy()
        self.n_lines = S + S + DIAG_COUNT + DIAG_COUNT
        self.line_target = np.zeros(self.n_lines, dtype=np.int32)
        self.line_rho = np.zeros(self.n_lines, dtype=np.int32)
        self.line_u = np.zeros(self.n_lines, dtype=np.int32)
        self.line_members = [[] for _ in range(self.n_lines)]
        self.cell_lines = [[] for _ in range(N)]

        idx = 0
        for r in range(S):
            t = int(np.sum(csm[r, :]))
            self.line_target[idx] = t; self.line_rho[idx] = t; self.line_u[idx] = S
            self.line_members[idx] = [(r, c) for c in range(S)]; idx += 1
        for c in range(S):
            t = int(np.sum(csm[:, c]))
            self.line_target[idx] = t; self.line_rho[idx] = t; self.line_u[idx] = S
            self.line_members[idx] = [(r, c) for r in range(S)]; idx += 1
        for d in range(DIAG_COUNT):
            offset = d - (S - 1); mems = []; t = 0
            for r in range(S):
                cc = r + offset
                if 0 <= cc < S: t += int(csm[r, cc]); mems.append((r, cc))
            self.line_target[idx] = t; self.line_rho[idx] = t
            self.line_u[idx] = len(mems); self.line_members[idx] = mems; idx += 1
        for x in range(DIAG_COUNT):
            mems = []; t = 0
            for r in range(S):
                cc = x - r
                if 0 <= cc < S: t += int(csm[r, cc]); mems.append((r, cc))
            self.line_target[idx] = t; self.line_rho[idx] = t
            self.line_u[idx] = len(mems); self.line_members[idx] = mems; idx += 1
        for li in range(self.n_lines):
            for (r, c) in self.line_members[li]:
                self.cell_lines[r * S + c].append(li)

    def assign(self, r, c, v):
        assert self.cell[r, c] == UNKNOWN, f"cell ({r},{c}) already assigned"
        self.cell[r, c] = v
        for li in self.cell_lines[r * S + c]:
            self.line_u[li] -= 1; self.line_rho[li] -= v

    def intbound(self):
        forced = 0
        queue = list(range(self.n_lines))
        in_queue = set(queue)
        while queue:
            li = queue.pop(0); in_queue.discard(li)
            u, rho = int(self.line_u[li]), int(self.line_rho[li])
            if u == 0: continue
            if rho < 0 or rho > u: return -1
            fv = None
            if rho == 0: fv = 0
            elif rho == u: fv = 1
            if fv is None: continue
            for (r, c) in self.line_members[li]:
                if self.cell[r, c] != UNKNOWN: continue
                self.assign(r, c, fv); forced += 1
                for li2 in self.cell_lines[r * S + c]:
                    if li2 not in in_queue: queue.append(li2); in_queue.add(li2)
        return forced

    def row_unknowns(self, r):
        return int(np.sum(self.cell[r] == UNKNOWN))

    def col_unknowns(self, c):
        return int(np.sum(self.cell[:, c] == UNKNOWN))

    def total_known(self):
        return int(np.sum(self.cell >= 0))


# ---------------------------------------------------------------------------
# Vectorized candidate generation
# ---------------------------------------------------------------------------
def generate_candidates(G_crc, c_vec, crc_val, free_positions, known_positions,
                        rho, max_free=22, max_cands=500000):
    f = len(free_positions)
    if f == 0: return []

    G = np.zeros((33, f), dtype=np.uint8)
    target = np.zeros(33, dtype=np.uint8)
    for i in range(32):
        t = ((crc_val >> (31 - i)) & 1) ^ int(c_vec[i])
        for (bp, v) in known_positions:
            t ^= int(G_crc[i, bp]) * v
        target[i] = t & 1
        for j, bp in enumerate(free_positions):
            G[i, j] = G_crc[i, bp]
    target[32] = rho % 2
    G[32, :f] = 1

    # GaussElim
    pivotCol = [-1] * 33; pivotRow = 0
    for col in range(f):
        if pivotRow >= 33: break
        found = -1
        for rr in range(pivotRow, 33):
            if G[rr, col]: found = rr; break
        if found < 0: continue
        if found != pivotRow:
            G[[pivotRow, found]] = G[[found, pivotRow]]
            target[pivotRow], target[found] = target[found], target[pivotRow]
        for rr in range(33):
            if rr != pivotRow and G[rr, col]:
                G[rr] ^= G[pivotRow]; target[rr] ^= target[pivotRow]
        pivotCol[pivotRow] = col; pivotRow += 1

    rank = pivotRow
    for rr in range(rank, 33):
        if not np.any(G[rr, :f]) and target[rr]: return []

    n_free = f - rank
    if n_free > max_free: return None

    pivot_set = set(pivotCol[i] for i in range(rank) if pivotCol[i] >= 0)
    free_idx = np.array([j for j in range(f) if j not in pivot_set], dtype=np.int32)
    pivot_idx = np.array([pivotCol[i] for i in range(rank) if pivotCol[i] >= 0], dtype=np.int32)
    pivot_ranks = np.array([i for i in range(rank) if pivotCol[i] >= 0], dtype=np.int32)

    n_cand = 1 << n_free
    if n_cand > max_cands: return None

    if n_free > 0:
        fa_range = np.arange(n_cand, dtype=np.uint32)
        free_asgn = np.zeros((n_cand, n_free), dtype=np.uint8)
        for fi in range(n_free): free_asgn[:, fi] = (fa_range >> fi) & 1
    else:
        free_asgn = np.zeros((1, 0), dtype=np.uint8); n_cand = 1

    all_vals = np.zeros((n_cand, f), dtype=np.uint8)
    for fi in range(n_free): all_vals[:, free_idx[fi]] = free_asgn[:, fi]

    G_free = G[pivot_ranks][:, free_idx] if n_free > 0 else np.zeros((len(pivot_ranks), 0), dtype=np.uint8)
    tp = target[pivot_ranks]
    if n_free > 0:
        fc = (G_free @ free_asgn.T) % 2
        pv = (tp[:, None] ^ fc) % 2
    else:
        pv = tp[:, None].repeat(n_cand, axis=1)
    for pi, pc in enumerate(pivot_idx): all_vals[:, pc] = pv[pi]

    sums = np.sum(all_vals, axis=1, dtype=np.int32)
    valid = all_vals[sums == rho]
    return [valid[i] for i in range(len(valid))]


# ---------------------------------------------------------------------------
# Cell-level determinations from domain
# ---------------------------------------------------------------------------
def extract_determined_cells(domain, free_positions):
    """Find cells where ALL candidates agree on the value."""
    if not domain or len(domain) <= 1:
        if len(domain) == 1:
            return {pos: int(domain[0][j]) for j, pos in enumerate(free_positions)}
        return {}

    determined = {}
    cands = np.array(domain, dtype=np.uint8)
    for j, pos in enumerate(free_positions):
        col_vals = cands[:, j]
        if np.all(col_vals == 0):
            determined[pos] = 0
        elif np.all(col_vals == 1):
            determined[pos] = 1
    return determined


def filter_domain_by_cell(domain, free_positions, cell_pos, value):
    """Remove candidates inconsistent with cell_pos = value."""
    if not domain: return domain
    idx = None
    for j, pos in enumerate(free_positions):
        if pos == cell_pos: idx = j; break
    if idx is None: return domain  # cell not in this domain
    return [c for c in domain if c[idx] == value]


# ---------------------------------------------------------------------------
# Main solver loop
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.60c: Iterative AC with dual CRC-32")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--max-free", type=int, default=25)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60c_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    csm = load_csm(mp4_path, args.block)
    density = float(np.sum(csm)) / N

    print(f"B.60c: Iterative Arc Consistency (S={S}, max_free={args.max_free})")
    print(f"  Block: {args.block}, density: {density:.3f}")

    G_crc, c_vec = build_crc32_gen()
    row_crcs = [compute_crc32(csm[r]) for r in range(S)]
    col_crcs = [compute_crc32(csm[:, c]) for c in range(S)]
    bh = compute_block_hash(csm)

    cs = ConstraintState(csm)
    ib = cs.intbound()
    print(f"  IntBound: {ib} cells ({cs.total_known()}/{N})\n")

    # Iterative domain generation + determination extraction + propagation
    total_determined_from_domains = 0
    iteration = 0

    while True:
        iteration += 1
        print(f"{'='*60}")
        print(f"Iteration {iteration}: {cs.total_known()}/{N} known ({cs.total_known()/N*100:.1f}%)")
        print(f"{'='*60}")

        if cs.total_known() >= N:
            print("  ALL CELLS KNOWN")
            break

        # Generate row domains for tractable rows
        row_domains = {}  # r -> (free_cols, candidates)
        t0 = time.time()
        for r in range(S):
            u = cs.row_unknowns(r)
            if u == 0: continue
            fc = [c for c in range(S) if cs.cell[r, c] == UNKNOWN]
            known = [(c, int(cs.cell[r, c])) for c in range(S) if cs.cell[r, c] >= 0]
            rho = int(cs.line_rho[r])
            cands = generate_candidates(G_crc, c_vec, row_crcs[r], fc, known, rho,
                                        max_free=args.max_free)
            if cands is None: continue
            if len(cands) == 0:
                print(f"  ROW {r} INCONSISTENT (0 candidates)")
                return
            row_domains[r] = (fc, cands)

        # Generate col domains for tractable cols
        col_domains = {}  # c -> (free_rows, candidates)
        for c in range(S):
            u = cs.col_unknowns(c)
            if u == 0: continue
            fr = [r for r in range(S) if cs.cell[r, c] == UNKNOWN]
            known = [(r, int(cs.cell[r, c])) for r in range(S) if cs.cell[r, c] >= 0]
            rho = int(cs.line_rho[S + c])
            cands = generate_candidates(G_crc, c_vec, col_crcs[c], fr, known, rho,
                                        max_free=args.max_free)
            if cands is None: continue
            if len(cands) == 0:
                print(f"  COL {c} INCONSISTENT (0 candidates)")
                return
            col_domains[c] = (fr, cands)

        t_gen = time.time() - t0
        total_row_cands = sum(len(v[1]) for v in row_domains.values())
        total_col_cands = sum(len(v[1]) for v in col_domains.values())
        print(f"  Domains: {len(row_domains)} rows ({total_row_cands} cands), "
              f"{len(col_domains)} cols ({total_col_cands} cands) [{t_gen:.1f}s]")

        if not row_domains and not col_domains:
            print("  No tractable rows or columns. Stuck.")
            break

        # AC-3: cross-filter row and column domains
        t0 = time.time()
        ac3_changed = True
        ac3_iters = 0
        while ac3_changed:
            ac3_changed = False
            ac3_iters += 1

            # Column domains prune row domains
            for c, (fr, col_cands) in list(col_domains.items()):
                if not col_cands: continue
                # What values can each row-position take in this column?
                cands_arr = np.array(col_cands, dtype=np.uint8)
                for j, r in enumerate(fr):
                    if r not in row_domains: continue
                    row_fc, row_cands = row_domains[r]
                    if c not in row_fc: continue
                    ci = row_fc.index(c)
                    can_be_0 = bool(np.any(cands_arr[:, j] == 0))
                    can_be_1 = bool(np.any(cands_arr[:, j] == 1))
                    if can_be_0 and can_be_1: continue
                    new_cands = [rc for rc in row_cands if
                                 (rc[ci] == 0 and can_be_0) or (rc[ci] == 1 and can_be_1)]
                    if len(new_cands) < len(row_cands):
                        ac3_changed = True
                        row_domains[r] = (row_fc, new_cands)
                        if not new_cands:
                            print(f"  AC-3: row {r} emptied by col {c}")
                            return

            # Row domains prune column domains
            for r, (fc, row_cands) in list(row_domains.items()):
                if not row_cands: continue
                cands_arr = np.array(row_cands, dtype=np.uint8)
                for j, c in enumerate(fc):
                    if c not in col_domains: continue
                    col_fr, col_cands = col_domains[c]
                    if r not in col_fr: continue
                    ri = col_fr.index(r)
                    can_be_0 = bool(np.any(cands_arr[:, j] == 0))
                    can_be_1 = bool(np.any(cands_arr[:, j] == 1))
                    if can_be_0 and can_be_1: continue
                    new_cands = [cc for cc in col_cands if
                                 (cc[ri] == 0 and can_be_0) or (cc[ri] == 1 and can_be_1)]
                    if len(new_cands) < len(col_cands):
                        ac3_changed = True
                        col_domains[c] = (col_fr, new_cands)
                        if not new_cands:
                            print(f"  AC-3: col {c} emptied by row {r}")
                            return

        t_ac3 = time.time() - t0
        total_row_after = sum(len(v[1]) for v in row_domains.values())
        total_col_after = sum(len(v[1]) for v in col_domains.values())
        print(f"  AC-3: {ac3_iters} iters, rows {total_row_cands}->{total_row_after}, "
              f"cols {total_col_cands}->{total_col_after} [{t_ac3:.1f}s]")

        # Extract cell-level determinations from domains
        newly_determined = {}

        for r, (fc, cands) in row_domains.items():
            det = extract_determined_cells(cands, fc)
            for pos, val in det.items():
                rr, cc = r, pos
                if cs.cell[rr, cc] == UNKNOWN:
                    newly_determined[(rr, cc)] = val

        for c, (fr, cands) in col_domains.items():
            det = extract_determined_cells(cands, fr)
            for pos, val in det.items():
                rr, cc = pos, c
                if cs.cell[rr, cc] == UNKNOWN:
                    if (rr, cc) in newly_determined:
                        if newly_determined[(rr, cc)] != val:
                            print(f"  CONFLICT at ({rr},{cc}): row says {newly_determined[(rr,cc)]}, col says {val}")
                            return
                    else:
                        newly_determined[(rr, cc)] = val

        if not newly_determined:
            # Try assigning unique rows/cols directly
            unique_assigned = 0
            for r, (fc, cands) in row_domains.items():
                if len(cands) == 1:
                    for j, c in enumerate(fc):
                        if cs.cell[r, c] == UNKNOWN:
                            newly_determined[(r, c)] = int(cands[0][j])
                            unique_assigned += 1
            for c, (fr, cands) in col_domains.items():
                if len(cands) == 1:
                    for j, r in enumerate(fr):
                        if cs.cell[r, c] == UNKNOWN:
                            if (r, c) not in newly_determined:
                                newly_determined[(r, c)] = int(cands[0][j])
                                unique_assigned += 1
            if unique_assigned > 0:
                print(f"  Unique domains: {unique_assigned} cells")

        if not newly_determined:
            # Report domain sizes and stop
            row_sizes = sorted([len(v[1]) for v in row_domains.values()])
            col_sizes = sorted([len(v[1]) for v in col_domains.values()])
            print(f"  No new determinations. Domains stuck.")
            if row_sizes:
                print(f"  Row domain sizes: {row_sizes[:10]}{'...' if len(row_sizes)>10 else ''}")
            if col_sizes:
                print(f"  Col domain sizes: {col_sizes[:10]}{'...' if len(col_sizes)>10 else ''}")
            break

        # Apply determinations
        for (r, c), v in newly_determined.items():
            if cs.cell[r, c] == UNKNOWN:
                cs.assign(r, c, v)
        total_determined_from_domains += len(newly_determined)
        print(f"  Determined: {len(newly_determined)} cells -> {cs.total_known()}/{N} "
              f"({cs.total_known()/N*100:.1f}%)")

        # IntBound cascade
        ib = cs.intbound()
        if ib < 0:
            print(f"  INCONSISTENCY in IntBound after determination")
            return
        if ib > 0:
            print(f"  IntBound cascade: +{ib} cells -> {cs.total_known()}/{N} "
                  f"({cs.total_known()/N*100:.1f}%)")

    # Final result
    print(f"\n{'='*60}")
    print(f"B.60c Final Result")
    print(f"{'='*60}")
    print(f"  Total known: {cs.total_known()} / {N} ({cs.total_known()/N*100:.1f}%)")
    print(f"  Cells from domain analysis: {total_determined_from_domains}")
    print(f"  Iterations: {iteration}")

    if cs.total_known() == N:
        recon_bh = compute_block_hash(cs.cell)
        verified = (recon_bh == bh)
        print(f"  SHA-256: {'VERIFIED' if verified else 'MISMATCH'}")

    # Save
    result = {
        "experiment": "B.60c",
        "block": args.block,
        "density": round(density, 4),
        "total_known": cs.total_known(),
        "pct": round(cs.total_known() / N * 100, 2),
        "determined_from_domains": total_determined_from_domains,
        "iterations": iteration,
        "max_free": args.max_free,
    }
    Path(args.out).write_text(json.dumps(result, indent=2))
    print(f"  Results: {args.out}")


if __name__ == "__main__":
    main()
