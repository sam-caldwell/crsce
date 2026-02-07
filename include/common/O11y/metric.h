/**
 * @file metric.h
 * @brief Structured metric emission (JSONL) with standardized timestamping.
 */
#pragma once

#include <string>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace crsce::o11y {

    // Tags are optional string key/value pairs. Timestamp is always added by the implementation.
    void metric(const std::string &name,
                std::int64_t value = 1,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    void metric(const std::string &name,
                double value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    void metric(const std::string &name,
                const std::string &value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});

    void metric(const std::string &name,
                bool value,
                std::initializer_list<std::pair<std::string, std::string>> tags = {});
}
