#!/usr/bin/env python3
"""
B.60: Row-Column Alternating Algebraic Solver with Dual CRC-32

The REAL B.60 solver. Not "add VH to GaussElim" — that was the lazy B.60b
that rightfully got H4. This solver exploits VH as per-column algebraic
completion, creating a cross-axis cascade:

    Row LH completion → columns lose unknowns → column VH completion
    → rows lose unknowns → row LH completion → ...

Architecture:
    Phase 1: IntBound fixpoint (same 3,600 cells as always)
    Phase 2: Row-column alternating algebraic completion
        - Per-row CRC-32 candidate generation (LH, 33 GF(2) equations)
        - Per-column CRC-32 candidate generation (VH, 33 GF(2) equations)
        - When a row/column has ≤32 unknowns: CRC-32 determines it
        - Cross-axis cascade: completed rows reduce column unknowns and vice versa
    Phase 3: Forward-checking search with VH column pruning
        - Row candidates validated against column CRC-32 consistency
        - When columns reach ≤32 unknowns: algebraic completion as oracle
    Phase 4: SHA-256 block hash verification

Modes:
    --simulate: Assign correct values to show cascade dynamics (fast)
    --solve: Real search with backtracking + VH pruning

Usage:
    python3 tools/b60_real_solver.py --simulate
    python3 tools/b60_real_solver.py --simulate --block 5
    python3 tools/b60_real_solver.py --solve
    python3 tools/b60_real_solver.py --solve --block 0
"""

import argparse
import copy
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
N = S * S
DIAG_COUNT = 2 * S - 1
UNKNOWN = -1


