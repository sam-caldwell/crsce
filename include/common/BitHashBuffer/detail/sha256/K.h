/**
 * @file K.h
 * @brief SHA-256 round constants table (64 entries).
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <array>
#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name K
     * @brief SHA-256 round constants table (64 entries).
     */
    extern const std::array<u32, 64> K;
}
