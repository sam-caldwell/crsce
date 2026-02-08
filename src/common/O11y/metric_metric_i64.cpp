/**
 * @file metric_metric_i64.cpp
 * @brief Emit integral metric with optional tags.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/metric.h"
#include "common/O11y/detail/metric_helpers.h"

#include <initializer_list>
#include <cstdint>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name metric
     * @brief Emit integral metric with optional tags.
     * @param name Metric name.
     * @param value Integral value.
     * @param tags Optional key/value tags.
     * @return void
     */
    void metric(const std::string &name, std::int64_t value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        detail::metric_impl(name, value, tags);
    }
}
