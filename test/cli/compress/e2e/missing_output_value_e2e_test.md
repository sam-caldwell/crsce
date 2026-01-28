# Compress CLI: Missing `-out` Value

## Intent

- Verify the CLI returns a usage error when `-out` is provided without a value.

## Features Under Test

- Argument parsing and error classification in `validate_in_out`.

## Expectations

- Exit code is `2` (usage error) when `-out` is missing its value (usage is printed).

## Notes

- Distinct from `-in` without a value (which returns `4`, insufficient arguments).
