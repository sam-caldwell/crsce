/**
 * @file RunSha256Stdin.h
 * @brief Execute external SHA-256 tool via stdin and capture hex digest.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/HasherUtils/detail/run_sha256_stdin.h"

namespace crsce::common::hasher {
    /**
     * @name RunSha256StdinTag
     * @brief Tag type to satisfy one-definition-per-header for hasher utilities.
     */
    struct RunSha256StdinTag {
    };
} // namespace crsce::common::hasher
