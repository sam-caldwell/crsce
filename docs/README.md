<div>
    <table>
        <tr>
            <td style="min-width:200px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>CRSCE Documentation</h1>
                <p>
                    This index links to all documentation under `docs/`. These pages derive from the repository’s
                    authoritative online sources.
                </p>
            </td>
        </tr>
    </table>
</div>

## Getting Started

Start here if you’re new:

- Overview and Concepts: `docs/overview.md`
- Usage Guides: `docs/usage.md`

### Full index

- [Overview and Concepts](./overview.md)
- [Theory of Operation](./theory.md)
- [File Format](./format.md)
- [Decompression Notes](decompression.md)
- [Usage Guides](./usage.md)
- [Build and Test](./build.md)
- [Security and Robustness](./security.md)
- [Git Hooks](./git-hooks.md)

### Tools and Plugins

- [Clang Plugin: One Test Per File](../tools/clang-plugins/one-test-per-file/README.md)

### Notes

- The “Theory” and “Format” pages capture the acceptance criteria and bit/byte ordering used by the tools.
- The decompression algorithm notes outline a practical CPU strategy (DE + GOBP/LBP) with LH chain verification.
- For contribution, coding style, and PR process details, see [CONTRIBUTING.md](../CONTRIBUTING.md)`.
