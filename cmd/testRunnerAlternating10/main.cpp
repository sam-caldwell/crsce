/**
 * @file cmd/testRunnerAlternating10/main.cpp
 * @brief CLI entry for TestRunnerAlternating10; delegates to testrunner_alternating10::cli::run().
 */
#include "testrunnerAlternating10/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerAlternating10";
    return crsce::testrunner_alternating10::cli::run(out_dir);
} catch (const std::exception &e) {
    std::cerr << "testRunnerAlternating10 error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerAlternating10 error: unknown exception" << '\n';
    return 1;
}

