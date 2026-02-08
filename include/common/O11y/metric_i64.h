/**
 * @file metric_i64.h
 * @brief Emit integral metric with optional tags (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <initializer_list>
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
    void metric(const std::string &name, std::int64_t value = 1,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});
}

