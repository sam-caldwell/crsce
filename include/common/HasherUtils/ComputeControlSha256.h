/**
 * @file ComputeControlSha256.h
 * @brief Resolve and execute system SHA-256 tool to compute control hash.
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
} // namespace crsce::common::hasher
