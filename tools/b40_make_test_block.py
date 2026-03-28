#!/usr/bin/env python3
"""
B.40 Utility: Create a synthetic .crsce file from a random CSM.

Generates a random 511x511 binary matrix, computes all cross-sums and SHA-1/SHA-256
hashes, and packs them into a valid .crsce file. The decompressor's RowDecomposedController
can then run on this file with CRSCE_B40_PROFILE enabled to collect hash-failure
correlation data at the plateau.

DI is set to 0. The solver will search for the first lex-order solution, generating
SHA-1 failures along the way — exactly the profiling data B.40 needs.

Usage:
    python3 tools/b40_make_test_block.py --output /tmp/b40_test.crsce
    python3 tools/b40_make_test_block.py --output /tmp/b40_test.crsce --seed 42
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
N = S * S  # 261,121

# LCG parameters (must match C++ LtpTable.cpp)
LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

# Default LTP seeds (B.26c winners + B.27 extensions)
SEEDS = [
    0x435253434C545056,  # CRSCLTPV
    0x435253434C545050,  # CRSCLTPP
    0x435253434C545033,  # CRSCLTP3
    0x435253434C545034,  # CRSCLTP4
    0x435253434C545035,  # CRSCLTP5
    0x435253434C545036,  # CRSCLTP6
]
NUM_LTP = 6


def lcg_next(state: int) -> int:
    return (state * LCG_A + LCG_C) & (LCG_MOD - 1)


def build_ltp_assignments(ltp_file: pathlib.Path | None) -> list[np.ndarray]:
    """Build or load LTP assignments (6 sub-tables)."""
    if ltp_file is not None:
        data = ltp_file.read_bytes()
        magic, version, s_field, num_sub = struct.unpack_from("<4sIII", data, 0)
        assert magic == b"LTPB" and s_field == S
        assignments = []
        offset = 16
        for _ in range(min(num_sub, NUM_LTP)):
            a = np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy()
            assignments.append(a)
            offset += N * 2
        # Fill remaining from FY seeds
        if len(assignments) < NUM_LTP:
            fy = build_fy_seeds(NUM_LTP)
            for i in range(len(assignments), NUM_LTP):
                assignments.append(fy[i])
        return assignments[:NUM_LTP]
    return build_fy_seeds(NUM_LTP)


def build_fy_seeds(num_sub: int) -> list[np.ndarray]:
    """Build LTP via pool-chained Fisher-Yates shuffle."""
    pool = list(range(N))
    assignments = []
    for seed_idx in range(num_sub):
        state = SEEDS[seed_idx]
        for i in range(N - 1, 0, -1):
            state = lcg_next(state)
            j = int(state % (i + 1))
            pool[i], pool[j] = pool[j], pool[i]
        a = np.empty(N, dtype=np.uint16)
        for line in range(S):
            base = line * S
            for pos in range(S):
                a[pool[base + pos]] = line
        assignments.append(a)
    return assignments


def compute_cross_sums(csm: np.ndarray, ltp: list[np.ndarray]) -> dict:
    """Compute all cross-sum vectors from a 511x511 binary matrix."""
    lsm = np.sum(csm, axis=1).astype(np.uint16)  # row sums
    vsm = np.sum(csm, axis=0).astype(np.uint16)  # col sums

    dsm = np.zeros(2 * S - 1, dtype=np.uint16)
    xsm = np.zeros(2 * S - 1, dtype=np.uint16)
    for r in range(S):
        for c in range(S):
            d = c - r + S - 1
            x = r + c
            dsm[d] += csm[r, c]
            xsm[x] += csm[r, c]

    ltp_sums = []
    flat = csm.flatten()
    for sub in range(len(ltp)):
        sums = np.zeros(S, dtype=np.uint16)
        for cell in range(N):
            sums[ltp[sub][cell]] += flat[cell]
        ltp_sums.append(sums)

    return {"lsm": lsm, "vsm": vsm, "dsm": dsm, "xsm": xsm, "ltp": ltp_sums}


def compute_hashes(csm: np.ndarray) -> tuple:
    """Compute per-row SHA-1 and block SHA-256."""
    lh = []
    for r in range(S):
        # Row message: 511 bits + 1 padding zero = 512 bits = 64 bytes
        bits = np.zeros(512, dtype=np.uint8)
        for c in range(S):
            bits[c] = csm[r, c]
        msg = np.packbits(bits)  # 64 bytes, MSB-first
        lh.append(hashlib.sha1(msg.tobytes()).digest())

    # Block hash: all 261,121 bits row-major, packed MSB-first
    flat_bits = csm.flatten()
    padded = np.zeros((N + 7) // 8 * 8, dtype=np.uint8)
    padded[:N] = flat_bits
    block_msg = np.packbits(padded)
    bh = hashlib.sha256(block_msg.tobytes()).digest()

    return lh, bh


def pack_bits(buf: bytearray, bit_offset: int, value: int, num_bits: int) -> int:
    """Pack value into buf at bit_offset using MSB-first bit ordering."""
    for i in range(num_bits - 1, -1, -1):
        byte_idx = bit_offset // 8
        bit_idx = 7 - (bit_offset % 8)
        bit_val = (value >> i) & 1
        buf[byte_idx] |= (bit_val << bit_idx)
        bit_offset += 1
    return bit_offset


def diag_len(k: int) -> int:
    return min(k + 1, S, (2 * S - 1) - k)


def serialize_block(sums: dict, lh: list, bh: bytes, di: int) -> bytes:
    """Serialize a single block payload."""
    # LH: 511 x 20 bytes
    payload = bytearray()
    for r in range(S):
        payload.extend(lh[r])

    # BH: 32 bytes
    payload.extend(bh)

    # DI: 1 byte
    payload.append(di & 0xFF)

    # Cross-sum bitstream
    # Total bits: LSM(4599) + VSM(4599) + DSM(8185) + XSM(8185)
    #           + 6*LTP(4599) = 53,162 bits
    total_cross_bits = (4599 + 4599 + 8185 + 8185 + NUM_LTP * 4599)
    cross_bytes = (total_cross_bits + 7) // 8
    bitstream = bytearray(cross_bytes)
    offset = 0

    # LSM: 511 x 9 bits
    for val in sums["lsm"]:
        offset = pack_bits(bitstream, offset, int(val), 9)

    # VSM: 511 x 9 bits
    for val in sums["vsm"]:
        offset = pack_bits(bitstream, offset, int(val), 9)

    # DSM: 1021 variable-width
    for k in range(2 * S - 1):
        bits = diag_len(k).bit_length()
        if bits == 0:
            bits = 1
        offset = pack_bits(bitstream, offset, int(sums["dsm"][k]), bits)

    # XSM: 1021 variable-width
    for k in range(2 * S - 1):
        bits = diag_len(k).bit_length()
        if bits == 0:
            bits = 1
        offset = pack_bits(bitstream, offset, int(sums["xsm"][k]), bits)

    # LTP1-6: 511 x 9 bits each
    for sub in range(NUM_LTP):
        for val in sums["ltp"][sub]:
            offset = pack_bits(bitstream, offset, int(val), 9)

    payload.extend(bitstream)
    return bytes(payload)


def serialize_file(original_size: int, block_payload: bytes) -> bytes:
    """Serialize a complete .crsce file (header + 1 block)."""
    # Header (28 bytes)
    header = bytearray(28)
    struct.pack_into("<I", header, 0, 0x43525343)  # "CRSC" as little-endian uint32
    struct.pack_into("<H", header, 4, 1)        # version
    struct.pack_into("<H", header, 6, 28)       # header_bytes
    struct.pack_into("<Q", header, 8, original_size)
    struct.pack_into("<Q", header, 16, 1)       # block_count = 1

    # CRC-32 over bytes 0-23
    import binascii
    crc = binascii.crc32(bytes(header[:24])) & 0xFFFFFFFF
    struct.pack_into("<I", header, 24, crc)

    return bytes(header) + block_payload


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--ltp-file", type=pathlib.Path, default=None)
    parser.add_argument("--density", type=float, default=0.5,
                        help="Probability of each cell being 1 (default: 0.5)")
    args = parser.parse_args()

    print(f"Generating random {S}x{S} CSM (seed={args.seed}, density={args.density})...",
          flush=True)
    rng = np.random.default_rng(args.seed)
    csm = (rng.random((S, S)) < args.density).astype(np.uint8)
    ones = int(np.sum(csm))
    print(f"  CSM: {ones} ones out of {N} cells ({100*ones/N:.1f}%)", flush=True)

    print("Loading LTP assignments...", flush=True)
    ltp = build_ltp_assignments(args.ltp_file)
    print(f"  {len(ltp)} sub-tables loaded", flush=True)

    print("Computing cross-sums...", flush=True)
    sums = compute_cross_sums(csm, ltp)

    print("Computing SHA-1 lateral hashes and SHA-256 block hash...", flush=True)
    lh, bh = compute_hashes(csm)

    print("Serializing block payload...", flush=True)
    payload = serialize_block(sums, lh, bh, di=0)
    print(f"  Payload size: {len(payload)} bytes", flush=True)

    print("Serializing .crsce file...", flush=True)
    # Original size = S*S bits = 32640.125 bytes → 32641 bytes (rounded up)
    original_size = (N + 7) // 8
    file_data = serialize_file(original_size, payload)
    print(f"  File size: {len(file_data)} bytes", flush=True)

    args.output.write_bytes(file_data)
    print(f"Written to {args.output}", flush=True)


if __name__ == "__main__":
    main()
