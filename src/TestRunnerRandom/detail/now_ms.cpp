/**
 * @file now_ms.cpp
 * @brief Milliseconds since epoch utility.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/now_ms.h"
#include <chrono>
#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name now_ms
     * @brief Return milliseconds since UNIX epoch.
     * @return Current time in milliseconds.
     */
    std::int64_t now_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
}
