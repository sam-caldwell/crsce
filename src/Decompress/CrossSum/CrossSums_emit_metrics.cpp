/**
 * @file CrossSums_emit_metrics.cpp
 */
#include "decompress/CrossSum/CrossSum.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"

namespace crsce::decompress {
    void CrossSums::emit_metrics(const char *prefix, const Csm &csm, const ConstraintState *st) const {
        lsm_.emit_metrics(prefix, csm, st);
        vsm_.emit_metrics(prefix, csm, st);
        dsm_.emit_metrics(prefix, csm, st);
        xsm_.emit_metrics(prefix, csm, st);
    }
}
