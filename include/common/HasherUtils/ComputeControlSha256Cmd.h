/**
 * @file ComputeControlSha256Cmd.h
 * @brief Compute control SHA-256 using an explicit command vector.
 * © Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>
#include <vector>

namespace crsce::common::hasher {
    // Compute control hash by running the provided command vector (argv[0..]).
    // Returns true on success and stores lowercase 64-char hex in hex_out.
    bool compute_control_sha256_cmd(const std::vector<std::string> &cmd,
                                    const std::vector<std::uint8_t> &data,
                                    std::string &hex_out);

    /**
     * @name ComputeControlSha256CmdTag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ComputeControlSha256CmdTag {};
} // namespace crsce::common::hasher
