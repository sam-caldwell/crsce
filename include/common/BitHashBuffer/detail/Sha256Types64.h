/**
 * @file Sha256Types64.h
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 * @brief 64-bit alias used by SHA-256 routines.
 */
#pragma once

#include <cstdint>

namespace crsce::common::detail::sha256 {
    /**
     * @name u64
     * @brief Unsigned 64-bit integer alias used in SHA-256 routines (length encoding).
     */
    using u64 = std::uint64_t;
} // namespace crsce::common::detail::sha256
