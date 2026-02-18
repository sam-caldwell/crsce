/**
 * @file Heartbeat.cpp
 * @brief Compressor CLI Heartbeat manager implementation.
 */

#include "compress/Cli/Heartbeat.h"

#include <cstdlib>
#include <cstdint>
#include <atomic>

namespace crsce::compress::cli::detail {
    void heartbeat_worker(std::atomic<bool> *run_flag, unsigned interval_ms);
}

namespace crsce::compress::cli {
    namespace {
        inline unsigned parse_interval_from_env(unsigned fallback_ms) {
            if (const char *p = std::getenv("CRSCE_HEARTBEAT_MS"); p && *p) { // NOLINT(concurrency-mt-unsafe)
                const std::int64_t v = std::strtoll(p, nullptr, 10);
                if (v > 0) { return static_cast<unsigned>(v); }
            }
            return fallback_ms;
        }
    }

    Heartbeat::Heartbeat()
        : interval_ms_(parse_interval_from_env(1000U)),
          worker_(&crsce::compress::cli::detail::heartbeat_worker) {}

    Heartbeat::Heartbeat(const unsigned interval_ms)
        : interval_ms_(interval_ms == 0U ? 1000U : interval_ms),
          worker_(&crsce::compress::cli::detail::heartbeat_worker) {}

    Heartbeat::~Heartbeat() { try { wait(); } catch (...) { } }

    void Heartbeat::start() {
        if (started_) { return; }
        started_ = true;
        run_.store(true, std::memory_order_relaxed);
        auto *fn = worker_ ? worker_ : &crsce::compress::cli::detail::heartbeat_worker;
        thr_ = std::thread(fn, &run_, interval_ms_);
    }

    void Heartbeat::wait() {
        if (!started_) { return; }
        run_.store(false, std::memory_order_relaxed);
        if (thr_.joinable()) { thr_.join(); }
        started_ = false;
    }
}

