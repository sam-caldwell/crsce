/**
 * @file CompressApp.cpp
 * @brief Implements the compressor CLI runner: parse args, validate paths, run compression.
 */
#include "compress/Cli/CompressApp.h"

#include "common/ArgParser/ArgParser.h"
#include "compress/Compress/Compress.h"

#include <exception>
#include <print>
#include <span>
#include <string>
#include <sys/stat.h>
#include <cstddef>
#include <cstdio>

namespace crsce::compress::cli {
    /**
     * @brief Run the compression CLI pipeline.
     */
    int run(std::span<char*> args) {
        try {
            crsce::common::ArgParser parser("compress");
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

                if (crsce::compress::Compress cx(opt.input, opt.output); !cx.compress_file()) {
                    std::println(stderr, "error: compression failed");
                    return 4;
                }
            } else {
                std::println("Hello, World (compress)!");
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
} // namespace crsce::compress::cli
