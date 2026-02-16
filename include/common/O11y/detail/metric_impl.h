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
#include "common/O11y/O11y.h"

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
        // Upsert into unified store asynchronously; no immediate JSON emission.
        const crsce::o11y::O11y::Tags t{tags.begin(), tags.end()};
        if constexpr (std::is_same_v<Value, std::string>) {
            crsce::o11y::O11y::instance().event(std::string(name), value);
        } else if constexpr (std::is_same_v<Value, bool>) {
            crsce::o11y::O11y::instance().metric(std::string(name), value, t);
        } else if constexpr (std::is_floating_point_v<Value>) {
            crsce::o11y::O11y::instance().metric(std::string(name), static_cast<double>(value), t);
        } else { // integral-like
            crsce::o11y::O11y::instance().metric(std::string(name), static_cast<std::int64_t>(value), t);
        }

        if (crsce::o11y::detail::flush_enabled()) {
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
}
