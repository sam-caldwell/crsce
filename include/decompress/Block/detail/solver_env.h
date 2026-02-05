/**
 * @file solver_env.h
 * @brief Helpers for reading solver-related environment knobs with sane defaults.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::detail {
    /**
     * @brief Read CRSCE_SOLVER_SEED/CRSCE_SOLVER_RANDOMIZE and return a seed value.
     * @param fallback Default to use when no env is set.
     * @return Seed value.
     */
    std::uint64_t read_seed_or_default(std::uint64_t fallback);

    /**
     * @brief Whether DE/GOBP SUMU tracing is enabled via CRSCE_TRACE_SUMU.
     * @return true if enabled.
     */
    bool trace_sumu_enabled();

    /**
     * @brief Read a double override from environment; return default if unset/invalid.
     */
    double read_env_double(const char *name, double defv);

    /**
     * @brief Read an int override from environment; return default if unset/invalid.
     */
    int read_env_int(const char *name, int defv);
}

