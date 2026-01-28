# Compress CLI: Missing Output Parent Directory

## Intent

- Validate behavior when the output’s parent directory does not exist.

## Features Under Test

- Error propagation from `Compress::compress_file()` to the CLI.
- Exit‑code mapping for runtime I/O failures.

## Expectations

- Exit code is `4` (pipeline failure) when the output parent directory is missing.

## Theory

- `validate_in_out` only checks that the output file does not already exist; it does not create parent directories.
- `Compress::compress_file()` opens the output file with `std::ofstream`. If the parent directory is missing, the open
  fails and the function returns `false`.
- `compress::cli::run` maps a failed `compress_file()` to an exit code of `4` after printing
  `error: compression failed`.
