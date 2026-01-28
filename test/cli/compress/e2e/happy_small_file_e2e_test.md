# Compress CLI: Small File Success

## Intent

- Validate the happy‑path: a small input file compresses successfully via the CLI.

## Features Under Test

- CLI argument parsing and validation (`-in`, `-out`).
- Filesystem checks (input exists, output does not).
- Compression pipeline wiring: `compress::cli::run` → `Compress::compress_file()`.
- Container file creation at the requested output path.

## Expectations

- Exit code is `0`.
- Output file exists after the run.
- The test does not assert specific output size or payload contents.

## Notes

- For any non‑empty input, the v1 container will include a 28‑byte header and at least one 18,652‑byte block payload, so
  total size is typically `28 + 18652` for small inputs. This test purposefully limits itself to existence/exit‑code to
  keep it fast and focused on the CLI path.
