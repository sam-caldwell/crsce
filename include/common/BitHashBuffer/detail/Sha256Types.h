/**
 * @file Sha256Types.h
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Fundamental type aliases used by SHA-256 routines.
 */
#pragma once

#include <cstdint>

namespace crsce::common::detail::sha256 {

/**
 * @name u8
 * @brief Unsigned 8-bit integer alias used in SHA-256 routines.
 */
using u8 = std::uint8_t;

// Additional width aliases are defined in companion headers to satisfy one-definition-per-header.

} // namespace crsce::common::detail::sha256
