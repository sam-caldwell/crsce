/**
 * @file cmd/testRunnerRandom/main.cpp
 * @brief CLI entry for TestRunnerRandom; delegates to testrunner::cli::run().
 */
#include "testrunner/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>

/**
 * @brief Program entry point for testRunnerRandom CLI.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerRandom";
    return crsce::testrunner::cli::run(out_dir);
} catch (const std::exception &e) {
    std::cerr << "testRunnerRandom error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerRandom error: unknown exception" << '\n';
    return 1;
}
