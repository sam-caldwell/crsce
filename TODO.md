# Task List (ToDo and High-Level Planning)

## Introduction

### Purpose

Deliver a **research-grade CRSCE v1 implementation** that can:

1. **Compress** an input byte stream into a conforming CRSCE v1 container.
2. **Decompress** a CRSCE v1 container back to the original bytes by reconstructing each block's Cross-Sum Matrix (CSM)
   and validating it via acceptance constraints.

### Definition of Done

- [ ] `compress` produces a CRSCE v1 file that:
    - [ ] writes a correct v1 header (`CRSC`, version=1, header_bytes=28, CRC32, sizes)
    - [ ] writes exactly `block_count` fixed-size payloads (18,652 bytes each)
    - [ ] uses canonical bit ordering and packing
    - [ ] stores correct `LH`, `LSM`, `VSM`, `DSM`, `XSM` for every block
- [ ] `decompress` consumes a CRSCE v1 file and:
    - [ ] validates header, framing, and sizes defensively
    - [ ] reconstructs each block's `511 x 511` CSM
    - [ ] accepts a block **if** cross-sums match AND LH chain matches
    - [ ] writes exactly `original_file_size_bytes` bytes
    - [ ] fails hard on invalid blocks or timeouts
- [ ] Tests:
    - [ ] unit tests for header/payload primitives and math
    - [ ] conformance tests for the spec vectors (TV0...TV3)
    - [ ] end-to-end roundtrip tests (`compress` -> `decompress`)
    - [ ] \>=95% test coverage gate is satisfied.

## Key constants and invariants

These values are fixed by CRSCE v1:

- Matrix dimension: `s = 511` (CSM is `511 x 511` bits) indexed 0 ,..., 510
- Cross-sum bit width: `b = 9` bits per element
- Payload layout per block (in order): `LH`, then `LSM`, `VSM`, `DSM`, `XSM`, then 4 zero pad bits
- Payload byte size per block: `18,652` bytes (`149,216` bits)
- Header layout (v1): 28 bytes, little-endian fields, CRC32 over the first 24 bytes

Implementation MUST treat these as hard constants.

### Acronyms & Parameters

- CSM: Cross-Sum Matrix;
- LH: Lateral Hash;
- LE: little-endian.

### CRSCE File Format

#### File Header (v1) (28 bytes LE)

| Offset | Field                    | Size (bytes) | Type   | Description                            |
|--------|--------------------------|--------------|--------|----------------------------------------|
| 0      | magic                    | 4            | bytes  | ASCII `"CRSC"`                         |
| 4      | version                  | 2            | uint16 | `1`                                    |
| 6      | header_bytes             | 2            | uint16 | `28`                                   |
| 8      | original_file_size_bytes | 8            | uint64 | Original (unpadded) file size in bytes |
| 16     | block_count              | 8            | uint64 | Number of CRSCE blocks                 |
| 24     | header_crc32             | 4            | uint32 | CRC-32 over bytes `[0..23]`            |

(Note: All multibyte fields are little-endian; the exact layout belongs in the spec and HeaderV1.h comments.)

#### Block Payload (v1)

| Field       | Size                    | Description                                             |
|-------------|-------------------------|---------------------------------------------------------|
| LH[0..510]  | 16,352 bytes (511 × 32) | 511 digests (SHA-256) for the first 511 rows of the CSM |
| LSM[0..510] | 4,599 bits (511 × 9)    | packed 9-bit lateral cross sums                         |
| VSM[0..510] | 4,599 bits (511 × 9)    | packed 9-bit vertical cross sums                        |
| DSM[0..510] | 4,599 bits (511 × 9)    | packed 9-bit diagonal cross sums                        |
| XSM[0..510] | 4,599 bits (511 × 9)    | packed 9-bit anti-diagonal cross sums                   |
| padding     | 4 bits                  | zero bits to reach 149,216 bits total                   |

> Notes:
>
> - All multibyte fields are little-endian; the exact layout belongs in the spec and HeaderV1.h comments.
> - CSM bit packing (I/O): bits are packed MSB-first within bytes, where each cross-sum is a `uint16_t` in memory
    and bits v8,...v0 are packed into the final output byte array.

