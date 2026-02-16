/**
 * @file Timeouts.h
 * @brief CSM timeout constants
 */
#pragma once

#include <chrono>

namespace crsce::decompress {
    inline constexpr std::chrono::milliseconds CSM_READ_LOCK_TIMEOUT{ std::chrono::seconds(10) };
}

