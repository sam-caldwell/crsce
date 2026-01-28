# Compress CLI: Output Already Exists

## Intent

- Ensure the CLI refuses to overwrite an existing output file.

## Features Under Test

- Shared in/out validation (`validate_in_out`): output non‑existence check using `stat`.

## Expectations

- Exit code is `3` when the output path already exists.

## Theory

- `validate_in_out` requires that `stat(output)` fails, i.e., the output must not exist yet. If it exists, the function
  prints an error and returns `3` without invoking the compression pipeline.
