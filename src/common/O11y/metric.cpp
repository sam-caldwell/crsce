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
#include <thread>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <sstream>

namespace crsce::o11y {
    namespace {
        inline char hex_nibble(unsigned char v) {
            return (v < 10U) ? static_cast<char>('0' + v) : static_cast<char>('a' + (v - 10U));
        }

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

        inline std::uint64_t now_ms() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }

        class AsyncSink {
        public:
            AsyncSink() {
                const char *p = std::getenv("CRSCE_METRICS_PATH"); // NOLINT(concurrency-mt-unsafe)
                if (p && *p) { path_ = p; } else { path_ = "metrics.jsonl"; }
                os_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
                worker_ = std::thread([this]() { this->run(); });
            }
            ~AsyncSink() {
                {
                    const std::scoped_lock lk(mu_);
                    stop_ = true;
                }
                cv_.notify_all();
                if (worker_.joinable()) { worker_.join(); }
            }
            void enqueue(const std::string &line) {
                {
                    const std::scoped_lock lk(mu_);
                    q_.push_back(line);
                }
                cv_.notify_one();
            }
            AsyncSink(const AsyncSink&) = delete;
            AsyncSink& operator=(const AsyncSink&) = delete;
            AsyncSink(AsyncSink&&) = delete;
            AsyncSink& operator=(AsyncSink&&) = delete;
        private:
            void run() {
                for (;;) {
                    std::string line;
                    {
                        std::unique_lock<std::mutex> lk(mu_);
                        cv_.wait(lk, [this](){ return stop_ || !q_.empty(); });
                        if (stop_ && q_.empty()) { break; }
                        line = std::move(q_.front());
                        q_.pop_front();
                        if (!os_.is_open()) {
                            os_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
                        }
                    }
                    os_ << line << '\n';
                }
                os_.flush();
            }
            std::string path_;
            std::ofstream os_;
            std::mutex mu_;
            std::condition_variable cv_;
            std::deque<std::string> q_;
            bool stop_{false};
            std::thread worker_;
        };

        inline AsyncSink &sink() {
            static AsyncSink s;
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
            sink().enqueue(oss.str());
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

    // Obj builder methods
    Obj &Obj::add(const std::string &key, std::int64_t v) {
        ObjField f; f.t = ObjField::Type::I64; f.k = key; f.i64 = v; fields_.push_back(std::move(f)); return *this;
    }
    Obj &Obj::add(const std::string &key, double v) {
        ObjField f; f.t = ObjField::Type::F64; f.k = key; f.f64 = v; fields_.push_back(std::move(f)); return *this;
    }
    Obj &Obj::add(const std::string &key, const std::string &v) {
        ObjField f; f.t = ObjField::Type::STR; f.k = key; f.str = v; fields_.push_back(std::move(f)); return *this;
    }
    Obj &Obj::add(const std::string &key, bool v) {
        ObjField f; f.t = ObjField::Type::BOOL; f.k = key; f.b = v; fields_.push_back(std::move(f)); return *this;
    }

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
        sink().enqueue(oss.str());
    }
}
