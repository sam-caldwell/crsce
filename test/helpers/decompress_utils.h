/**
 * @file decompress_utils.h
 * @brief Shared test helpers for decoding sums, verifying CSM cross-sums, appending bits, and solving blocks.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/LHChainVerifier/LHChainVerifier.h"

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
    auto diag_index = [](std::size_t r, std::size_t c) -> std::size_t { return (r + c) % S; };
    auto xdiag_index = [](std::size_t r, std::size_t c) -> std::size_t {
        return (r >= c) ? (r - c) : (r + S - c);
    };

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

// Solve a block using deterministic elimination + GOBP and verify sums/LH.
inline bool solve_block(std::span<const std::uint8_t> lh,
                        std::span<const std::uint8_t> sums,
                        crsce::decompress::Csm &csm_out,
                        const std::string &seed) {
    using crsce::decompress::Csm;
    using crsce::decompress::ConstraintState;
    using crsce::decompress::DeterministicElimination;
    using crsce::decompress::GobpSolver;
    using crsce::decompress::LHChainVerifier;
    constexpr std::size_t S = Csm::kS;
    constexpr std::size_t vec_bytes = 575U;
    const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
    const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
    const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
    const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

    ConstraintState st{};
    st.R_row = lsm; st.R_col = vsm; st.R_diag = dsm; st.R_xdiag = xsm;
    st.U_row.fill(S); st.U_col.fill(S); st.U_diag.fill(S); st.U_xdiag.fill(S);

    csm_out.reset();
    DeterministicElimination det{csm_out, st};
    GobpSolver gobp{csm_out, st, /*damping*/ 0.25, /*assign_confidence*/ 0.995};
    constexpr int kMaxIters = 200;
    for (int iter = 0; iter < kMaxIters; ++iter) {
        std::size_t progress = 0;
        progress += det.solve_step();
        progress += gobp.solve_step();
        if (det.solved() || gobp.solved()) { break; }
        if (progress == 0) { (void)gobp.solve_step(); break; }
    }
    if (!(det.solved() && gobp.solved())) { return false; }
    if (!verify_cross_sums(csm_out, lsm, vsm, dsm, xsm)) { return false; }
    const LHChainVerifier verifier(seed);
    return verifier.verify_all(csm_out, lh);
}

} // namespace crsce::testhelpers
