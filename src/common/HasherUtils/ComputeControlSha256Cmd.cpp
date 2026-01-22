/**
 * @file ComputeControlSha256Cmd.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/HasherUtils/ComputeControlSha256Cmd.h"
#include "common/HasherUtils/RunSha256Stdin.h"
#include <vector>
#include <string>
#include <cstdint>

namespace crsce::common::hasher {

/**

 * @brief Implementation detail.

 */

bool compute_control_sha256_cmd(const std::vector<std::string> &cmd,
                                const std::vector<std::uint8_t> &data,
                                std::string &hex_out) {
  return run_sha256_stdin(cmd, data, hex_out);
}

} // namespace crsce::common::hasher