#### Module decomposition

| Root Directory      | Component                  | Description                              |
|---------------------|----------------------------|------------------------------------------|
| `(include \| src)/` | `common/`                  | Common utilities                         |
| `common`            | `ArgParser`                | CLI argument parsing                     |
| `common`            | `BitHashBuffer`            | Row-Hash Chaining Class                  |
| `common`            | `SHA256`                   | SHA-256 and row hash chaining helper     |
| `common`            | `format`                   | CRSCE file format parsing/serialization  |
| `(include/src)`     | `compress`                 | CRSCE compression functionality          |
| `(include/src)`     | `decompress`               | CRSCE decompression functionality        |
| `decompress`        | `Csm`                      | CRSCE CSM representation                 |
| `decompress`        | `Decompressor`             | CRSCE decompressor                       |
| `decompress`        | `Solver`                   | Polymorphic CRSCE solver interface       |
| `decompress`        | `DeterministicElimination` | CRSCE deterministic elimination features |
| `decompress`        | `GobpSolver`               | CRSCE GOBP Solver                        |

---

## Delivery Phases

- [x] **Phase 1 — Tooling & Hygiene**
    - [x] Clang Plugins
        - Finalize and document `OneDefinitionPerHeader` and `OneDefinitionPerCppFile` (usage, diagnostics, wiring).
        - Ensure make deps builds plugins under `tools/*`; tidy plugins land under `build/tools/clang-plugins/`.
        - Lint auto-loads tidy plugins via `-load` (already wired); maintain `NOLINT` guards in fixtures.
        - [x] Ensure dependabot is running.
        - [x] Build/Lint/Test Pipeline is working
        - [x] `hello_world` project is working
        - [x] basic `cmd/compress` and `cmd/decompress` are working as hello_world examples
        - [x] CLI Argument Parser (`ArgParser` class) is working
        - [x] extend `cmd/compress` and `cmd/decompress` to use `ArgParser`

- [x] **Phase 2 — CRSCE Compression Features**
    - [x] Implement `CrossSum` class
    - [x] Implement `BitHashBuffer` class
    - [x] Implement `Compress` class
    - [x] Extend `cmd/compress` to use `Compress` class
    - [x] Develop end-to-end compression tests using random, generated input data and known, fixed input data.

- [ ] **Phase 3 — CRSCE Decompression Features**
    - [x] Implement `Csm` class (100% feature complete; >99% test coverage; linters/tests green)
    - [x] Implement `LHChainVerifier` class (100% feature complete; tests added; linters/tests green)
    - [x] Implement `Solver` interface (abstract API + tests)
    - [x] Implement `DeterministicElimination` class (forced-move pass + tests; hash_step placeholder)
    - [x] Implement `GobpSolver` class (CPU single-host; damping + thresholds; full tests; linters green)
    - [ ] Implement `Decompressor` class
        - [x] Header parsing and validation (v1)
        - [x] Block payload splitting (LH and sums)
    - [ ] Extend `cmd/decompress` to use `Decompressor` class
    - [ ] Develop end-to-end decompression tests using random
    - [ ] Develop an end-to-end compression-decompression test using random inputs and automated complete cycle.

- [ ] **Phase 4 — Testing and Data Collection**
    - [ ] Improved Reliability:
        - [ ] Add Improved error handling for compressor stack
        - [ ] Add Improved error handling for decompressor stack
        - [ ] Ensure the decompressor stack fails if any block is empty where header does not indicate an empty file.
        - [ ] Ensure the decompressor can safely handle empty files.
        - [ ] Add collision detection to the decompressor stack (solver).
    - [ ] Observability Features:
        - [ ] Add metrics to the compressor stack
            - compression time per block (in nanoseconds)
            - compression time per file (in nanoseconds)
            - number of blocks compressed
            - number of bytes compressed
        - [ ] Add metrics to the decompressor stack
            - decompression time per block (in nanoseconds)
            - decompression time per file (in nanoseconds)
            - number of blocks decompressed
            - number of bytes decompressed
            - number of collisions detected
            - number of blocks with empty payloads
            - number of blocks with invalid headers
            - number of blocks with invalid payloads
            - number of CSM elements solved by hash-based deterministic elimination
            - number of CSM elements solved by deterministic elimination
            - number of CSM elements solved by GOBP
            - number of cycles through decompression stack
            - time spent in DE (hashes) (in nanoseconds)
            - time spent in DE (forced moves) (in nanoseconds)
            - time spend in GOBP (in nanoseconds)
            - time spend decoding blocks to LH and cross-sums vectors (in nanoseconds)
    - [ ] Add Debugging logic to emit the full state of the compressor stack and decompressor stack at each step.
    - [ ] Add functionality to emit the decompressor state on collision detection.
    - [ ] Develop automated test on HP DL 580 G7 (40 CPU Cores, 128GB RAM)
    - [ ] Test should run end-to-end test cycles on random data.
        - Count the number of collisions (preserving input data)
        - Capture start, stop times for compression and decompression.
    - [ ] Emit metrics as JSON to syslog (and capture this for later analysis).
    - [ ] Increase test coverage to ~99% (ideally 100%).
