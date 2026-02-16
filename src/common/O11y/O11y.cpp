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
#include <functional>
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
#include <cstdlib>
#include <atomic>

#include "common/O11y/detail/escape_json.h"
#include "common/O11y/detail/events_path.h"
#include "common/O11y/detail/now_ms.h"
#include "common/O11y/detail/write_line_sync.h"
#include "common/O11y/detail/flush_enabled.h"
#include "common/O11y/detail/write_line_to_path_sync.h"

namespace crsce::o11y {
/**
 * @name instance
 * @brief Singleton accessor for O11y store.
 * @return O11y& Reference to the singleton instance.
 */
O11y &O11y::instance() {
    static O11y inst; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,misc-use-anonymous-namespace)
    return inst;
}

// Pull in the implementation inside the target namespace so dependent symbols resolve.
#include "detail/O11y_impl.inc"

} // namespace crsce::o11y
