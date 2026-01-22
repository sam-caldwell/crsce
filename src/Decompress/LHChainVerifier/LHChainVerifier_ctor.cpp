/**
 * @file LHChainVerifier_ctor.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "common/BitHashBuffer/detail/Sha256.h"

#include <cstdint>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @brief Implementation detail.
     */
    LHChainVerifier::LHChainVerifier(std::string seed) {
        const std::vector<std::uint8_t> seed_bytes(seed.begin(), seed.end());
        seed_hash_ = crsce::common::detail::sha256::sha256_digest(seed_bytes.data(), seed_bytes.size());
    }
} // namespace crsce::decompress
