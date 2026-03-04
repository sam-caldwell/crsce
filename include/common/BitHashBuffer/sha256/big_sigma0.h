/**
 * @file big_sigma0.h
 * @author Sam Caldwell
 * @brief SHA-256 Σ0 function.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/Sha256Types32.h" // u32
#include "common/BitHashBuffer/sha256/rotr.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name big_sigma0
     * @brief SHA-256 Σ0 function.
     * @param x Input word.
     * @return Σ0(x) = ROTR2(x) ^ ROTR13(x) ^ ROTR22(x).
     */
    constexpr u32 big_sigma0(const u32 x) {
        return rotr(x, 2U) ^ rotr(x, 13U) ^ rotr(x, 22U);
    }
}