- [ ] **Phase 5 — Documentation**
    - [ ] Write paper describing design, implementation and findings.

---

## Compressor Components

### Dependency: CrossSum Class

#### Purpose

- Create a common class of s elements of uint16 representing the CRSCE cross-sum vectors.

#### Input

- None

#### Output

- Serialized 9-bit packed vectors as a byte array.

#### Theory of operation

- **Constructor**:
    - Declares a memory object (s elements of uint16).
- `increment(i)` method:
    - increments `Element[i]++`
- `increment_diagonal(r,c)` method:
    - increments `Element[d(r,c)]++`, where `d(r,c)=(r+c) mod s`.
- `increment_antidiagonal(r,c)` method:
    - increments `Element[x(r,c)]++`, where `x(r,c)=(r−c) mod s`.
- `Serialize(OutputArray &output)` method:
    - Iterates over `Element[0,...,s-1]` and packs each 9-bit (b8,...b0), contiguous across bytes; no
      per-vector padding (block-level padding is applied once after all four vectors)
    - Appends the result to the output byte array (&output) passed in by address.

### Dependency: LH chain (BitHashBuffer) Class

#### Purpose

- Create a class which will accept raw bit data representing a single row of CSM.
- Calculate a sha256 digest for each row as part of the lateral hash chain where:

  ```text
  LH[0] = SHA256(seedBytes|Row[0])
  LH[1] = SHA256(Row[0]|Row[1])
  ...
  LH[r] = SHA256(Row[s-2]|...|Row[s-1])
  ```

  where `s=511`.

#### Input: seed (byte array)

#### Output: Serialized 256-bit digests as a byte-array

#### Theory of operation

- **Constructor:**
    - Declares memory object (256-bit digests).
    - Calculate `seedBytes` as sha256(hard_coded_seed) (as defined in specification).
- `push(bit)` method:
    - Buffers the row state bit by bit internally to create a 512-bit row (511 bits of data plus one padding bit).
    - Calculates the sha256 digest of a given row every 511 bits and stores the same in an internal vector.
    - The vector is then cleared to prepare for the next row.
- `hashRow(chain, row)` (private) method:
    - Computes the sha256 digest of a row as part of the chain: `SHA256(chain,row)`
- `serialize(OutputArray &output)` method:
    - Iterates over `digests[0,...,255]` and packs each 256-bit digest into the output byte array.
    - Appends the result to the output byte array (&output) passed in by address.

### Compress Class

#### Purpose

- Stream the input and compute the four cross‑sum vectors and the LH chain digests (no 511×511 matrix in memory).
- Handle multiple blocks; write the header once and flush each finished block payload directly to the output file.

#### Inputs

- Source filename
- Output filename

#### Output

- Writes header v1 (with original size and blockCount), then flushes each completed block payload (LH then
  LSM/VSM/DSM/XSM) to the output file.

#### Theory of operation

- **Constructor**:
    - Takes the source and output filename;
    - Verifies the source file exists and opens for reading.
    - Opens the output file for writing.
    - Initializes a `BitHashBuffer` (LH) and the four cross‑sum vectors (`LSM`, `VSM`, `DSM`, `XSM`).
