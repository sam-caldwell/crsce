/**
 * @file metric_metric_f64.cpp
 * @brief Emit floating-point metric with optional tags.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "common/O11y/metric.h"
#include "common/O11y/detail/metric_helpers.h"

#include <initializer_list>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name metric
     * @brief Emit floating-point metric with optional tags.
     * @param name Metric name.
     * @param value Floating-point value.
     * @param tags Optional key/value tags.
     * @return void
     */
    void metric(const std::string &name, double value,
                std::initializer_list<std::pair<std::string, std::string>> tags) {
        detail::metric_impl(name, value, tags);
    }
}
