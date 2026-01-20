/**
 * File: cmd/decompress/main.cpp
 * Brief: CLI entry for decompressor; validates args and prints a greeting.
 */
#include "common/ArgParser/ArgParser.h"
#include <cstdio>
#include <exception>
#include <print>
#include <span>
#include <sys/stat.h>

/**
 * Main entry: parse -in/-out and validate file preconditions.
 */
auto main(const int argc, char *argv[]) -> int {
  try {
    crsce::common::ArgParser parser("decompress");
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
    }
    std::println("Hello, World (decompress)!");
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
