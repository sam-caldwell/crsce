#!/usr/bin/env python3
"""
B.45a: Correct-Path Propagation Depth — 4+4 vs 8-LTP.

Assigns cells in row-major order using the CORRECT CSM values and measures
how many additional cells are forced by propagation at each step. This
simulates the solver's behavior on the correct path (DI=0, no wrong branches)
and shows where the propagation cascade exhausts.

The depth at which propagation stops forcing = the propagation zone boundary.
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import argparse
import pathlib
import struct
import sys
import time
from collections import deque

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.")

S = 511
N = S * S

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

SEEDS_4PLUS4 = [
    0x435253434C545056, 0x435253434C545050,
    0x435253434C545033, 0x435253434C545034,
]
SEEDS_8LTP = [
    0x435253434C545056, 0x435253434C545050,
    0x435253434C545033, 0x435253434C545034,
    0x4352534350415231, 0x4352534350415232,
    0x4352534350415233, 0x4352534350415234,
]


def lcg_next(state):
    return (state * LCG_A + LCG_C) & (LCG_MOD - 1)


def build_fy_partition(seed):
    pool = list(range(N))
    state = seed
    for i in range(N - 1, 0, -1):
        state = lcg_next(state)
        j = int(state % (i + 1))
        pool[i], pool[j] = pool[j], pool[i]
    a = np.empty(N, dtype=np.uint16)
    for line in range(S):
        base = line * S
        for pos in range(S):
            a[pool[base + pos]] = line
    return a


def load_ltpb(path, num_sub=4):
    data = path.read_bytes()
    _, _, _, file_num_sub = struct.unpack_from("<4sIII", data, 0)
    assignments = []
    offset = 16
    for _ in range(min(file_num_sub, num_sub)):
        a = np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy()
        assignments.append(a)
        offset += N * 2
    return assignments


class ConstraintSim:
    """Lightweight constraint propagation simulator."""

    def __init__(self, csm_flat, lines_per_cell, line_cells, line_targets):
        self.csm = csm_flat
        self.lines_per_cell = lines_per_cell
        self.n_lines = len(line_targets)
        self.line_cells = line_cells
        self.line_u = np.array([len(lc) for lc in line_cells], dtype=np.int32)
        self.line_rho = np.array(line_targets, dtype=np.int32)
        self.assigned = np.full(N, -1, dtype=np.int8)
        self.total_forced = 0

    def assign(self, cell, val):
        if self.assigned[cell] != -1:
            return
        self.assigned[cell] = val
        for li in self.lines_per_cell[cell]:
            self.line_u[li] -= 1
            if val == 1:
                self.line_rho[li] -= 1

    def propagate(self):
        """Run forcing cascade. Returns number of newly forced cells."""
        forced = 0
        changed = True
        while changed:
            changed = False
            for li in range(self.n_lines):
                u = self.line_u[li]
                rho = self.line_rho[li]
                if u <= 0:
                    continue
                if rho == 0:
                    for cell in self.line_cells[li]:
                        if self.assigned[cell] == -1:
                            self.assign(cell, 0)
                            forced += 1
                            changed = True
                elif rho == u:
                    for cell in self.line_cells[li]:
                        if self.assigned[cell] == -1:
                            self.assign(cell, 1)
                            forced += 1
                            changed = True
        self.total_forced += forced
        return forced


def build_4plus4(csm, ltp4):
    flat = csm.flatten()
    lpc = [[] for _ in range(N)]
    lc = []
    lt = []
    li = 0
    for r in range(S):
        cells = [r * S + c for c in range(S)]
        lc.append(cells)
        lt.append(int(np.sum(flat[cells])))
        for c in cells: lpc[c].append(li)
        li += 1
    for c in range(S):
        cells = [r * S + c for r in range(S)]
        lc.append(cells)
        lt.append(int(np.sum(flat[cells])))
        for cl in cells: lpc[cl].append(li)
        li += 1
    for d in range(2 * S - 1):
        cells = [r * S + (d - S + 1 + r) for r in range(S) if 0 <= d - S + 1 + r < S]
        lc.append(cells)
        lt.append(int(np.sum(flat[cells])))
        for cl in cells: lpc[cl].append(li)
        li += 1
    for x in range(2 * S - 1):
        cells = [r * S + (x - r) for r in range(S) if 0 <= x - r < S]
        lc.append(cells)
        lt.append(int(np.sum(flat[cells])))
        for cl in cells: lpc[cl].append(li)
        li += 1
    for sub in range(4):
        asn = ltp4[sub]
        llc = [[] for _ in range(S)]
        for cell in range(N): llc[asn[cell]].append(cell)
        for k in range(S):
            lc.append(llc[k])
            lt.append(int(np.sum(flat[llc[k]])))
            for cl in llc[k]: lpc[cl].append(li)
            li += 1
    return lpc, lc, lt


def build_8ltp(csm, ltp8):
    flat = csm.flatten()
    lpc = [[] for _ in range(N)]
    lc = []
    lt = []
    li = 0
    for sub in range(8):
        asn = ltp8[sub]
        llc = [[] for _ in range(S)]
        for cell in range(N): llc[asn[cell]].append(cell)
        for k in range(S):
            lc.append(llc[k])
            lt.append(int(np.sum(flat[llc[k]])))
            for cl in llc[k]: lpc[cl].append(li)
            li += 1
    return lpc, lc, lt


def run_correct_path(csm_flat, lpc, lc, lt, label):
    """Assign cells in row-major order (correct values) and measure propagation."""
    sim = ConstraintSim(csm_flat, lpc, lc, lt)

    # Initial propagation
    init_forced = sim.propagate()
    total_assigned = init_forced  # cells assigned so far (propagation-forced)

    branched = 0
    last_report_row = -1

    for flat_idx in range(N):
        if sim.assigned[flat_idx] != -1:
            continue  # Already assigned by propagation

        # Branch: assign the correct value
        val = int(csm_flat[flat_idx])
        sim.assign(flat_idx, val)
        branched += 1
        total_assigned += 1

        # Propagate from this assignment
        cascade = sim.propagate()
        total_assigned += cascade

        r = flat_idx // S
        if r != last_report_row and r % 50 == 0:
            last_report_row = r
            print(f"  [{label}] row {r}: assigned={total_assigned}/{N} "
                  f"branched={branched} cascade_this={cascade}", flush=True)

    print(f"  [{label}] DONE: total_assigned={total_assigned} branched={branched} "
          f"propagation_forced={sim.total_forced}", flush=True)
    return branched, sim.total_forced


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ltp-file", type=pathlib.Path, default=None)
    parser.add_argument("--csm-seed", type=int, default=42)
    args = parser.parse_args()

    rng = np.random.default_rng(args.csm_seed)
    csm = (rng.random((S, S)) < 0.5).astype(np.uint8)
    flat = csm.flatten()

    if args.ltp_file:
        ltp4 = load_ltpb(args.ltp_file, 4)
    else:
        ltp4 = [build_fy_partition(s) for s in SEEDS_4PLUS4]

    ltp8 = list(ltp4) + [build_fy_partition(s) for s in SEEDS_8LTP[4:]]

    print("=" * 60)
    print("4+4 SYSTEM (baseline)")
    print("=" * 60)
    t0 = time.time()
    lpc4, lc4, lt4 = build_4plus4(csm, ltp4)
    b4, f4 = run_correct_path(flat, lpc4, lc4, lt4, "4+4")
    t4 = time.time() - t0
    print(f"  Time: {t4:.1f}s")

    print()
    print("=" * 60)
    print("8-LTP SYSTEM")
    print("=" * 60)
    t0 = time.time()
    lpc8, lc8, lt8 = build_8ltp(csm, ltp8)
    b8, f8 = run_correct_path(flat, lpc8, lc8, lt8, "8LTP")
    t8 = time.time() - t0
    print(f"  Time: {t8:.1f}s")

    print()
    print("=" * 60)
    print("COMPARISON")
    print("=" * 60)
    print(f"  4+4:  branched={b4:,}  propagation_forced={f4:,}  forced_pct={100*f4/N:.1f}%")
    print(f"  8LTP: branched={b8:,}  propagation_forced={f8:,}  forced_pct={100*f8/N:.1f}%")
    delta = f8 - f4
    print(f"  Delta: {delta:+,} forced cells ({100*delta/max(f4,1):+.1f}%)")


if __name__ == "__main__":
    main()
