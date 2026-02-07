/**
 * @file big_sigma1.h
 * @brief SHA-256 Σ1 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name big_sigma1
     * @brief SHA-256 Σ1 function.
     * @param x Input word.
     * @return Σ1(x) = ROTR6(x) ^ ROTR11(x) ^ ROTR25(x).
     */
    u32 big_sigma1(u32 x);
}
