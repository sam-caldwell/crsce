/**
 * @file cmd/hasher/main.cpp
 * @brief Generate 32KB random data, compute SHA-256 via system sha256sum/shasum
 *        and via internal Sha256, compare, and return 0 on match, 1 otherwise.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "common/BitHashBuffer/detail/Sha256.h"
#include "common/HasherUtils/ToHexLower.h"
#include "common/HasherUtils/ComputeControlSha256.h"

#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using u8 = crsce::common::detail::sha256::u8;

/**
 * @brief Program entry point for hasher utility.
 * @return 0 on match; 1 on mismatch or control failure; 2 on unhandled exception.
 */
int main() try {
    // 1) Generate 32KB random input
    constexpr std::size_t kLen = 32 * 1024;
    std::vector<u8> data(kLen);
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution dist(0, 255);
        for (auto &b: data) {
            b = static_cast<u8>(dist(gen));
        }
    }

    // 2) Candidate hash using internal SHA-256
    const auto cand = crsce::common::detail::sha256::sha256_digest(data.data(), data.size());
    const std::string cand_hex = crsce::common::hasher::to_hex_lower(cand);

    // 3) Control hash via system utility
    std::string ctrl_hex;
    if (!crsce::common::hasher::compute_control_sha256(data, ctrl_hex)) {
        return 1;
    }

    // 4) Compare and exit accordingly
    if (cand_hex == ctrl_hex) {
        return 0;
    }
    std::cerr << "candidate: " << cand_hex << "\ncontrol:   " << ctrl_hex << '\n';
    return 1;
} catch (...) {
    std::cerr << "unhandled exception in hasher\n";
    return 2;
}
