/**
 * @file metric.cpp
 * @brief Implementation for structured metric emission (JSONL) with internal timestamps.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "common/O11y/metric.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <initializer_list>
#include <utility>
#include <ios>
#include <mutex>
#include <string>
#include <sstream>
#include <unordered_map>
#include <atomic>

namespace crsce::o11y {
    namespace {
        /**
         * @name hex_nibble
         * @brief Convert a hex nibble to ASCII.
         * @param v Unsigned 4-bit value (low nibble used).
         * @return char ASCII hex digit.
         */
        inline char hex_nibble(unsigned char v) {
            return (v < 10U) ? static_cast<char>('0' + v) : static_cast<char>('a' + (v - 10U));
        }

        /**
         * @name escape_json
         * @brief Escape a string for JSON output.
         * @param s Input string.
         * @return std::string Escaped string.
         */
        inline std::string escape_json(const std::string &s) {
            std::string out;
            out.reserve(s.size() + 8U);
            for (const char ch : s) {
                switch (ch) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(ch) < 0x20U) {
                            // encode as \u00XX
                            const auto uc = static_cast<unsigned char>(ch);
                            out += "\\u00";
                            out += hex_nibble((uc >> 4U) & 0x0FU);
                            out += hex_nibble(uc & 0x0FU);
                        } else {
                            out += ch;
                        }
                }
            }
            return out;
        }

        /**
         * @name now_ms
         * @brief Milliseconds since UNIX epoch.
         * @return std::uint64_t Milliseconds since epoch.
         */
        inline std::uint64_t now_ms() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }

        // Synchronous output helpers (test-friendly, avoids background threads)
        inline std::string current_path() {
            if (const char *p = std::getenv("CRSCE_METRICS_PATH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) { return std::string(p); }
            return std::string("build/metrics.jsonl");
        }
        inline bool flush_enabled() {
            if (const char *p = std::getenv("CRSCE_METRICS_FLUSH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) { return (*p!='0'); }
            return false;
        }
        inline void write_line_sync(const std::string &line) {
            static std::mutex mu;
            const std::scoped_lock lk(mu);
            const std::string path = current_path();
            // Ensure parent directory exists (e.g., create 'build/' for default path)
            try {
                const std::filesystem::path p(path);
                const auto dir = p.parent_path();
                if (!dir.empty()) { std::filesystem::create_directories(dir); }
            } catch (...) {
                // Swallow directory creation errors; proceed to attempt file open
            }
            std::ofstream os(path, std::ios::out | std::ios::app | std::ios::binary);
            if (!os.is_open()) { return; }
            os << line << '\n';
            if (flush_enabled()) { os.flush(); }
        }
        inline std::uint64_t incr_counter_and_get_sync(const std::string &name) {
            static std::mutex mu;
            static std::unordered_map<std::string, std::uint64_t> counters;
            const std::scoped_lock lk(mu);
            return ++counters[name];
        }

        template <typename Value>
        /**
         * @name metric_impl
         * @brief Serialize a simple metric value with optional tags.
         * @param name Metric name.
         * @param value Metric value (overloaded on type).
         * @param tags Optional key/value tags.
         * @return void
         */
        void metric_impl(const std::string &name, const Value &value,
                         const std::initializer_list<std::pair<std::string, std::string>> &tags) {
            std::ostringstream oss;
            oss.setf(std::ios::fixed); oss.precision(6);
            oss << '{';
            oss << "\"ts_ms\":" << now_ms() << ',';
            oss << "\"name\":\"" << escape_json(name) << "\",";
            // value field
            if constexpr (std::is_same_v<Value, std::string>) {
                oss << "\"value\":\"" << escape_json(value) << "\"";
            } else if constexpr (std::is_same_v<Value, bool>) {
                oss << "\"value\":" << (value ? "true" : "false");
            } else {
                oss << "\"value\":" << value;
            }
            // tags
            if (tags.size() != 0) {
                oss << ",\"tags\":{";
                bool first = true;
                for (const auto &kv : tags) {
                    if (!first) { oss << ','; }
                    first = false;
                    oss << "\"" << escape_json(kv.first) << "\":\"" << escape_json(kv.second) << "\"";
                }
                oss << '}';
            }
            oss << '}';
            write_line_sync(oss.str());
        }
    } // namespace

    /** @name metric @brief Emit integral metric. */
    void metric(const std::string &name, std::int64_t value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    /** @name metric @brief Emit floating-point metric. */
    void metric(const std::string &name, double value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    /** @name metric @brief Emit string metric. */
    void metric(const std::string &name, const std::string &value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    /** @name metric @brief Emit boolean metric. */
    void metric(const std::string &name, bool value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    // Obj builder methods
    /** @name Obj::add @brief Add integer field. */
    Obj &Obj::add(const std::string &key, std::int64_t v) {
        ObjField f; f.t = ObjField::Type::I64; f.k = key; f.i64 = v; fields_.push_back(std::move(f)); return *this;
    }
    /** @name Obj::add @brief Add double field. */
    Obj &Obj::add(const std::string &key, double v) {
        ObjField f; f.t = ObjField::Type::F64; f.k = key; f.f64 = v; fields_.push_back(std::move(f)); return *this;
    }
    /** @name Obj::add @brief Add string field. */
    Obj &Obj::add(const std::string &key, const std::string &v) {
        ObjField f; f.t = ObjField::Type::STR; f.k = key; f.str = v; fields_.push_back(std::move(f)); return *this;
    }
    /** @name Obj::add @brief Add boolean field. */
    Obj &Obj::add(const std::string &key, bool v) {
        ObjField f; f.t = ObjField::Type::BOOL; f.k = key; f.b = v; fields_.push_back(std::move(f)); return *this;
    }

    /** @name metric @brief Emit structured object metric. */
    void metric(const Obj &o) {
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << now_ms() << ',';
        oss << "\"name\":\"" << escape_json(o.name()) << "\",";
        oss << "\"fields\":{";
        bool first = true;
        for (const auto &f : o.fields()) {
            if (!first) { oss << ','; }
            first = false;
            oss << "\"" << escape_json(f.k) << "\":";
            switch (f.t) {
                case ObjField::Type::I64: oss << f.i64; break;
                case ObjField::Type::F64: oss << f.f64; break;
                case ObjField::Type::STR: oss << '"' << escape_json(f.str) << '"'; break;
                case ObjField::Type::BOOL: oss << (f.b ? "true" : "false"); break;
            }
        }
        oss << "}}";
        write_line_sync(oss.str());
    }

    /** @name counter @brief Emit an increment-only counter event. */
    void counter(const std::string &name) {
        const auto cnt = incr_counter_and_get_sync(name);
        std::ostringstream oss;
        oss << '{';
        oss << "\"ts_ms\":" << now_ms() << ',';
        oss << "\"name\":\"" << escape_json(name) << "\",";
        oss << "\"count\":" << cnt;
        oss << '}';
        write_line_sync(oss.str());
    }

    /** @name event @brief Emit a one-off event with optional tags. */
    void event(const std::string &name,
               std::initializer_list<std::pair<std::string, std::string>> tags) {
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << now_ms() << ',';
        oss << "\"name\":\"" << escape_json(name) << "\"";
        if (tags.size() != 0) {
            oss << ",\"tags\":{";
            bool first = true;
            for (const auto &kv : tags) {
                if (!first) { oss << ','; }
                first = false;
                oss << "\"" << escape_json(kv.first) << "\":\"" << escape_json(kv.second) << "\"";
            }
            oss << '}';
        }
        oss << '}';
        write_line_sync(oss.str());
    }

    // Debug gating for GOBP
    /** @name gobp_debug_enabled @brief Check if GOBP debug events are enabled via env var. */
    bool gobp_debug_enabled() noexcept {
        static std::atomic<int> flag{-1};
        const int v = flag.load(std::memory_order_relaxed);
        if (v >= 0) { return v == 1; }
        const char *e = std::getenv("CRSCE_GOBP_DEBUG"); // NOLINT(concurrency-mt-unsafe)
        const int nv = (e && *e) ? 1 : 0;
        flag.store(nv, std::memory_order_relaxed);
        return nv == 1;
    }

    /** @name debug_event_gobp @brief Conditionally emit a GOBP debug event. */
    void debug_event_gobp(const std::string &name,
                          std::initializer_list<std::pair<std::string, std::string>> tags) {
        if (!gobp_debug_enabled()) { return; }
        event(name, tags);
    }
}
