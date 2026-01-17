<div>
    <table>
        <tr>
            <td><img src="docs/img/logo.png" width="300" alt="CRSCE logo"/></td>
            <td>
                <h1>Basic commands</h1>
                <p>
                    This document discusses the CRSCE command artifacts and their usage.
                </p>
            </td>
        </tr>
    </table>
</div>

## Compression

```bash
# Compress a file
build/bin/compress -in input.bin -out output.crsce
```

- Required flags: `-in <path>` and `-out <path>`.
- On error cases, the tool prints a usage string and returns a non‑zero exit code.
- Validation performed by the CLI wrapper before invoking the core logic:
    - The input file must exist.
    - The output file must not already exist.

## Decompression

```bash
# Decompress a file produced by CRSCE
build/bin/decompress -in output.crsce -out recovered.bin
```

- Required flags: `-in <path>` and `-out <path>`.
- Acceptance criteria are strict and described in docs/format.md and docs/theory.md.
- On any parsing or acceptance failure, the program must stop and return an error (fail‑hard by default).

## Help and errors

```bash
build/bin/compress   -h
build/bin/decompress -h
```

## Typical diagnostics from the CLI wrapper

- `usage: compress -in <file> -out <file>`
- `error: input file does not exist: <path>`
- `error: output file already exists: <path>`

## Exit codes

- 0: success
- non‑zero: failure (parsing error, missing/invalid arguments, invalid file, etc.)
