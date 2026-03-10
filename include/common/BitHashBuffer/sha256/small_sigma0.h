/**
 * @file small_sigma0.h
 * @author Sam Caldwell
 * @brief SHA-256 σ0 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types32.h" // u32
#include "common/BitHashBuffer/sha256/rotr.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name small_sigma0
     * @brief SHA-256 σ0 function.
     * @param x Input word.
     * @return σ0(x) = ROTR7(x) ^ ROTR18(x) ^ (x >> 3).
     */
    constexpr u32 small_sigma0(const u32 x) { return rotr(x, 7U) ^ rotr(x, 18U) ^ (x >> 3U); }
}
