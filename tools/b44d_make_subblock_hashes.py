#!/usr/bin/env python3
"""
B.44d: Compute CRC-32 sub-block hashes for a synthetic CSM.

Divides each 511-bit row into 8 blocks of 64 bits (block 7 = 63 bits + 1 padding).
Computes CRC-32 of each block. Writes to a sidecar file.

Format (B44D):
  "B44D" magic (4 bytes)
  uint16 dim = 511 (2 bytes)
  uint8 blocks_per_row = 8 (1 byte)
  uint8 block_size = 64 (1 byte)
  For each row (511 rows):
    uint32[8] crc32 values (one per block)

Usage:
    python3 tools/b44d_make_subblock_hashes.py --csm-seed 42 --output /tmp/b44d_subblock.bin
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import argparse
import binascii
import pathlib
import struct
import sys

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.")

S = 511
BLOCKS_PER_ROW = 8
BLOCK_SIZE = 64  # bits


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--csm-seed", type=int, default=42)
    parser.add_argument("--density", type=float, default=0.5)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    rng = np.random.default_rng(args.csm_seed)
    csm = (rng.random((S, S)) < args.density).astype(np.uint8)

    sidecar = bytearray()
    sidecar.extend(b"B44D")
    sidecar.extend(struct.pack("<H", S))
    sidecar.append(BLOCKS_PER_ROW)
    sidecar.append(BLOCK_SIZE)

    for r in range(S):
        for b in range(BLOCKS_PER_ROW):
            c_start = b * BLOCK_SIZE
            c_end = min(c_start + BLOCK_SIZE, S)
            # Pack block bits MSB-first into bytes
            block_bits = np.zeros(BLOCK_SIZE, dtype=np.uint8)
            for i, c in enumerate(range(c_start, c_end)):
                block_bits[i] = csm[r, c]
            block_bytes = np.packbits(block_bits)  # MSB-first, 8 bytes
            crc = binascii.crc32(block_bytes.tobytes()) & 0xFFFFFFFF
            sidecar.extend(struct.pack("<I", crc))

    args.output.write_bytes(bytes(sidecar))
    print(f"Wrote {len(sidecar)} bytes ({S} rows x {BLOCKS_PER_ROW} blocks x CRC-32)")


if __name__ == "__main__":
    main()
