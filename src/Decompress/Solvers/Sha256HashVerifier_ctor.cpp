/**
 * @file Sha256HashVerifier_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Sha256HashVerifier constructor implementation.
 */
#include "decompress/Solvers/Sha256HashVerifier.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name Sha256HashVerifier
     * @brief Construct a hash verifier for s rows.
     * @param s Matrix dimension.
     * @throws None
     */
    Sha256HashVerifier::Sha256HashVerifier(const std::uint16_t s)
        : s_(s), expected_(s, std::array<std::uint8_t, 32>{}) {}
} // namespace crsce::decompress::solvers
