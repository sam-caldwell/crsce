/**
 * @file cmd/testRunnerZeros/main.cpp
 * @brief CLI entry for TestRunnerZeros; delegates to testrunner_zeros::cli::run().
 */
#include "testrunnerZeros/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>

int main() try {
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerZeros";
    return crsce::testrunner_zeros::cli::run(out_dir);
} catch (const std::exception &e) {
    std::cerr << "testRunnerZeros error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerZeros error: unknown exception" << '\n';
    return 1;
}