# ---------------------------------------------------------------------------
# CSM loading
# ---------------------------------------------------------------------------
def load_csm_from_file(path, block_idx=0):
    data = Path(path).read_bytes()
    csm = np.zeros((S, S), dtype=np.int8)
    start_bit = block_idx * N
    for i in range(N):
        src_bit = start_bit + i
        src_byte = src_bit // 8
        if src_byte < len(data):
            val = (data[src_byte] >> (7 - (src_bit % 8))) & 1
            csm[i // S, i % S] = val
    return csm


# ---------------------------------------------------------------------------
# CRC-32
# ---------------------------------------------------------------------------
def build_crc32_gen():
    """Build 32x127 GF(2) generator matrix and affine constant."""
    total_bytes = (S + 1 + 7) // 8
    zero_msg = bytes(total_bytes)
    c0 = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = np.array([(c0 >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)

    G = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(total_bytes)
        msg[col // 8] |= (1 << (7 - (col % 8)))
        crc_one = zlib.crc32(bytes(msg)) & 0xFFFFFFFF
        col_val = crc_one ^ c0
        for i in range(32):
            G[i, col] = (col_val >> (31 - i)) & 1
    return G, c_vec


def compute_crc32_vec(bits):
    """CRC-32 of a 127-bit vector (+ 1 pad bit = 128 bits = 16 bytes)."""
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
# Solver state
# ---------------------------------------------------------------------------
class SolverState:
    """Mutable solver state with IntBound propagation and CRC-32 completion."""

    def __init__(self, csm_original):
        self.original = csm_original.copy()
        self.cell = np.full((S, S), UNKNOWN, dtype=np.int8)

        # CRC-32 data
        self.G_crc, self.c_vec = build_crc32_gen()
        self.row_crcs = np.array([compute_crc32_vec(csm_original[r]) for r in range(S)], dtype=np.uint32)
        self.col_crcs = np.array([compute_crc32_vec(csm_original[:, c]) for c in range(S)], dtype=np.uint32)
        self.bh = compute_block_hash(csm_original)

        # Cross-sum targets
        self.row_target = np.array([int(np.sum(csm_original[r])) for r in range(S)], dtype=np.int32)
        self.col_target = np.array([int(np.sum(csm_original[:, c])) for c in range(S)], dtype=np.int32)

        diag_target = np.zeros(DIAG_COUNT, dtype=np.int32)
        anti_target = np.zeros(DIAG_COUNT, dtype=np.int32)
        for r in range(S):
            for c in range(S):
                diag_target[c - r + (S - 1)] += int(csm_original[r, c])
                anti_target[r + c] += int(csm_original[r, c])
        self.diag_target = diag_target
        self.anti_target = anti_target

        # Line residuals and unknown counts
        # Lines: 0..126 = rows, 127..253 = cols, 254..506 = diags, 507..759 = anti-diags
        self.n_lines = S + S + DIAG_COUNT + DIAG_COUNT  # 760
        self.line_rho = np.zeros(self.n_lines, dtype=np.int32)
        self.line_u = np.zeros(self.n_lines, dtype=np.int32)
        self.line_members = [[] for _ in range(self.n_lines)]

        idx = 0
        for r in range(S):
            self.line_rho[idx] = self.row_target[r]
            self.line_u[idx] = S
            self.line_members[idx] = [(r, c) for c in range(S)]
            idx += 1
        for c in range(S):
            self.line_rho[idx] = self.col_target[c]
            self.line_u[idx] = S
            self.line_members[idx] = [(r, c) for r in range(S)]
            idx += 1
        for d in range(DIAG_COUNT):
            offset = d - (S - 1)
            mems = []
            for r in range(S):
                cc = r + offset
                if 0 <= cc < S:
                    mems.append((r, cc))
            self.line_rho[idx] = self.diag_target[d]
            self.line_u[idx] = len(mems)
            self.line_members[idx] = mems
            idx += 1
        for x in range(DIAG_COUNT):
            mems = []
            for r in range(S):
                cc = x - r
                if 0 <= cc < S:
                    mems.append((r, cc))
            self.line_rho[idx] = self.anti_target[x]
            self.line_u[idx] = len(mems)
            self.line_members[idx] = mems
            idx += 1

        # Cell-to-line index
        self.cell_lines = [[0] * 0 for _ in range(N)]
        for li in range(self.n_lines):
            for (r, c) in self.line_members[li]:
                self.cell_lines[r * S + c].append(li)

    def assign(self, r, c, v):
        """Assign cell (r,c) = v. Update line stats."""
        assert self.cell[r, c] == UNKNOWN
        self.cell[r, c] = v
        for li in self.cell_lines[r * S + c]:
            self.line_u[li] -= 1
            self.line_rho[li] -= v

    def row_unknowns(self, r):
        return int(np.sum(self.cell[r] == UNKNOWN))

    def col_unknowns(self, c):
        return int(np.sum(self.cell[:, c] == UNKNOWN))

    def total_known(self):
        return int(np.sum(self.cell >= 0))

    def intbound(self):
        """IntBound propagation. Returns count of newly forced cells."""
        forced = 0
        queue = list(range(self.n_lines))
        in_queue = set(queue)

        while queue:
            li = queue.pop(0)
            in_queue.discard(li)

            u = self.line_u[li]
            rho = self.line_rho[li]
            if u == 0:
                continue
            if rho < 0 or rho > u:
                return -1  # inconsistent

            fv = None
            if rho == 0:
                fv = 0
            elif rho == u:
                fv = 1
            if fv is None:
                continue

            for (r, c) in self.line_members[li]:
                if self.cell[r, c] != UNKNOWN:
                    continue
                self.assign(r, c, fv)
                forced += 1
                for li2 in self.cell_lines[r * S + c]:
                    if li2 not in in_queue:
                        queue.append(li2)
                        in_queue.add(li2)

        return forced

    def snapshot(self):
        """Save mutable state for backtracking."""
        return (
            self.cell.copy(),
            self.line_rho.copy(),
            self.line_u.copy(),
        )

    def restore(self, snap):
        """Restore from snapshot."""
        self.cell, self.line_rho, self.line_u = snap[0], snap[1], snap[2]

    # ------------------------------------------------------------------
    # CRC-32 candidate generation (works for both rows and columns)
    # ------------------------------------------------------------------
    def _generate_candidates(self, axis, idx, max_free=22, max_candidates=50000):
        """Generate CRC-32 + sum-filtered candidates for a row or column.

        axis: 'row' or 'col'
        idx: row or column index
        Returns: (free_positions, candidates) or None if intractable.
            free_positions: list of (r,c) tuples for the free cells
            candidates: list of uint8 arrays (values for free cells)
        """
        if axis == 'row':
            crc = self.row_crcs[idx]
            free_pos = [(idx, c) for c in range(S) if self.cell[idx, c] == UNKNOWN]
            known_vals = [(c, int(self.cell[idx, c])) for c in range(S) if self.cell[idx, c] >= 0]
            # Bit position in CRC message = column index for rows
            free_bits = [c for (_, c) in free_pos]
            known_bits = known_vals  # (bit_position, value)
            rho = int(self.line_rho[idx])  # row line = idx
        else:
            crc = self.col_crcs[idx]
            free_pos = [(r, idx) for r in range(S) if self.cell[r, idx] == UNKNOWN]
            known_vals = [(r, int(self.cell[r, idx])) for r in range(S) if self.cell[r, idx] >= 0]
            # Bit position in CRC message = row index for columns
            free_bits = [r for (r, _) in free_pos]
            known_bits = known_vals
            rho = int(self.line_rho[S + idx]) if axis == 'col' else int(self.line_rho[idx])

        f = len(free_pos)
        if f == 0:
            return free_pos, []

        # Build 33 x f GF(2) system: 32 CRC + 1 parity
        G = np.zeros((33, f), dtype=np.uint8)
        target = np.zeros(33, dtype=np.uint8)

        for i in range(32):
            t = ((crc >> (31 - i)) & 1) ^ int(self.c_vec[i])
            for (bp, v) in known_bits:
                t ^= int(self.G_crc[i, bp]) * v
            target[i] = t & 1
            for j, bp in enumerate(free_bits):
                G[i, j] = self.G_crc[i, bp]

        # Parity equation
        target[32] = rho % 2
        for j in range(f):
            G[32, j] = 1

        # GF(2) Gaussian elimination
        pivotCol = [-1] * 33
        pivotRow = 0
        for col in range(f):
            if pivotRow >= 33:
                break
            found = -1
            for rr in range(pivotRow, 33):
                if G[rr, col]:
                    found = rr
                    break
            if found < 0:
                continue
            if found != pivotRow:
                G[[pivotRow, found]] = G[[found, pivotRow]]
                target[pivotRow], target[found] = target[found], target[pivotRow]
            for rr in range(33):
                if rr != pivotRow and G[rr, col]:
                    G[rr] ^= G[pivotRow]
                    target[rr] ^= target[pivotRow]
            pivotCol[pivotRow] = col
            pivotRow += 1

        rank = pivotRow

        # Consistency check
        for rr in range(rank, 33):
            if np.any(G[rr, :f]):
                continue
            if target[rr]:
                return free_pos, []  # inconsistent

        n_free = f - rank
        if n_free > max_free:
            return None  # intractable

        # Identify free variable indices
        pivot_set = set(pivotCol[i] for i in range(rank) if pivotCol[i] >= 0)
        free_idx = np.array([j for j in range(f) if j not in pivot_set], dtype=np.int32)
        pivot_idx = np.array([pivotCol[i] for i in range(rank) if pivotCol[i] >= 0], dtype=np.int32)
        pivot_ranks = np.array([i for i in range(rank) if pivotCol[i] >= 0], dtype=np.int32)

        # --- Vectorized enumeration of 2^n_free candidates ---
        n_cand = 1 << n_free

        # Build all free-variable assignments: (n_cand x n_free) matrix
        if n_free > 0:
            fa_range = np.arange(n_cand, dtype=np.uint32)
            free_assignments = np.zeros((n_cand, n_free), dtype=np.uint8)
            for fi in range(n_free):
                free_assignments[:, fi] = (fa_range >> fi) & 1
        else:
            free_assignments = np.zeros((1, 0), dtype=np.uint8)
            n_cand = 1

        # Build full value matrix: (n_cand x f)
        all_vals = np.zeros((n_cand, f), dtype=np.uint8)

        # Set free variables
        for fi in range(n_free):
            all_vals[:, free_idx[fi]] = free_assignments[:, fi]

        # Back-substitution for pivot values (vectorized)
        # For pivot row i with pivot column pc:
        #   vals[pc] = target[i] XOR sum(G[i, free_idx[fi]] * vals[free_idx[fi]])
        G_free = G[pivot_ranks][:, free_idx] if n_free > 0 else np.zeros((len(pivot_ranks), 0), dtype=np.uint8)
        target_pivots = target[pivot_ranks]

        if n_free > 0:
            # Matrix multiply over GF(2): (n_pivots x n_free) @ (n_free x n_cand) → (n_pivots x n_cand)
            free_contrib = (G_free @ free_assignments.T) % 2  # (n_pivots x n_cand)
            pivot_vals = (target_pivots[:, None] ^ free_contrib) % 2  # (n_pivots x n_cand)
        else:
            pivot_vals = target_pivots[:, None].repeat(n_cand, axis=1)

        for pi, pc in enumerate(pivot_idx):
            all_vals[:, pc] = pivot_vals[pi]

        # Row/column sum filter (vectorized)
        row_sums = np.sum(all_vals, axis=1, dtype=np.int32)
        sum_mask = row_sums == rho

        # Per-cell bounds filter (vectorized)
        # Precompute can_be_0 and can_be_1 for each free position
        can_be_0 = np.ones(f, dtype=bool)
        can_be_1 = np.ones(f, dtype=bool)
        for j in range(f):
            r, c = free_pos[j]
            for li in self.cell_lines[r * S + c]:
                lr = int(self.line_rho[li])
                lu = int(self.line_u[li])
                if lr > lu - 1:
                    can_be_0[j] = False
                if lr - 1 < 0 or lr - 1 > lu - 1:
                    can_be_1[j] = False

        # Apply bounds: for each candidate, check all cells
        bounds_mask = np.ones(n_cand, dtype=bool)
        for j in range(f):
            if not can_be_0[j]:
                bounds_mask &= (all_vals[:, j] != 0)
            if not can_be_1[j]:
                bounds_mask &= (all_vals[:, j] != 1)

        # Combined filter
        valid = sum_mask & bounds_mask
        valid_vals = all_vals[valid]

        if len(valid_vals) > max_candidates:
            return None

        candidates = [valid_vals[i] for i in range(len(valid_vals))]
        return free_pos, candidates

    def generate_row_candidates(self, r, **kwargs):
        return self._generate_candidates('row', r, **kwargs)

    def generate_col_candidates(self, c, **kwargs):
        return self._generate_candidates('col', c, **kwargs)

    def try_complete_columns(self, threshold=40):
        """Try to algebraically complete columns with ≤threshold unknowns.

        Returns (n_completed, n_forced_cells, inconsistent).
        """
        total_completed = 0
        total_forced = 0

        changed = True
        while changed:
            changed = False
            for c in range(S):
                u = self.col_unknowns(c)
                if u == 0 or u > threshold:
                    continue

                result = self.generate_col_candidates(c)
                if result is None:
                    continue  # intractable
                free_pos, candidates = result

                if len(candidates) == 0:
                    return total_completed, total_forced, True  # inconsistent

                if len(candidates) == 1:
                    # Column uniquely determined — assign all free cells
                    for j, (r, cc) in enumerate(free_pos):
                        if self.cell[r, cc] == UNKNOWN:
                            self.assign(r, cc, int(candidates[0][j]))
                            total_forced += 1
                    total_completed += 1

                    # IntBound cascade
                    ib = self.intbound()
                    if ib < 0:
                        return total_completed, total_forced, True
                    total_forced += ib
                    changed = True

        return total_completed, total_forced, False

    def try_complete_rows(self, threshold=40):
        """Try to algebraically complete rows with ≤threshold unknowns.

        Returns (n_completed, n_forced_cells, inconsistent).
        """
        total_completed = 0
        total_forced = 0

        changed = True
        while changed:
            changed = False
            for r in range(S):
                u = self.row_unknowns(r)
                if u == 0 or u > threshold:
                    continue

                result = self.generate_row_candidates(r)
                if result is None:
                    continue
                free_pos, candidates = result

                if len(candidates) == 0:
                    return total_completed, total_forced, True

                if len(candidates) == 1:
                    for j, (rr, cc) in enumerate(free_pos):
                        if self.cell[rr, cc] == UNKNOWN:
                            self.assign(rr, cc, int(candidates[0][j]))
                            total_forced += 1
                    total_completed += 1

                    ib = self.intbound()
                    if ib < 0:
                        return total_completed, total_forced, True
                    total_forced += ib
                    changed = True

        return total_completed, total_forced, False


# ---------------------------------------------------------------------------
# Simulation: assign correct values, measure cascade dynamics
# ---------------------------------------------------------------------------
def run_simulation(csm, block_label):
    """Assign correct row values one at a time. After each, try column completion."""
    print(f"\n{'=' * 72}")
    print(f"B.60 SIMULATION (correct values) — {block_label}")
    print(f"{'=' * 72}")

    density = float(np.sum(csm)) / N
    print(f"  Density: {density:.3f}")

    st = SolverState(csm)
    t0 = time.time()

    # Phase 1: IntBound
    ib = st.intbound()
    print(f"  IntBound: {ib} cells forced ({st.total_known()} total, {st.total_known()/N*100:.1f}%)")
    print()

    # Phase 2: Row-column alternating completion (simulation — use correct values)
    rows_solved = 0
    cols_completed_total = 0
    log = []

    # First, try algebraic completion without any row assignment
    rc, rf, inc = st.try_complete_rows()
    cc, cf, inc2 = st.try_complete_columns()
    if rc > 0 or cc > 0:
        print(f"  Pre-search algebraic: {rc} rows, {cc} cols completed ({st.total_known()} total)")

    print(f"\n  {'Rows':>4s} {'ColsDone':>8s} {'Known':>7s} {'%':>6s} "
          f"{'RowU':>5s} {'MinColU':>7s} {'Cascade':>7s} {'NewCols':>7s}")
    print(f"  {'-'*60}")

    while st.total_known() < N:
        # Find row with fewest unknowns (that has unknowns)
        best_r = -1
        best_u = 999
        for r in range(S):
            u = st.row_unknowns(r)
            if 0 < u < best_u:
                best_u = u
                best_r = r
        if best_r < 0:
            break

        # Assign correct values for this row
        newly_assigned = 0
        for c in range(S):
            if st.cell[best_r, c] == UNKNOWN:
                st.assign(best_r, c, int(csm[best_r, c]))
                newly_assigned += 1
        rows_solved += 1

        # IntBound cascade
        cascade = st.intbound()
        if cascade < 0:
            print(f"  INCONSISTENCY at row {rows_solved}")
            break

        # Try algebraic row completion (rows that dropped to ≤32)
        rc, rf, inc = st.try_complete_rows()
        if inc:
            print(f"  INCONSISTENCY in row completion at row {rows_solved}")
            break

        # Try algebraic column completion (columns that dropped to ≤32)
        cc, cf, inc = st.try_complete_columns()
        if inc:
            print(f"  INCONSISTENCY in column completion at row {rows_solved}")
            break
        cols_completed_total += cc

        # Stats
        min_col_u = min((st.col_unknowns(c) for c in range(S) if st.col_unknowns(c) > 0), default=0)

        entry = {
            'rows_solved': rows_solved,
            'cols_completed': cols_completed_total,
            'total_known': st.total_known(),
            'row_u': best_u,
            'min_col_u': min_col_u,
            'cascade': cascade + rf + cf,
            'new_cols': cc,
        }
        log.append(entry)

        pct = st.total_known() / N * 100
        print(f"  {rows_solved:4d} {cols_completed_total:8d} {st.total_known():7d} {pct:5.1f}% "
              f"{best_u:5d} {min_col_u:7d} {cascade+rf+cf:7d} {cc:7d}")

        if st.total_known() >= N:
            break

    elapsed = time.time() - t0
    print(f"\n  Simulation complete in {elapsed:.1f}s")
    print(f"  Rows solved (correct assignment): {rows_solved}")
    print(f"  Columns algebraically completed (VH): {cols_completed_total}")
    print(f"  Total known: {st.total_known()} / {N} ({st.total_known()/N*100:.1f}%)")

    if st.total_known() == N:
        recon_bh = compute_block_hash(st.cell)
        if recon_bh == st.bh:
            print(f"  SHA-256: VERIFIED")
        else:
            print(f"  SHA-256: MISMATCH")

    # Key metric: at what row-solve count did columns first become VH-completable?
    first_col = None
    for entry in log:
        if entry['new_cols'] > 0:
            first_col = entry['rows_solved']
            break
    if first_col is not None:
        print(f"\n  CROSS-AXIS CASCADE first triggered at row {first_col}")
    else:
        print(f"\n  CROSS-AXIS CASCADE never triggered (no columns reached ≤32 unknowns)")

    return log


# ---------------------------------------------------------------------------
# Real solver: row-serial search with VH column pruning
# ---------------------------------------------------------------------------
class SearchStats:
    def __init__(self):
        self.candidates_tried = 0
        self.backtracks = 0
        self.max_depth = 0
        self.col_completions = 0
        self.col_prunings = 0
        self.row_completions = 0


def solve_real(csm, block_label, max_time=300):
    """Real B.60 solver with row-serial search + VH column pruning."""
    print(f"\n{'=' * 72}")
    print(f"B.60 REAL SOLVER — {block_label}")
    print(f"{'=' * 72}")

    density = float(np.sum(csm)) / N
    print(f"  Density: {density:.3f}, max_time: {max_time}s")

    st = SolverState(csm)
    stats = SearchStats()
    t0 = time.time()

    # Phase 1: IntBound
    ib = st.intbound()
    print(f"  IntBound: {ib} cells forced ({st.total_known()} total, {st.total_known()/N*100:.1f}%)")

    # Phase 1b: Algebraic completion (rows/cols with ≤32 unknowns)
    rc, rf, _ = st.try_complete_rows()
    cc, cf, _ = st.try_complete_columns()
    if rc > 0 or cc > 0:
        print(f"  Pre-search algebraic: {rc} rows, {cc} cols ({st.total_known()} total)")

    print(f"  Starting search with {st.total_known()} known cells...")

    # Phase 2: Forward-checking search
    solved = _search_recursive(st, stats, 0, t0, max_time)

    elapsed = time.time() - t0
    print(f"\n  Result: {'SOLVED' if solved else 'NOT SOLVED'}")
    print(f"  Max depth:       {stats.max_depth}")
    print(f"  Candidates tried:{stats.candidates_tried}")
    print(f"  Backtracks:      {stats.backtracks}")
    print(f"  Col completions: {stats.col_completions}")
    print(f"  Col prunings:    {stats.col_prunings}")
    print(f"  Row completions: {stats.row_completions}")
    print(f"  Time:            {elapsed:.1f}s")
    print(f"  Total known:     {st.total_known()} / {N}")

    if solved and st.total_known() == N:
        recon_bh = compute_block_hash(st.cell)
        verified = (recon_bh == st.bh)
        print(f"  SHA-256:         {'VERIFIED' if verified else 'MISMATCH'}")

    return solved, stats


def _search_recursive(st, stats, depth, t0, max_time):
    """Row-level forward-checking search with VH column pruning."""
    if time.time() - t0 > max_time:
        return False

    if depth > stats.max_depth:
        stats.max_depth = depth

    # Check if fully solved
    if st.total_known() >= N:
        return True

    # Find the best row to branch on: fewest candidates among tractable rows
    best_row = None
    best_free_pos = None
    best_candidates = None
    best_count = float('inf')

    # Check up to 8 rows with fewest unknowns
    row_by_u = []
    for r in range(S):
        u = st.row_unknowns(r)
        if u > 0:
            row_by_u.append((u, r))
    row_by_u.sort()

    for _, r in row_by_u[:8]:
        result = st.generate_row_candidates(r, max_free=22, max_candidates=20000)
        if result is None:
            continue
        free_pos, candidates = result
        if len(candidates) == 0:
            return False  # this row has no valid candidates → dead end
        if len(candidates) > 8000:
            continue  # skip — too many
        if len(candidates) < best_count:
            best_row = r
            best_free_pos = free_pos
            best_candidates = candidates
            best_count = len(candidates)
            if best_count <= 5:
                break  # good enough

    if best_row is None:
        return False  # no tractable row

    if depth < 20 or depth % 5 == 0:
        elapsed = time.time() - t0
        print(f"  [depth {depth:3d}] row {best_row:3d}: u={st.row_unknowns(best_row)}, "
              f"cands={best_count}, known={st.total_known()}, "
              f"bt={stats.backtracks}, t={elapsed:.0f}s", flush=True)

    # Try each candidate
    for cand in best_candidates:
        stats.candidates_tried += 1
        snap = st.snapshot()

        # Assign this candidate
        for j, (r, c) in enumerate(best_free_pos):
            if st.cell[r, c] == UNKNOWN:
                st.assign(r, c, int(cand[j]))

        # IntBound propagation
        ib = st.intbound()
        if ib < 0:
            stats.backtracks += 1
            st.restore(snap)
            continue

        # Row algebraic completion
        rc, rf, inc = st.try_complete_rows()
        if inc:
            stats.backtracks += 1
            stats.col_prunings += 1
            st.restore(snap)
            continue
        stats.row_completions += rc

        # COLUMN CRC-32 COMPLETION — the key B.60 innovation
        cc, cf, inc = st.try_complete_columns()
        if inc:
            # VH detected inconsistency — prune this candidate
            stats.backtracks += 1
            stats.col_prunings += 1
            st.restore(snap)
            continue
        stats.col_completions += cc

        # Forward check: all lines feasible
        feasible = True
        for li in range(st.n_lines):
            if st.line_rho[li] < 0 or st.line_rho[li] > st.line_u[li]:
                feasible = False
                break
        if not feasible:
            stats.backtracks += 1
            st.restore(snap)
            continue

        # Recurse
        if _search_recursive(st, stats, depth + 1, t0, max_time):
            return True

        stats.backtracks += 1
        st.restore(snap)

    return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.60: Row-column alternating solver")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--simulate", action="store_true", help="Simulation with correct values")
    parser.add_argument("--solve", action="store_true", help="Real search with VH pruning")
    parser.add_argument("--max-time", type=int, default=300, help="Max search time in seconds")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    csm = load_csm_from_file(mp4_path, args.block)
    density = float(np.sum(csm)) / N
    label = f"mp4_block_{args.block} (density={density:.3f})"

    print(f"B.60: Row-Column Alternating Algebraic Solver (S={S})")
    print(f"  Block: {args.block}, density: {density:.3f}")

    if not args.simulate and not args.solve:
        args.simulate = True
        args.solve = True

    results = {}

    if args.simulate:
        sim_log = run_simulation(csm, label)
        results['simulation'] = sim_log

    if args.solve:
        solved, stats = solve_real(csm, label, max_time=args.max_time)
        results['search'] = {
            'solved': solved,
            'max_depth': stats.max_depth,
            'candidates_tried': stats.candidates_tried,
            'backtracks': stats.backtracks,
            'col_completions': stats.col_completions,
            'col_prunings': stats.col_prunings,
            'row_completions': stats.row_completions,
        }

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(results, indent=2, default=str))
    print(f"\nResults: {out_path}")


if __name__ == "__main__":
    main()