- `compress()` method:
    - If the source file is empty (zero-length), the output file will be written as CRSCE metadata with zero blocks and
      the program will terminate.
    - Compute the original input size (bytes).
    - Compute total blocks: `blockCount = ceil((original_size * 8) / (s * s))`.
    - Open the output file and write header v1 (LE) including original size and blockCount.
    - Call `serialize()`
    - Close the output file.
    - Close the source file.
- `serialize()` method:
    - Read the source file 32KB at a time (chunk size);
    - set r,c = 0,0 to initialize the block coordinates.
    - For each chunk:
        - For each byte in the chunk:
            - For bit 0,...,7 in the byte
                - If bit is 1, increment the corresponding cross-sum vector
                    - `LSM.increment(r)`
                    - `VSM.increment(c)`
                    - `DSM.increment((r + c) < s ? (r + c) : (r + c - s))`
                    - `XSM.increment(r >= c ? (r - c) : (r + s - c))`
                - increment r until r>s-1 (wrap around), then increment c.
                - When c==s,
                    - Flush the cross sums and LH chain to the output file  (see `flushBlock()`).
                    - Reset r,c=0, reset the cross sum vectors and LH array to initial state.
    - If r,c != 0,0 after all chunks are processed
        - Pad the remaining bits for the LH chain to ensure sxs bits are accounted for in the block.
        - Flush the remaining sums and LH chain (see `flushBlock()`)
- `flushBlock()` method:
    - Finalize and FLUSH the block payload to the output file in this order:
        1) LH[0..510] (511 × 32 bytes)
        2) Cross‑sum vectors: LSM, VSM, DSM, XSM as one contiguous 4 × (511 × 9)‑bit stream

## Decompressor Components

### Dependency: `Csm` Class

#### Purpose

- Represents the cross-sum matrix (CSM) for a block during decompression using a linear byte array.
- Provides methods and operators to access specific CSM bits at (r,c) coordinates.
- Implements a 511x511 locking layer to lock specific bits during the decompression process.
- Implements a 511x511 data layer to store statistics required by LBP.

#### Input

- None

#### Output

- Decompressed bits, serialized into a byte array.

#### Theory of Operation

- *** Constructor:**
    - Declares a memory object (511x511 matrix) of type `bit`. Type bit (0,1) is used to represent the CSM bits.
    - Declares a locking layer (511x511 matrix) of type `bool`.
    - Declares a data layer (511x511 matrix) of type `double`.
- `put(r,c,v)`
    - stores a bit value (v) at bit position (r,c) in the CSM matrix.
- `get(r,c)`
    - returns the bit value at bit position (r,c) in the CSM matrix.
- `lock(r,c)`
    - locks a bit at position (r,c) in the CSM matrix indicating the bit is solved.
- `isLocked(r,c)`
    - returns true if the bit at position (r,c) is locked.
- `getData(r,c)`
    - returns the underlying data value at bit position (r,c) in the CSM matrix. This is separate from the bit value.

### Dependency: `LHChainVerifier` Class

#### Purpose

- Verify reconstructed rows against LH using chained SHA‑256 chaining:

  ```text
  seedHash = SHA256(seedBytes)
  LH[0] = SHA256(seedHash || Row[0])
  LH[i] = SHA256(LH[i−1] || Row[i])  // i > 0
  ```

#### Theory of Operation

- For each row r ∈ [0..510]:
    - Pack 511 bits MSB‑first and append one pad bit (0) to form a 64‑byte row
    - Compute chain and compare Hi to LH[r]; reject on any mismatch

### Dependency: LoopyBeliefsPropagation (implements LBP)

#### Purpose

- Reconstruct the CSM for a block by maintaining exact feasibility of all four cross-sum constraint families while using
  loopy belief propagation (LBP) only as probabilistic guidance for selecting the next deterministic moves (
  decimation/branching).
- Integrate cleanly with Deterministic Elimination (DE) so the decompressor can alternate: (1) sound forced moves,
  then (2) guided choices when no forced moves remain.
- Non-distributed requirement: any “GOBP-inspired” behavior is implemented locally only (single machine). No multi-host
  protocol, no network message exchange, no remote solvers.

#### Theory of Operation

