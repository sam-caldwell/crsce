/**
 * @file cmd/uselessTest/main.cpp
 * @brief Compresses and then decompresses docs/testData/useless-machine.mp4 using CRSCE tools.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <fstream>

#include "testRunnerRandom/detail/sha512.h"
#include "testRunnerRandom/detail/run_process.h"
#include "common/Util/ValidateContainer.h"
#include "testRunnerRandom/detail/resolve_exe.h"
#include "common/Util/detail/watchdog.h"

namespace fs = std::filesystem;

int main() try {
    // Arm process watchdog
    crsce::common::util::detail::watchdog();
    std::cout << "Starting uselessTest...\n";
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path repo_root = fs::path(CRSCE_BIN_DIR).parent_path();
    const fs::path src_path = repo_root / "docs/testData/useless-machine.mp4";
    const fs::path build_root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = build_root / "uselessTest";
    const fs::path cx_path = out_dir / "useless-machine.crsce";
    const fs::path dx_path = out_dir / "useless-machine.mp4";

    // Ensure output directory exists; remove conflicting file if present
    if (fs::exists(out_dir) && !fs::is_directory(out_dir)) {
        std::error_code ec_rm; fs::remove(out_dir, ec_rm);
    }
    std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);
    if (ec_mk) { std::cerr << "failed to create output dir\n"; return 1; }
    // Clean up any stale artifacts from a prior run to avoid CLI preflight failures
    {
        std::error_code ec;
        if (fs::exists(cx_path)) { fs::remove(cx_path, ec); }
        if (fs::exists(dx_path)) { fs::remove(dx_path, ec); }
    }

    // Always announce useful locations up-front for debugging
    std::cout << "USL_LOG_DIR=" << out_dir.string() << "\n";

    // Determine binary locations: prefer TEST_BINARY_DIR (wrappers), then repo bin
    const std::string cx_exe = crsce::testrunner::detail::resolve_exe("compress");
    const std::string dx_exe = crsce::testrunner::detail::resolve_exe("decompress");
    bool using_wrappers = false;
    if (const char *w = std::getenv("CRSCE_WRAPPED_BINARIES"); w && *w) { // NOLINT(concurrency-mt-unsafe)
        const std::string v = w;
        using_wrappers = (v != "0");
    }

    std::cout << "USL_COMPRESS_EXE=" << cx_exe << "\n";
    std::cout << "USL_DECOMPRESS_EXE=" << dx_exe << "\n";
    std::cout << "USL_SRC=" << src_path.string() << "\n";
    std::cout << "USL_CONTAINER=" << cx_path.string() << "\n";
    std::cout << "USL_RECON=" << dx_path.string() << "\n";
    std::cout << "USL_WRAPPED=" << (using_wrappers ? 1 : 0) << "\n";

    // Compute input hash for logging and comparison
    const std::string input_hash = crsce::testrunner::detail::compute_sha512(src_path);

    // Helper for writing small log files
    auto write_text = [](const fs::path &p, const std::string &text) {
        std::ofstream os(p, std::ios::binary);
        if (!os.good()) { return false; }
        os.write(text.data(), static_cast<std::streamsize>(text.size()));
        return os.good();
    };

    // Compress
    {
        std::cout << "Starting compress phase...\n";
        const std::vector<std::string> argv = { cx_exe, "-in", src_path.string(), "-out", cx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        if (res.exit_code != 0) {
            std::ostringstream cmd;
            for (std::size_t i = 0; i < argv.size(); ++i) {
                if (i) { cmd << ' '; }
                cmd << argv[i];
            }
            const fs::path so = out_dir / "compress.stdout.txt";
            const fs::path se = out_dir / "compress.stderr.txt";
            (void)write_text(so, res.out);
            (void)write_text(se, res.err);
            std::cout << "USL_COMPRESS_CMD=" << cmd.str() << "\n";
            std::cout << "USL_COMPRESS_EXIT=" << res.exit_code << "\n";
            std::cout << "USL_COMPRESS_STDOUT=" << so.string() << "\n";
            std::cout << "USL_COMPRESS_STDERR=" << se.string() << "\n";
            std::cout << "failed\n";
            return 2;
        }
    }

    // Validate CRSCE container format per specification
    {
        std::cout << "Validating container format...\n";
        if (using_wrappers) {
            std::cout << "USL_CONTAINER_VALIDATION=skipped\n";
            std::cout << "USL_CONTAINER_REASON=wrapped-binaries\n";
        } else {
            std::string why;
            if (!crsce::common::util::validate_container(cx_path, why)) {
                std::cerr << "uselessTest: container validation failed: " << why << "\n";
                std::cout << "USL_CONTAINER_VALIDATION=fail\n";
                std::cout << "USL_CONTAINER_REASON=" << why << "\n";
                std::cout << "failed\n";
                return 2;
            }
            std::cout << "USL_CONTAINER_VALIDATION=ok\n";
        }
    }

    // Decompress
    std::string recon_hash;
    {
        std::cout << "Starting decompress phase...\n";
        const std::vector<std::string> argv = { dx_exe, "-in", cx_path.string(), "-out", dx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        std::cout << "USL_DECOMPRESS_EXIT=" << res.exit_code << "\n";
        if (res.exit_code != 0) {
            std::ostringstream cmd;
            for (std::size_t i = 0; i < argv.size(); ++i) {
                if (i) { cmd << ' '; }
                cmd << argv[i];
            }
            const fs::path so = out_dir / "decompress.stdout.txt";
            const fs::path se = out_dir / "decompress.stderr.txt";
            (void)write_text(so, res.out);
            (void)write_text(se, res.err);
            std::cout << "USL_DECOMPRESS_CMD=" << cmd.str() << "\n";
            std::cout << "USL_DECOMPRESS_EXIT=" << res.exit_code << "\n";
            std::cout << "USL_DECOMPRESS_STDOUT=" << so.string() << "\n";
            std::cout << "USL_DECOMPRESS_STDERR=" << se.string() << "\n";
            // Extract row completion log path if present in stdout
            {
                const std::string key = "ROW_COMPLETION_LOG=";
                const auto p = res.out.find(key);
                if (p != std::string::npos) {
                    const auto eol = res.out.find('\n', p);
                    const auto start = p + key.size();
                    const std::string path = (eol == std::string::npos) ? res.out.substr(start) : res.out.substr(start, eol - start);
                    if (!path.empty()) {
                        std::cout << "USL_ROW_COMPLETION_LOG=" << path << "\n";
                    }
                }
            }
            std::cout << "failed\n";
            return 2;
        }
        recon_hash = crsce::testrunner::detail::compute_sha512(dx_path);
    }

    if (recon_hash == input_hash) {
        std::cout << "success\n";
        return 0;
    }
    std::cout << "USL_INPUT_HASH=" << input_hash << "\n";
    std::cout << "USL_RECON_HASH=" << recon_hash << "\n";
    std::cout << "failed\n";
    return 2;
} catch (const std::exception &e) {
    std::cout << "USL_EXCEPTION=" << e.what() << "\n";
    std::cout << "failed\n";
    return 2;
} catch (...) {
    std::cout << "USL_EXCEPTION=unknown\n";
    std::cout << "failed\n";
    return 2;
}
