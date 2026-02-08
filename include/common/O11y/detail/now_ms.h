/**
 * @file now_ms.h
 * @brief Milliseconds since UNIX epoch helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <chrono>
#include <cstdint>

namespace crsce::o11y::detail {
    /**
     * @name now_ms
     * @brief Milliseconds since UNIX epoch.
     * @return std::uint64_t Milliseconds since epoch.
     */
    inline std::uint64_t now_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
}

