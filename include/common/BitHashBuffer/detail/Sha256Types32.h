/**
 * @file Sha256Types32.h
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 * @brief 32-bit alias used by SHA-256 routines.
 */
#pragma once

#include <cstdint>

namespace crsce::common::detail::sha256 {
    /**
     * @name u32
     * @brief Unsigned 32-bit integer alias used in SHA-256 routines.
     */
    using u32 = std::uint32_t;
} // namespace crsce::common::detail::sha256
