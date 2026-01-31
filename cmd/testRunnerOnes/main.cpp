/**
 * @file cmd/testRunnerOnes/main.cpp
 * @brief CLI entry for TestRunnerOnes; delegates to testrunner_ones::cli::run().
 */
#include "testrunnerOnes/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerOnes";
    return crsce::testrunner_ones::cli::run(out_dir);
} catch (const std::exception &e) {
    std::cerr << "testRunnerOnes error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerOnes error: unknown exception" << '\n';
    return 1;
}
