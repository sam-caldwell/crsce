/**
 * @file metric_bool.h
 * @brief Emit boolean metric with optional tags (declaration).
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
     * @brief Emit boolean metric with optional tags.
     * @param name Metric name.
     * @param value Boolean value.
     * @param tags Optional key/value tags.
     * @return void
     */
    void metric(const std::string &name, bool value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});
}

