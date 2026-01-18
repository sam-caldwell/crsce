<div>
    <table>
        <tr>
            <td width="300px"><img src="docs/img/logo.png" alt="CRSCE logo"/></td>
            <td>
                <h1>CRSCE — Cross-Sums Compression and Expansion</h1>
                <p>
                    CRSCE is a research proof-of-concept compression format and toolchain that encodes a 511×511 bit
                    Cross‑Sum Matrix (CSM) per block using four families of cyclic cross‑sum constraints and a chained
                    SHA‑256 Lateral Hash (LH) array. The project ships two C++23 binaries, `compress` and `decompress`,
                    built with CMake/Ninja and verified by CTest. The format prioritizes integrity and deterministic
                    verification; it does not provide confidentiality.
                </p>
            </td>
        </tr>
    </table>
</div>

## Quick Start

```bash
make ready                     # verify local tooling
make ready/fix                 # install/fix local tooling.
make clean                     # clean build artifacts
make configure                 # configure CMake
make lint                      # lint all the things!
make build                     # build all projects
make test                      # run unit, integration and end-to-end tests
make cover                     # optional: enforce code coverage
```

* Artifacts are written under `build/<preset>/`.
* Binaries are available at `build/bin/compress` and `build/bin/decompress` after a successful build.
* A `hello_world/` example is available at `cmd/hello_world/main.cpp` This is a canary for the build system.

## Documentation

See the documentation index at [docs/README.md](docs/README.md) for an overview, theory, format details, usage guides,
build instructions, and security notes.

## Usage

```bash
# Compress a file
build/bin/compress   -in input.bin  -out output.crsce

# Decompress a file
build/bin/decompress -in output.crsce -out recovered.bin
```

## Project Layout

| Directory  | Description                                          |
|------------|------------------------------------------------------|
| `src/`     | core modules (`Compress/`, `Decompress/`, `common/`) |
| `include/` | public headers                                       |
| `cmd/`     | entrypoints (`compress/`, `decompress/`)             |
| `test/`    | CTest suites (`test/<tool>/{unit,integration,e2e}`)  |
| `cmake/`   | CMake helpers; presets in `CMakePresets.json`        |
| `docs/`    | additional documentation                             |
| `build/`   | per‑preset build artifacts (created by the build)    |

## References

* [Documentation](docs/README.md)
* [CRSCE specification](https://samcaldwell.net/2024/12/03/cross-sums-compression-and-expansion-crsce/)

## License

Copyright &copy; 2026 Sam Caldwell. See [LICENSE.txt](LICENSE.txt) for details.
