/**
 * @file CrossSums_from_packed.cpp
 * @brief Build CrossSums aggregate from a packed 9-bit-per-entry buffer.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Utils/detail/decode9.tcc"
#include <span>
#include <cstdint>
#include <cstddef>

namespace crsce::decompress {
    /**
     * @name from_packed
     * @brief Decode 9-bit packed vectors for all families and return CrossSums.
     * @param packed Span of bytes containing 4 concatenated 9-bit streams.
     * @return CrossSums Aggregate of decoded families.
     */
    CrossSums CrossSums::from_packed(std::span<const std::uint8_t> packed) {
        constexpr std::size_t S = Csm::kS;
        constexpr std::size_t vec_bytes = 575U;
        const auto lsm_v = decode_9bit_stream<S>(packed.subspan(0 * vec_bytes, vec_bytes));
        const auto vsm_v = decode_9bit_stream<S>(packed.subspan(1 * vec_bytes, vec_bytes));
        const auto dsm_v = decode_9bit_stream<S>(packed.subspan(2 * vec_bytes, vec_bytes));
        const auto xsm_v = decode_9bit_stream<S>(packed.subspan(3 * vec_bytes, vec_bytes));
        return CrossSums{
            CrossSum{CrossSumFamily::Row, lsm_v},
            CrossSum{CrossSumFamily::Col, vsm_v},
            CrossSum{CrossSumFamily::Diag, dsm_v},
            CrossSum{CrossSumFamily::XDiag, xsm_v}
        };
    }
}
