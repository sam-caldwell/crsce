/**
 * @file Compressor_computeCrossSums.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::computeCrossSums -- compute all eight cross-sum families and fill the payload.
 */
#include "compress/Compressor/Compressor.h"

#include <cstdint>
#include <vector>

#include "common/CrossSum/AntiDiagSum.h"
#include "common/CrossSum/ColSum.h"
#include "common/CrossSum/DiagSum.h"
#include "common/CrossSum/RowSum.h"
#include "common/CrossSum/ToroidalSlopeSum.h"
#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "decompress/Solvers/LtpTable.h"

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

        // LTP partition sums: one per LTP line (kS lines each).
        std::vector<std::uint16_t> ltp1Sums(kS, 0);
        std::vector<std::uint16_t> ltp2Sums(kS, 0);

        // Accumulate each cell's value into all cross-sum families.
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

                // LTP sums: look up the line index for this cell and add v.
                if (v != 0) {
                    const auto &ltp = decompress::solvers::ltpFlatIndices(r, c);
                    const auto ltp1Line = static_cast<std::uint16_t>(
                        ltp[0] - static_cast<std::uint16_t>(decompress::solvers::kLtp1Base)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    const auto ltp2Line = static_cast<std::uint16_t>(
                        ltp[1] - static_cast<std::uint16_t>(decompress::solvers::kLtp2Base)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    ltp1Sums[ltp1Line]++; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    ltp2Sums[ltp2Line]++; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                }
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

        // Fill the payload with LTP1SM and LTP2SM.
        for (std::uint16_t k = 0; k < kS; ++k) {
            payload.setLTP1SM(k, ltp1Sums[k]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            payload.setLTP2SM(k, ltp2Sums[k]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

} // namespace crsce::compress
