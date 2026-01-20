/**
 * @file Compress_pop_all_lh_bytes.cpp
 * @brief Retrieve all queued LH digests as bytes (32*N bytes).
 */
#include "compress/Compress/Compress.h"
#include <cstdint>
#include <vector>

namespace crsce::compress {
    std::vector<std::uint8_t> Compress::pop_all_lh_bytes() {
        std::vector<std::uint8_t> out;
        while (true) {
            const auto h = lh_.popHash();
            if (!h.has_value()) {
                break;
            }
            out.insert(out.end(), h->begin(), h->end());
        }
        return out;
    }
} // namespace crsce::compress
