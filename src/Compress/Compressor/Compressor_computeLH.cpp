/**
 * @file Compressor_computeLH.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::computeLH -- compute SHA-256 lateral hashes for each CSM row.
 */
#include "compress/Compressor/Compressor.h"

#include <cstdint>

#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "common/LateralHash/LateralHash.h"

namespace crsce::compress {

    /**
     * @name computeLH
     * @brief Compute SHA-256 lateral hashes for each CSM row and fill the payload.
     * @param csm The populated cross-sum matrix.
     * @param payload The CompressedPayload to fill with LH digests.
     * @return void
     * @throws None
     */
    void Compressor::computeLH(const common::Csm &csm, common::format::CompressedPayload &payload) {
        for (std::uint16_t r = 0; r < kS; ++r) {
            const auto row = csm.getRow(r);
            const auto digest = common::LateralHash::compute(row);
            payload.setLH(r, digest);
        }
    }

} // namespace crsce::compress
