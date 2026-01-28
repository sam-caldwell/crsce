# Compress CLI: Predictable Output Size

## Intent

- Verify the CRSCE container size invariants for multiple block counts.
- Ensure the CLI produces a constant per‑block payload size and a total file size that matches the header’s
  `block_count`.

## Features Under Test

- End‑to‑end compressor CLI pipeline (`compress::cli::run`).
- Containerization via `Compress::compress_file()` including:
    - `HeaderV1` layout and `block_count` computation.
    - Per‑block payload layout: LH region and serialized cross‑sums.

## Expectations

- For N in {1, 2, 5, 10, 20} blocks:
    - Total file size equals `28 + N * 18652` bytes.
    - Per‑block payload bytes equals `18652` bytes.
    - Header field `block_count` equals `N` (LE64 at offset 16).

## Theory of Operation

- HeaderV1 (28 bytes total):
    - Bytes 0–3: magic `CRSC`.
    - Bytes 4–5: version (LE16) = 1.
    - Bytes 6–7: header size (LE16) = 28.
    - Bytes 8–15: original size in bytes (LE64).
    - Bytes 16–23: `block_count` (LE64).
    - Bytes 24–27: CRC32 of the first 24 header bytes (LE32).
- Block payload (per block = 18652 bytes):
    - LH region: 511 digests × 32 bytes each = 16352 bytes.
    - Cross‑sums: four vectors (LSM, VSM, DSM, XSM), each 511 values at 9 bits/value.
        - Bits per vector: `511 * 9 = 4599` bits → `ceil(4599/8) = 575` bytes.
        - Four vectors: `4 * 575 = 2300` bytes.
    - Total: `16352 + 2300 = 18652` bytes / block.
- Block count calculation (in `compress_file()`):
    - `bits_per_block = 511 * 511` (data bits per block).
    - Let `total_bits = original_size_bytes * 8`.
    - `block_count = (total_bits == 0) ? 0 : ceil(total_bits / bits_per_block)`
      implemented as integer math: `(total_bits + bits_per_block - 1) / bits_per_block`.
- Input sizing to get exactly N blocks (minimal case used by the test):
    - Minimal bits to yield N blocks: `bits = (N - 1) * bits_per_block + 1`.
    - Minimal bytes: `ceil(bits / 8)`.
    - The test uses a deterministic RNG to populate the input; content does not affect size invariants.

## Constraints

- The test checks size and header invariants only (not payload contents).
- The per‑block payload layout and sizes must remain stable for the v1 container.
