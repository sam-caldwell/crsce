/**
 * @file Compressor_computeBH.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::computeBH -- compute SHA-256 block hash of the full CSM.
 */
#include "compress/Compressor/Compressor.h"

#include "common/BlockHash/BlockHash.h"
#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"

namespace crsce::compress {

    /**
     * @name computeBH
     * @brief Compute the SHA-256 block hash of the full CSM and store it in the payload.
     * @param csm The populated cross-sum matrix.
     * @param payload The CompressedPayload to fill with the BH digest.
     * @return void
     * @throws None
     */
    void Compressor::computeBH(const common::Csm &csm, common::format::CompressedPayload &payload) {
        const auto digest = common::BlockHash::compute(csm);
        payload.setBH(digest);
    }

} // namespace crsce::compress
