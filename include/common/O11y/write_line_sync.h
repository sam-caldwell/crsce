/**
 * @file write_line_sync.h
 * @brief Append one JSON line to the metrics file. Creates parent dir on demand.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <filesystem>
#include <fstream>
#include <ios>
#include <mutex>
#include <string>
#include <cstdio>

#include "common/O11y/current_path.h"
#include "common/O11y/flush_enabled.h"

namespace crsce::o11y::detail {
    /**
     * @name write_line_sync
     * @brief Append one JSON line to the metrics file. Creates parent dir on demand.
     * @param line One JSON-encoded record (no trailing newline required).
     * @return void
     */
    inline void write_line_sync(const std::string &line) {
        static std::mutex mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        const std::scoped_lock lk(mu);
        const std::string path = current_path();
        try {
            const std::filesystem::path p(path);
            const auto dir = p.parent_path();
            if (!dir.empty()) { std::filesystem::create_directories(dir); }
        } catch (...) {
            // ignore
        }
        if (FILE *f = std::fopen(path.c_str(), "ab")) { // NOLINT(cppcoreguidelines-owning-memory)
            (void)std::fwrite(line.data(), 1, line.size(), f);
            (void)std::fputc('\n', f);
            if (flush_enabled()) { (void)std::fflush(f); }
            std::fclose(f); // NOLINT(cppcoreguidelines-owning-memory)
            return;
        }
        std::ofstream os(path, std::ios::out | std::ios::app | std::ios::binary);
        if (!os.is_open()) { return; }
        os << line << '\n';
        if (flush_enabled()) { os.flush(); }
    }
}

