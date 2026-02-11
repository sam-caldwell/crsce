/**
 * @file Detail_verify_row_sums.cpp
 * @brief verify_row_sums for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
    bool verify_row_sums(const Csm &csm, const std::span<const std::uint16_t> lsm) {
        const std::size_t S = Csm::kS;
        if (lsm.size() < S) { return false; }
        for (std::size_t r = 0; r < S; ++r) {
            std::size_t ones = 0;
            for (std::size_t c = 0; c < S; ++c) { if (csm.get(r, c)) { ++ones; } }
            if (ones != static_cast<std::size_t>(lsm[r])) { return false; }
        }
        return true;
    }
}

