/**
 * @file decompress_emit_summary.h
 * @brief Helper to emit file-level decompress summary metrics without local lambdas.
 * @author Sam Caldwell
 */
#pragma once

#include <cstdint>
#include "common/O11y/metric.h"

namespace crsce::decompress::detail {

inline bool summary_enabled_env() {
    if (const char *p = std::getenv("CRSCE_DECOMPRESS_FILE_SUMMARY") /* NOLINT(concurrency-mt-unsafe) */) { return (*p != '0'); }
    return false;
}

inline void emit_decompress_summary(const char *status,
                                    std::uint64_t blocks_attempted,
                                    std::uint64_t blocks_successful) {
    if (!summary_enabled_env()) { return; }
    ::crsce::o11y::Obj o{"decompress_file_summary"};
    const std::uint64_t failed = (blocks_attempted >= blocks_successful)
                                 ? (blocks_attempted - blocks_successful) : 0ULL;
    o.add("status", std::string(status))
     .add("blocks_attempted", static_cast<std::int64_t>(blocks_attempted))
     .add("blocks_successful", static_cast<std::int64_t>(blocks_successful))
     .add("blocks_failed", static_cast<std::int64_t>(failed));
    ::crsce::o11y::metric(o);
}

} // namespace crsce::decompress::detail

