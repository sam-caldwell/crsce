/**
 * @file Compressor_computeCrossSums.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::computeCrossSums -- compute all eight cross-sum families and fill the payload.
 *
 * B.21: 4 joint-tiled variable-length LTP partition sums (LTP1SM–LTP4SM). Each cell belongs to
 * 1 or 2 LTP sub-tables depending on its membership.
 */
#include "compress/Compressor/Compressor.h"

#include <cstdint>
#include <vector>

#include "common/CrossSum/AntiDiagSum.h"
#include "common/CrossSum/ColSum.h"
#include "common/CrossSum/DiagSum.h"
#include "common/CrossSum/RowSum.h"
#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::compress {

    /**
     * @name computeCrossSums
     * @brief Compute row, column, diagonal, anti-diagonal, and 4 LTP partition sums from the CSM.
     * @param csm The populated cross-sum matrix.
     * @param payload The CompressedPayload to fill with LSM, VSM, DSM, XSM, LTP1SM–LTP4SM.
     * @return void
     * @throws None
     */
    void Compressor::computeCrossSums(const common::Csm &csm, common::format::CompressedPayload &payload) {
        common::RowSum lsm(kS);
        common::ColSum vsm(kS);
        common::DiagSum dsm(kS);
        common::AntiDiagSum xsm(kS);

        // LTP partition sums: one per LTP line (kS lines each).
        std::vector<std::uint16_t> ltp1Sums(kS, 0);
        std::vector<std::uint16_t> ltp2Sums(kS, 0);
        std::vector<std::uint16_t> ltp3Sums(kS, 0);
        std::vector<std::uint16_t> ltp4Sums(kS, 0);

        // Accumulate each cell's value into all cross-sum families.
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto v = csm.get(r, c);
                lsm.set(r, c, v);
                vsm.set(r, c, v);
                dsm.set(r, c, v);
                xsm.set(r, c, v);

                // LTP sums: look up the 1-2 sub-table memberships (B.21) and accumulate.
                if (v != 0) {
                    const auto &mem = decompress::solvers::ltpMembership(r, c);
                    for (std::uint8_t j = 0; j < mem.count; ++j) {
                        const auto f = mem.flat[j]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                        if (f < static_cast<std::uint16_t>(decompress::solvers::kLtp2Base)) {
                            ltp1Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(decompress::solvers::kLtp1Base))]++; // NOLINT
                        } else if (f < static_cast<std::uint16_t>(decompress::solvers::kLtp3Base)) {
                            ltp2Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(decompress::solvers::kLtp2Base))]++; // NOLINT
                        } else if (f < static_cast<std::uint16_t>(decompress::solvers::kLtp4Base)) {
                            ltp3Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(decompress::solvers::kLtp3Base))]++; // NOLINT
                        } else {
                            ltp4Sums[static_cast<std::uint16_t>(f - static_cast<std::uint16_t>(decompress::solvers::kLtp4Base))]++; // NOLINT
                        }
                    }
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

        // Fill the payload with LTP1SM through LTP4SM.
        for (std::uint16_t k = 0; k < kS; ++k) {
            payload.setLTP1SM(k, ltp1Sums[k]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            payload.setLTP2SM(k, ltp2Sums[k]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            payload.setLTP3SM(k, ltp3Sums[k]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            payload.setLTP4SM(k, ltp4Sums[k]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

} // namespace crsce::compress
