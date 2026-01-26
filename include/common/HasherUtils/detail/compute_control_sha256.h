/**
 * @file compute_control_sha256.h
 * @brief Resolve and execute system SHA-256 tool to compute control hash.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::common::hasher {
    /**
     * @name compute_control_sha256
     * @brief Compute a control SHA-256 digest using system tooling.
     * @param data Raw bytes to hash.
     * @param hex_out Output parameter receiving a 64-char lowercase hex digest on success.
     * @return true on success; false on failure (with error printed to stderr).
     */
    bool compute_control_sha256(const std::vector<std::uint8_t> &data,
                                std::string &hex_out);
}
