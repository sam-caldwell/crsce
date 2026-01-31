/**
 * @file cmd/testRunnerAlternating01/main.cpp
 * @brief CLI entry for TestRunnerAlternating01; delegates to testrunner_alternating01::cli::run().
 */
#include "testrunnerAlternating01/Cli/detail/run_single.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include "testrunner/detail/json_escape.h"
#include "testrunner/Cli/detail/extract_exit_code.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"

using crsce::testrunner::cli::extract_exit_code;

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerAlternating01";
    // ReSharper disable once CppTooWideScopeInitStatement
    const std::vector<std::uint64_t> block_sizes = {
        1ULL,
        2ULL,
        5ULL,
        10ULL,
        20ULL
    };

    for (const auto block_size : block_sizes) {
        return crsce::testrunner_alternating01::cli::run(out_dir, block_size);
    }

} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating01\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating01\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating01\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating01\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
