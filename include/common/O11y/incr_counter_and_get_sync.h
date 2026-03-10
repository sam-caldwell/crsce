/**
 * @file incr_counter_and_get_sync.h
 * @brief Increment and retrieve a named counter (thread-safe).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace crsce::o11y::detail {
    /**
     * @name incr_counter_and_get_sync
     * @brief Increment a named counter and return the updated value.
     * @param name Counter name.
     * @return std::uint64_t Updated count after increment.
     */
    inline std::uint64_t incr_counter_and_get_sync(const std::string &name) {
        static std::mutex mu; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        static std::unordered_map<std::string, std::uint64_t> counters; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        const std::scoped_lock lk(mu);
        return ++counters[name];
    }
}

