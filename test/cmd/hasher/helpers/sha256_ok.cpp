// Simple helper: reads stdin, computes internal SHA-256, prints hex digest
#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "common/HasherUtils/detail/to_hex_lower.h"
#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

using crsce::common::detail::sha256::sha256_digest;
using u8 = crsce::common::detail::sha256::u8;

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
    std::cout << crsce::common::hasher::to_hex_lower(digest) << "  -\n";
    return 0;
} catch (...) {
    return 2;
}
