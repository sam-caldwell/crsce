// Implementation dependencies
#include <algorithm>
#include <array>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <mutex>
#include <functional>
#include <optional>
#include <queue>
#include <random>
#include <ios>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <stop_token>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdlib>
#include <atomic>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include "common/O11y/escape_json.h"
#include "common/O11y/events_path.h"
#include "common/O11y/now_ms.h"
#include "common/O11y/flush_enabled.h"
#include "common/O11y/write_line_to_path_sync.h"

namespace crsce::o11y {

    namespace {
        /**
         * @name generate_uuid_v4
         * @brief Generate a random UUID v4 string (xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx).
         * @return std::string UUID string.
         */
        inline std::string generate_uuid_v4() {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<std::uint64_t> dist;
            const std::uint64_t a = dist(gen);
            const std::uint64_t b = dist(gen);
            // Format as UUID v4: set version nibble to 4, variant bits to 10xx
            const auto hi = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
            const auto lo = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            oss << std::setw(8) << static_cast<unsigned>((hi >> 32U) & 0xFFFFFFFFU) << '-';
            oss << std::setw(4) << static_cast<unsigned>((hi >> 16U) & 0xFFFFU) << '-';
            oss << std::setw(4) << static_cast<unsigned>(hi & 0xFFFFU) << '-';
            oss << std::setw(4) << static_cast<unsigned>((lo >> 48U) & 0xFFFFU) << '-';
            oss << std::setw(12) << (lo & 0x0000FFFFFFFFFFFFULL);
            return oss.str();
        }
    } // namespace

    namespace {
        /**
         * @name get_pid
         * @brief Return the current process ID as uint64.
         * @return std::uint64_t Process ID.
         */
        inline std::uint64_t get_pid() {
#if defined(__unix__) || defined(__APPLE__)
            return static_cast<std::uint64_t>(::getpid());
#else
            return 0;
#endif
        }
    } // namespace

