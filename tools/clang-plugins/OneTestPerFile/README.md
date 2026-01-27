# OneTestPerFile Clang Plugin

## Purpose

- Enforces for files under `test/**/*.cpp`:
    - At most one construct per file: either a single helper free function definition or a single `TEST(...)` macro.
    - File must start at line 1 with a Doxygen header block that contains:
        - `@file <current filename>`
        - `@copyright … Sam Caldwell … LICENSE.txt`
    - Every helper function definition and every `TEST(...)` must be immediately preceded by a Doxygen block (`/** ... */`)
      that includes `@brief`. For `TEST(...)`, the Doxygen block must include both `@name` and `@brief`.

## Build

- Requirements: LLVM/Clang development packages available in your environment.
- Recommended: `make deps` (builds tools into `build/tools/...` incrementally).

## Usage

- Load the plugin during compilation. Example with `clang++`:
    - macOS:

      ```c++
        clang++ -Xclang \
                -load \
                -Xclang build/tools/clang-plugins/OneTestPerFile/libOneTestPerFile.dylib \
                -Xclang \
                -plugin \
                -Xclang OneTestPerFile \
                -c test/some/path/foo_test.cpp
      ```

    - Linux:

      ```c++
        clang++ -Xclang \
                -load \
                -Xclang build/tools/clang-plugins/OneTestPerFile/libOneTestPerFile.so \
                -Xclang \
                -plugin \
                -Xclang OneTestPerFile \
                -c test/some/path/foo_test.cpp
      ```

## Notes

- Only `.cpp` files whose path contains `/test/` (or `\\test\\` on Windows) are checked.
- Files under any `fixtures/` directory are excluded from OTPF enforcement.
- The plugin reports diagnostics as errors when violations are found.
- The header docstring check is literal and compares the first bytes of the file with the block above, including
  whitespace.
