/**
 * @file RunSha256Stdin.h
 * @brief Execute external SHA-256 tool via stdin and capture hex digest.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::common::hasher {
    // Execute an external SHA-256 tool reading from stdin and capture hex digest.
    // Returns true on success and stores a 64-char lowercase hex in hex_out.
    bool run_sha256_stdin(const std::vector<std::string> &cmd,
                          const std::vector<std::uint8_t> &data,
                          std::string &hex_out);
} // namespace crsce::common::hasher
