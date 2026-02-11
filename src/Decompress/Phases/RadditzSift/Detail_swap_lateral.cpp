/**
 * @file Detail_swap_lateral.cpp
 * @brief swap_lateral for Radditz helpers.
 * @author Sam Caldwell
 */
#include "decompress/Phases/RadditzSift/Detail.h"

namespace crsce::decompress::phases::detail {
    bool swap_lateral(Csm &csm, const std::size_t r, const std::size_t cf, const std::size_t ct) {
        if (cf == ct) { return false; }
        if (csm.is_locked(r, cf) || csm.is_locked(r, ct)) { return false; }
        const bool b_from = csm.get(r, cf);
        const bool b_to   = csm.get(r, ct);
        if (!b_from || b_to) { return false; }
        csm.put(r, cf, false);
        csm.put(r, ct, true);
        return true;
    }
}

