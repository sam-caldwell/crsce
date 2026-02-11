/**
 * @file phase_to_cstr.h
 * @brief Map BlockSolveSnapshot::Phase to c-string.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

/**
 * @brief Convert a solver Phase to a short c-string label.
 */
inline const char *phase_to_cstr(BlockSolveSnapshot::Phase p) {
    switch (p) {
        case BlockSolveSnapshot::Phase::init: return "init";
        case BlockSolveSnapshot::Phase::de: return "de";
        case BlockSolveSnapshot::Phase::rowPhase: return "row-phase";
        case BlockSolveSnapshot::Phase::radditzSift: return "radditz-sift";
        case BlockSolveSnapshot::Phase::gobp: return "gobp";
        case BlockSolveSnapshot::Phase::verify: return "verify";
        case BlockSolveSnapshot::Phase::endOfIterations: return "end-of-iterations";
    }
    return "unknown";
}

} // namespace crsce::decompress::detail
