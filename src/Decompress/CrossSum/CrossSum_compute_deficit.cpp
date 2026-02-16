/**
 * @file CrossSum_compute_deficit.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    CrossSum::Vec CrossSum::compute_deficit(const Csm &csm) const {
        const Vec cnt = compute_counts(csm);
        Vec out{};
        for (std::size_t i = 0; i < kS; ++i) {
            const auto t = tgt_.at(i);
            const auto c = cnt.at(i);
            out.at(i) = (c > t) ? static_cast<std::uint16_t>(c - t)
                                : static_cast<std::uint16_t>(t - c);
        }
        return out;
    }
}
