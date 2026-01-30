/**
 * @file generate_file.h
 * @brief Generate random input, hash it, and log step.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include "testrunner/Cli/detail/generated_input.h"

namespace crsce::testrunner::cli {
    /**
     * @name generate_file
     * @brief Create a random input file, compute SHA-512, and log.
     * @param out_dir Directory to place the generated file.
     * @return GeneratedInput containing path, size, blocks, and hash.
     */
    GeneratedInput generate_file(const std::filesystem::path &out_dir);
}
