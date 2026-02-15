/**
 * @file Decompressor_decompress_file.cpp
 * @brief Drive block-wise decompression and write reconstructed bytes to output_path_.
 */

#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

#include "decompress/Block/detail/solve_block.h"
#include "decompress/Utils/detail/append_bits_from_csm.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Decompressor/detail/log_decompress_failure.h"
#include "decompress/Utils/detail/decompress_emit_summary.h"
#include "decompress/Decompressor/detail/RowLog.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"

#include "common/O11y/event.h"
#include "common/O11y/metric_i64.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace crsce::decompress {
    /**
     * @name decompress_files
     * @brief this is the top-level decompressor function
     * @return bool error state
     */
    bool Decompressor::decompress_file() {
        const auto run_start = std::chrono::system_clock::now();
        HeaderV1Fields hdr{};

        std::uint64_t blocks_attempted = 0;
        std::uint64_t blocks_successful = 0;

        if (!read_header(hdr)) {
            ::crsce::decompress::detail::emit_decompress_summary("invalid_header", 0, 0);
            return false;
        }

        std::vector<std::uint8_t> output_bytes;
        output_bytes.reserve(1024);
        std::uint8_t curr = 0;
        int bit_pos = 0;

        static constexpr std::uint64_t bits_per_block =
                static_cast<std::uint64_t>(Csm::kS) * static_cast<std::uint64_t>(Csm::kS);
        const std::uint64_t total_bits = hdr.original_size_bytes * 8U;

        // Announce completion-stats log path early for harnesses
        touch_and_announce_rowlog(output_path_);

        ::crsce::o11y::metric("decompress_begin_blocks", static_cast<std::int64_t>(hdr.block_count));

        for (std::uint64_t i = 0; i < hdr.block_count; ++i) {
            ::crsce::o11y::metric("block_open", static_cast<std::int64_t>(i));

            auto payload = read_block();
            if (!payload.has_value()) {
                ::crsce::o11y::event("decompress_error", {
                                         {"type", std::string("short_read")}, {"block", std::to_string(i)}
                                     });
                ::crsce::decompress::detail::emit_decompress_summary("short_read", blocks_attempted, blocks_successful);
                return false;
            }

            ++blocks_attempted;

            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums;

            if (!split_payload(*payload, lh, sums)) {
                ::crsce::o11y::event("decompress_error", {
                                         {"type", std::string("bad_block_payload")},
                                         {"block", std::to_string(i)}
                                     });
                ::crsce::decompress::detail::emit_decompress_summary("bad_block_payload",
                                                                     blocks_attempted,
                                                                     blocks_successful);
                return false;
            }

            const std::uint64_t bits_done = (static_cast<std::uint64_t>(output_bytes.size()) * 8U)
                                            + static_cast<std::uint64_t>(bit_pos);

            if (bits_done >= total_bits) {
                break;
            }
            const std::uint64_t remain = total_bits - bits_done;
            const std::uint64_t to_take = std::min(remain, bits_per_block);
            ::crsce::o11y::metric("block_pre_solve", static_cast<std::int64_t>(i),
                                  {
                                      {"remain_bits", std::to_string(remain)},
                                      {"take_bits", std::to_string(to_take)}
                                  }
            );

            Csm csm; // solver target grid
            bool solved_ok = false;
            try {
                ::crsce::o11y::metric("block_solve_begin", static_cast<std::int64_t>(i),
                                      {{"valid_bits", std::to_string(to_take)}});

                solved_ok = solve_block(lh, sums, csm, to_take);

                ::crsce::o11y::metric("block_solve_end", static_cast<std::int64_t>(i), {
                                          {"ok", (solved_ok ? std::string("true") : std::string("false"))}
                                      });
            } catch (const std::exception &ex) {
                ::crsce::o11y::event("decompress_error", {
                                         {"type", std::string("solve_exception")}, {"block", std::to_string(i)},
                                         {"what", std::string(ex.what())}
                                     });
            } catch (...) {
                ::crsce::o11y::event("decompress_error", {
                                         {"type", std::string("solve_exception_unknown")}, {"block", std::to_string(i)}
                                     });
            } // end try...catch...

            if (!solved_ok) {
                ::crsce::o11y::event("decompress_error", {
                                         {"type", std::string("solve_failed")}, {"block", std::to_string(i)}
                                     });

                if (const auto snap = get_block_solve_snapshot(); snap.has_value()) {
                    try {
                        crsce::decompress::detail::log_decompress_failure(
                            hdr.block_count,
                            blocks_attempted,
                            blocks_successful,
                            i, *snap
                        );
                    } catch (...) {
                        /* ignore */
                    }
                }

                write_row_completion_stats_failure(output_path_, i, csm, lh, sums, run_start);

                ::crsce::decompress::detail::emit_decompress_summary("solve_failed", blocks_attempted,
                                                                     blocks_successful);
                return false;
            }

            append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
            ++blocks_successful;
            write_row_completion_stats_success(output_path_, i, csm, lh, sums, run_start);
        }

        // Finalize output
        std::ofstream out(output_path_, std::ios::binary);

        if (!out.good()) {
            ::crsce::o11y::event("decompress_error", {
                                     {"type", std::string("open_output_failed")},
                                     {"path", output_path_}
                                 }
            );
            ::crsce::decompress::detail::emit_decompress_summary("open_output_failed",
                                                                 blocks_attempted,
                                                                 blocks_successful);
            return false;
        }
        for (const auto b: output_bytes) out.put(static_cast<char>(b));
        if (!out.good()) {
            ::crsce::o11y::event(
                "decompress_error", {
                    {"type", std::string("write_failed")},
                    {"path", output_path_}
                }
            );
            ::crsce::decompress::detail::emit_decompress_summary(
                "write_failed",
                blocks_attempted,
                blocks_successful
            );
            return false;
        }

        ::crsce::decompress::detail::emit_decompress_summary("ok", blocks_attempted, blocks_successful);
        return true;
    }
} // namespace crsce::decompress
