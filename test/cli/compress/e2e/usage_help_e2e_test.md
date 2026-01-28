# Compress CLI: Usage Help (`-h`)

## Intent

- Verify that `-h` requests short‑circuit the pipeline and return success.

## Features Under Test

- Help handling in `validate_in_out`.

## Expectations

- Exit code is `0` when `-h` is provided.
- Usage text is printed.

## Notes

- The presence of `-h` causes the validator to print usage and return `0` without running any further checks.
