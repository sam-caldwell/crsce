#!/usr/bin/env python3
"""
B.45: Generate 8-uniform + 3-variable-length LTP sidecar for the C++ solver.

Reads input data (first block), builds 11 LTP partitions (8 uniform 511-cell
+ 3 variable-length 1-to-511-cell), computes cross-sum targets, and writes
a sidecar file that the modified solver reads via CRSCE_B45_SIDECAR.

Sidecar format (B45S):
  "B45S" magic (4 bytes)
  uint16 dim = 511 (2 bytes)
  uint8 num_uniform = 8 (1 byte)
  uint8 num_varlen = 3 (1 byte)
  For each uniform partition (8):
    uint16[N] assignments (cell -> line 0..510)
    uint16[511] target sums
  For each variable-length partition (3):
    uint16 num_lines = 1021 (2 bytes)
    uint16[1021] line_lengths
    uint16[N] assignments (cell -> line 0..1020)
    uint16[1021] target sums

Usage:
    python3 tools/b45_make_sidecar.py --input /tmp/b44d_mp4_1block.bin --output /tmp/b45_sidecar.bin
"""
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

UNIFORM_SEEDS = [
    0x435253434C545056, 0x435253434C545050,
    0x435253434C545033, 0x435253434C545034,
    0x4352534350415231, 0x4352534350415232,
    0x4352534350415233, 0x4352534350415234,
]
VARLEN_SEEDS = [
    0x4352534350415239,
    0x435253435041523A,
    0x435253435041523B,
]


def lcg_next(state):
    return (state * LCG_A + LCG_C) & (LCG_MOD - 1)


def build_uniform_partition(seed):
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


def varlen_sizes():
    return [min(d + 1, S, (2 * S - 1) - d) for d in range(2 * S - 1)]


def build_varlen_partition(seed):
    sizes = varlen_sizes()
    pool = list(range(N))
    state = seed
    for i in range(N - 1, 0, -1):
        state = lcg_next(state)
        j = int(state % (i + 1))
        pool[i], pool[j] = pool[j], pool[i]
    a = np.empty(N, dtype=np.uint16)
    pos = 0
    for line_idx, size in enumerate(sizes):
        for _ in range(size):
            a[pool[pos]] = line_idx
            pos += 1
    return a, sizes


def load_csm_from_file(path):
    data = path.read_bytes()
    bits = []
    for byte in data:
        for bit in range(7, -1, -1):
            bits.append((byte >> bit) & 1)
            if len(bits) >= N:
                break
        if len(bits) >= N:
            break
    while len(bits) < N:
        bits.append(0)
    return np.array(bits[:N], dtype=np.uint8).reshape(S, S)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", type=pathlib.Path, required=True,
                        help="Input data file (first block used)")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--ltp-file", type=pathlib.Path, default=None,
                        help="LTPB file for first 4 uniform partitions")
    args = parser.parse_args()

    csm = load_csm_from_file(args.input)
    flat = csm.flatten()
    print(f"CSM: {int(np.sum(csm))} ones / {N} ({100*np.sum(csm)/N:.1f}%)", flush=True)

    # Build 8 uniform partitions
    uniform_parts = []
    if args.ltp_file:
        data = args.ltp_file.read_bytes()
        _, _, _, file_num_sub = struct.unpack_from("<4sIII", data, 0)
        offset = 16
        for i in range(min(file_num_sub, 4)):
            a = np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy()
            uniform_parts.append(a)
            offset += N * 2
    for i in range(len(uniform_parts), 8):
        print(f"  Building uniform partition {i}...", flush=True)
        uniform_parts.append(build_uniform_partition(UNIFORM_SEEDS[i]))

    # Build 3 variable-length partitions
    varlen_parts = []
    for i in range(3):
        print(f"  Building varlen partition {i}...", flush=True)
        a, sizes = build_varlen_partition(VARLEN_SEEDS[i])
        varlen_parts.append((a, sizes))

    # Write sidecar
    out = bytearray()
    out.extend(b"B45S")
    out.extend(struct.pack("<H", S))
    out.append(8)  # num_uniform
    out.append(3)  # num_varlen

    for i, asn in enumerate(uniform_parts):
        out.extend(asn.astype("<u2").tobytes())
        sums = np.zeros(S, dtype=np.uint16)
        for cell in range(N):
            sums[asn[cell]] += flat[cell]
        out.extend(sums.astype("<u2").tobytes())
        print(f"  Uniform {i}: sum range [{sums.min()}, {sums.max()}]", flush=True)

    for i, (asn, sizes) in enumerate(varlen_parts):
        n_lines = len(sizes)
        out.extend(struct.pack("<H", n_lines))
        out.extend(np.array(sizes, dtype="<u2").tobytes())
        out.extend(asn.astype("<u2").tobytes())
        sums = np.zeros(n_lines, dtype=np.uint16)
        for cell in range(N):
            sums[asn[cell]] += flat[cell]
        out.extend(sums.astype("<u2").tobytes())
        print(f"  Varlen {i}: {n_lines} lines, sum range [{sums.min()}, {sums.max()}]", flush=True)

    args.output.write_bytes(bytes(out))
    print(f"Wrote {len(out)} bytes to {args.output}", flush=True)


if __name__ == "__main__":
    main()
