/**
 * @file MuLockFlag.h
 * @brief MU-guard flag for per-cell write serialization (not a resolve-lock).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::decompress {
    /**
     * @enum MuLockFlag
     * @brief Controls whether a per-cell MU guard is taken during writes.
     *        This is strictly for concurrency; it does not mark a cell resolved.
     */
    enum class MuLockFlag : std::uint8_t {
        Locked = 1,
        Unlocked = 0
    };
}
