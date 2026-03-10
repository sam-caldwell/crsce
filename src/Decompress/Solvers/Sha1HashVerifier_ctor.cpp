/**
 * @file Sha1HashVerifier_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Sha1HashVerifier constructor implementation.
 */
#include "decompress/Solvers/Sha1HashVerifier.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name Sha1HashVerifier
     * @brief Construct a hash verifier for s rows with zero-initialized 20-byte digests.
     * @param s Matrix dimension.
     * @throws None
     */
    Sha1HashVerifier::Sha1HashVerifier(const std::uint16_t s)
        : s_(s), expected_(s, std::array<std::uint8_t, kSha1DigestBytes>{}) {}
} // namespace crsce::decompress::solvers
