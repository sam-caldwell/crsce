/**
 * @file big_sigma1.h
 * @author Sam Caldwell
 * @brief SHA-256 Σ1 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types32.h" // u32
#include "common/BitHashBuffer/sha256/rotr.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name big_sigma1
     * @brief SHA-256 Σ1 function.
     * @param x Input word.
     * @return Σ1(x) = ROTR6(x) ^ ROTR11(x) ^ ROTR25(x).
     */
    constexpr u32 big_sigma1(const u32 x) {
        return rotr(x, 6U) ^ rotr(x, 11U) ^ rotr(x, 25U);
    }
}
