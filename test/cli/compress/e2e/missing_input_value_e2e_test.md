# Compress CLI: Missing `-in` Value

## Intent

- Verify the CLI returns the correct error code when `-in` is provided without a value.

## Features Under Test

- Argument parsing and error classification in `validate_in_out`.

## Expectations

- Exit code is `4` (insufficient arguments) when `-in` is missing its value.

## Notes

- Distinct from `-out` missing its value (which returns `2`, a usage error), the `-in` case is treated explicitly as
  insufficient arguments (`4`).
