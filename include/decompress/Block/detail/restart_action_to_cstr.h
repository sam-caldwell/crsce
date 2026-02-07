/**
 * @file restart_action_to_cstr.h
 * @brief Map RestartAction enum to short c-string for JSON output.
 * @author Sam Caldwell
 */
#pragma once

#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

inline const char* restart_action_to_cstr(BlockSolveSnapshot::RestartAction a) {
    using RA = BlockSolveSnapshot::RestartAction;
    switch (a) {
        case RA::lockIn: return "lock-in";
        case RA::lockInRow: return "lock-in-row";
        case RA::lockInMicro: return "lock-in-micro";
        case RA::lockInPrefix: return "lock-in-prefix";
        case RA::restart: return "restart";
        case RA::restartContradiction: return "restart-contradiction";
        case RA::lockInParRs: return "lock-in-par-rs";
        case RA::polishShake: return "polish-shake";
        case RA::lockInFinal: return "lock-in-final";
        case RA::lockInPair: return "lock-in-pair";
    }
    return "unknown";
}

} // namespace crsce::decompress::detail

