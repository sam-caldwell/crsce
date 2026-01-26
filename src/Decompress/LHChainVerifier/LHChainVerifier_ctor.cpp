/**
 * @file LHChainVerifier_ctor.cpp
 * @brief Implementation of LHChainVerifier constructor.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @name LHChainVerifier::LHChainVerifier
     * @brief Construct an LHChainVerifier and derive the initial seed hash.
     * @param seed Seed string used to derive the first link hash.
     * @return void
     */
    LHChainVerifier::LHChainVerifier(std::string seed) {
        const std::vector<std::uint8_t> seed_bytes(seed.begin(), seed.end());
        seed_hash_ = crsce::common::detail::sha256::sha256_digest(seed_bytes.data(), seed_bytes.size());
    }
} // namespace crsce::decompress
