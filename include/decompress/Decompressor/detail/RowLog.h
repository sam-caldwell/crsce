/**
 * @file RowLog.h
 * @brief Row-completion statistics logging helpers (class wrapper for ODPH).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <string>
#include <cstdint>
#include <span>
#include <chrono>

#include "decompress/Csm/Csm.h"

namespace crsce::decompress {
    /**
     * @class RowLog
     * @brief Utilities for emitting row-completion JSON snapshots and announcements.
     */
    class RowLog {
    public:
        static void touch_and_announce_rowlog(const std::string &output_path);

        static void write_row_completion_stats_failure(
            const std::string &output_path,
            std::uint64_t block_index,
            const Csm &csm,
            std::span<const std::uint8_t> lh,
            std::span<const std::uint8_t> sums,
            const std::chrono::system_clock::time_point &run_start);

        static void write_row_completion_stats_success(
            const std::string &output_path,
            std::uint64_t block_index,
            const Csm &csm,
            std::span<const std::uint8_t> lh,
            std::span<const std::uint8_t> sums,
            const std::chrono::system_clock::time_point &run_start);
    };
} // namespace crsce::decompress
