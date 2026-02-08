/**
 * @file gobp_debug_enabled.h
 * @brief Debug gating for GOBP via environment (declaration).
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

namespace crsce::o11y {
    /**
     * @name gobp_debug_enabled
     * @brief Check if GOBP debug events are enabled via env var CRSCE_GOBP_DEBUG.
     * @return bool True when enabled; false otherwise.
     */
    bool gobp_debug_enabled() noexcept;
}

