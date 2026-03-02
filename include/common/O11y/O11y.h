/**
 * @file O11y.h
 * @brief Unified observability store for events with tag support.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace crsce::o11y {
    /**
     * @class O11y
     * @brief Central store for events. Supports upsert semantics and key/value tags.
     */
    class O11y {
    public:
        using Tags = std::vector<std::pair<std::string, std::string>>;

        struct EventData {
            std::uint64_t count{0};
            std::uint64_t last_ts_ms{0};
        };

        // Access singleton instance
        static O11y &instance();

        // Events (fire-and-forget, asynchronous)
        void event(std::string name, Tags tags = {});
        void event(std::string name, std::string message, std::optional<std::pair<std::string,std::string>> tag = std::nullopt);

        // Explicitly start the background worker (optional; async ops auto-start)
        void start() { ensure_started(); }

        // Optional getter (not used in hot path; helpful for tests/tools)
        std::optional<EventData> get_event(const std::string &name, const Tags &tags = {}) const;

        O11y(const O11y&) = delete;
        O11y &operator=(const O11y&) = delete;
        O11y(O11y&&) = delete;
        O11y &operator=(O11y&&) = delete;

    private:
        O11y();
        ~O11y();

        static std::string canonicalize_tags(const Tags &tags);

        // Shared store protection
        mutable std::mutex mu_;
        // name -> tagKey -> value
        std::unordered_map<std::string, std::unordered_map<std::string, EventData>> events_;

        // Event queue (purged on flush)
        struct QueuedEvent { std::string name; Tags tags; std::uint64_t ts_ms{0}; std::string message; };
        std::vector<QueuedEvent> events_queue_;

        // Per-process identity (set once in constructor)
        std::string run_id_;
        std::uint64_t pid_{0};

        // Background coroutine scheduler
        std::mutex sched_mu_;
        std::condition_variable sched_cv_;
        std::queue<std::coroutine_handle<>> jobs_;
        std::jthread worker_;
        std::atomic<bool> started_{false};
        std::uint64_t event_flush_ms_{1000};

    public:
        void ensure_started();
        void worker_loop(const std::stop_token &st);
        void flush_events();
        /**
         * @brief Return whether the scheduler queue is non-empty.
         * Caller must hold `sched_mu_` when invoking.
         * @return bool True if there are pending jobs.
         */
        bool has_jobs() const { return !jobs_.empty(); }

        // Internal sync operation (scheduled on background thread)
        void do_event(const std::string &name, const Tags &tags, std::uint64_t ts_ms, const std::string &message);

        // Internal coroutine utilities (exposed for TU access only)
        struct BackgroundAwaiter {
        private:
            O11y *self_;
        public:
            explicit BackgroundAwaiter(O11y *s) : self_(s) {}
            [[nodiscard]] bool await_ready() const noexcept { return false; }
            void await_resume() const noexcept {}
            void await_suspend(std::coroutine_handle<> h) const;
        };
        [[nodiscard]] BackgroundAwaiter background() noexcept { return BackgroundAwaiter{this}; }

        struct Task {
            struct promise_type {
                Task get_return_object() noexcept { return {}; }
                std::suspend_never initial_suspend() noexcept { return {}; }
                std::suspend_never final_suspend() noexcept { return {}; }
                void return_void() noexcept {}
                void unhandled_exception() noexcept {}
            };
        };
    };
} // namespace crsce::o11y
