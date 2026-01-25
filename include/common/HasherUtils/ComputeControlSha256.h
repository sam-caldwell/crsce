/**
 * @file ComputeControlSha256.h
 * @brief Resolve and execute system SHA-256 tool to compute control hash.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::common::hasher {
    // Resolve and execute a system SHA-256 tool based on environment overrides
    // and common defaults (sha256sum/shasum). Prints a helpful error to stderr
    // and returns false on failure. On success, stores a 64-char lowercase hex
    // digest in hex_out and returns true.
    bool compute_control_sha256(const std::vector<std::uint8_t> &data,
                                std::string &hex_out);

    /**
     * @name ComputeControlSha256Tag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ComputeControlSha256Tag {};
} // namespace crsce::common::hasher
