/**
 * @file metric_impl.h
 * @brief Serialize a simple metric value with optional tags (template helper).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <initializer_list>
#include <ios>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "common/O11y/detail/escape_json.h"
#include "common/O11y/detail/now_ms.h"
#include "common/O11y/detail/write_line_sync.h"

namespace crsce::o11y::detail {
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

