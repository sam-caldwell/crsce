/**
 * @file metric_helpers.h
 * @brief Inline helpers for metrics: escaping, timestamps, output, and counters.
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <mutex>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace crsce::o11y::detail {

    /**
     * @name hex_nibble
     * @brief Convert a 4-bit value into lowercase hex character.
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
     * @return std::string Escaped string safe for JSON values.
     */
    inline std::string escape_json(const std::string &s) {
        std::string out; out.reserve(s.size() + 8U);
        for (const char ch : s) {
            switch (ch) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20U) {
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

    /**
     * @name current_path
     * @brief Determine metrics output file path.
     * @return std::string Path from CRSCE_METRICS_PATH or default build/metrics.jsonl.
     */
    inline std::string current_path() {
        if (const char *p = std::getenv("CRSCE_METRICS_PATH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            return std::string(p);
        }
        return std::string("build/metrics.jsonl");
    }

    /**
     * @name flush_enabled
     * @brief Check if immediate flush is enabled via CRSCE_METRICS_FLUSH.
     * @return bool True if set and non-zero; false otherwise.
     */
    inline bool flush_enabled() {
        if (const char *p = std::getenv("CRSCE_METRICS_FLUSH") /* NOLINT(concurrency-mt-unsafe) */; p && *p) {
            return (*p != '0');
        }
        return false;
    }

    /**
     * @name write_line_sync
     * @brief Append one JSON line to the metrics file. Creates parent dir on demand.
     * @param line One JSON-encoded record (no trailing newline required).
     * @return void
     */
    inline void write_line_sync(const std::string &line) {
        static std::mutex mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        const std::scoped_lock lk(mu);
        const std::string path = current_path();
        try {
            const std::filesystem::path p(path);
            const auto dir = p.parent_path();
            if (!dir.empty()) { std::filesystem::create_directories(dir); }
        } catch (...) {
            // ignore
        }
        if (FILE *f = std::fopen(path.c_str(), "ab")) { // NOLINT(cppcoreguidelines-owning-memory)
            (void)std::fwrite(line.data(), 1, line.size(), f);
            (void)std::fputc('\n', f);
            if (flush_enabled()) { (void)std::fflush(f); }
            std::fclose(f); // NOLINT(cppcoreguidelines-owning-memory)
            return;
        }
        std::ofstream os(path, std::ios::out | std::ios::app | std::ios::binary);
        if (!os.is_open()) { return; }
        os << line << '\n';
        if (flush_enabled()) { os.flush(); }
    }

    /**
     * @name incr_counter_and_get_sync
     * @brief Increment a named counter and return the updated value.
     * @param name Counter name.
     * @return std::uint64_t Updated count after increment.
     */
    inline std::uint64_t incr_counter_and_get_sync(const std::string &name) {
        static std::mutex mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        static std::unordered_map<std::string, std::uint64_t> counters; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        const std::scoped_lock lk(mu);
        return ++counters[name];
    }

    /**
     * @name metric_impl
     * @brief Serialize a simple metric value with optional tags.
     * @param name Metric name.
     * @param value Metric value (overloaded on type).
     * @param tags Optional key/value tags.
     * @return void
     */
    template <typename Value>
    inline void metric_impl(const std::string &name,
                            const Value &value,
                            std::initializer_list<std::pair<std::string, std::string>> tags) {
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << now_ms() << ',';
        oss << "\"name\":\"" << escape_json(name) << "\",";
        if constexpr (std::is_same_v<Value, std::string>) {
            oss << "\"value\":\"" << escape_json(value) << "\"";
        } else if constexpr (std::is_same_v<Value, bool>) {
            oss << "\"value\":" << (value ? "true" : "false");
        } else {
            oss << "\"value\":" << value;
        }
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
}

