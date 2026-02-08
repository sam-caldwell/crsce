/**
 * @file ValidateContainer.h
 * @brief Declaration of CRSCE container validator utility.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <filesystem>
#include <string>

namespace crsce::common::util {
    /**
     * @name validate_container
     * @brief Validate that a file is a syntactically correct CRSCE v1 container.
     * @param cx_path Path to the candidate CRSCE container file.
     * @param err Output parameter set to a human-readable reason on failure.
     * @return bool True if the container passes structural validation; false otherwise.
     * @details This function performs light-weight structural checks:
     *          - Verifies minimum size for header presence
     *          - Parses and validates the v1 header (magic, version, sizes, CRC32)
     *          - Confirms file size matches header-declared block count and layout
     *          - Scans each block to ensure LH and cross-sum segments have expected sizes
     */
    bool validate_container(const std::filesystem::path &cx_path, std::string &err);
}

