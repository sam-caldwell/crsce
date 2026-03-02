/**
 * @file metric_impl.h
 * @brief Route legacy metric_impl calls to the event API (template helper).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>

#include "common/O11y/O11y.h"

namespace crsce::o11y::detail {
    /**
     * @name metric_impl
     * @brief Forward a legacy metric call to the event API, dropping the value.
     * @param name Event name.
     * @param value Ignored (retained for source compatibility).
     * @param tags Optional key/value tags.
     * @return void
     */
    template <typename Value>
    inline void metric_impl(const std::string &name,
                            const Value &value,
                            std::initializer_list<std::pair<std::string, std::string>> tags) {
        const crsce::o11y::O11y::Tags t{tags.begin(), tags.end()};
        if constexpr (std::is_same_v<Value, std::string>) {
            crsce::o11y::O11y::instance().event(std::string(name), value);
        } else {
            crsce::o11y::O11y::instance().event(std::string(name), t);
        }
    }
}
