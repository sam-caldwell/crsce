<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>Build and Test</h1>
                <p>
                  This document defines the build/test automation process and tooling.
                </p>
            </td>
        </tr>
    </table>
</div>

## Overview

This project targets C++23 and uses CMake + Ninja with presets. A Makefile wraps the common workflows. The authoritative
commands and presets are defined in the repository (`CMakePresets.json`, `Makefile.d/*.mk`).

## Prerequisites

- CMake and Ninja
- Homebrew Clang/LLVM (project is configured to use it via presets)
- Python 3 and pip (for venv and flake8)
- Node.js/npm (for markdownlint)

## Quick start

```bash
make ready          # verify tooling
make ready/fix      # install/fix tooling
make clean          # clear build/
make configure      # configure using the default preset
make lint           # markdownlint, shellcheck, flake8, clang-tidy
make build          # compile
make test           # run CTest suites
```

## Presets

- See CMakePresets.json for available presets. Typical entries include `llvm-debug`, `llvm-release`,
  and platform‑specific variants. Artifacts are written under `build/<preset>/`.

## Tips

- `make lint` will fail on any tidy warning (warnings are treated as errors). Fix code; do not weaken linters.
- `make cover` configures an instrumented build and enforces a line coverage threshold (≥ 95%).
- If your environment differs, pass `PRESET=<name>` to `make configure/build/test` to select another preset.
