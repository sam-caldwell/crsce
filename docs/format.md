<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>File Format</h1>
                <p>
                    This section specifies the on‑disk layout for CRSCE v1 files.
                </p>
            </td>
        </tr>
    </table>
</div>

## Header (v1, 28 bytes, little‑endian)

- magic: 4 bytes = "CRSC"
- version: uint16 = 1
- header_bytes: uint16 = 28
- original_file_size_bytes: uint64
- block_count: uint64
- header_crc32: uint32 (CRC over all preceding header fields)

## Blocks

- Each block encodes one 511×511 CSM derived from input bits. The final block is zero‑padded to the full size.
- The fixed block payload length is exactly 149,216 bits (18,652 bytes) with fields in this order:
  1) LH[0..510] — 511 digests, each 256 bits (32 bytes)
  2) LSM[0..510] — 511 integers, each 9 bits, MSB‑first
  3) VSM[0..510] — same
  4) DSM[0..510] — same
  5) XSM[0..510] — same
  6) Trailing padding — 4 bits (expected zero)

## Notes

- Cross‑sum ranges: each LSM/VSM/DSM/XSM entry must be in [0, 511].
- Bit order: multi‑bit entries are serialized MSB‑first; the payload is a byte‑aligned bitstream.
- Byte mapping: the CSM represents input bits row‑major; during compression, bytes are expanded MSB‑first to bits;
  during decompression, bits are packed MSB‑first into bytes.
- Padding: the last block’s trailing bytes after the original size are discarded when writing the decompressed output.

## Acceptance (decoder)

A block is accepted if and only if the reconstructed CSM simultaneously:

- Reproduces the stored LSM/VSM/DSM/XSM vectors exactly for all indices; and
- Recomputes the LH chain from the reconstructed rows exactly (same seed, same chaining) for all r ∈ {0..510}.

Any mismatch results in rejection of that block. Decoders should fail‑hard by default and not produce partial output.
