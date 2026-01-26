/**
 * @file run_sha256_stdin.h
 * @brief Execute external SHA-256 tool via stdin and capture hex digest.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::common::hasher {
    /**
     * @name run_sha256_stdin
     * @brief Execute an external SHA-256 tool, sending input via stdin.
     * @param cmd Command vector where element 0 is the executable.
     * @param data Raw bytes to hash (written to the process stdin).
     * @param hex_out Output parameter receiving a 64-char lowercase hex digest on success.
     * @return true on success; false on failure (with error printed to stderr).
     */
    bool run_sha256_stdin(const std::vector<std::string> &cmd,
                          const std::vector<std::uint8_t> &data,
                          std::string &hex_out);
}
