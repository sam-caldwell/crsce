/**
 * @file O11y.h
 * @brief Unified observability store for counters, metrics, and events with tag support.
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
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace crsce::o11y {
    /**
     * @class O11y
     * @brief Central store for counters, metrics, and events. Supports upsert semantics and key/value tags.
     */
    class O11y {
    public:
        using Tags = std::vector<std::pair<std::string, std::string>>;

        enum class MetricType : std::uint8_t { I64, U64, U32, U16, F64, BOOL };
        struct MetricValue {
            MetricType t{MetricType::I64};
            std::int64_t i64{0};
            std::uint64_t u64{0};
            std::uint32_t u32{0};
            std::uint16_t u16{0};
            double f64{0.0};
            bool b{false};
            std::uint64_t ts_ms{0};
        };

        struct EventData {
            std::uint64_t count{0};
            std::uint64_t last_ts_ms{0};
        };

        struct CounterData {
            std::uint64_t count{0};
            std::uint64_t last_ts_ms{0};
        };

        // Access singleton instance
        static O11y &instance();

        // Async APIs (fire-and-forget): all methods are asynchronous
        void counter(std::string name, Tags tags = {});

        // Metrics (overloads)
        void metric(std::string name, std::int64_t v, Tags tags = {});
        void metric(std::string name, int v, Tags tags = {});
        void metric(std::string name, std::uint64_t v, Tags tags = {});
        void metric(std::string name, std::uint32_t v, Tags tags = {});
        void metric(std::string name, std::uint16_t v, Tags tags = {});
        void metric(std::string name, double v, Tags tags = {});
        void metric(std::string name, float v, Tags tags = {});
        void metric(std::string name, bool v, Tags tags = {});

        // Events
        void event(std::string name, Tags tags = {});
        void event(std::string name, std::string message, std::optional<std::pair<std::string,std::string>> tag = std::nullopt);

        // Explicitly start the background worker (optional; async ops auto-start)
        void start() { ensure_started(); }

        // Optional getters (not used in hot path; helpful for tests/tools)
        std::optional<std::uint64_t> get_counter(const std::string &name, const Tags &tags = {}) const;
        std::optional<MetricValue> get_metric(const std::string &name, const Tags &tags = {}) const;
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
        std::unordered_map<std::string, std::unordered_map<std::string, CounterData>> counters_;
        std::unordered_map<std::string, std::unordered_map<std::string, MetricValue>> metrics_;
        std::unordered_map<std::string, std::unordered_map<std::string, EventData>> events_;

        // Event queue (purged on flush)
        struct QueuedEvent { std::string name; Tags tags; std::uint64_t ts_ms{0}; std::string message; };
        std::vector<QueuedEvent> events_queue_;

        // Background coroutine scheduler
        std::mutex sched_mu_;
        std::condition_variable sched_cv_;
        std::queue<std::coroutine_handle<>> jobs_;
        std::jthread worker_;
        std::atomic<bool> started_{false};
        std::uint64_t flush_ms_{250};

    public:
        void ensure_started();
        void worker_loop(std::stop_token st);
        void flush_now();

        // Internal sync operations (scheduled on background thread)
        void do_counter(const std::string &name, const Tags &tags, std::uint64_t ts_ms);
        void do_metric(const std::string &name, const MetricValue &mv, const Tags &tags);
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
