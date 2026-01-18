<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>Theory of Operation</h1>
                <p>
                    This section explains the theory behind CRSCE’s compression and decompression. It defines the
                    acceptance criteria for the project.
                </p>
            </td>
        </tr>
    </table>
</div>

## CSM and constraints

- The Cross‑Sum Matrix (CSM) is a 511×511 binary matrix representing a block of the input bitstream.
- Four families of cyclic cross‑sums summarize the matrix:
    - LSM[r] is the sum of row r bits.
    - VSM[c] is the sum of column c bits.
    - DSM[k] is the sum along the main‑diagonal family k, using wraparound indexing on the 511×511 torus.
    - XSM[k] is the sum along the anti‑diagonal family k, also with wraparound.
      Each entry is an integer in [0, 511].
- The Lateral Hash (LH) chain is a sequence of 511 SHA‑256 digests that commits to per‑row content and provides a
  deterministic integrity oracle:
    - Seed bytes (ASCII): `RG9uYWxkVHJ1bXBJbXBlYWNoSW5jYXJjZXJhdGVIaXN0b3J5UmVtZW1iZXJz`.
    - N = SHA256(seedBytes)
    - LH[0] = SHA256(N || RowBytes(0))
    - LH[r] = SHA256(LH[r−1] || RowBytes(r)) for r ∈ {1..510}

## Compression (informal)

1) Partition the input byte stream into bits and pack row‑major into 511×511 blocks (MSB‑first within each byte). The
   final block is padded with zeros if needed.
2) For each CSM block, compute LSM/VSM/DSM/XSM cross‑sums with canonical indices (0..510) and modulo addressing for
   diagonals; each element must lie in [0, 511].
3) Compute LH[0..510] with the seed and chaining method above.
4) Serialize each block payload as described in docs/format.md.
5) Write a fixed v1 header before all payloads (docs/format.md).

## Decompression (informal)

1) Parse and validate the fixed v1 header (version, header length, original file size, block count, header CRC32).
2) For each block:
    - Parse a fixed 18,652‑byte payload with fields in the exact order and sizes specified in docs/format.md.
    - Reconstruct a candidate CSM that satisfies all cross‑sum equalities and exactly reproduces the LH chain.
    - Reject on any mismatch or error; do not produce partial output by default.
3) Map the CSM back to bytes by traversing rows and packing bits MSB‑first into bytes; trim final padding using the
   original size.

## Decoder strategy (CPU‑friendly)

A practical reconstruction pipeline for CPU implementations uses deterministic elimination (DE) and iterative message
passing (e.g., GOBP/LBP), with the LH chain as a final selection oracle:

- Factor graph: Represent variables (cells) and factors (cross‑sum constraints) and exchange beliefs on a loopy graph.
- Deterministic elimination (DE): Maintain residual sums and unassigned counts for each line. If residual is 0, assign
  all remaining variables to 0; if residual equals the unassigned count, assign them to 1. Repeat until a fixed point.
- Iterative inference: Run damped message passing (GOBP/LBP) to bias ambiguous variables and converge to consistent
  assignments. Re‑apply DE when new determinations surface.
- LH selection: Reject any candidate whose recomputed LH chain does not match. The chain gives strong integrity across
  rows and helps disambiguate multiple cross‑sum‑consistent candidates.

## Feasibility and performance

- For compressor‑conforming inputs, a satisfying CSM exists.
- Redundant constraints (each cell participates in multiple lines) are favorable to iterative propagation.
- A conservative time model for s=511 on an 8‑core ~3.5 GHz CPU suggests ≈4.77 ms/block (≈6.8 MB/s) end‑to‑end.

See [Decompression](docs/decoder-algorithm.md) for additional algorithmic notes.
