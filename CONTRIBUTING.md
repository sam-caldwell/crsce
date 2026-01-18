<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>Code Contributions Guide</h1>
                <p>
                    Welcome! This document explains how to build, test, lint, and contribute changes to this
                    repository. It reflects the project’s conventions and quality gates so your PRs can land smoothly.
                </p>
            </td>
        </tr>
    </table>
</div>

## TL;DR Checklist

- Setup: `make ready`
- Configure: `make configure PRESET=arm64-debug` (or your target preset)
- Build: `make build PRESET=arm64-debug`
- Lint: `make lint` (all linters must pass)
- Tests: `make test PRESET=arm64-debug` (unit → integration → e2e; must be green)
- Coverage: `make cover` (must meet ≥ 95% line coverage gate)
- Style: C++23, 120‑col soft wrap (source) and 120‑col hard wrap (Markdown); no tabs
- Tests added: happy and sad paths for all new behavior, registered with CTest
- Docs: update `docs/` and add required Doxygen‑style comments in source headers/impls
- PR: small, focused commits; include what/why, repro steps, and relevant build/test output

## Repository Layout

- `src/`          Core implementation: `Compress/`, `Decompress/`, `common/`
- `include/`      Public headers
- `cmd/`          Entry points: `compress/`, `decompress/`
- `test/`         Tests organized as `test/<tool>/{unit,integration,e2e}`; wired via CTest
- `cmake/`        CMake helpers
- `CMakeLists.txt` and `CMakePresets.json` for builds
- `Makefile` and `Makefile.d/` for common dev tasks
- `venv/` and `node/` local tooling directories
- `build/<preset>/` per‑preset artifacts (e.g., `build/bin/compress`, `build/bin/decompress`)

## Prerequisites and Setup

- Use `make ready` to verify/install dev prerequisites (cmake, ninja, linters, Python venv, local Node tooling).
- The project targets C++23 and builds with CMake + Ninja. Use the provided presets in `CMakePresets.json`.

## Build

- Configure: `make configure PRESET=arm64-debug`
- Build: `make build PRESET=arm64-debug`
- Direct build example: `cmake --build build/arm64-debug`
- Binaries are produced under `build/bin/`.

## Test

- Run all tests: `make test PRESET=arm64-debug` (executes CTest)
- Coverage gate: `make cover` (GCC/gcov) must achieve ≥ 95% line coverage
- Test organization: place tests under `test/<tool>/{unit,integration,e2e}` and register them in `test/CMakeLists.txt`
- Each feature requires both happy‑path and failure‑path tests; test execution stops on first failure.
- End‑to‑end tests should execute the binaries and validate stdin/stdout/stderr behavior (no internal APIs).

## Lint and Static Analysis

- Run all linters: `make lint`
- Tools: markdownlint, shellcheck, flake8, and C++ static analysis (`cppcheck`)
- All linters must pass before a PR is merged.

## Coding Style and Conventions

- Language: C++23
- Width: source uses 120‑column soft wrap; Markdown uses a 120‑column hard wrap
- Tabs: disallowed (use spaces)
- Includes: use explicit `#include` paths rooted at project folders
- Layout: one primary class/function per file; keep headers focused under `include/`
- Doxygen‑style comments are required for every class, function, macro, enum, etc.
- Single‑responsibility and DRY principles apply; prefer polymorphic and extensible designs.

Required Doxygen header format for each source/header file:

```cpp
/**
 * @file <filename>
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
```

Required Doxygen comment block for constructs:

```cpp
/**
 * @name <identifier>
 * @brief <Brief description>
 * @usage <usage statement>
 * @throws <exception description>
 * @param <parameter_name> <parameter description>
 * @return <return value description>
 */
```

## Feature Notes: Compress and Decompress

The project implements the CRSCE format. When working on features, keep these acceptance points in mind.

### Compress

- CLI: `compress -in <file> -out <file>`
- Header (28 bytes, v1, little‑endian): magic `"CRSC"` (4), version `uint16=1`, header_bytes `uint16=28`,
  original_file_size_bytes `uint64`, block_count `uint64`, header_crc32 `uint32` over preceding header fields
