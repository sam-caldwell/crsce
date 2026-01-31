/**
 * @file cmd/testRunnerOnes/main.cpp
 * @brief CLI entry for TestRunnerOnes; delegates to testrunner_ones::cli::run().
 */
#include "testrunnerOnes/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include "testrunner/detail/json_escape.h"
#include "testrunner/Cli/detail/extract_exit_code.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"

using crsce::testrunner::cli::extract_exit_code;

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerOnes";
    return crsce::testrunner_ones::cli::run(out_dir);
} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
