/**
 * @file CrossSum_satisfied_pct.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include <cstddef>

namespace crsce::decompress {
    int CrossSum::satisfied_pct(const Csm &csm) const {
        const Vec cnt = compute_counts(csm);
        std::size_t ok = 0;
        for (std::size_t i = 0; i < kS; ++i) {
            if (cnt.at(i) == tgt_.at(i)) { ++ok; }
        }
        return static_cast<int>((ok * 100ULL) / kS);
    }
}
