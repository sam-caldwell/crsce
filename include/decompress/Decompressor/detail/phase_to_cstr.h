/**
 * @file phase_to_cstr.h
 * @brief Map BlockSolveSnapshot::Phase to c-string.
 * @author Sam Caldwell
 */
#pragma once

#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

inline const char *phase_to_cstr(BlockSolveSnapshot::Phase p) {
    using P = BlockSolveSnapshot::Phase;
    switch (p) {
        case P::init: return "init";
        case P::de: return "de";
        case P::gobp: return "gobp";
        case P::verify: return "verify";
        case P::endOfIterations: return "end-of-iterations";
    }
    return "unknown";
}

} // namespace crsce::decompress::detail

