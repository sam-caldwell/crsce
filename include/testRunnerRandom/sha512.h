/**
 * @file sha512.h
 * @brief Compute SHA-512 via external tools.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include <string>

namespace crsce::testrunner::detail {
    /**
     * @name compute_sha512
     * @brief Compute SHA-512 of a file using system tools.
     */
    std::string compute_sha512(const std::filesystem::path &file);
}
