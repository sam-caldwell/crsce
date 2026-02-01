/**
 * @file preflight.h
 * @brief Alternating01 input preflight checks for CSM bit patterns.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#pragma once

#include <cstdint>
#include <filesystem>

namespace crsce::testrunner_alternating01::cli {
    /**
     * @name preflight_check_alternating01
     * @brief Validate that the generated Alternating01 input yields expected CSM bits for the first block.
     * @param in_path Filesystem path to the generated input file.
     * @param bytes Size of the generated input file in bytes.
     * @return void
     * @throws std::runtime_error if any invariant fails or the file cannot be read.
     */
    void preflight_check_alternating01(const std::filesystem::path &in_path,
                                       std::uint64_t bytes);
}

