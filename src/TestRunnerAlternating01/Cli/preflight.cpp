/**
 * @file preflight.cpp
 * @brief Implementation of Alternating01 preflight checks for CSM bit patterns.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "testrunnerAlternating01/Cli/preflight.h"

#include "common/FileBitSerializer/FileBitSerializer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <format>
#include <vector>

namespace crsce::testrunner_alternating01::cli {

    constexpr std::size_t kCsmS = 127;

    /**
     * @name preflight_check_alternating01
     * @brief Validate that the generated Alternating01 input yields expected CSM bits for the first block.
     * @param in_path Filesystem path to the generated input file.
     * @param bytes Size of the generated input file in bytes.
     * @return void
     * @throws std::runtime_error if any invariant fails or the file cannot be read.
     */
    void preflight_check_alternating01(const std::filesystem::path &in_path,
                                       const std::uint64_t bytes) {
        constexpr std::size_t S = kCsmS;
        const std::uint64_t bits = bytes * 8ULL;
        if (bits < static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S)) {
            return; // skip preflight unless we have at least one full block
        }
        crsce::common::FileBitSerializer s(in_path.string());
        if (!s.good()) { throw std::runtime_error("preflight: failed to open input file"); }

        std::vector<int> b; b.reserve(S * S);
        for (std::size_t i = 0; i < S * S; ++i) {
            auto v = s.pop();
            if (!v.has_value()) { throw std::runtime_error("preflight: unexpected EOF"); }
            b.push_back(*v ? 1 : 0);
        }
        // 1) main diagonal zeros
        for (std::size_t i = 0; i < S; ++i) {
            if (b.at((i * S) + i) != 0) {
                throw std::runtime_error(std::format("preflight: CSM[i,i] not zero at i={}", i));
            }
        }
        // 2) CSM[0,1] == 1
        if (b.at((0 * S) + 1) != 1) { throw std::runtime_error("preflight: CSM[0,1] != 1"); }
        // 3) CSM[1,0] == 1
        if (b.at((1 * S) + 0) != 1) { throw std::runtime_error("preflight: CSM[1,0] != 1"); }
        // 4) CSM[510,509] == 1
        if (b.at(((S - 1) * S) + (S - 2)) != 1) { throw std::runtime_error("preflight: CSM[510,509] != 1"); }
        // 5) CSM[509,510] == 1
        if (b.at(((S - 2) * S) + (S - 1)) != 1) { throw std::runtime_error("preflight: CSM[509,510] != 1"); }
        // 6) CSM[0,510] == CSM[510,0]
        if (b.at((0 * S) + (S - 1)) != b.at(((S - 1) * S) + 0)) {
            throw std::runtime_error("preflight: CSM[0,510] != CSM[510,0]");
        }

        // 7) DSM counts on Alternating01 for toroidal diagonals:
        //    For each d, sum over r of b[r][(r + d) % S]. Expected:
        //      - if d even: d
        //      - if d odd:  S - d
        for (std::size_t d = 0; d < S; ++d) {
            std::size_t sum_d = 0;
            for (std::size_t r = 0; r < S; ++r) {
                const std::size_t c = (r + d) % S; // c = (r + d) mod S
                sum_d += static_cast<std::size_t>(b.at((r * S) + c));
            }
            const std::size_t expect_d = ((d & 1U) == 0U) ? d : (S - d);
            if (sum_d != expect_d) {
                throw std::runtime_error(std::format("preflight: DSM parity mismatch at d={}", d));
            }
        }

        // 8) XSM counts on Alternating01 for toroidal anti-diagonals:
        //    For each x, sum over r of b[r][(x - r) mod S]. Expected:
        //      - if x even: S - x - 1
        //      - if x odd:  x + 1
        for (std::size_t x = 0; x < S; ++x) {
            std::size_t sum_x = 0;
            for (std::size_t r = 0; r < S; ++r) {
                const std::size_t c = (x + S - (r % S)) % S; // c = (x - r) mod S
                sum_x += static_cast<std::size_t>(b.at((r * S) + c));
            }
            const std::size_t expect_x = ((x & 1U) == 0U) ? (S - x - 1) : (x + 1);
            if (sum_x != expect_x) {
                throw std::runtime_error(std::format("preflight: XSM parity mismatch at x={}", x));
            }
        }
    }
}
