/**
 * @file cmd/testRunnerRandom/main.cpp
 * @author Sam Caldwell
 * @brief CLI entry for TestRunnerRandom; delegates to testrunner::cli::run().
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "testRunnerRandom/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include <cstdint>
#include <span>
#include <cstddef>
#include <algorithm>
#include <cstdlib>
#include "common/O11y/metric_i64.h"
#include "common/O11y/counter.h"
#include "common/O11y/event.h"
#include "testRunnerRandom/detail/json_escape.h"
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"
#include "common/Util/detail/watchdog.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"

using crsce::testrunner_random::cli::extract_exit_code;

/**
 * @brief Program entry point for testRunnerRandom CLI.
 * @return Process exit code (0 on success, non-zero represents an error state).
 */
int main(const int argc, char *argv[]) try {
    // Arm process watchdog (default 10 minutes)
    crsce::common::util::detail::watchdog();

    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerRandom";


    // Optional args: --min-bytes <N> --max-bytes <N>
    // Defaults increased to 256 KiB for stronger GOBP exercise
    std::uint64_t min_bytes = 262144ULL;                        // 256 KiB
    std::uint64_t max_bytes = 262144ULL;                        // 256 KiB

    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (const std::string arg = args[i]; arg == "--min-bytes" && (i + 1) < args.size()) {
            min_bytes = static_cast<std::uint64_t>(std::stoull(args[++i]));
        } else if (arg == "--max-bytes" && (i + 1) < args.size()) {
            max_bytes = static_cast<std::uint64_t>(std::stoull(args[++i]));
        }
    }

    max_bytes = std::max(max_bytes, min_bytes);
    const bool o11y_on = ([](){ if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */) { return (*p!='0'); } return false; })();
    if (o11y_on) {
        ::crsce::o11y::metric("testrunner_begin", 1LL, {{"runner", std::string("testRunnerRandom")}});
        ::crsce::o11y::counter("testrunner_runs_attempted");
    }
    const int rc = crsce::testrunner_random::cli::run(out_dir, min_bytes, max_bytes);
    if (o11y_on) {
        if (rc == 0) { ::crsce::o11y::counter("testrunner_runs_success"); }
        else { ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerRandom")}, {"type", std::string("nonzero_rc")}});
               ::crsce::o11y::counter("testrunner_runs_failed"); }
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerRandom")}, {"status", (rc==0?std::string("OK"):std::string("FAIL"))}});
    }
    return rc;

} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerRandom")}, {"type", std::string("decompress_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerRandom")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerRandom")}, {"type", std::string("compress_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerRandom")}, {"status", std::string("FAIL")}});
    }
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerRandom")}, {"type", std::string("std_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerRandom")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerRandom")}, {"type", std::string("unknown_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerRandom")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerRandom\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
