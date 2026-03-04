/**
 * @file O11y.h
 * @brief Unified observability store for events with tag support.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#pragma once

#include <atomic>
#include <chrono>
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

        /**
         * @name MetricKind
         * @brief Distinguishes gauges (raw snapshots) from counters (monotonic, rate-derived).
         */
        enum class MetricKind : std::uint8_t { Gauge, Counter };

        /**
         * @name MetricField
         * @brief A single named numeric value with its kind.
         */
        struct MetricField {
            /** @name name  @brief Field name. */
            std::string name;
            /** @name value @brief Raw numeric value. */
            std::uint64_t value;
            /** @name kind  @brief Gauge or Counter. */
            MetricKind kind;
        };

        /**
         * @name MetricFields
         * @brief A collection of metric fields emitted together.
         */
        using MetricFields = std::vector<MetricField>;

        // Access singleton instance
        static O11y &instance();

        // Events (fire-and-forget, asynchronous)
        void event(std::string name, Tags tags = {});
        void event(std::string name, std::string message, std::optional<std::pair<std::string,std::string>> tag = std::nullopt);

        /**
         * @name metric
         * @brief Emit numeric fields asynchronously; the background thread derives rates for counters.
         * @param name Event name.
         * @param fields Numeric fields (gauges emitted raw; counters produce _per_sec and avg_ derivatives).
         */
        void metric(std::string name, MetricFields fields);

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

        /**
         * @name MetricWindowState
         * @brief Per-event-name windowing state for rate computation (background thread only).
         */
        struct MetricWindowState {
            /** @name prev_steady  @brief Timestamp of the previous metric emission. */
            std::chrono::steady_clock::time_point prev_steady;
            /** @name epoch_steady @brief Timestamp of the first metric emission (for avg rates). */
            std::chrono::steady_clock::time_point epoch_steady;
            /** @name prev_counter_values @brief Counter values at previous emission (for windowed delta). */
            std::unordered_map<std::string, std::uint64_t> prev_counter_values;
            /** @name initialized @brief False until the first emission for this event name. */
            bool initialized{false};
        };

        /** @name metric_windows_ @brief Per-event-name windowing state (background thread only). */
        std::unordered_map<std::string, MetricWindowState> metric_windows_; // NOLINT(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)

        /**
         * @name do_metric
         * @brief Process a metric emission on the background thread, computing derived rates.
         * @param name Event name.
         * @param fields Numeric fields.
         * @param ts_ms Wall-clock timestamp (milliseconds since epoch).
         * @param now Monotonic timestamp for rate computation.
         */
        void do_metric(const std::string &name, const MetricFields &fields,
                        std::uint64_t ts_ms, std::chrono::steady_clock::time_point now);

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
