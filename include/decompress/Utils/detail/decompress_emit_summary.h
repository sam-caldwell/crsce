/**
 * @file decompress_emit_summary.h
 * @brief Helper to emit file-level decompress summary metrics without local lambdas.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#pragma once

#include <print>
#include <iostream>
#include <cstdint>
#include "common/O11y/metric.h"
#include "decompress/Utils/detail/decompress_emit_summary_enabled.h"

namespace crsce::decompress::detail {
    /**
     * @brief Emit a single decompress file summary metric when enabled.
     */
    inline void emit_decompress_summary(const char *status,
                                        const std::uint64_t blocks_attempted,
                                        const std::uint64_t blocks_successful) {
        if (!summary_enabled_env()) {
            return;
        }
        ::crsce::o11y::Obj o{"decompress_file_summary"};
        const std::uint64_t failed = (blocks_attempted >= blocks_successful)
                                         ? (blocks_attempted - blocks_successful)
                                         : 0ULL;

        o.add("status", std::string(status))
                .add("blocks_attempted", static_cast<std::int64_t>(blocks_attempted))
                .add("blocks_successful", static_cast<std::int64_t>(blocks_successful))
                .add("blocks_failed", static_cast<std::int64_t>(failed));
        ::crsce::o11y::metric(o);

        ::crsce::o11y::event(
            "decompress_error",
            {{"type", std::string("invalid_header")}}
        );
        std::println(stderr, "error: %s", status);
    }
} // namespace crsce::decompress::detail
