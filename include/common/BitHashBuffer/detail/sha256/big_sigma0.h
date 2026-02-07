/**
 * @file big_sigma0.h
 * @brief SHA-256 Σ0 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name big_sigma0
     * @brief SHA-256 Σ0 function.
     * @param x Input word.
     * @return Σ0(x) = ROTR2(x) ^ ROTR13(x) ^ ROTR22(x).
     */
    u32 big_sigma0(u32 x);
}
