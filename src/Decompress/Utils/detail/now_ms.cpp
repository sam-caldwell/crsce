/**
 * @file now_ms.cpp
 * @brief Milliseconds since epoch utility for decompressor.
 * @author Sam Caldwell
 */
#include "decompress/Utils/detail/now_ms.h"

#include <chrono>
#include <cstdint>

namespace crsce::decompress::detail {
std::uint64_t now_ms() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
    );
}
}
