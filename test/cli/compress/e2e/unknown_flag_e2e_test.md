# Compress CLI: Unknown Flag

## Intent

- Ensure the CLI rejects unknown flags and reports usage.

## Features Under Test

- `ArgParser` integration and error handling in `validate_in_out`.

## Expectations

- Exit code is `2` (usage error) when an unknown flag is provided.
- Usage text is printed to stderr.

## Notes

- Unknown flags cause parsing to fail and are classified as a usage error (`2`), distinct from missing `-in` value (
  `4`).
