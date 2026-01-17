<div>
    <table>
        <tr>
            <td><img src="docs/img/logo.png" width="300" alt="CRSCE logo"/></td>
            <td>
                <h1>Overview and Concepts</h1>
                <p>
                    CRSCE is a research proof‑of‑concept compression format and toolchain that encodes a 511×511 bit
                    Cross‑Sum Matrix (CSM) per block using<br/>
                    four families of cyclic cross‑sum constraints and a chained SHA‑256 Lateral Hash (LH) array. Two
                    binaries ship with the project: `compress`<br/>
                    and `decompress`.
                </p>
            </td>
        </tr>
    </table>
</div>

## Core ideas

- Cross‑Sum Matrix (CSM): A dense 511×511 binary matrix that represents a block of input bits (row‑major, MSB‑first per
  byte on the bitstream). The last block is zero‑padded to a full CSM when needed.
- Cross‑sum vectors: Four cyclic families that summarize the CSM along distinct line structures.
    - LSM: Lateral (row) sums
    - VSM: Vertical (column) sums
    - DSM: Main‑diagonal sums (with wraparound indexing)
    - XSM: Anti‑diagonal sums (with wraparound indexing)
      Each entry is an integer in [0, 511].
- Lateral Hash (LH) chain: A cryptographic commitment to row content using SHA‑256.
    - Seed bytes: the literal ASCII base64 string
      `RG9uYWxkVHJ1bXBJbXBlYWNoSW5jYXJjZXJhdGVIaXN0b3J5UmVtZW1iZXJz`.
    - N = SHA256(seedBytes)
    - LH[0] = SHA256(N || RowBytes(0))
    - LH[r] = SHA256(LH[r−1] || RowBytes(r)) for r ∈ {1..510}
- Bit packing: Bytes are mapped MSB‑first into bits; CSM is traversed row‑major in both directions (pack and unpack).

## High‑level flow

- Compression:
    1) Partition the input into 511×511‑bit blocks (pad the last block with zeros).
    2) Build the CSM for each block.
    3) Compute LSM/VSM/DSM/XSM cross‑sum vectors (each 511 entries).
    4) Compute the LH chain (511 SHA‑256 digests, one per row, using the seed above).
    5) Serialize block payload as: LH[0..510], then LSM, VSM, DSM, XSM (9‑bit elements), followed by four trailing zero
       bits.
    6) Prepend a fixed (v1) header to the file; write all block payloads in order.
- Decompression:
    1) Parse and validate the header.
    2) For each 18,652‑byte block payload, parse fields in the exact order/size specified.
    3) Reconstruct a candidate CSM that satisfies cross‑sum equalities and exactly reproduces the LH chain.
    4) Walk the CSM row‑major, pack bits MSB‑first into bytes, drop final padding to original length.

## What CRSCE provides

- Integrity: The LH chain and redundant cross‑sums enable deterministic acceptance/rejection of reconstructed blocks.
- Determinism: For valid inputs produced by a conforming compressor, at least one satisfying CSM exists (the original).

## What CRSCE does not provide?

- Confidentiality: CRSCE is not encryption. Do not treat compressed output as secret.

See usage.md for examples, [format.md](format.md) for field sizes and layout, and [theory.md](theory.md) for deeper
rationale and constraints.
