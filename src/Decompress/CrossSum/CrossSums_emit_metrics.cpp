/**
 * @file CrossSums_emit_metrics.cpp
 * @brief Emit metrics for all CrossSum families under a common prefix.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSums.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress {
    /**
     * @name emit_metrics
     * @brief Emit min/max error, satisfaction percent, and optional unknown totals for all families.
     * @param prefix Metric name prefix.
     * @param csm Cross‑Sum Matrix.
     * @param st Optional constraint state (for unknown totals).
     * @return void
     */
    void CrossSums::emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st) const {
        lsm_.emit_metrics(prefix, csm, st);
        vsm_.emit_metrics(prefix, csm, st);
        dsm_.emit_metrics(prefix, csm, st);
        xsm_.emit_metrics(prefix, csm, st);
    }
}
