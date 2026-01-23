/**
 * @file DecompressApp.cpp
 * @brief Implements the decompressor CLI runner: parse args, validate paths, run decompression.
 */
#include "decompress/Cli/DecompressApp.h"

#include "common/ArgParser/ArgParser.h"
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Block/BlockSolver.h"
#include "decompress/Utils/AppendBits.h"
#include "decompress/Csm/Csm.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <cstdio>
#include <cstddef>

namespace crsce::decompress::cli {
    /**
     * @brief Run the decompression CLI pipeline.
     */
    int run(std::span<char*> args) {
        try {
            crsce::common::ArgParser parser("decompress");
            if (args.size() > 1) {
                const bool parsed_ok = parser.parse(args);
                const auto &opt = parser.options();
                if (!parsed_ok || opt.help) {
                    std::println(stderr, "usage: {}", parser.usage());
                    return parsed_ok && opt.help ? 0 : 2;
                }

                // Validate file paths per usage: input must exist; output must NOT exist
                struct stat statbuf{};
                if (stat(opt.input.c_str(), &statbuf) != 0) {
                    std::println(stderr, "error: input file does not exist: {}", opt.input);
                    return 3;
                }
                if (stat(opt.output.c_str(), &statbuf) == 0) {
                    std::println(stderr, "error: output file already exists: {}", opt.output);
                    return 3;
                }

                // Decompress: iterate blocks, reconstruct CSMs and accumulate original bytes in memory
                crsce::decompress::Decompressor dx(opt.input);
                crsce::decompress::HeaderV1Fields hdr{};

                std::vector<std::uint8_t> output_bytes;
                output_bytes.reserve(1024);
                std::uint8_t curr = 0;
                int bit_pos = 0;
                constexpr std::string seed = "CRSCE_v1_seed"; // must match compressor default
                bool ok = true;

                const bool drove = dx.for_each_block(hdr, [&](const std::span<const std::uint8_t> lh,
                                                            const std::span<const std::uint8_t> sums) {
                    if (!ok) { return; }
                    static constexpr std::uint64_t bits_per_block = static_cast<std::uint64_t>(crsce::decompress::Csm::kS) * static_cast<std::uint64_t>(crsce::decompress::Csm::kS);
                    const std::uint64_t total_bits = hdr.original_size_bytes * 8U;
                    const std::uint64_t bits_done = (static_cast<std::uint64_t>(output_bytes.size()) * 8U) + static_cast<std::uint64_t>(bit_pos);
                    if (bits_done >= total_bits) {
                        return; // nothing more to do
                    }

                    // Solve block
                    crsce::decompress::Csm csm;
                    if (const bool solved = crsce::decompress::solve_block(lh, sums, csm, seed); !solved) {
                        ok = false;
                        return;
                    }

                    // Append up to the remaining bits from this block (row-major)
                    const std::uint64_t remain = total_bits - bits_done;
                    const std::uint64_t to_take = std::min(remain, bits_per_block);
                    crsce::decompress::append_bits_from_csm(csm, to_take, output_bytes, curr, bit_pos);
                });

                if (!drove || !ok) {
                    std::println(stderr, "error: decompression failed");
                    return 4;
                }

                // Write all accumulated bytes to the output path
                std::ofstream out(opt.output, std::ios::binary);
                if (!out.good()) {
                    std::println(stderr, "error: could not open output for write: {}", opt.output);
                    return 4;
                }
                if (!output_bytes.empty()) {
                    for (const auto b: output_bytes) {
                        out.put(static_cast<char>(b));
                    }
                }
                if (!out.good()) {
                    std::println(stderr, "error: write failed: {}", opt.output);
                    return 4;
                }
            }
            return 0;
        } catch (const std::exception &e) {
            std::fputs("error: ", stderr);
            std::fputs(e.what(), stderr);
            std::fputs("\n", stderr);
            return 1;
        } catch (...) {
            std::fputs("error: unknown exception\n", stderr);
            return 1;
        }
    }
} // namespace crsce::decompress::cli
