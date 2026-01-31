/**
 * @file make_input.h
 * @brief Produce GeneratedInput metadata (hash, blocks) for an existing input file and log the hash event.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <filesystem>

#include "testrunner/Cli/detail/generated_input.h"

namespace crsce::testrunner::cli {
    /**
     * @name make_input
     * @brief Compute SHA-512 and blocks metadata for an input file and log the hash timing.
     * @param in_path Existing input file path.
     * @param in_bytes Size of the input in bytes.
     * @return GeneratedInput with path, bytes, blocks, and sha512 populated.
     */
    GeneratedInput make_input(const std::filesystem::path &in_path, std::uint64_t in_bytes);
}

