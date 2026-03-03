/**
 * @file RowDecomposedController_buildCsm.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief RowDecomposedController::buildCsm implementation.
 */
#include "decompress/Solvers/RowDecomposedController.h"

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
    auto RowDecomposedController::buildCsm() const -> crsce::common::Csm {
        const auto &cs = static_cast<const ConstraintStore &>(*store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        crsce::common::Csm csm;
        for (std::uint16_t r = 0; r < kS; ++r) {
            csm.setRow(r, cs.getRow(r));
        }
        return csm;
    }

} // namespace crsce::decompress::solvers
