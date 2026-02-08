/**
 * @file restart_action_to_cstr.h
 * @brief Map RestartAction enum to short c-string for JSON output.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

/**
 * @brief Convert a RestartAction to a short c-string label.
 */
inline const char* restart_action_to_cstr(BlockSolveSnapshot::RestartAction a) {
    switch (a) {
        case BlockSolveSnapshot::RestartAction::lockIn: return "lock-in";
        case BlockSolveSnapshot::RestartAction::lockInRow: return "lock-in-row";
        case BlockSolveSnapshot::RestartAction::lockInMicro: return "lock-in-micro";
        case BlockSolveSnapshot::RestartAction::lockInPrefix: return "lock-in-prefix";
        case BlockSolveSnapshot::RestartAction::restart: return "restart";
        case BlockSolveSnapshot::RestartAction::restartContradiction: return "restart-contradiction";
        case BlockSolveSnapshot::RestartAction::lockInParRs: return "lock-in-par-rs";
        case BlockSolveSnapshot::RestartAction::polishShake: return "polish-shake";
        case BlockSolveSnapshot::RestartAction::lockInFinal: return "lock-in-final";
        case BlockSolveSnapshot::RestartAction::lockInPair: return "lock-in-pair";
    }
    return "unknown";
}

} // namespace crsce::decompress::detail
