/**
 * @file compute_control_sha256_cmd.h
 * @brief Compute control SHA-256 using an explicit command vector.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::common::hasher {
    /**
     * @name compute_control_sha256_cmd
     * @brief Run a specific command (argv-style) to compute a SHA-256 digest.
     * @param cmd Command vector where element 0 is the executable.
     * @param data Raw bytes to hash (written to the process stdin).
     * @param hex_out Output parameter receiving a 64-char lowercase hex digest on success.
     * @return true on success; false on failure (with error printed to stderr).
     */
    bool compute_control_sha256_cmd(const std::vector<std::string> &cmd,
                                    const std::vector<std::uint8_t> &data,
                                    std::string &hex_out);
}
