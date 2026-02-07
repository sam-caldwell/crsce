/**
 * @file metric.cpp
 * @brief Implementation for structured metric emission (JSONL) with internal timestamps.
 */
#include "common/O11y/metric.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <utility>
#include <ios>
#include <mutex>
#include <string>
#include <sstream>

namespace crsce::o11y {
    namespace {
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
                            static constexpr std::array<char, 16> HEX{'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
                            const auto uc = static_cast<unsigned char>(ch);
                            out += "\\u00";
                            out += HEX[(uc >> 4U) & 0x0FU];
                            out += HEX[uc & 0x0FU];
                        } else {
                            out += ch;
                        }
                }
            }
            return out;
        }

        inline std::uint64_t now_ms() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }

        class Sink {
        public:
            Sink() {
                const char *p = std::getenv("CRSCE_METRICS_PATH"); // NOLINT(concurrency-mt-unsafe)
                if (p && *p) { path_ = p; } else { path_ = "metrics.jsonl"; }
                os_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
            }
            void write_line(const std::string &line) {
                const std::scoped_lock lk(mu_);
                if (!os_.is_open()) {
                    os_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
                }
                os_ << line << '\n';
                // intentionally not flushing to keep buffered writes
            }
        private:
            std::string path_;
            std::ofstream os_;
            std::mutex mu_;
        };

        inline Sink &sink() {
            static Sink s;
            return s;
        }

        template <typename Value>
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
            sink().write_line(oss.str());
        }
    } // namespace

    void metric(const std::string &name, std::int64_t value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    void metric(const std::string &name, double value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    void metric(const std::string &name, const std::string &value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }

    void metric(const std::string &name, bool value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        metric_impl(name, value, tags);
    }
}
