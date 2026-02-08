/**
 * @file metric_counter.cpp
 * @brief Emit increment-only counter events.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/O11y/counter.h"
#include "common/O11y/detail/incr_counter_and_get_sync.h"
#include "common/O11y/detail/escape_json.h"
#include "common/O11y/detail/now_ms.h"
#include "common/O11y/detail/write_line_sync.h"

#include <ios>
#include <sstream>
#include <string>

namespace crsce::o11y {
    /**
     * @name counter
     * @brief Increment a named counter and emit the updated count.
     * @param name Counter name.
     * @return void
     */
    void counter(const std::string &name) {
        const auto cnt = detail::incr_counter_and_get_sync(name);
        std::ostringstream oss; oss.setf(std::ios::fixed); oss.precision(6);
        oss << '{';
        oss << "\"ts_ms\":" << detail::now_ms() << ',';
        oss << "\"name\":\"" << detail::escape_json(name) << "\",";
        if (cnt < 10) {
            oss << "\"count\":0" << cnt;
        } else {
            oss << "\"count\":" << cnt;
        }
        oss << '}';
        detail::write_line_sync(oss.str());
    }
}
