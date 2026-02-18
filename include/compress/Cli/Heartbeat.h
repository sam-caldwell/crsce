/**
 * @file Heartbeat.h
 * @brief Heartbeat manager for the compressor CLI; mirrors decompressor pattern.
 */
#pragma once

#include <atomic>
#include <thread>

namespace crsce::compress::cli {
    class Heartbeat {
    public:
        Heartbeat();
        explicit Heartbeat(unsigned interval_ms);
        ~Heartbeat();

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
        Heartbeat(unsigned interval_ms, WorkerFn worker)
            : interval_ms_{(interval_ms == 0U ? 1000U : interval_ms)}, worker_{worker} {}
#endif
    };
}

