/**
 * @file Sha256.h
 * @brief Aggregator for SHA-256 declarations; includes per-function headers.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include "common/BitHashBuffer/detail/Sha256Types.h"      // u8
#include "common/BitHashBuffer/detail/Sha256Types32.h"    // u32
#include "common/BitHashBuffer/detail/Sha256Types64.h"    // u64

#include "common/BitHashBuffer/detail/sha256/rotr.h"
#include "common/BitHashBuffer/detail/sha256/ch.h"
#include "common/BitHashBuffer/detail/sha256/maj.h"
#include "common/BitHashBuffer/detail/sha256/big_sigma0.h"
#include "common/BitHashBuffer/detail/sha256/big_sigma1.h"
#include "common/BitHashBuffer/detail/sha256/small_sigma0.h"
#include "common/BitHashBuffer/detail/sha256/small_sigma1.h"
#include "common/BitHashBuffer/detail/sha256/store_be32.h"
#include "common/BitHashBuffer/detail/sha256/load_be32.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "common/BitHashBuffer/detail/sha256/K.h"

namespace crsce::common::detail::sha256 {
    /**
     * @name Sha256Tag
     * @brief Tag type to satisfy one-definition-per-header for the aggregator.
     */
    struct Sha256Tag {
    };
}
