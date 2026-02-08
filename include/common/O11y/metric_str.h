/**
 * @file metric_str.h
 * @brief Emit string metric with optional tags (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <initializer_list>
#include <string>
#include <utility>

namespace crsce::o11y {
    /**
     * @name metric
     * @brief Emit string metric with optional tags.
     * @param name Metric name.
     * @param value String value.
     * @param tags Optional key/value tags.
     * @return void
     */
    void metric(const std::string &name, const std::string &value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});
}

