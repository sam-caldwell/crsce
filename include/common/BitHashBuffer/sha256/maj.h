/**
 * @file maj.h
 * @author Sam Caldwell
 * @brief SHA-256 majority function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name maj
     * @brief SHA-256 majority function.
     * @param x Word x.
     * @param y Word y.
     * @param z Word z.
     * @return Majority bit per position among x, y, z.
     */
    constexpr u32 maj(const u32 x, const u32 y, const u32 z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }
}
