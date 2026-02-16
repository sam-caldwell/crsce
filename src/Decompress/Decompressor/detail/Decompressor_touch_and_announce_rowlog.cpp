/**
 * @file Decompressor_touch_and_announce_rowlog.cpp
 * @author Sam Caldwell
 * @brief Implementation
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Decompressor/detail/RowLog.h"

#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <ios>

namespace crsce::decompress {
    /**
     * @name touch_and_announce_rowlog
     * @brief create the completion_stats log file then announce its location
     * @param output_path log directory
     * @return void
     */
    void RowLog::touch_and_announce_rowlog(const std::string &output_path) {
    try {
        namespace fs = std::filesystem;
        const auto outp = fs::path(output_path);
        const fs::path outdir = outp.has_parent_path() ? outp.parent_path() : fs::path(".");
        const fs::path rowlog_announce = outdir / "completion_stats.log";
        std::println("ROW_COMPLETION_LOG={}", rowlog_announce.string());
        const std::ofstream os(rowlog_announce, std::ios::binary | std::ios::app);
        (void)os.good();
    } catch (...) {
        // best-effort only; do not throw
    }
}

} // namespace crsce::decompress