    /**
     * @name O11y
     * @brief Construct O11y; generate run_id, capture PID, read flush interval from env.
     */
    O11y::O11y() : run_id_(generate_uuid_v4()), pid_(get_pid()) {
        if (const char *p = std::getenv("CRSCE_O11Y_EVENT_FLUSH_MS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            const auto v = static_cast<std::uint64_t>(std::strtoull(p, nullptr, 10));
            if (v >= 10 && v <= 60000) { event_flush_ms_ = v; }
        }
    }

    /**
     * @name ~O11y
     * @brief Stop worker and flush pending events.
     */
    O11y::~O11y() {
        if (started_.load(std::memory_order_acquire)) {
            worker_.request_stop();
            {
                const std::scoped_lock lk(sched_mu_);
            }
            sched_cv_.notify_all();
            if (worker_.joinable()) { worker_.join(); }
        }
        try { flush_events(); } catch (...) { /* ignore */ }
    }

    /**
     * @name canonicalize_tags
     * @brief Deterministically render tags as sorted key=value pairs joined by '|'.
     * @param tags Input vector of key/value tags.
     * @return std::string Canonical key string (or empty if no tags).
     */
    /**
     * @name tag_first_less
     * @brief Comparator for sorting tag pairs by key.
     * @param a First pair.
     * @param b Second pair.
     * @return bool True if a.first < b.first.
     */
    namespace { inline bool tag_first_less(const std::pair<std::string, std::string> &a,
                               const std::pair<std::string, std::string> &b) {
        return a.first < b.first;
    } }

    std::string O11y::canonicalize_tags(const Tags &tags) {
        if (tags.empty()) { return {}; }
        std::vector<std::pair<std::string, std::string>> kv = tags;
        std::ranges::sort(kv, tag_first_less);
        std::ostringstream oss;
        bool first = true;
        for (const auto &p : kv) {
            if (!first) { oss << '|'; }
            first = false;
            oss << p.first << '=' << p.second;
        }
        return oss.str();
    }

    /**
     * @name do_event
     * @brief Increment event counter and enqueue JSONL event record.
     * @param name Event name.
     * @param tags Tag vector.
     * @param ts_ms Milliseconds epoch.
     * @param message Optional message.
     * @return void
     */
    void O11y::do_event(const std::string &name, const Tags &tags, const std::uint64_t ts_ms, const std::string &message) {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        auto &byTag = events_[name];
        EventData &e = byTag[key];
        ++e.count;
        e.last_ts_ms = ts_ms;
        events_queue_.push_back(QueuedEvent{.name=name, .tags=tags, .ts_ms=ts_ms, .message=message});
    }

    /**
     * @name ensure_started
     * @brief Start background worker thread if not already running.
     * @return void
     */
    /**
     * @name start_worker_forward
     * @brief Forwarder to invoke worker_loop on an instance.
     * @param st Stop token.
     * @param self O11y instance pointer.
     * @return void
     */
    namespace { inline void start_worker_forward(const std::stop_token &st, O11y *self) {
        self->worker_loop(st);
    } }

    void O11y::ensure_started() {
        bool expected = false;
        if (started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            worker_ = std::jthread(start_worker_forward, this);
        }
    }

    /**
     * @name BackgroundAwaiter::await_suspend
     * @brief Schedule coroutine handle onto background queue.
     * @param h Coroutine handle.
     * @return void
     */
    void O11y::BackgroundAwaiter::await_suspend(std::coroutine_handle<> h) const {
        {
            const std::scoped_lock lk(self_->sched_mu_);
            self_->jobs_.push(h);
        }
        self_->sched_cv_.notify_one();
    }

    /**
     * @name worker_loop
     * @brief Process scheduled jobs and periodically flush to disk until stop.
     * @param st Cooperative stop token.
     * @return void
     */
    /**
     * @name should_wake
     * @brief Predicate to wake scheduler when stopped or jobs exist.
     * @param self O11y instance.
     * @param st Stop token.
     * @return bool True if stop requested or jobs available.
     */
    namespace { inline bool should_wake(O11y *self, const std::stop_token &st) {
        return st.stop_requested() || self->has_jobs();
    } }

    void O11y::worker_loop(const std::stop_token &st) {
        using namespace std::chrono;
        const auto now0 = steady_clock::now();
        auto next_event_flush = now0 + milliseconds(event_flush_ms_);
        while (!st.stop_requested()) {
            const auto wake_at = next_event_flush;
            std::unique_lock lk(sched_mu_);
            if (jobs_.empty()) {
                auto pred = [this, &st]() { return should_wake(this, st); };
                sched_cv_.wait_until(lk, wake_at, pred);
            }
            std::queue<std::coroutine_handle<>> local;
            std::swap(local, jobs_);
            lk.unlock();
            while (!local.empty()) {
                auto h = local.front(); local.pop();
                if (h) { h.resume(); }
            }
            const auto now = steady_clock::now();
            if (now >= next_event_flush) {
                try { flush_events(); } catch (...) { /* ignore */ }
                next_event_flush = now + milliseconds(event_flush_ms_);
            }
        }
        try { flush_events(); } catch (...) { /* ignore */ }
    }

    /**
     * @name flush_events
     * @brief Emit queued events to events path.
     * @return void
     */
    void O11y::flush_events() {
        const std::string path = detail::events_path();

        // Capture CPU load averages once per flush batch
        std::array<double, 3> loadavg = {0.0, 0.0, 0.0};
#if defined(__unix__) || defined(__APPLE__)
        ::getloadavg(loadavg.data(), 3);
#endif

        std::vector<QueuedEvent> ev;
        {
            const std::scoped_lock lk(mu_);
            ev.swap(events_queue_);
        }
        for (const auto &e : ev) {
            std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(2);
            oss << '{';
            oss << "\"ts_ms\":" << e.ts_ms;
            oss << ",\"run_id\":\"" << run_id_ << "\"";
            oss << ",\"pid\":" << pid_;
            oss << ",\"load_avg_1m\":" << loadavg.at(0);
            oss << ",\"load_avg_5m\":" << loadavg.at(1);
            oss << ",\"load_avg_15m\":" << loadavg.at(2);
            oss << ",\"name\":\"" << detail::escape_json(e.name) << "\"";
            if (!e.message.empty()) {
                oss << ",\"message\":\"" << detail::escape_json(e.message) << "\"";
            }
            if (!e.tags.empty()) {
                oss << ",\"tags\":{";
                bool first = true;
                for (const auto &kv : e.tags) {
                    if (!first) { oss << ','; }
                    first = false;
                    oss << "\"" << detail::escape_json(kv.first) << "\":\"" << detail::escape_json(kv.second) << "\"";
                }
                oss << '}';
            }
            oss << '}';
            detail::write_line_to_path_sync(oss.str(), path);
        }
    }

    /**
     * @name do_metric
     * @brief Compute derived rates for counters and delegate to do_event().
     * @param name Event name.
     * @param fields Numeric fields.
     * @param ts_ms Wall-clock timestamp (ms since epoch).
     * @param now Monotonic timestamp for rate computation.
     */
    void O11y::do_metric(const std::string &name, const MetricFields &fields,
                          const std::uint64_t ts_ms,
                          const std::chrono::steady_clock::time_point now) {
        auto &w = metric_windows_[name];
        Tags tags;
        tags.reserve(fields.size() * 3);

        if (!w.initialized) {
            // First call: set epoch, store counter baselines, emit rates as 0
            w.epoch_steady = now;
            w.prev_steady = now;
            w.initialized = true;
            for (const auto &f : fields) {
                tags.emplace_back(f.name, std::to_string(f.value));
                if (f.kind == MetricKind::Counter) {
                    w.prev_counter_values[f.name] = f.value;
                    tags.emplace_back(f.name + "_per_sec", "0");
                    tags.emplace_back("avg_" + f.name + "_per_sec", "0");
                }
            }
        } else {
            using namespace std::chrono;
            const auto windowUs = duration_cast<microseconds>(now - w.prev_steady).count();
            const auto totalUs = duration_cast<microseconds>(now - w.epoch_steady).count();

            for (const auto &f : fields) {
                tags.emplace_back(f.name, std::to_string(f.value));
                if (f.kind == MetricKind::Counter) {
                    const auto prevIt = w.prev_counter_values.find(f.name);
                    const std::uint64_t prevVal = (prevIt != w.prev_counter_values.end())
                        ? prevIt->second : 0;
                    const auto delta = f.value - prevVal;

                    const auto rate = windowUs > 0
                        ? static_cast<std::uint64_t>(static_cast<double>(delta) * 1e6
                            / static_cast<double>(windowUs))
                        : std::uint64_t{0};
                    const auto avg = totalUs > 0
                        ? static_cast<std::uint64_t>(static_cast<double>(f.value) * 1e6
                            / static_cast<double>(totalUs))
                        : std::uint64_t{0};

                    tags.emplace_back(f.name + "_per_sec", std::to_string(rate));
                    tags.emplace_back("avg_" + f.name + "_per_sec", std::to_string(avg));
                    w.prev_counter_values[f.name] = f.value;
                }
            }
            w.prev_steady = now;
        }

        do_event(name, tags, ts_ms, std::string{});
    }

    // Local helpers to avoid capturing 'this' in coroutine lambdas
    namespace {
        using crsce::o11y::O11y;
        /**
         * @name co_event
         * @brief Coroutine that records event and optionally flushes.
         * @param self O11y instance pointer.
         * @param n Event name.
         * @param t Tags.
         * @param message Optional message.
         * @return O11y::Task
         */
        O11y::Task co_event(O11y *self, std::string n, O11y::Tags t, std::string message) {
            co_await self->background();
            self->do_event(n, t, crsce::o11y::detail::now_ms(), message);
            if (crsce::o11y::detail::flush_enabled()) {
                try { self->flush_events(); } catch (...) { }
            }
        }

        /**
         * @name co_metric
         * @brief Coroutine that processes a metric emission on the background thread.
         * @param self O11y instance pointer.
         * @param n Event name.
         * @param f Metric fields.
         * @param now Monotonic timestamp captured at call site.
         * @return O11y::Task
         */
        O11y::Task co_metric(O11y *self, std::string n, O11y::MetricFields f,
                              std::chrono::steady_clock::time_point now) {
            co_await self->background();
            self->do_metric(n, f, crsce::o11y::detail::now_ms(), now);
            if (crsce::o11y::detail::flush_enabled()) {
                try { self->flush_events(); } catch (...) { }
            }
        }
    }

    // Public async API
    /**
     * @name event
     * @brief Emit a tag-only event (async).
     * @param name Event name.
     * @param tags Tag vector.
     * @return void
     */
    void O11y::event(std::string name, Tags tags) {
        ensure_started();
        (void) co_event(this, std::move(name), std::move(tags), std::string{});
    }

    /**
     * @name event
     * @brief Emit an event with message and optional single tag (async).
     * @param name Event name.
     * @param message Message string.
     * @param tag Optional single tag to attach.
     * @return void
     */
    void O11y::event(std::string name, std::string message, std::optional<std::pair<std::string,std::string>> tag) {
        ensure_started();
        Tags t;
        if (tag.has_value()) { t.emplace_back(tag->first, tag->second); }
        (void) co_event(this, std::move(name), std::move(t), std::move(message));
    }

    /**
     * @name metric
     * @brief Emit numeric fields asynchronously; the background thread derives rates for counters.
     * @param name Event name.
     * @param fields Metric fields.
     */
    void O11y::metric(std::string name, MetricFields fields) {
        ensure_started();
        const auto now = std::chrono::steady_clock::now();
        (void) co_metric(this, std::move(name), std::move(fields), now);
    }

    /**
     * @name get_event
     * @brief Return aggregate event data for a tag combination if present.
     * @param name Event name.
     * @param tags Tag vector.
     * @return std::optional<O11y::EventData> Event data if present; nullopt otherwise.
     */
    std::optional<O11y::EventData> O11y::get_event(const std::string &name, const Tags &tags) const {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        const auto it = events_.find(name);
        if (it == events_.end()) { return std::nullopt; }
        const auto jt = it->second.find(key);
        if (jt == it->second.end()) { return std::nullopt; }
        return jt->second;
    }
} // namespace crsce::o11y
