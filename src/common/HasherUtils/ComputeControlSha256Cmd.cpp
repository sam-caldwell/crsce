/**
 * @file ComputeControlSha256Cmd.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/HasherUtils/ComputeControlSha256Cmd.h"
#include "common/HasherUtils/detail/run_sha256_stdin.h"
#include <vector>
#include <string>
#include <cstdint>

namespace crsce::common::hasher {
    /**
     * @name compute_control_sha256_cmd
     * @brief Run a specific command to compute SHA-256 over stdin data.
     * @param cmd Executable and arguments vector (e.g., {"sha256sum", "-"}).
     * @param data Input bytes to hash.
     * @param hex_out Output string to receive lowercase hex digest.
     * @return bool True on success; false on execution or format failure.
     */
    bool compute_control_sha256_cmd(const std::vector<std::string> &cmd,
                                    const std::vector<std::uint8_t> &data,
                                    std::string &hex_out) {
        return run_sha256_stdin(cmd, data, hex_out);
    }
} // namespace crsce::common::hasher
