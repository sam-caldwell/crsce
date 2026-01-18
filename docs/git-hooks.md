<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>Git Hooks</h1>
                <p>
                  This page explains the project’s Git hooks, how to install them, and what they check before allowing
                  a commit. Hooks are stored in <code>git-hooks/</code> and linked into <code>.git/hooks/</code> by the
                  build tooling.
                </p>
            </td>
        </tr>
    </table>
</div>

## Overview

The repository ships two local hooks:

- `pre-commit` (Python 3): blocks commits if whitespace errors are present, linters fail, or coverage is below the
  threshold.
- `commit-msg` (Python 3): enforces a minimal commit message length (≥ 10 characters excluding comments/blank lines).

Hooks run locally and are intended to match CI behavior, so problems are caught early.

## Installation

Creates symlinks from `.git/hooks/` to `git-hooks/`:

```bash
make hooks
```

### Notes

- The `make hooks` target calls a CMake script that creates or refreshes the symlinks for all files in `git-hooks/`.
- The repository must already be a Git repository (i.e., `.git/` exists).
- You can re-run `make hooks` any time to refresh links after hook updates.

This instructs Git to look directly in `git-hooks/` for hooks (no symlinks). Either approach is fine; the Makefile
target standardizes behavior across environments.

## Hook behavior

### `pre-commit`

Runs fast, local quality gates and blocks the commit on any failure:

1) Whitespace check on staged changes

   ```bash
    git diff-index --check --cached <against> --
   ```

   `<against>` is `HEAD`, or the empty tree hash for the first commit.
2) Linters

   ```bash
   make lint
   ```

   Runs markdownlint, shellcheck, flake8, and clang-tidy via CMake scripts.
3) Coverage enforcement

   ```bash
   make cover
   ```

   Configures an instrumented build, runs tests, and enforces a minimum line coverage threshold (≥ 95%).

### `commit-msg`

Validates that the commit message contains at least 10 non-comment, non-empty characters. If the requirement is not
met, the commit is blocked.

## Prerequisites

- Python 3 on your PATH (both hooks are Python scripts).
- Build toolchain set up: run `make ready` to verify, or `make ready/fix` to install/fix prerequisites.
- For `make lint` (clang-tidy), ensure a configured build exists with a compile database:

```bash
make configure
```

- For `make cover`, Homebrew LLVM tools (`clang++`, `llvm-profdata`, `llvm-cov`) must be available; `make ready/fix`
  will install or guide setup as needed.

## Tips and troubleshooting

- Missing a compile database for clang-tidy: run `make configure` before `make lint`.
- Coverage tools not found: run `make ready/fix`.
- Hooks not firing: ensure symlinks exist (`make hooks`) or configure `core.hooksPath`.
- Debugging: you can run hooks directly. For example,

```bash
git-hooks/pre-commit
# or
git-hooks/commit-msg .git/COMMIT_EDITMSG
```

To bypass hooks temporarily (not recommended in shared branches):

```bash
git commit --no-verify
```

Use this sparingly and only when you understand the CI implications.
