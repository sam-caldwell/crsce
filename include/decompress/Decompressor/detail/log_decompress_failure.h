/**
 * @file log_decompress_failure.h
 * @brief JSON logging helper for block-solve failures during decompression.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "decompress/Block/detail/BlockSolverStatus.h"

namespace crsce::decompress::detail {
    /**
     * @name log_decompress_failure
     * @brief Emit a structured JSON record for a failed block solve using the last snapshot.
     * @param total_blocks Total blocks in the container header.
     * @param blocks_attempted Number of blocks attempted so far.
     * @param blocks_successful Number of successful blocks so far.
     * @param failed_index Index of the block that failed.
     * @param s Snapshot captured from the solver.
     * @return void
     */
    void log_decompress_failure(std::uint64_t total_blocks,
                                std::uint64_t blocks_attempted,
                                std::uint64_t blocks_successful,
                                std::uint64_t failed_index,
                                const crsce::decompress::BlockSolveSnapshot &s);
}

