/**
 * @file cmd/hello_world/main.cpp
 * @brief Simple hello world used by CI sanity checks.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include <cstdio>
#include <exception>
#include <print>

/**
 * @brief Program entry point for hello_world sample.
 * @return 0 on success; non-zero on failure.
 */
auto main() -> int {
    try {
        std::println("hello world");
        return 0;
    } catch (const std::exception &e) {
        std::fputs("error: ", stderr);
        std::fputs(e.what(), stderr);
        std::fputs("\n", stderr);
        return 1;
    } catch (...) {
        std::fputs("error: unknown exception\n", stderr);
        return 1;
    }
}
