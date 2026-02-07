/**
 * @file ch.h
 * @brief SHA-256 choose function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name ch
     * @brief SHA-256 choose function.
     * @param x Word x.
     * @param y Word y.
     * @param z Word z.
     * @return (x & y) ^ (~x & z)
     */
    u32 ch(u32 x, u32 y, u32 z);
}
