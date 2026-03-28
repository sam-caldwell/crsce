#!/usr/bin/env python3
"""
B.45a: LTP-Only Constraint System — Propagation Depth Simulation.

Compares the propagation cascade depth of two constraint system configurations:
  1. Baseline 4+4: 4 geometric (LSM+VSM+DSM+XSM) + 4 LTP
  2. 8-LTP: 8 Fisher-Yates LTP sub-tables, no geometric families

For each configuration, constructs the constraint store from a known CSM,
runs greedy propagation (no DFS, just cascade forcing), and measures how
many cells are determined before the cascade exhausts.

This gives the propagation-zone depth — the number of cells forced before
the solver would need to branch. The meeting band begins where propagation
stops.

Usage:
    python3 tools/b45a_ltp_only_sim.py
    python3 tools/b45a_ltp_only_sim.py --ltp-file tools/b38e_t31000000_best_s137.bin
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
    _, _, s_field, file_num_sub = struct.unpack_from("<4sIII", data, 0)
    assignments = []
    offset = 16
    for _ in range(min(file_num_sub, num_sub)):
        a = np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy()
        assignments.append(a)
        offset += N * 2
    return assignments


def simulate_propagation(csm_flat, lines_per_cell, line_cells, line_targets):
    """
    Simulate constraint propagation on a binary CSM.

    lines_per_cell[flat] = list of line indices this cell participates in
    line_cells[line_idx] = list of flat cell indices on this line
    line_targets[line_idx] = target sum for this line

    Returns the number of cells forced by propagation cascade.
    """
    n_lines = len(line_targets)
    n_cells = len(csm_flat)

    # Per-line state
    line_u = np.array([len(line_cells[i]) for i in range(n_lines)], dtype=np.int32)
    line_a = np.zeros(n_lines, dtype=np.int32)  # assigned-ones count
    line_rho = np.array(line_targets, dtype=np.int32)  # residual = target - assigned

    # Per-cell state
    assigned = np.zeros(n_cells, dtype=np.int8)  # -1=unassigned initially, 0 or 1 when assigned
    assigned[:] = -1

    forced = 0
    queue = deque()

    def try_force_line(line_idx):
        """Check if line can force unknowns."""
        nonlocal forced
        rho = line_rho[line_idx]
        u = line_u[line_idx]
        if u <= 0:
            return
        if rho == 0:
            # Force all unknowns to 0
            for cell in line_cells[line_idx]:
                if assigned[cell] == -1:
                    queue.append((cell, 0))
        elif rho == u:
            # Force all unknowns to 1
            for cell in line_cells[line_idx]:
                if assigned[cell] == -1:
                    queue.append((cell, 1))

    def assign_cell(cell, val):
        """Assign a cell and update line stats."""
        nonlocal forced
        if assigned[cell] != -1:
            return  # Already assigned
        assigned[cell] = val
        forced += 1
        for line_idx in lines_per_cell[cell]:
            line_u[line_idx] -= 1
            if val == 1:
                line_a[line_idx] += 1
                line_rho[line_idx] -= 1

    # Initial: queue all lines for forcing check
    for line_idx in range(n_lines):
        try_force_line(line_idx)

    # Propagation loop
    while queue:
        cell, val = queue.popleft()
        if assigned[cell] != -1:
            continue
        assign_cell(cell, val)
        # Check all lines through this cell
        for line_idx in lines_per_cell[cell]:
            try_force_line(line_idx)

    return forced


def build_4plus4_system(csm, ltp4):
    """Build lines for 4 geometric + 4 LTP families."""
    flat = csm.flatten()
    lines_per_cell = [[] for _ in range(N)]
    line_cells = []
    line_targets = []
    line_idx = 0

    # Rows (511 lines)
    for r in range(S):
        cells = [r * S + c for c in range(S)]
        line_cells.append(cells)
        line_targets.append(int(np.sum(flat[cells])))
        for cell in cells:
            lines_per_cell[cell].append(line_idx)
        line_idx += 1

    # Columns (511 lines)
    for c in range(S):
        cells = [r * S + c for r in range(S)]
        line_cells.append(cells)
        line_targets.append(int(np.sum(flat[cells])))
        for cell in cells:
            lines_per_cell[cell].append(line_idx)
        line_idx += 1

    # Diagonals (1021 lines)
    for d in range(2 * S - 1):
        cells = []
        for r in range(S):
            c = d - S + 1 + r
            if 0 <= c < S:
                cells.append(r * S + c)
        line_cells.append(cells)
        line_targets.append(int(np.sum(flat[cells])))
        for cell in cells:
            lines_per_cell[cell].append(line_idx)
        line_idx += 1

    # Anti-diagonals (1021 lines)
    for x in range(2 * S - 1):
        cells = []
        for r in range(S):
            c = x - r
            if 0 <= c < S:
                cells.append(r * S + c)
        line_cells.append(cells)
        line_targets.append(int(np.sum(flat[cells])))
        for cell in cells:
            lines_per_cell[cell].append(line_idx)
        line_idx += 1

    # 4 LTP sub-tables
    for sub in range(4):
        asn = ltp4[sub]
        # Build cells per line
        ltp_line_cells = [[] for _ in range(S)]
        for cell in range(N):
            ltp_line_cells[asn[cell]].append(cell)
        for k in range(S):
            line_cells.append(ltp_line_cells[k])
            line_targets.append(int(np.sum(flat[ltp_line_cells[k]])))
            for cell in ltp_line_cells[k]:
                lines_per_cell[cell].append(line_idx)
            line_idx += 1

    return lines_per_cell, line_cells, line_targets


def build_8ltp_system(csm, ltp8):
    """Build lines for 8 LTP-only families."""
    flat = csm.flatten()
    lines_per_cell = [[] for _ in range(N)]
    line_cells = []
    line_targets = []
    line_idx = 0

    for sub in range(8):
        asn = ltp8[sub]
        ltp_line_cells = [[] for _ in range(S)]
        for cell in range(N):
            ltp_line_cells[asn[cell]].append(cell)
        for k in range(S):
            line_cells.append(ltp_line_cells[k])
            line_targets.append(int(np.sum(flat[ltp_line_cells[k]])))
            for cell in ltp_line_cells[k]:
                lines_per_cell[cell].append(line_idx)
            line_idx += 1

    return lines_per_cell, line_cells, line_targets


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ltp-file", type=pathlib.Path, default=None)
    parser.add_argument("--csm-seed", type=int, default=42)
    parser.add_argument("--density", type=float, default=0.5)
    args = parser.parse_args()

    rng = np.random.default_rng(args.csm_seed)
    csm = (rng.random((S, S)) < args.density).astype(np.uint8)
    flat = csm.flatten()
    print(f"CSM: {int(np.sum(csm))} ones / {N} cells ({100*np.sum(csm)/N:.1f}%)", flush=True)

    # Build LTP partitions
    print("\nBuilding LTP partitions...", flush=True)
    if args.ltp_file:
        ltp4 = load_ltpb(args.ltp_file, 4)
    else:
        ltp4 = [build_fy_partition(SEEDS_4PLUS4[i]) for i in range(4)]

    ltp8_partitions = []
    for i in range(8):
        if i < len(ltp4):
            ltp8_partitions.append(ltp4[i])
        else:
            print(f"  Building partition {i} (seed=0x{SEEDS_8LTP[i]:016x})...", flush=True)
            ltp8_partitions.append(build_fy_partition(SEEDS_8LTP[i]))

    # Test 1: 4+4 system (baseline)
    print("\n" + "=" * 60, flush=True)
    print("SYSTEM 1: 4 geometric + 4 LTP (baseline)", flush=True)
    print("=" * 60, flush=True)
    t0 = time.time()
    lpc, lc, lt = build_4plus4_system(csm, ltp4)
    print(f"  Lines: {len(lt)}, built in {time.time()-t0:.1f}s", flush=True)
    t0 = time.time()
    forced_4plus4 = simulate_propagation(flat, lpc, lc, lt)
    elapsed = time.time() - t0
    print(f"  Propagation forced: {forced_4plus4} / {N} cells ({100*forced_4plus4/N:.1f}%)", flush=True)
    print(f"  Meeting band: {N - forced_4plus4} cells ({100*(N-forced_4plus4)/N:.1f}%)", flush=True)
    print(f"  Equivalent row depth: ~{forced_4plus4 // S}", flush=True)
    print(f"  Elapsed: {elapsed:.1f}s", flush=True)

    # Test 2: 8-LTP system
    print("\n" + "=" * 60, flush=True)
    print("SYSTEM 2: 8 LTP-only (no geometric)", flush=True)
    print("=" * 60, flush=True)
    t0 = time.time()
    lpc8, lc8, lt8 = build_8ltp_system(csm, ltp8_partitions)
    print(f"  Lines: {len(lt8)}, built in {time.time()-t0:.1f}s", flush=True)
    t0 = time.time()
    forced_8ltp = simulate_propagation(flat, lpc8, lc8, lt8)
    elapsed = time.time() - t0
    print(f"  Propagation forced: {forced_8ltp} / {N} cells ({100*forced_8ltp/N:.1f}%)", flush=True)
    print(f"  Meeting band: {N - forced_8ltp} cells ({100*(N-forced_8ltp)/N:.1f}%)", flush=True)
    print(f"  Equivalent row depth: ~{forced_8ltp // S}", flush=True)
    print(f"  Elapsed: {elapsed:.1f}s", flush=True)

    # Summary
    print("\n" + "=" * 60, flush=True)
    print("SUMMARY", flush=True)
    print("=" * 60, flush=True)
    delta = forced_8ltp - forced_4plus4
    print(f"  4+4 propagation: {forced_4plus4} cells (row ~{forced_4plus4 // S})")
    print(f"  8-LTP propagation: {forced_8ltp} cells (row ~{forced_8ltp // S})")
    print(f"  Delta: {delta:+d} cells ({100*delta/forced_4plus4:+.1f}%)")
    if delta > 0:
        print(f"  RESULT: 8-LTP propagates DEEPER (+{delta} cells)")
    elif delta < 0:
        print(f"  RESULT: 8-LTP propagates SHALLOWER ({delta} cells)")
    else:
        print(f"  RESULT: IDENTICAL propagation depth")


if __name__ == "__main__":
    main()
