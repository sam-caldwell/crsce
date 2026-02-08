/**
 * @file metric_debug_event_gobp.cpp
 * @brief Conditional emission of GOBP debug events.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/O11y/debug_event_gobp.h"
#include "common/O11y/event.h"
#include "common/O11y/gobp_debug_enabled.h"

#include <initializer_list>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name debug_event_gobp
     * @brief Emit event only when gobp_debug_enabled() is true.
     * @param name Event name.
     * @param tags Optional key/value tags.
     * @return void
     */
    void debug_event_gobp(const std::string &name,
                          std::initializer_list<std::pair<std::string, std::string>> tags) {
        if (!gobp_debug_enabled()) { return; }
        event(name, tags);
    }
}
