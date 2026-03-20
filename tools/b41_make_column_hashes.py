#!/usr/bin/env python3
"""
B.41 Utility: Compute column SHA-1 hashes for a synthetic CSM and write to a sidecar file.

The sidecar file contains 511 SHA-1 digests (20 bytes each), one per column.
Column c's message is 512 bits = 64 bytes: the 511 bits of column c (top-to-bottom)
plus 1 trailing zero bit, packed MSB-first.

The solver reads this via CRSCE_B41_COLUMN_HASHES env var.

Usage:
    python3 tools/b41_make_column_hashes.py --csm-seed 42 --output /tmp/b41_col_hashes.bin
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import argparse
import hashlib
import pathlib
import struct
import sys

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.")

S = 511


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--csm-seed", type=int, default=42,
                        help="RNG seed for the random CSM (must match b40_make_test_block.py)")
    parser.add_argument("--density", type=float, default=0.5)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    rng = np.random.default_rng(args.csm_seed)
    csm = (rng.random((S, S)) < args.density).astype(np.uint8)

    # Compute SHA-1 for each column
    # Column message: 511 bits (cells from rows 0-510) + 1 padding zero = 512 bits = 64 bytes
    sidecar = bytearray()
    # Header: "B41C" magic + uint16 dim
    sidecar.extend(b"B41C")
    sidecar.extend(struct.pack("<H", S))

    for c in range(S):
        bits = np.zeros(512, dtype=np.uint8)
        for r in range(S):
            bits[r] = csm[r, c]
        msg = np.packbits(bits)  # 64 bytes, MSB-first
        digest = hashlib.sha1(msg.tobytes()).digest()
        sidecar.extend(digest)

    args.output.write_bytes(bytes(sidecar))
    print(f"Wrote {len(sidecar)} bytes ({S} column SHA-1 hashes) to {args.output}")


if __name__ == "__main__":
    main()
