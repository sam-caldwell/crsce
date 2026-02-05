/**
 * @file solver_env_read_seed.cpp
 * @brief Implementation of read_seed_or_default() helper.
 */
#include "decompress/Block/detail/solver_env.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace crsce::decompress::detail {
    std::uint64_t read_seed_or_default(const std::uint64_t fallback) {
        if (const char *s = std::getenv("CRSCE_SOLVER_SEED"); s && *s) { // NOLINT(concurrency-mt-unsafe)
            char *end = nullptr; // NOLINT(misc-const-correctness)
            const auto v = std::strtoull(s, &end, 0);
            if (end != s) { return static_cast<std::uint64_t>(v); }
        }
        if (const char *r = std::getenv("CRSCE_SOLVER_RANDOMIZE"); // NOLINT(concurrency-mt-unsafe)
            r && (*r == '1' || *r == 'y' || *r == 'Y' || *r == 't' || *r == 'T')) {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            return static_cast<std::uint64_t>(ns);
        }
        return fallback;
    }
}
