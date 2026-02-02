/**
 * @file cmd/testRunnerRandom/main.cpp
 * @brief CLI entry for TestRunnerRandom; delegates to testrunner::cli::run().
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "testRunnerRandom/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include <cstdint>
#include <span>
#include <cstddef>
#include <algorithm>
#include "testRunnerRandom/detail/json_escape.h"
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"

using crsce::testrunner_random::cli::extract_exit_code;

/**
 * @brief Program entry point for testRunnerRandom CLI.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
int main(const int argc, char *argv[]) try {

    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerRandom";


    // Optional args: --min-bytes <N> --max-bytes <N>
    std::uint64_t min_bytes = 1024ULL;                          // 1 KiB
    std::uint64_t max_bytes = 1024ULL * 1024ULL * 1024ULL;      // 1 GiB

    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (const std::string arg = args[i]; arg == "--min-bytes" && (i + 1) < args.size()) {
            min_bytes = static_cast<std::uint64_t>(std::stoull(args[++i]));
        } else if (arg == "--max-bytes" && (i + 1) < args.size()) {
            max_bytes = static_cast<std::uint64_t>(std::stoull(args[++i]));
        }
    }

    max_bytes = std::max(max_bytes, min_bytes);
    return crsce::testrunner_random::cli::run(out_dir, min_bytes, max_bytes);

} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
