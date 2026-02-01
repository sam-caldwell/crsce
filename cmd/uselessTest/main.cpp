/**
 * @file cmd/uselessTest/main.cpp
 * @brief Compresses and then decompresses docs/testData/useless-machine.mp4 using CRSCE tools.
 */

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>
#include <cstdlib>

#include "testRunnerRandom/detail/sha512.h"
#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

int main() try {
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path src_path = root / "docs/testData/useless-machine.mp4";
    const fs::path out_dir = bin_dir / "uselessTest";
    const fs::path cx_path = out_dir / "useless-machine.crsce";
    const fs::path dx_path = out_dir / "useless-machine.mp4";

    // Ensure output directory exists; remove conflicting file if present
    if (fs::exists(out_dir) && !fs::is_directory(out_dir)) {
        std::error_code ec_rm; fs::remove(out_dir, ec_rm);
    }
    std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
    if (ec_mk) { std::cerr << "failed to create output dir" << std::endl; return 1; }

    // Determine binary locations; allow override via TEST_BINARY_DIR
    fs::path bin_base = bin_dir;
    if (const char *tbd = std::getenv("TEST_BINARY_DIR"); tbd && *tbd) { // NOLINT(concurrency-mt-unsafe)
        bin_base = fs::path(tbd);
    }
    const fs::path cx_exe = bin_base / "compress";
    const fs::path dx_exe = bin_base / "decompress";

    // Compute input hash for logging and comparison
    const std::string input_hash = crsce::testrunner::detail::compute_sha512(src_path);

    // Compress
    {
        const std::vector<std::string> argv = { cx_exe.string(), "-in", src_path.string(), "-out", cx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        if (res.exit_code != 0) { std::cout << "failed" << std::endl; return 1; }
    }

    // Decompress
    std::string recon_hash;
    {
        const std::vector<std::string> argv = { dx_exe.string(), "-in", cx_path.string(), "-out", dx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        if (res.exit_code != 0) { std::cout << "failed" << std::endl; return 1; }
        recon_hash = crsce::testrunner::detail::compute_sha512(dx_path);
    }

    if (recon_hash == input_hash) {
        std::cout << "success" << std::endl;
        return 0;
    }
    std::cout << "failed" << std::endl;
    return 1;
} catch (const std::exception &e) {
    std::cout << "failed" << std::endl;
    return 1;
} catch (...) {
    std::cout << "failed" << std::endl;
    return 1;
}
