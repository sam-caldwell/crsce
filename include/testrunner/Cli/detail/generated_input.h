/**
 * @file generated_input.h
 * @brief Data for a freshly generated random input file.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace crsce::testrunner::cli {
    /**
     * @name GeneratedInput
     * @brief Metadata produced by generate_file().
     */
    struct GeneratedInput {
        std::filesystem::path path;
        std::uint64_t bytes{0};
        std::uint64_t blocks{0};
        std::string sha512;
    };
}
