/**
 * File: cmd/decompress/main.cpp
 * Brief: CLI entry for decompressor; parse args, iterate blocks with Decompressor,
 *        reconstruct CSMs via solvers, verify LH, and write original bytes.
 */
#include "common/ArgParser/ArgParser.h"
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/DeterministicElimination.h"
#include "decompress/Gobp/GobpSolver.h"
#include "decompress/LHChainVerifier/LHChainVerifier.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {
    using crsce::decompress::Csm;
    using crsce::decompress::ConstraintState;
    using crsce::decompress::Decompressor;
    using crsce::decompress::DeterministicElimination;
    using crsce::decompress::GobpSolver;
    using crsce::decompress::LHChainVerifier;

    // Decode helper: interpret a 9-bit MSB-first stream into kCount values
    template<std::size_t kCount>
    std::array<std::uint16_t, kCount>
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

    // Compute cross-sums for a CSM and compare to expected vectors
    bool verify_cross_sums(const Csm &csm,
                           const std::array<std::uint16_t, Csm::kS> &lsm,
                           const std::array<std::uint16_t, Csm::kS> &vsm,
                           const std::array<std::uint16_t, Csm::kS> &dsm,
                           const std::array<std::uint16_t, Csm::kS> &xsm) {
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

    // Append up to 'bit_limit' bits from CSM (row-major) to an output byte buffer (MSB-first),
    // tracking partial byte state across calls via curr/bit_pos.
    void append_bits_from_csm(const Csm &csm,
                              std::uint64_t bit_limit,
                              std::vector<std::uint8_t> &out,
                              std::uint8_t &curr,
                              int &bit_pos) {
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

    // Solve a block given LH and sums; returns true on success and fills 'csm_out'.
    bool solve_block(std::span<const std::uint8_t> lh,
                     std::span<const std::uint8_t> sums,
                     Csm &csm_out,
                     const std::string &seed) {
        // Decode cross-sum vectors (LSM, VSM, DSM, XSM)
        constexpr std::size_t S = Csm::kS;
        constexpr std::size_t vec_bytes = 575U;
        const auto lsm = decode_9bit_stream<S>(sums.subspan(0 * vec_bytes, vec_bytes));
        const auto vsm = decode_9bit_stream<S>(sums.subspan(1 * vec_bytes, vec_bytes));
        const auto dsm = decode_9bit_stream<S>(sums.subspan(2 * vec_bytes, vec_bytes));
        const auto xsm = decode_9bit_stream<S>(sums.subspan(3 * vec_bytes, vec_bytes));

        // Initialize constraint state: U = S; R = vector values
        ConstraintState st{};
        for (std::size_t i = 0; i < S; ++i) {
            st.U_row.at(i) = static_cast<std::uint16_t>(S);
            st.U_col.at(i) = static_cast<std::uint16_t>(S);
            st.U_diag.at(i) = static_cast<std::uint16_t>(S);
            st.U_xdiag.at(i) = static_cast<std::uint16_t>(S);
            st.R_row.at(i) = lsm.at(i);
            st.R_col.at(i) = vsm.at(i);
            st.R_diag.at(i) = dsm.at(i);
            st.R_xdiag.at(i) = xsm.at(i);
        }

        // Reset CSM
        csm_out.reset();

        // Run solver passes iteratively: deterministic elimination + GOBP
        DeterministicElimination det{csm_out, st};
        GobpSolver gobp{csm_out, st, /*damping*/ 0.25, /*assign_confidence*/ 0.995};

        constexpr int kMaxIters = 200; // simple cap to avoid infinite loops
        for (int iter = 0; iter < kMaxIters; ++iter) {
            std::size_t progress = 0;
            progress += det.solve_step();
            progress += gobp.solve_step();
            if (det.solved() || gobp.solved()) {
                break;
            }
            if (progress == 0) {
                // One more light GOBP polish pass even if no progress from elimination
                (void)gobp.solve_step();
                break;
            }
        }

        // Validate solved state: all U must be zero
        if (!(det.solved() && gobp.solved())) {
            return false;
        }

        // Verify cross-sums match exactly
        if (!verify_cross_sums(csm_out, lsm, vsm, dsm, xsm)) {
            return false;
        }

        // Verify LH chain
        const LHChainVerifier verifier(seed);
        return verifier.verify_all(csm_out, lh);
    }
} // anonymous namespace

/**
 * Main entry: parse -in/-out and validate file preconditions; then decompress.
 */
auto main(const int argc, char *argv[]) -> int {
    try {
        crsce::common::ArgParser parser("decompress");
        if (argc > 1) {
            const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
            const bool parsed_ok = parser.parse(args);
            // ReSharper disable once CppUseStructuredBinding
            const auto &opt = parser.options();
            if (!parsed_ok || opt.help) {
                std::println(stderr, "usage: {}", parser.usage());
                return parsed_ok && opt.help ? 0 : 2;
            }
            // Validate file paths per usage: input must exist; output must NOT exist
            struct stat statbuf{};
            if (stat(opt.input.c_str(), &statbuf) != 0) {
                std::println(stderr, "error: input file does not exist: {}", opt.input);
                return 3;
            }
            if (stat(opt.output.c_str(), &statbuf) == 0) {
                std::println(stderr, "error: output file already exists: {}", opt.output);
                return 3;
            }

            // Decompress: iterate blocks, reconstruct CSMs and accumulate original bytes in memory
            Decompressor dx(opt.input);
            crsce::decompress::HeaderV1Fields hdr{};

            std::vector<std::uint8_t> output_bytes;
            output_bytes.reserve(1024); // small initial reserve; grows as needed
            std::uint8_t curr = 0;
            int bit_pos = 0;
            const std::string seed = "CRSCE_v1_seed"; // must match compressor default
            bool ok = true;

            const bool drove = dx.for_each_block(hdr, [&](std::span<const std::uint8_t> lh,
                                                        std::span<const std::uint8_t> sums) {
                if (!ok) { return; }
                // If we've already collected all bytes, ignore any extra blocks silently
                static constexpr std::uint64_t bits_per_block = static_cast<std::uint64_t>(Csm::kS) * static_cast<std::uint64_t>(Csm::kS);
                const std::uint64_t total_bits = hdr.original_size_bytes * 8U;
                const std::uint64_t bits_done = (static_cast<std::uint64_t>(output_bytes.size()) * 8U) + static_cast<std::uint64_t>(bit_pos);
                if (bits_done >= total_bits) {
                    return; // nothing more to do
                }

                // Solve block
                Csm csm;
                const bool solved = solve_block(lh, sums, csm, seed);
                if (!solved) {
                    ok = false;
                    return;
                }

                // Append up to remaining bits from this block (row-major)
                const std::uint64_t remain = total_bits - bits_done;
                const std::uint64_t to_take = std::min(remain, bits_per_block);
                append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
            });

            if (!drove || !ok) {
                std::println(stderr, "error: decompression failed");
                return 4;
            }

            // Sanity: ensure we ended on a byte boundary for the declared size
            if (bit_pos != 0) {
                // Should not happen since original_size_bytes * 8 is divisible by 8
                // Drop any partial to avoid writing extra
            }

            // Write all accumulated bytes to the output path
            std::ofstream out(opt.output, std::ios::binary);
            if (!out.good()) {
                std::println(stderr, "error: could not open output for write: {}", opt.output);
                return 4;
            }
            if (!output_bytes.empty()) {
                for (const auto b: output_bytes) {
                    out.put(static_cast<char>(b));
                }
            }
            if (!out.good()) {
                std::println(stderr, "error: write failed: {}", opt.output);
                return 4;
            }
        }
        return 0;
    } catch (const std::exception &e) {
        std::fputs("error: ", stderr);
        std::fputs(e.what(), stderr);
        std::fputs("\n", stderr);
        return 1;
    } catch (...) {
        std::fputs("error: unknown exception\n", stderr);
        return 1;
    }
}
