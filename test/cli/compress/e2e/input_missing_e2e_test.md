# Compress CLI: Missing Input Path

## Intent

- Ensure the CLI rejects a non‑existent input file.

## Features Under Test

- Shared in/out validation (`validate_in_out`): input existence check using `stat`.
- Error code contract for filesystem preconditions.

## Expectations

- Exit code is `3` when the input path does not exist.
- The output path is not created.

## Theory

- `validate_in_out` performs:
    - Parse of `-in <file> -out <file>`.
    - `stat(input)` must succeed; else it prints an error and returns `3`.
    - `stat(output)` must fail (output must not exist); otherwise returns `3`.
