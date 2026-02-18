/**
 * @file run.cpp
 * @brief Implements the decompressor CLI runner: parse args, validate paths, run decompression.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/O11y/O11y.h"
#include "decompress/Cli/run.h"
#include "decompress/Decompressor/Decompressor.h"

#include <exception>
#include <string>
#include <cstdint> // NOLINT


namespace crsce::decompress::cli {
    /**
     * @name run
     * @brief Run the decompression CLI pipeline.
     * @param input input filename (source)
     * @param output output filename (target)
     * @return Process exit code: 0 on success; non-zero on failure.
     */
    int run(const std::string &input, const std::string &output) {
        try {
            crsce::decompress::Decompressor decompressor(input, output);
            const bool ok = decompressor.decompress_file();
            return ok ? 0 : 4;
        } catch (const std::exception &e) {
            ::crsce::o11y::O11y::instance().event("decompress_error", {
                                                      {"type", std::string("exception")},
                                                      {"what", std::string(e.what())}
                                                  });
            ::crsce::o11y::O11y::instance().counter("decompress_files_failed");
            return 99;
        } catch (...) {
            ::crsce::o11y::O11y::instance().event("decompress_error",
                                                  {
                                                      {"type", std::string("exception_unknown")}
                                                  });
            ::crsce::o11y::O11y::instance().counter("decompress_files_failed");
            return 100;
        }
    }
} // namespace crsce::decompress::cli
