/**
 * @file counter.h
 * @brief Increment-only counter event (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <string>

namespace crsce::o11y {
    /**
     * @name counter
     * @brief Increment a named counter and emit the updated count.
     * @param name Counter name.
     * @return void
     */
    void counter(const std::string &name);
}

