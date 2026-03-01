/**
 * @file heartbeat_worker.cpp
 * @brief Implementation of decompressor CLI heartbeat thread entrypoint.
 * @author Sam Caldwell
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include <chrono>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <sstream>
#include <string>
#include <fstream>
#include <ios>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace crsce::decompress::cli::detail {
    void heartbeat_worker(std::atomic<bool> *run_flag, const unsigned interval_ms) { // NOLINT(misc-use-internal-linkage)
        using namespace std::chrono;
        const char *hb_path = std::getenv("CRSCE_HEARTBEAT_PATH"); // NOLINT(concurrency-mt-unsafe)
        std::ofstream hb_stream;
        if (hb_path && *hb_path) { hb_stream.open(hb_path, std::ios::out | std::ios::app); }
        std::uint64_t pid_ul = 0ULL;
#if defined(__unix__) || defined(__APPLE__)
        pid_ul = static_cast<std::uint64_t>(::getpid());
#endif
        while (run_flag && run_flag->load(std::memory_order_relaxed)) {
            const auto now = system_clock::now();
            const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
            const auto ts_ms = static_cast<std::int64_t>(ms);
            std::ostringstream oss;
            oss << '[' << ts_ms << "] pid=" << pid_ul << " phase=decompress";
            oss << '\n';
            const std::string line = oss.str();
            std::fputs(line.c_str(), stdout);
            std::fflush(stdout);
            if (hb_stream.is_open()) { hb_stream << line; hb_stream.flush(); }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
        if (hb_stream.is_open()) { hb_stream.close(); }
    }
}
