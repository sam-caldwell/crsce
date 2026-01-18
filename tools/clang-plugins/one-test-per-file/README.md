One-Test-Per-File Clang Plugin

Purpose
- Enforces for files under `test/**/*.cpp`:
  - Exactly one `TEST(...)` macro per file.
  - File must start at line 1 with a Doxygen header block that contains:
    - `@file <current filename>`
    - `@copyright … Sam Caldwell … LICENSE.txt`
  - Every function definition and every `TEST(...)` must be immediately preceded by a Doxygen block (`/** ... */`) that includes `@brief`.

Build
- Requirements: LLVM/Clang development packages available in your environment.
- Commands:
  - `cmake -S tools/clang-plugins/one-test-per-file -B build/one-test-per-file`
  - `cmake --build build/one-test-per-file -j`

Usage
- Load the plugin during compilation. Example with `clang++`:
  - macOS: `clang++ -Xclang -load -Xclang build/one-test-per-file/libone_test_per_file.dylib -Xclang -plugin -Xclang one-test-per-file -c test/some/path/foo_test.cpp`
  - Linux: `clang++ -Xclang -load -Xclang build/one-test-per-file/libone_test_per_file.so -Xclang -plugin -Xclang one-test-per-file -c test/some/path/foo_test.cpp`

Notes
- Only `.cpp` files whose path contains `/test/` (or `\\test\\` on Windows) are checked.
- The plugin reports diagnostics as errors when violations are found.
- The header docstring check is literal and compares the first bytes of the file with the block above, including whitespace.
