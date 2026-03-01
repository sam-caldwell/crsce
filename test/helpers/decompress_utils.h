/**
 * @file decompress_utils.h
 * @brief Shared test helpers for decoding sums, verifying CSM cross-sums, appending bits, and solving blocks.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/DeterministicElimination.h"
#include "decompress/RowHashVerifier/RowHashVerifier.h"

namespace crsce::testhelpers {

// Decode a packed MSB-first 9-bit stream into kCount 16-bit values.
template<std::size_t kCount>
inline std::array<std::uint16_t, kCount>
decode_9bit_stream(const std::span<const std::uint8_t> bytes) {
    std::array<std::uint16_t, kCount> vals{};
    for (std::size_t i = 0; i < kCount; ++i) {
        std::uint16_t v = 0;
        for (std::size_t b = 0; b < 9; ++b) {
            const std::size_t bit_index = (i * 9) + b;
            const std::size_t byte_index = bit_index / 8;
            const int bit_in_byte = static_cast<int>(bit_index % 8);
            const std::uint8_t byte = bytes[byte_index];
            const auto bit = static_cast<std::uint16_t>((byte >> (7 - bit_in_byte)) & 0x1U);
            v = static_cast<std::uint16_t>((v << 1) | bit);
        }
        vals.at(i) = v;
    }
    return vals;
}

// Verify that CSM-derived cross-sums match expected vectors.
inline bool verify_cross_sums(const crsce::decompress::Csm &csm,
                              const std::array<std::uint16_t, crsce::decompress::Csm::kS> &lsm,
                              const std::array<std::uint16_t, crsce::decompress::Csm::kS> &vsm,
                              const std::array<std::uint16_t, crsce::decompress::Csm::kS> &dsm,
                              const std::array<std::uint16_t, crsce::decompress::Csm::kS> &xsm) {
    using crsce::decompress::Csm;
    constexpr std::size_t S = Csm::kS;
    auto diag_index = [](std::size_t r, std::size_t c) -> std::size_t { return (c >= r) ? (c - r) : (c + S - r); };
    auto xdiag_index = [](std::size_t r, std::size_t c) -> std::size_t { return (r + c) % S; };

    std::array<std::uint16_t, S> row{};
    std::array<std::uint16_t, S> col{};
    std::array<std::uint16_t, S> diag{};
    std::array<std::uint16_t, S> xdg{};

    for (std::size_t r = 0; r < S; ++r) {
        for (std::size_t c = 0; c < S; ++c) {
            const bool bit = csm.get(r, c);
            if (!bit) { continue; }
            row.at(r) = static_cast<std::uint16_t>(row.at(r) + 1U);
            col.at(c) = static_cast<std::uint16_t>(col.at(c) + 1U);
            diag.at(diag_index(r, c)) = static_cast<std::uint16_t>(diag.at(diag_index(r, c)) + 1U);
            xdg.at(xdiag_index(r, c)) = static_cast<std::uint16_t>(xdg.at(xdiag_index(r, c)) + 1U);
        }
    }
    return std::equal(row.begin(), row.end(), lsm.begin()) &&
           std::equal(col.begin(), col.end(), vsm.begin()) &&
           std::equal(diag.begin(), diag.end(), dsm.begin()) &&
           std::equal(xdg.begin(), xdg.end(), xsm.begin());
}

// Append up to bit_limit bits from CSM to output buffer (MSB-first), preserving partial-byte state.
inline void append_bits_from_csm(const crsce::decompress::Csm &csm,
                                 std::uint64_t bit_limit,
                                 std::vector<std::uint8_t> &out,
                                 std::uint8_t &curr,
                                 int &bit_pos) {
    using crsce::decompress::Csm;
    constexpr std::size_t S = Csm::kS;
    std::uint64_t emitted = 0;
    for (std::size_t r = 0; r < S && emitted < bit_limit; ++r) {
        for (std::size_t c = 0; c < S && emitted < bit_limit; ++c) {
            const bool b = csm.get(r, c);
            if (b) { curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos))); }
            ++bit_pos;
            if (bit_pos >= 8) {
                out.push_back(curr);
                curr = 0;
                bit_pos = 0;
            }
            ++emitted;
        }
    }
}

} // namespace crsce::testhelpers
