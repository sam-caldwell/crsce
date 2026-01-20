/**
 * File: cmd/hello_world/main.cpp
 * Brief: simple hello world used by CI sanity checks.
 */
#include <cstdio>
#include <exception>
#include <print>

/**
 * Main: prints a simple greeting to stdout.
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
