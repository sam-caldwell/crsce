/**
 * File: cmd/decompress/main.cpp
 * Brief: CLI entry for decompressor; validates args and prints a greeting.
 */
#include <iostream>
#include "CommandLineArgs/ArgParser.h"
#include <filesystem>

/**
 * Main entry: parse -in/-out and validate file preconditions.
 */
int main(const int argc, char* argv[]) {
  crsce::common::ArgParser parser("decompress");
  if (argc > 1) {
    const bool ok = parser.parse(argc, argv);
    // ReSharper disable once CppUseStructuredBinding
    const auto& opt = parser.options();
    if (!ok || opt.help) {
      std::cerr << "usage: " << parser.usage() << std::endl;
      return ok && opt.help ? 0 : 2;
    }
    // Validate file paths per usage: input must exist; output must NOT exist
    namespace fs = std::filesystem;
    if (!fs::exists(opt.input)) {
      std::cerr << "error: input file does not exist: " << opt.input << std::endl;
      return 3;
    }
    if (fs::exists(opt.output)) {
      std::cerr << "error: output file already exists: " << opt.output << std::endl;
      return 3;
    }
  }
  std::cout << "Hello, World (decompress)!" << std::endl;
  return 0;
}
