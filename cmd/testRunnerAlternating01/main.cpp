/**
 * @file cmd/testRunnerAlternating01/main.cpp
 * @brief CLI entry for TestRunnerAlternating01; delegates to testrunner_alternating01::cli::run().
 */
#include "testrunnerAlternating01/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerAlternating01";
    return crsce::testrunner_alternating01::cli::run(out_dir);
} catch (const std::exception &e) {
    std::cerr << "testRunnerAlternating01 error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerAlternating01 error: unknown exception" << '\n';
    return 1;
}

