/**
 * @file cmd/testRunnerZeroes/main.cpp
 * @brief CLI entry for TestRunnerZeroes; delegates to testrunner_zeroes::cli::run().
*/
#include "testrunnerZeroes/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerZeroes";
    return crsce::testrunner_zeroes::cli::run(out_dir);
} catch (const std::exception &e) {
    std::cerr << "testRunnerZeroes error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerZeroes error: unknown exception" << '\n';
    return 1;
}
