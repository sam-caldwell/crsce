/**
 * @file metric.h
 * @brief Structured metric emission (JSONL) with standardized timestamping.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <initializer_list>
#include <utility>

namespace crsce::o11y {

    // Tags are optional string key/value pairs. Timestamp is always added by the implementation.
    void metric(const std::string &name,
                std::int64_t value = 1,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    void metric(const std::string &name,
                double value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    void metric(const std::string &name,
                const std::string &value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    void metric(const std::string &name,
                bool value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    struct ObjField {
        enum class Type : std::uint8_t { I64, F64, STR, BOOL };
        Type t{Type::STR};
        std::string k;
        std::int64_t i64{0};
        double f64{0.0};
        std::string str;
        bool b{false};
    };

    struct Obj {
    private:
        std::string name_;
        std::vector<ObjField> fields_;
    public:
        explicit Obj(std::string n) : name_(std::move(n)) {}
        Obj &add(const std::string &key, std::int64_t v);
        Obj &add(const std::string &key, double v);
        Obj &add(const std::string &key, const std::string &v);
        Obj &add(const std::string &key, bool v);
        [[nodiscard]] const std::string &name() const noexcept { return name_; }
        [[nodiscard]] const std::vector<ObjField> &fields() const noexcept { return fields_; }
    };

    // Emit a structured object: {ts_ms, name, fields:{...}}
    void metric(const Obj &o);

    // Increment a named counter asynchronously and emit a time-stamped count.
    void counter(const std::string &name);

    // Record a named event with optional tags (no numeric value), asynchronously.
    void event(const std::string &name,
               std::initializer_list<std::pair<std::string, std::string>> tags = {});

    // Debug gating for GOBP: controlled by env var CRSCE_GOBP_DEBUG.
    // Uses an internal cached flag; evaluation and emission remain asynchronous.
    bool gobp_debug_enabled() noexcept;
    void debug_event_gobp(const std::string &name,
                          std::initializer_list<std::pair<std::string, std::string>> tags = {});
}
