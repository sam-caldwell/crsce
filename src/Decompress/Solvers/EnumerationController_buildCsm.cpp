/**
 * @file EnumerationController_buildCsm.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief EnumerationController::buildCsm implementation.
 */
#include "decompress/Solvers/EnumerationController.h"

#include <cstdint>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {
    /**
     * @name buildCsm
     * @brief Build a CSM from the current constraint store state.
     * @return A fully-assigned CSM.
     * @throws None
     */
    auto EnumerationController::buildCsm() const -> crsce::common::Csm {
        // Devirtualize: store_ is guaranteed to be ConstraintStore (final class)
        const auto &cs = static_cast<const ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        crsce::common::Csm csm;
        for (std::uint16_t r = 0; r < kS; ++r) {
            csm.setRow(r, cs.getRow(r));
        }
        return csm;
    }
} // namespace crsce::decompress::solvers
