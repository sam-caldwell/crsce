/**
 * @file ComputeControlSha256.h
 * @brief Resolve and execute system SHA-256 tool to compute control hash.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include "common/HasherUtils/detail/compute_control_sha256.h"

namespace crsce::common::hasher {
    /**
     * @name ComputeControlSha256Tag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct ComputeControlSha256Tag {
    };
} // namespace crsce::common::hasher
