/**
 * @file solver_env_read_env_double.h
 * @brief Declare read_env_double helper.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

namespace crsce::decompress::detail {
    double read_env_double(const char *name, double defv);
}

