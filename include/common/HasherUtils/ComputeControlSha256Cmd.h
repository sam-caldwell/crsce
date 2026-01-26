/**
 * @file ComputeControlSha256Cmd.h
 * @brief Compute control SHA-256 using an explicit command vector.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/HasherUtils/detail/compute_control_sha256_cmd.h"

namespace crsce::common::hasher {
    /**
     * @name ComputeControlSha256CmdTag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ComputeControlSha256CmdTag {
    };
} // namespace crsce::common::hasher
