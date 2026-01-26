/**
 * @file small_sigma0.h
 * @brief SHA-256 σ0 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types32.h" // u32

namespace crsce::common::detail::sha256 {
    /**
     * @name small_sigma0
     * @brief SHA-256 σ0 function.
     * @param x Input word.
     * @return σ0(x) = ROTR7(x) ^ ROTR18(x) ^ (x >> 3).
     */
    u32 small_sigma0(u32 x);
}
