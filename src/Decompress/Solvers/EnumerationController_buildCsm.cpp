/**
 * @file EnumerationController_buildCsm.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::buildCsm implementation.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <cstdint>

#include "common/Csm/Csm.h"

namespace crsce::decompress::solvers {
    /**
     * @name buildCsm
     * @brief Build a CSM from the current constraint store state.
     * @return A fully-assigned CSM.
     * @throws None
     */
    auto EnumerationController::buildCsm() const -> crsce::common::Csm {
        crsce::common::Csm csm;
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                csm.set(r, c, store_->getCellValue(r, c));
            }
        }
        return csm;
    }
} // namespace crsce::decompress::solvers
