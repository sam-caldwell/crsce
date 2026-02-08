/**
 * @file solver_env_read_env_int.h
 * @brief Declare read_env_int helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

namespace crsce::decompress::detail {
    /**
     * @brief Read an int from environment or return default.
     */
    int read_env_int(const char *name, int defv);
}
