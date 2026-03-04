/**
 * @file small_sigma1.h
 * @author Sam Caldwell
 * @brief SHA-256 σ1 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types32.h" // u32
#include "common/BitHashBuffer/sha256/rotr.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name small_sigma1
     * @brief SHA-256 σ1 function.
     * @param x Input word.
     * @return σ1(x) = ROTR17(x) ^ ROTR19(x) ^ (x >> 10).
     */
    constexpr u32 small_sigma1(const u32 x) {
        return rotr(x, 17U) ^ rotr(x, 19U) ^ (x >> 10U);
    }
}
