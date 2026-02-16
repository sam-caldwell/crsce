/**
 * @file RowLog.h
 * @brief Declarations for row-completion statistics logging helpers.
 */
#pragma once

#include <string>
#include <cstdint>
#include <span>
#include <chrono>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {

// Announce and touch the completion stats log file next to output.
void touch_and_announce_rowlog(const std::string &output_path);

// Emit failure-path row completion stats JSON snapshot for a block.
void write_row_completion_stats_failure(
    const std::string &output_path,
    std::uint64_t block_index,
    const Csm &csm,
    std::span<const std::uint8_t> lh,
    std::span<const std::uint8_t> sums,
    const std::chrono::system_clock::time_point &run_start);

// Emit success-path row completion stats JSON snapshot for a block.
void write_row_completion_stats_success(
    const std::string &output_path,
    std::uint64_t block_index,
    const Csm &csm,
    std::span<const std::uint8_t> lh,
    std::span<const std::uint8_t> sums,
    const std::chrono::system_clock::time_point &run_start);

} // namespace crsce::decompress

