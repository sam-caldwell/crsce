/**
 * @file compute_prefix.h
 * @brief Incrementally compute verified LH prefix for current CSM/state.
 * @author Sam Caldwell
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Csm/detail/Csm.h"
#include "decompress/DeterministicElimination/detail/ConstraintState.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"

namespace crsce::decompress::detail {

/**
 * @name compute_prefix
 * @brief Extend the verified LH prefix if leading rows remain fully determined and valid.
 * @param pc_ver_seen Per-row version numbers cached when last verified.
 * @param pc_ok Per-row verification flags (nonzero if row verified).
 * @param pc_prefix_len In/out current prefix length [0..pc_prefix_len).
 * @param csm Current CSM.
 * @param st Current constraint state.
 * @param lh LH digest span.
 * @param snap In/out snapshot for accumulating LH verification time.
 * @return New prefix length (same as updated pc_prefix_len).
 */
std::size_t compute_prefix(std::vector<std::uint64_t> &pc_ver_seen,
                           std::vector<char> &pc_ok,
                           std::size_t &pc_prefix_len,
                           Csm &csm,
                           ConstraintState &st,
                           std::span<const std::uint8_t> lh,
                           BlockSolveSnapshot &snap);

} // namespace crsce::decompress::detail

