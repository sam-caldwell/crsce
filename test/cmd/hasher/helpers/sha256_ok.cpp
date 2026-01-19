// Simple helper: reads stdin, computes internal SHA-256, prints hex digest
#include "common/BitHashBuffer/detail/Sha256.h"
#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

namespace {
    std::string to_hex_lower(const std::array<u8, 32> &d) {
        static constexpr std::array<char, 16> kHex{
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
        };
        std::string out;
        out.resize(64);
        for (std::size_t i = 0; i < d.size(); ++i) {
            const u8 b = d.at(i);
            out.at((2 * i) + 0) = kHex.at((b >> 4) & 0x0F);
            out.at((2 * i) + 1) = kHex.at(b & 0x0F);
        }
        return out;
    }
}

int main() try {
    std::string raw;
    std::array<char, 4096> buf{};
    while (std::cin.good()) {
        std::cin.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        auto got = std::cin.gcount();
        if (got <= 0) {
            break;
        }
        raw.append(buf.data(), static_cast<std::size_t>(got));
    }
    std::vector<u8> data;
    data.reserve(raw.size());
    for (char c: raw) {
        data.push_back(static_cast<u8>(static_cast<unsigned char>(c)));
    }
    const auto digest = sha256_digest(data.data(), data.size());
    std::cout << to_hex_lower(digest) << "  -\n";
    return 0;
} catch (...) {
    return 2;
}
