/**
 * @file write_line_to_path_sync.h
 * @brief Append one JSON line to a specified file (thread-safe).
 */
#pragma once

#include <filesystem>
#include <fstream>
#include <ios>
#include <mutex>
#include <string>
#include <cstdio>

namespace crsce::o11y::detail {
    inline void write_line_to_path_sync(const std::string &line, const std::string &path) {
        static std::mutex mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        const std::scoped_lock lk(mu);
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
            std::fclose(f); // NOLINT(cppcoreguidelines-owning-memory)
            return;
        }
        std::ofstream os(path, std::ios::out | std::ios::app | std::ios::binary);
        if (!os.is_open()) { return; }
        os << line << '\n';
    }
}

