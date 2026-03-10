/**
 * @file ch.h
 * @author Sam Caldwell
 * @brief SHA-256 choose function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name ch
     * @brief SHA-256 choose function.
     * @param x Word x.
     * @param y Word y.
     * @param z Word z.
     * @return (x & y) ^ (~x & z)
     */
    constexpr u32 ch(const u32 x, const u32 y, const u32 z) {
        return (x & y) ^ (~x & z);
    }
}
