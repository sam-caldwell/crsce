# Compress CLI: No‑Args Greeting

## Intent

- Confirm the CLI’s no‑argument behavior is friendly and successful.

## Features Under Test

- Early no‑arg short‑circuit in `compress::cli::run`.

## Expectations

- Exit code is `0`.
- A greeting is printed (e.g., `crsce-compress: ready`).

## Notes

- No parsing or filesystem checks occur in this path; it exits before running the compression pipeline.
