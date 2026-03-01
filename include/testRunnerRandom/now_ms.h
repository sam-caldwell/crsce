/**
 * @file now_ms.h
 * @brief Milliseconds since epoch utility.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <cstdint>

namespace crsce::testrunner::detail {
    /**
     * @name now_ms
     * @brief Milliseconds since UNIX epoch (UTC).
     * @return Current time in milliseconds.
     */
    std::int64_t now_ms();
}