- Empty input: block_count must be 0
- Block size: CSM is 511×511 bits; last block padded with zeros to full size
- Bit mapping: row‑major; bytes are little‑endian on stream, each byte serialized MSB‑first into bits
- Payload per block: exactly 149,216 bits (18,652 bytes) in this order:
  1) `LH[0..510]` (each 256 bits)
  2) `LSM[0..510]` (each 9 bits, MSB‑first)
  3) `VSM[0..510]` (each 9 bits, MSB‑first)
  4) `DSM[0..510]` (each 9 bits, MSB‑first)
  5) `XSM[0..510]` (each 9 bits, MSB‑first)
  6) four trailing zero padding bits
- Cross‑sum computation: canonical loop bounds 0..510 with modulo addressing for diagonals; each entry in 0..511
- LH chain: SHA‑256 with seed `RG9uYWxkVHJ1bXBJbXBlYWNoSW5jYXJjZXJhdGVIaXN0b3J5UmVtZW1iZXJz`,
  `N=SHA256(seed)`, `LH[0]=SHA256(N||RowBytes(0))`, `LH[r]=SHA256(LH[r-1]||RowBytes(r))` for r=1..510

### Decompress

- CLI: `decompress -in <file> -out <file>`
- Must parse fixed 18,652‑byte payloads per block in the exact field order and lengths above
- Acceptance: reconstructed CSM must exactly match stored cross‑sum vectors and recomputed LH chain; otherwise reject
- Byte stream: traverse CSM row‑major, pack bits MSB‑first into bytes; remove end padding using original size
- Fail‑hard: stop and return error on invalid blocks or timeout; do not produce partial output by default

## Security and Robustness Notes

- All `decompress` inputs are untrusted. Validate header fields, block sizes, indices, and bounds before allocation.
- Enforce strict parsing and acceptance rules; treat nonzero trailing padding bits as a format error if applicable.
- Implement timeouts or guardrails for decompression attempts that exceed configured limits.
- SHA‑256 must match the standard exactly for LH computations.

## Adding and Registering Tests

- Place new tests under `test/<tool>/{unit,integration,e2e}`.
- Each test file should define exactly one test function.
- Register tests in `test/CMakeLists.txt` so they run under CTest.
- Provide both happy and sad path coverage for each feature.
- For e2e tests, launch the binary from `build/bin/` and validate stdin/stdout/stderr; avoid direct access to internals.

## Contribution Workflow

1) Discuss: open an issue for new features or non‑trivial changes; describe motivation and approach.
2) Branch: create a topic branch (e.g., `feature/<slug>`, `fix/<slug>`).
3) Commit: small, focused commits with imperative subjects (e.g., `add cmake build system`, `fix: lint markdown`).
4) Lint & Test: run `make lint`, `make test`, and `make cover` locally; ensure all pass.
5) PR: include what/why, linked issues, reproduction steps (commands), and relevant build/test output.
6) Review: respond to feedback; keep changes narrowly scoped; prefer follow‑ups for unrelated work.

### Commit Message Guidelines

- Subject is short and imperative (e.g., `fix: bounds check in reader`).
- Body (optional) explains rationale, not just what changed; reference issues when relevant.
- Avoid squashing unrelated changes into a single commit.

### Pull Request Checklist

- `make ready` completed
- `make configure` + `make build` for all impacted presets
- `make lint` passes (no warnings left unaddressed)
- `make test` passes (unit → integration → e2e)
- `make cover` meets ≥ 95% coverage
- Tests added/updated with happy and sad paths
- Doxygen comments complete; docs in `docs/` updated if behavior or interfaces changed
- PR description includes commands to reproduce and relevant outputs

## Documentation

- Author documentation in Markdown under `docs/` with a 120‑column hard wrap.

## Tips for Consistency

- Prefer simple, readable algorithms; avoid premature optimization unless justified by tests/benchmarks.
- Keep changes minimal and focused; do not fix unrelated issues in the same PR.
- When touching bit‑packing/serialization, add focused tests that validate bit order and byte alignment explicitly.
- Use `git blame` and `git log` to align with existing patterns and intentions.

## Getting Help

- If anything is unclear or you hit build/test issues, open an issue with your environment details and command output.

## Please seek first to understand what has been done before you start

![That's a paddlin](docs/img/paddlin.png)
