/**
 * @file log_decompress_failure.cpp
 * @brief JSON logging helper for block-solve failures during decompression.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/Decompressor/detail/log_decompress_failure.h"

#include <cstddef>
#include <cstdint>
#include <print>
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Decompressor/detail/phase_to_cstr.h"
#include "common/O11y/metric.h"

namespace crsce::decompress::detail {
    // phase_to_cstr provided in header to avoid extra constructs
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
    void log_decompress_failure(const std::uint64_t total_blocks,
                                const std::uint64_t blocks_attempted,
                                const std::uint64_t blocks_successful,
                                const std::uint64_t failed_index,
                                const crsce::decompress::BlockSolveSnapshot &s) {
        // Aggregate unknown sums by family
        std::size_t sumU_row = 0; for (auto v : s.U_row) { sumU_row += static_cast<std::size_t>(v); }
        std::size_t sumU_col = 0; for (auto v : s.U_col) { sumU_col += static_cast<std::size_t>(v); }
        std::size_t sumU_diag = 0; for (auto v : s.U_diag) { sumU_diag += static_cast<std::size_t>(v); }
        std::size_t sumU_xdg = 0; for (auto v : s.U_xdiag) { sumU_xdg += static_cast<std::size_t>(v); }

        // Emit a compact o11y summary (avoid large arrays)
        ::crsce::o11y::Obj o{"decompress_failure"};
        o.add("total_blocks", static_cast<std::int64_t>(total_blocks))
         .add("blocks_attempted", static_cast<std::int64_t>(blocks_attempted))
         .add("blocks_successful", static_cast<std::int64_t>(blocks_successful))
         .add("block_index", static_cast<std::int64_t>(failed_index))
         .add("phase", std::string(phase_to_cstr(s.phase)))
         .add("iter", static_cast<std::int64_t>(s.iter))
         .add("solved", static_cast<std::int64_t>(s.solved))
         .add("unknown_total", static_cast<std::int64_t>(s.unknown_total))
         .add("U_rows_sum", static_cast<std::int64_t>(sumU_row))
         .add("U_cols_sum", static_cast<std::int64_t>(sumU_col))
         .add("U_diags_sum", static_cast<std::int64_t>(sumU_diag))
         .add("U_xdiags_sum", static_cast<std::int64_t>(sumU_xdg))
         .add("message", s.message);
        ::crsce::o11y::metric(o);

        // Print JSON (stdout) for test runners to capture
        std::print("{{\n");
        std::print("  \"step\":\"decompress-failure\",\n");
        std::print("  \"total_blocks\":{},\n", total_blocks);
        std::print("  \"blocks_attempted\":{},\n", blocks_attempted);
        std::print("  \"blocks_successful\":{},\n", blocks_successful);
        std::print("  \"blocks_failed\":{},\n", static_cast<std::uint64_t>(1));
        std::print("  \"block_index\":{},\n", failed_index);
        std::print("  \"phase\":\"{}\",\n", phase_to_cstr(s.phase));
        std::print("  \"iter\":{},\n", s.iter);
        std::print("  \"solved\":{},\n", s.solved);
        std::print("  \"unknown_total\":{},\n", s.unknown_total);

        // Emit LSM/VSM/DSM/XSM arrays without introducing lambdas (ODPCPP-friendly)
        std::print("  \"lsm\":[");
        for (std::size_t k = 0; k < s.lsm.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.lsm[k]));
        }
        std::print("],\n");

        std::print("  \"vsm\":[");
        for (std::size_t k = 0; k < s.vsm.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.vsm[k]));
        }
        std::print("],\n");

        std::print("  \"dsm\":[");
        for (std::size_t k = 0; k < s.dsm.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.dsm[k]));
        }
        std::print("],\n");

        std::print("  \"xsm\":[");
        for (std::size_t k = 0; k < s.xsm.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.xsm[k]));
        }
        std::print("],\n");

        // Unknowns per family
        std::print("  \"U_row\":[");
        for (std::size_t k = 0; k < s.U_row.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.U_row[k]));
        }
        std::print("],\n");

        std::print("  \"U_col\":[");
        for (std::size_t k = 0; k < s.U_col.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.U_col[k]));
        }
        std::print("],\n");

        std::print("  \"U_diag\":[");
        for (std::size_t k = 0; k < s.U_diag.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.U_diag[k]));
        }
        std::print("],\n");

        std::print("  \"U_xdiag\":[");
        for (std::size_t k = 0; k < s.U_xdiag.size(); ++k) {
            std::print("{}{}", (k ? "," : ""), static_cast<std::uint64_t>(s.U_xdiag[k]));
        }
        std::print("],\n");

        std::print(
            "  \"unknown_sums\":{{\"rows\":{},\"cols\":{},\"diags\":{},\"xdiags\":{}}},\n",
            sumU_row, sumU_col, sumU_diag, sumU_xdg);
        std::print("  \"message\":\"{}\"\n", s.message);
        std::print("}}\n");
    }
}
