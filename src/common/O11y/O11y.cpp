/**
 * @file O11y.cpp
 * @brief Unified observability store implementation.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "common/O11y/O11y.h"

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <ios>
#include <sstream>
#include <string>
#include <thread>
#include <stop_token>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/O11y/detail/escape_json.h"
#include "common/O11y/detail/events_path.h"
#include "common/O11y/detail/now_ms.h"
#include "common/O11y/detail/write_line_sync.h"
#include "common/O11y/detail/flush_enabled.h"
#include "common/O11y/detail/write_line_to_path_sync.h"
#include <cstdlib>
#include <atomic>

namespace crsce::o11y {
    O11y &O11y::instance() {
        static O11y inst; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        return inst;
    }

    O11y::O11y() {
        if (const char *p = std::getenv("CRSCE_O11Y_FLUSH_MS") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            const auto v = static_cast<std::uint64_t>(std::strtoull(p, nullptr, 10));
            if (v >= 10 && v <= 60000) { flush_ms_ = v; }
        }
    }

    O11y::~O11y() {
        if (started_.load(std::memory_order_acquire)) {
            worker_.request_stop();
            {
                const std::scoped_lock lk(sched_mu_);
            }
            sched_cv_.notify_all();
            if (worker_.joinable()) { worker_.join(); }
        }
        try { flush_now(); } catch (...) { /* ignore */ }
    }

    std::string O11y::canonicalize_tags(const Tags &tags) {
        if (tags.empty()) { return {}; }
        std::vector<std::pair<std::string, std::string>> kv = tags;
        std::ranges::sort(kv, {}, [](const auto &p){ return p.first; });
        std::ostringstream oss;
        bool first = true;
        for (const auto &p : kv) {
            if (!first) { oss << '|'; }
            first = false;
            oss << p.first << '=' << p.second;
        }
        return oss.str();
    }

    void O11y::do_counter(const std::string &name, const Tags &tags, const std::uint64_t ts_ms) {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        auto &byTag = counters_[name];
        CounterData &c = byTag[key];
        ++c.count;
        c.last_ts_ms = ts_ms;
    }

    void O11y::do_metric(const std::string &name, const MetricValue &mv, const Tags &tags) {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        metrics_[name][key] = mv;
    }

    void O11y::do_event(const std::string &name, const Tags &tags, const std::uint64_t ts_ms, const std::string &message) {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        auto &byTag = events_[name];
        EventData &e = byTag[key];
        ++e.count;
        e.last_ts_ms = ts_ms;
        events_queue_.push_back(QueuedEvent{.name=name, .tags=tags, .ts_ms=ts_ms, .message=message});
    }

    void O11y::ensure_started() {
        bool expected = false;
        if (started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            worker_ = std::jthread([this](std::stop_token st){ worker_loop(std::move(st)); });
        }
    }

    void O11y::BackgroundAwaiter::await_suspend(std::coroutine_handle<> h) const {
        {
            const std::scoped_lock lk(self_->sched_mu_);
            self_->jobs_.push(h);
        }
        self_->sched_cv_.notify_one();
    }

    void O11y::worker_loop(std::stop_token st) {
        using namespace std::chrono;
        auto next_flush = steady_clock::now() + milliseconds(flush_ms_);
        while (!st.stop_requested()) {
            std::unique_lock lk(sched_mu_);
            if (jobs_.empty()) {
                sched_cv_.wait_until(lk, next_flush, [this, &st]{ return st.stop_requested() || !jobs_.empty(); });
            }
            std::queue<std::coroutine_handle<>> local;
            std::swap(local, jobs_);
            lk.unlock();
            while (!local.empty()) {
                auto h = local.front(); local.pop();
                if (h) { h.resume(); }
            }
            const auto now = steady_clock::now();
            if (now >= next_flush) {
                try { flush_now(); } catch (...) { /* ignore */ }
                next_flush = now + milliseconds(flush_ms_);
            }
        }
        try { flush_now(); } catch (...) { /* ignore */ }
    }

    void O11y::flush_now() {
        const auto ts = static_cast<std::uint64_t>(detail::now_ms());
        // 1) Flush queued events to events path
        std::vector<QueuedEvent> ev;
        {
            const std::scoped_lock lk(mu_);
            ev.swap(events_queue_);
        }
        if (!ev.empty()) {
            for (const auto &e : ev) {
                std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
                oss << '{';
                oss << "\"ts_ms\":" << e.ts_ms << ',';
                oss << "\"name\":\"" << detail::escape_json(e.name) << "\"";
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
                detail::write_line_to_path_sync(oss.str(), detail::events_path());
            }
        }
        // 2) Emit summary snapshot to metrics path
        std::unordered_map<std::string, std::unordered_map<std::string, CounterData>> counters;
        std::unordered_map<std::string, std::unordered_map<std::string, MetricValue>> metrics;
        {
            const std::scoped_lock lk(mu_);
            counters = counters_;
            metrics = metrics_;
        }
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << ts << ',';
        oss << "\"name\":\"summary\",";
        // Counters
        oss << "\"counters\":[";
        bool first = true;
        for (const auto &byName : counters) {
            for (const auto &byTag : byName.second) {
                if (!first) { oss << ','; }
                first = false;
                oss << '{'
                    << "\"name\":\"" << detail::escape_json(byName.first) << "\",";
                oss << "\"tags\":\"" << detail::escape_json(byTag.first) << "\",";
                oss << "\"count\":" << byTag.second.count << ",\"ts_ms\":" << byTag.second.last_ts_ms;
                oss << '}';
            }
        }
        oss << "],";
        // Metrics
        oss << "\"metrics\":[";
        first = true;
        for (const auto &byName : metrics) {
            for (const auto &byTag : byName.second) {
                if (!first) { oss << ','; }
                first = false;
                const MetricValue &mv = byTag.second;
                oss << '{'
                    << "\"name\":\"" << detail::escape_json(byName.first) << "\",";
                oss << "\"tags\":\"" << detail::escape_json(byTag.first) << "\",";
                oss << "\"t\":\"";
                switch (mv.t) {
                    case MetricType::I64: oss << "i64\"," << "\"value\":" << mv.i64; break;
                    case MetricType::U64: oss << "u64\"," << "\"value\":" << mv.u64; break;
                    case MetricType::U32: oss << "u32\"," << "\"value\":" << mv.u32; break;
                    case MetricType::U16: oss << "u16\"," << "\"value\":" << mv.u16; break;
                    case MetricType::F64: oss << "f64\"," << "\"value\":" << mv.f64; break;
                    case MetricType::BOOL: oss << "bool\"," << "\"value\":" << (mv.b ? "true" : "false"); break;
                }
                oss << ",\"ts_ms\":" << mv.ts_ms;
                oss << '}';
            }
        }
        oss << "]";
        oss << '}';
        detail::write_line_sync(oss.str());
    }

    // Local helpers to avoid capturing 'this' in coroutine lambdas
    namespace {
        using crsce::o11y::O11y;
        O11y::Task co_counter(O11y *self, std::string n, O11y::Tags t) {
            co_await self->background();
            self->do_counter(n, t, crsce::o11y::detail::now_ms());
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_metric_i64(O11y *self, std::string n, std::int64_t v, O11y::Tags t) {
            co_await self->background();
            O11y::MetricValue mv; mv.t = O11y::MetricType::I64; mv.i64 = v; mv.ts_ms = crsce::o11y::detail::now_ms();
            self->do_metric(n, mv, t);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_metric_u64(O11y *self, std::string n, std::uint64_t v, O11y::Tags t) {
            co_await self->background();
            O11y::MetricValue mv; mv.t = O11y::MetricType::U64; mv.u64 = v; mv.ts_ms = crsce::o11y::detail::now_ms();
            self->do_metric(n, mv, t);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_metric_u32(O11y *self, std::string n, std::uint32_t v, O11y::Tags t) {
            co_await self->background();
            O11y::MetricValue mv; mv.t = O11y::MetricType::U32; mv.u32 = v; mv.ts_ms = crsce::o11y::detail::now_ms();
            self->do_metric(n, mv, t);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_metric_u16(O11y *self, std::string n, std::uint16_t v, O11y::Tags t) {
            co_await self->background();
            O11y::MetricValue mv; mv.t = O11y::MetricType::U16; mv.u16 = v; mv.ts_ms = crsce::o11y::detail::now_ms();
            self->do_metric(n, mv, t);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_metric_f64(O11y *self, std::string n, double v, O11y::Tags t) {
            co_await self->background();
            O11y::MetricValue mv; mv.t = O11y::MetricType::F64; mv.f64 = v; mv.ts_ms = crsce::o11y::detail::now_ms();
            self->do_metric(n, mv, t);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_metric_bool(O11y *self, std::string n, bool v, O11y::Tags t) {
            co_await self->background();
            O11y::MetricValue mv; mv.t = O11y::MetricType::BOOL; mv.b = v; mv.ts_ms = crsce::o11y::detail::now_ms();
            self->do_metric(n, mv, t);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
        O11y::Task co_event(O11y *self, std::string n, O11y::Tags t, std::string message) {
            co_await self->background();
            self->do_event(n, t, crsce::o11y::detail::now_ms(), message);
            if (crsce::o11y::detail::flush_enabled()) { try { self->flush_now(); } catch (...) { } }
        }
    }

    // Public async API
    void O11y::counter(std::string name, Tags tags) {
        ensure_started();
        (void) co_counter(this, std::move(name), std::move(tags));
    }

    void O11y::metric(std::string name, std::int64_t v, Tags tags) {
        ensure_started();
        (void) co_metric_i64(this, std::move(name), v, std::move(tags));
    }

    void O11y::metric(std::string name, std::uint64_t v, Tags tags) {
        ensure_started();
        (void) co_metric_u64(this, std::move(name), v, std::move(tags));
    }

    void O11y::metric(std::string name, std::uint32_t v, Tags tags) {
        ensure_started();
        (void) co_metric_u32(this, std::move(name), v, std::move(tags));
    }

    void O11y::metric(std::string name, std::uint16_t v, Tags tags) {
        ensure_started();
        (void) co_metric_u16(this, std::move(name), v, std::move(tags));
    }

    void O11y::metric(std::string name, double v, Tags tags) {
        ensure_started();
        (void) co_metric_f64(this, std::move(name), v, std::move(tags));
    }

    void O11y::metric(std::string name, float v, Tags tags) {
        ensure_started();
        (void) co_metric_f64(this, std::move(name), static_cast<double>(v), std::move(tags));
    }

    void O11y::metric(std::string name, bool v, Tags tags) {
        ensure_started();
        (void) co_metric_bool(this, std::move(name), v, std::move(tags));
    }

    void O11y::event(std::string name, Tags tags) {
        ensure_started();
        (void) co_event(this, std::move(name), std::move(tags), std::string{});
    }

    void O11y::event(std::string name, std::string message, std::optional<std::pair<std::string,std::string>> tag) {
        ensure_started();
        Tags t;
        if (tag.has_value()) { t.emplace_back(tag->first, tag->second); }
        (void) co_event(this, std::move(name), std::move(t), std::move(message));
    }

    std::optional<std::uint64_t> O11y::get_counter(const std::string &name, const Tags &tags) const {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        const auto it = counters_.find(name);
        if (it == counters_.end()) { return std::nullopt; }
        const auto jt = it->second.find(key);
        if (jt == it->second.end()) { return std::nullopt; }
        return jt->second.count;
    }

    std::optional<O11y::MetricValue> O11y::get_metric(const std::string &name, const Tags &tags) const {
        const std::string key = canonicalize_tags(tags);
        const std::scoped_lock lk(mu_);
        const auto it = metrics_.find(name);
        if (it == metrics_.end()) { return std::nullopt; }
        const auto jt = it->second.find(key);
        if (jt == it->second.end()) { return std::nullopt; }
        return jt->second;
    }

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
