#!/usr/bin/env python3
"""
B.59 Spiral LTP Table Builder for S=127.

Constructs uniform-127 LTP sub-tables with center-spiral cell ordering.
Cells are sorted by Euclidean distance from a center point and assigned
to consecutive lines of 127 cells each.

Usage:
    python3 tools/b59_spiral_table.py --out tools/b59c_table.bin              # 2 rLTPs
    python3 tools/b59_spiral_table.py --out tools/b59e_table.bin --hybrid     # 1 rLTP + 1 yLTP
"""

import argparse
import struct
import sys
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
MAGIC = b"LTPB"
VERSION = 1

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64


def build_spiral_assignment(center_r, center_c):
    """Build uniform-S spiral LTP assignment: assignment[flat] = line_index."""
    cells = []
    for r in range(S):
        for c in range(S):
            dist_sq = (r - center_r) ** 2 + (c - center_c) ** 2
            cells.append((dist_sq, r * S + c))
    cells.sort()

    assignment = np.zeros(N, dtype=np.uint16)
    for i, (_, flat) in enumerate(cells):
        assignment[flat] = i // S
    return assignment


def build_fy_assignment(seed):
    """Build Fisher-Yates assignment replicating C++ LCG exactly."""
    pool = list(range(N))
    state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        j = int(state % (i + 1))
        pool[i], pool[j] = pool[j], pool[i]

    assignment = np.zeros(N, dtype=np.uint16)
    for line in range(S):
        for slot in range(S):
            assignment[pool[line * S + slot]] = line
    return assignment


def validate_assignment(a, label):
    """Validate uniform-S: each line has exactly S cells."""
    counts = np.bincount(a, minlength=S)
    if len(counts) != S or not np.all(counts == S):
        bad = [(int(k), int(counts[k])) for k in range(min(len(counts), S)) if counts[k] != S]
        sys.exit(f"VALIDATION FAILED for {label}: non-uniform lines: {bad[:5]}")
    print(f"  {label}: valid (127 lines x 127 cells)")


def write_ltpb(path, assignments):
    """Write LTPB file."""
    num_sub = len(assignments)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sIII", MAGIC, VERSION, S, num_sub))
        for a in assignments:
            f.write(a.astype("<u2").tobytes())
    size = 16 + num_sub * N * 2
    print(f"  Written: {path} ({size:,} bytes)")


def main():
    parser = argparse.ArgumentParser(description="B.59 spiral LTP table builder")
    parser.add_argument("--out", required=True, help="Output LTPB file path")
    parser.add_argument("--hybrid", action="store_true",
                        help="1 rLTP + 1 yLTP (default: 2 rLTPs)")
    parser.add_argument("--center1", default="63,63", help="rLTP1 center (r,c)")
    parser.add_argument("--center2", default="63,63", help="rLTP2 center (r,c) or yLTP seed suffix")
    parser.add_argument("--seed", default="Z", help="yLTP seed suffix char (for hybrid mode)")
    args = parser.parse_args()

    c1 = tuple(int(x) for x in args.center1.split(","))

    if args.hybrid:
        print(f"Building hybrid table: 1 rLTP (center {c1}) + 1 yLTP (seed CRSCLTP{args.seed})")
        a1 = build_spiral_assignment(c1[0], c1[1])
        validate_assignment(a1, "rLTP1")

        seed_val = int.from_bytes(b"CRSCLTP" + args.seed.encode(), "big")
        a2 = build_fy_assignment(seed_val)
        validate_assignment(a2, "yLTP2")
    else:
        c2 = tuple(int(x) for x in args.center2.split(","))
        print(f"Building 2 rLTP table: center1={c1}, center2={c2}")
        a1 = build_spiral_assignment(c1[0], c1[1])
        validate_assignment(a1, "rLTP1")
        a2 = build_spiral_assignment(c2[0], c2[1])
        validate_assignment(a2, "rLTP2")

    write_ltpb(Path(args.out), [a1, a2])


if __name__ == "__main__":
    main()
