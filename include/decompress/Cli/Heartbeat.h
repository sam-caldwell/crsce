/**
 * @file Heartbeat.h
 * @brief Lightweight wrapper class to manage the decompressor CLI heartbeat thread.
 *        Constructor initializes configuration; start() launches; wait() stops and joins.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

namespace crsce::decompress::cli {
    /**
     * @class Heartbeat
     * @brief RAII-friendly manager for the decompressor heartbeat thread.
     *        - Constructor captures interval from env or provided value.
     *        - start() launches the background worker.
     *        - wait() requests stop and joins the thread.
     */
    class Heartbeat {
    public:
        Heartbeat();
        explicit Heartbeat(unsigned interval_ms);
        ~Heartbeat();

        // Non-copyable, non-movable
        Heartbeat(const Heartbeat&) = delete;
        Heartbeat &operator=(const Heartbeat&) = delete;
        Heartbeat(Heartbeat&&) = delete;
        Heartbeat &operator=(Heartbeat&&) = delete;

        void start();
        void wait();

    private:
        using WorkerFn = void(*)(std::atomic<bool>*, unsigned);
        std::atomic<bool> run_{true};
        unsigned interval_ms_{1000U};
        std::thread thr_;
        bool started_{false};
        WorkerFn worker_{nullptr};

#ifdef CRSCE_TESTING
    public:
        // Test-only constructor to inject a stubbed worker function (inline to avoid lib linkage)
        Heartbeat(unsigned interval_ms, WorkerFn worker)
            : interval_ms_{(interval_ms == 0U ? 1000U : interval_ms)},
              worker_{worker} {}
#endif
    };
}
