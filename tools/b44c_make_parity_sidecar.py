#!/usr/bin/env python3
"""
B.44c: Compute parity partition values for a synthetic CSM and write to sidecar file.

The sidecar contains 8 FY-shuffled parity partitions. For each partition, every cell
is assigned to one of 511 parity lines. The stored parity bit per line is the XOR
of all cells on that line.

Format (B44P):
  "B44P" magic (4 bytes)
  uint16 dim = 511 (2 bytes)
  uint8 num_partitions (1 byte)
  For each partition:
    uint16[N] assignments (cell -> line index)
    uint8[511] target_parity (XOR of all cells on each line)

Usage:
    python3 tools/b44c_make_parity_sidecar.py --csm-seed 42 --output /tmp/b44c_parity.bin
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import argparse
import pathlib
import struct
import sys

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.")

S = 511
N = S * S

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

PARITY_SEEDS = [
    0x4352534350415231,  # CRSCPAR1
    0x4352534350415232,
    0x4352534350415233,
    0x4352534350415234,
    0x4352534350415235,
    0x4352534350415236,
    0x4352534350415237,
    0x4352534350415238,
]


def lcg_next(state: int) -> int:
    return (state * LCG_A + LCG_C) & (LCG_MOD - 1)


def build_fy_partition(seed: int) -> np.ndarray:
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


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--csm-seed", type=int, default=42)
    parser.add_argument("--density", type=float, default=0.5)
    parser.add_argument("--num-partitions", type=int, default=8)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    rng = np.random.default_rng(args.csm_seed)
    csm = (rng.random((S, S)) < args.density).astype(np.uint8)
    flat = csm.flatten()

    num_p = min(args.num_partitions, len(PARITY_SEEDS))
    print(f"Building {num_p} parity partitions (CSM seed={args.csm_seed})...", flush=True)

    sidecar = bytearray()
    sidecar.extend(b"B44P")
    sidecar.extend(struct.pack("<H", S))
    sidecar.append(num_p)

    for p in range(num_p):
        print(f"  Partition {p} (seed=0x{PARITY_SEEDS[p]:016x})...", flush=True)
        asn = build_fy_partition(PARITY_SEEDS[p])

        # Write assignments: uint16[N]
        sidecar.extend(asn.astype("<u2").tobytes())

        # Compute target parity for each line
        parity = np.zeros(S, dtype=np.uint8)
        for cell in range(N):
            parity[asn[cell]] ^= flat[cell]

        # Write target parities: uint8[S]
        sidecar.extend(parity.tobytes())

    args.output.write_bytes(bytes(sidecar))
    print(f"Wrote {len(sidecar)} bytes to {args.output}", flush=True)


if __name__ == "__main__":
    main()
