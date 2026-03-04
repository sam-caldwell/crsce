/**
 * @file Compressor_computeCrossSums.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::computeCrossSums -- compute all eight cross-sum families and fill the payload.
 */
#include "compress/Compressor/Compressor.h"

#include <cstdint>

#include "common/CrossSum/AntiDiagSum.h"
#include "common/CrossSum/ColSum.h"
#include "common/CrossSum/DiagSum.h"
#include "common/CrossSum/RowSum.h"
#include "common/CrossSum/ToroidalSlopeSum.h"
#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"

namespace crsce::compress {

    /**
     * @name computeCrossSums
     * @brief Compute row, column, diagonal, anti-diagonal, and 4 toroidal-slope sums from the CSM.
     * @param csm The populated cross-sum matrix.
     * @param payload The CompressedPayload to fill with LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2.
     * @return void
     * @throws None
     */
    void Compressor::computeCrossSums(const common::Csm &csm, common::format::CompressedPayload &payload) {
        common::RowSum lsm(kS);
        common::ColSum vsm(kS);
        common::DiagSum dsm(kS);
        common::AntiDiagSum xsm(kS);
        common::ToroidalSlopeSum hsm1(kS, 256);
        common::ToroidalSlopeSum sfc1(kS, 255);
        common::ToroidalSlopeSum hsm2(kS, 2);
        common::ToroidalSlopeSum sfc2(kS, 509);

        // Accumulate each cell's value into all eight cross-sum vectors.
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto v = csm.get(r, c);
                lsm.set(r, c, v);
                vsm.set(r, c, v);
                dsm.set(r, c, v);
                xsm.set(r, c, v);
                hsm1.set(r, c, v);
                sfc1.set(r, c, v);
                hsm2.set(r, c, v);
                sfc2.set(r, c, v);
            }
        }

        // Fill the payload with LSM (row sums) and VSM (column sums).
        for (std::uint16_t k = 0; k < kS; ++k) {
            payload.setLSM(k, lsm.getByIndex(k));
            payload.setVSM(k, vsm.getByIndex(k));
        }

        // Fill the payload with DSM (diagonal sums) and XSM (anti-diagonal sums).
        static constexpr std::uint16_t kDiagCount = (2 * kS) - 1;
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            payload.setDSM(k, dsm.getByIndex(k));
            payload.setXSM(k, xsm.getByIndex(k));
        }

        // Fill the payload with 4 toroidal-slope sums.
        for (std::uint16_t k = 0; k < kS; ++k) {
            payload.setHSM1(k, hsm1.getByIndex(k));
            payload.setSFC1(k, sfc1.getByIndex(k));
            payload.setHSM2(k, hsm2.getByIndex(k));
            payload.setSFC2(k, sfc2.getByIndex(k));
        }
    }

} // namespace crsce::compress
