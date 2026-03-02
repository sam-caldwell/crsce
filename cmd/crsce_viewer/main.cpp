/**
 * @file cmd/crsce_viewer/main.cpp
 * @author Sam Caldwell
 * @brief CLI entry for crsce_viewer; delegates to crsce::viewer::run_viewer().
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>

#include "crsce_viewer/viewer.h"

/**
 * @brief Program entry point for crsce_viewer.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code (0 on success).
 */
auto main(const int argc, char *argv[]) -> int try {
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    if (args.size() != 2U) {
        std::cerr << "usage: crsce_viewer <file.crsce>\n";
        return 2;
    }

    return crsce::viewer::run_viewer(std::string(args[1]), std::cout, std::cerr);
} catch (const std::exception &e) {
    std::cerr << "crsce_viewer: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "crsce_viewer: unknown exception\n";
    return 1;
}
