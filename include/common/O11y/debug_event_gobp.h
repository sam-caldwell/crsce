/**
 * @file debug_event_gobp.h
 * @brief Conditional emission of GOBP debug events (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

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
                          std::initializer_list<std::pair<std::string, std::string>> tags);
}