- **Constructor**
    - Accept references to Csm, stored cross-sums LSM/VSM/DSM/XSM, derived per-line state, stored LH[], and config (
      max_iterations, damping alpha, thresholds T0/T1, tol, etc.).
    - Maintain the exact line feasibility state for each constraint line:
        - U[i]: number of unassigned vars on the line.
        - R[i]: residual ones needed on the line (required sum minus assigned ones). This matches the DE residual
          framing in the decompression spec.
- **Core invariant (feasibility first)**
    - Always enforce: 0 ≤ R[i] ≤ U[i] for every line in every family; any violation is an immediate contradiction (fail
      branch / revert speculation). This is the “constraint-propagation” half of the hybrid.
- **DE integration (sound forced moves)**
    - Run DE rules whenever possible:
        - If R=0, all remaining vars on that line are forced 0.
        - If R=U, all remaining vars on that line are forced 1.
    - Every forced assignment updates all affected line states (R/U) and may trigger further forced moves (
      queue-driven).
- **BP layer (guidance only, never overrides feasibility)**
    - Model the unknown bits as variable nodes; line constraints are represented as structured “sum/cardinality”
      factors, but hard correctness is enforced by feasibility checks + final acceptance, not by trusting BP outputs.
    - Use LLR/log-domain messages, damping, clipping, and stable scheduling to improve numerical behavior (
      implementation guidance).
    - Hybrid rule: BP may propose “most confident” bits (largest |LLR|), but any hardening/branching must:
        - pass feasibility immediately (0 ≤ R ≤ U everywhere), and
        - preserve DE’s forced implications (DE always runs after each hard decision).
- **Hardening / decimation**
    - Compute per-variable belief p(x=1) from LLR.
    - If p ≥ T1 set 1; if p ≤ T0 set 0; otherwise leave undecided.
    - After each hardening, run DE again to propagate forced consequences.
- **LH gating (exact oracle)**
    - When a row becomes fully assigned, compute `RowBytes(r)` and verify against the LH chain.
        - On mismatch: reject the current branch (or fail-hard if speculation/backtracking is disabled), since LH is an
          exact accept/reject oracle for correctness.
        - Speculation / branching (optional, bounded)
        - When DE stalls and BP does not harden anything, branch on the variable with the largest |LLR| (or another
          deterministic tie-break).
        - Bound backtracks and iterations; revert immediately on feasibility violation or LH mismatch.
- **Termination**
    - Success: all variables are assigned, and the candidate satisfies all cross-sums + full LH chain, exactly (
      acceptance criteria).
    - Failure: budgets exceeded or no feasible branch remains → return error (fail-hard semantics per spec).
- **Local-only constraint (explicit)**
    - This implementation is single-host by design. Any “GOBP” concepts used here are internal architectural
      inspiration (
      message formats, structured-factor thinking, BP-guided branching), not a distributed protocol implementation.

### Decompressor Class

#### Purpose

- Parse the Phase‑2 header and fixed‑size block payloads.
- Reconstruct a CSM (511×511) per block that satisfies all four cross‑sum vectors.
- Verify each reconstructed row using LH chaining (same seed, bit/byte conventions) and emit original bytes.

#### Input

- Source filename (file to decompress)
- Output filename (file to which decompressed bytes are written)

#### Output

- Original bytes (row‑major), trimmed to the header’s original size

#### Theory of operation

- **Constructor**:

    - Open the source file for reading.
    - Open the output file for writing.
    - Read the header bytes (28 bytes) (Throw an exception on deviations)
    - Validate the magic/version (Throw an exception on deviations).
    - Validate all other header information (Throw an exception on deviations).
    - Configure the class state using header information.
    - initialize `LH` and the cross-sum vectors (`LSM`, `VSM`, `DSM`, `XSM`)
    - initialize a 'residual' cross-sum vector set (`rLSM`, `rVSM`, `rDSM`, `rXSM`) to track the number of unsolved bits
      in the cross-sums.
    - initialize the `CSM` matrix.

