/**
 * @file maj.h
 * @brief SHA-256 majority function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name maj
     * @brief SHA-256 majority function.
     * @param x Word x.
     * @param y Word y.
     * @param z Word z.
     * @return Majority bit per position among x, y, z.
     */
    u32 maj(u32 x, u32 y, u32 z);
}
