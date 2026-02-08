/**
 * @file metric_event.cpp
 * @brief Emit named events with optional tags.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/event.h"
#include "common/O11y/detail/escape_json.h"
#include "common/O11y/detail/now_ms.h"
#include "common/O11y/detail/write_line_sync.h"

#include <initializer_list>
#include <ios>
#include <sstream>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name event
     * @brief Emit a one-off event with optional tags.
     * @param name Event name.
     * @param tags Optional key/value tags.
     * @return void
     */
    void event(const std::string &name,
               std::initializer_list<std::pair<std::string, std::string>> tags) {
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << detail::now_ms() << ',';
        oss << "\"name\":\"" << detail::escape_json(name) << "\"";
        if (tags.size() != 0) {
            oss << ",\"tags\":{";
            bool first = true;
            for (const auto &kv : tags) {
                if (!first) { oss << ','; }
                first = false;
                oss << "\"" << detail::escape_json(kv.first) << "\":\"" << detail::escape_json(kv.second) << "\"";
            }
            oss << '}';
        }
        oss << '}';
        detail::write_line_sync(oss.str());
    }
}
