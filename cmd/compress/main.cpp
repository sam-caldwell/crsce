/**
 * File: cmd/compress/main.cpp
 * Brief: CLI entry for compressor; validates args and prints a greeting.
 */
#include "common/ArgParser/ArgParser.h"
#include <cstdio>
#include <exception>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <string>
#include <sys/stat.h>

#include "common/FileBitSerializer/FileBitSerializer.h"
#include "compress/Compress/Compress.h"

/**
 * Main entry: parse -in/-out and validate file preconditions.
 */
auto main(const int argc, char *argv[]) -> int {
    try {
        crsce::common::ArgParser parser("compress");
        if (argc > 1) {
            const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
            const bool parsed_ok = parser.parse(args);
            // ReSharper disable once CppUseStructuredBinding
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
                std::println(stderr, "error: output file already exists: {}",
                             opt.output);
                return 3;
            }
            // Stream input bits into the block accumulator and create the output file.
            crsce::common::FileBitSerializer in_bits(opt.input);
            if (!in_bits.good()) {
                std::println(stderr, "error: failed to open input file: {}", opt.input);
                return 3;
            }

            const std::ofstream out(opt.output, std::ios::binary);
            if (!out.good()) {
                std::println(stderr, "error: failed to create output file: {}", opt.output);
                return 3;
            }

            crsce::compress::Compress cx(opt.input, opt.output);
            while (true) {
                const auto b = in_bits.pop();
                if (!b.has_value()) {
                    break;
                }
                cx.push_bit(*b);
            }
            cx.finalize_block();
            // For now, do not write container payloads; just ensure output exists.
        }

        std::println("Hello, World (compress)!");
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
