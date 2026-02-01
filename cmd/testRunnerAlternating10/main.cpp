/**
 * @file cmd/testRunnerAlternating10/main.cpp
 * @brief CLI entry for TestRunnerAlternating10; delegates to testrunner_alternating10::cli::run().
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "testrunnerAlternating10/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include "testRunnerRandom/detail/json_escape.h"
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"

using crsce::testrunner_random::cli::extract_exit_code;

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerAlternating10";
    return crsce::testrunner_alternating10::cli::run(out_dir);
} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating10\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating10\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating10\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating10\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