- `loadBlock(block_data)` method:

    - Read exactly 18,652 bytes; reject deviations.
        - Parse LH table (16,352 bytes → 511 digests × 32 bytes).
        - Parse cross‑sum vectors from 2,300 bytes as a contiguous bitstream of 4 × (511 × 9) bits, then 4 pad bits.
            - Order: LSM, VSM, DSM, XSM; each 9‑bit value is MSB‑first (v8 … v0) across byte boundaries.
                - Ignore the final four pad bits (block‑level padding).
- `decompress()` method:
    - For each block b in [0...blockCount−1]:
        - call `loadBlock(block_data)` to initialize the cross-sum and `LH` vectors.
        - set the residual vector values to the cross-sum values (indicating zero bits solved in `CSM`).
        - call `deterministicElimination()` to solve obvious `CSM` states.
        - Verify each row’s `LH` via chained SHA‑256 (see `LHChainVerifier`).
        - Pack rows MSB‑first and append to the output buffer.
        - After all blocks, trim trailing padding using the header’s original size and write the output file.
- `deterministicElimination()` method (solves for obvious states in the `CSM` matrix):
    - call `deterministicEliminationHashStep()` (Note: this will only ever be called once)
    - set `solved` = 1
    - while (`solved` > 0):
        - call `solved *= deterministicEliminationBitStep()` (This will return 0 if bits were solved)
        - call `solved *= solveByInference()` method (This will return 0 if bits were solved).
        - The loop terminates when `solved` is zero.
        - Fail if any bits remain unsolved.
- `deterministicEliminationHashStep()` method
    - for r in [0..510]:
        - if `LH[r]` in known_hashes_table:
            - set `CSM[r,0,...s-1]` elements to the known_hashes_table solution.
            - deduct the number of set (1) bits in the known_hashes_table solution from the residual vector.
- `deterministicEliminationBitStep()` method
    - for i in [0..510]:
        - if `LSM[i] == 0` set all `CSM[i,0,...s-1]` elements to 0 and lock them as solved.
        - if `LSM[i] == 1` set all `CSM[i,0,...s-1]` elements to 1 and lock them as solved.
        - if `VSM[i] == 0` set all `CSM[0,...s-1,i]` elements to 0 and lock them as solved.
        - if `VSM[i] == 1` set all `CSM[0,...s-1,i]` elements to 1 and lock them as solved.
        - if `DSM[i] == 0` set all `CSM[d_to_r(i,s), d_to_c(i,s)]` elements to 0 and lock them as solved.
            - `d_to_r()` and `d_to_c()` perform wraparound diagonal coordinate transformation
        - if `DSM[i] == 1` set all `CSM[d_to_r(i,s), d_to_c(i,s)]` elements to 0 and lock them as solved.
            - `d_to_r()` and `d_to_c()` perform wraparound diagonal coordinate transformation
        - if `XSM[i] == 0` set all `CSM[x_to_r(i,s), x_to_c(i,s)]` elements to 0 and lock them as solved.
            - `x_to_r()` and `x_to_c()` perform wraparound anti-diagonal coordinate transformation
        - if `XSM[i] == 1` set all `CSM[x_to_r(i,s), x_to_c(i,s)]` elements to 0 and lock them as solved.
            - `x_to_r()` and `x_to_c()` perform wraparound anti-diagonal coordinate transformation
        - as bits are solved (by row, column, diagonal, or anti-diagonal), subtract them from the residual (r*SM)
          vectors.
- `solveByInference()` method
    - This instantiates the `LoopyBeliefsPropagation` and calls its `solve()` method.
    - When complete, it returns the number of solved bits.

- Acceptance (baseline): must fully assign all bits; otherwise fail the block

## Custom Exceptions

- Reject decompression solutions on:
    - header mismatch (`CrsceHeaderMismatchException`),
    - payload length mismatch (`CrscePayloadLengthMismatchException`),
    - malformed vector bitstreams (`CrsceMalformedVectorException`),
    - LH mismatches (`CrsceLateralHashMismatch`), or
    - Empty Block in compressed input for decompression (`CrsceEmptyBlockException`)
- Enforce bounds:
    - indices [0..510]  (`CrsceIndexOutOfBoundsException`),
    - line counts [0..511]  (`CrsceLineCountException`),
    - non‑zero residuals  (`CrsceNonZeroResidualException`)
