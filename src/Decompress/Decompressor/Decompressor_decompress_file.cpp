/**
 * @file Decompressor_decompress_file.cpp
 * @brief Drive block-wise decompression and write reconstructed bytes to output_path_.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

#include "decompress/Utils/detail/append_bits_from_csm.h"
#include "decompress/Csm/Csm.h"
#include "decompress/Block/detail/get_block_solve_snapshot.h"
#include "decompress/CrossSum/CrossSums.h"

#include "common/O11y/O11y.h"
#include "decompress/Decompressor/detail/RowLog.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <span>
#include <string>
#include <vector>
#include <cstdint> // NOLINT
#include <ios>
#include <chrono>

namespace crsce::decompress {
    /**
     * @name decompress_file
     * @brief Top-level decompressor entry; decodes header and blocks, writes out bytes.
     * @return bool True on success; false on error.
     */
    bool Decompressor::decompress_file() {
        HeaderV1Fields hdr{};
        const auto run_start = std::chrono::system_clock::now();

        if (!read_header(hdr)) {
            return false;
        }

        std::vector<std::uint8_t> output_bytes;
        output_bytes.reserve(1024);
        std::uint8_t curr = 0;
        int bit_pos = 0;

        static constexpr std::uint64_t bits_per_block =
                static_cast<std::uint64_t>(Csm::kS) * static_cast<std::uint64_t>(Csm::kS);

        const std::uint64_t total_bits = hdr.original_size_bytes * 8U;

        ::crsce::o11y::O11y::instance().event("decompress_file_start", {
                                                  {"block_count", std::to_string(hdr.block_count)}
                                              });

        // Announce row-completion log location up front
        ::crsce::decompress::RowLog::touch_and_announce_rowlog(output_path_);

        for (std::uint64_t block_id = 0; block_id < hdr.block_count; ++block_id) {
            ::crsce::o11y::O11y::instance().event("block_start", {
                                                      {"block_id", std::to_string(block_id)},
                                                      {"block_count", std::to_string(hdr.block_count)}
                                                  });

            auto payload = read_block();
            if (!payload.has_value()) {
                ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                          {"type", std::string("short_read")},
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)}
                                                      });
                return false;
            }

            ::crsce::o11y::O11y::instance().counter("blocks_attempted");
            std::span<const std::uint8_t> lh;
            std::span<const std::uint8_t> sums_packed;

            if (!split_payload(*payload, lh, sums_packed)) {
                ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                          {"type", std::string("bad_block_payload")},
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)}
                                                      });
                return false;
            }

            const std::uint64_t bits_done = (static_cast<std::uint64_t>(output_bytes.size()) * 8U)
                                            + static_cast<std::uint64_t>(bit_pos);

            if (bits_done >= total_bits) {
                break;
            }
            const std::uint64_t remain = total_bits - bits_done;
            const std::uint64_t to_take = std::min(remain, bits_per_block);


            ::crsce::o11y::O11y::instance().event("block_pre_solve", {
                                                      {"remain_bits", std::to_string(remain)},
                                                      {"take_bits", std::to_string(to_take)},
                                                      {"block", std::to_string(block_id)},
                                                      {"block_count", std::to_string(hdr.block_count)}
                                                  });

            Csm csm; // solver target grid
            bool solved_ok = false;

            try {
                ::crsce::o11y::O11y::instance().event("block_solve_begin", {
                                                          {"type", std::string("bad_block_payload")},
                                                          {"valid_bits", std::to_string(to_take)},
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)}
                                                      });
                //
                // Unpack the cross sums from the block payload
                //
                const auto sums = CrossSums::from_packed(sums_packed);

                //
                // Call the solver and decompress the block
                //
                solved_ok = this->solve_block(lh, sums, csm, to_take);

                ::crsce::o11y::O11y::instance().event("block_solve_end", {
                                                          {
                                                              "ok",
                                                              (solved_ok ? std::string("true") : std::string("false"))
                                                          },
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)}
                                                      });
            } catch (const std::exception &ex) {
                ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                          {"type", std::string("solve_exception")},
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)},
                                                          {"what", std::string(ex.what())}
                                                      });
            } catch (...) {
                ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                          {"type", std::string("solve_exception_unknown")},
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)}
                                                      }
                );
            } // end try...catch...

            if (!solved_ok) {
                ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                          {"type", std::string("solve_failed")},
                                                          {"block", std::to_string(block_id)},
                                                          {"block_count", std::to_string(hdr.block_count)}
                                                      });
                if (const auto snap = get_block_solve_snapshot(); snap.has_value()) {
                    ::crsce::o11y::O11y::instance().event("decompress_failure", {
                                                              {"block", std::to_string(block_id)},
                                                              {"block_count", std::to_string(hdr.block_count)}
                                                          });
                }
                // Emit a detailed failure snapshot for this block
                ::crsce::decompress::RowLog::write_row_completion_stats_failure(
                    output_path_, block_id, csm, lh, *payload, run_start);
                return false; // Do not emit bytes for an unverified block
            }
            // Only append bytes when the block has fully verified (cross-sums + LH)
            ::crsce::decompress::RowLog::write_row_completion_stats_success(
                output_path_, block_id, csm, lh, *payload, run_start);
            append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
            ::crsce::o11y::O11y::instance().counter("blocks_successful");
        }

        // Finalize output
        std::ofstream out(output_path_, std::ios::binary);

        if
        (

            !
            out
            .
            good()

        ) {
            ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                      {"type", std::string("open_output_failed")},
                                                      {"path", output_path_}
                                                  }
            );
            return false;
        }
        for
        (

            const auto b: output_bytes
        ) {
            out.put(static_cast<char>(b));
        }
        if
        (

            !
            out
            .
            good()

        ) {
            ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                      {"type", std::string("write_failed")},
                                                      {"path", output_path_}
                                                  }
            );
            return false;
        }
        return
                true;
    }
} // namespace crsce::decompress
