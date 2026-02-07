/**
 * @file pre_polish_audit_and_restart_on_contradiction.h
 * @brief Declare audit_and_restart_on_contradiction helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <span>
#include <cstdint>
#include "decompress/Csm/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {
    bool audit_and_restart_on_contradiction(Csm &csm_out,
                                            ConstraintState &st,
                                            std::span<const std::uint8_t> lh,
                                            Csm &baseline_csm,
                                            ConstraintState &baseline_st,
                                            BlockSolveSnapshot &snap,
                                            int rs,
                                            std::uint64_t valid_bits,
                                            int cooldown_ticks,
                                            int &since_last_restart);
}

