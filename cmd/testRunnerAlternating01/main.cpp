/**
 * @file cmd/testRunnerAlternating01/main.cpp
 * @author Sam Caldwell
 * @brief CLI entry for TestRunnerAlternating01; delegates to testrunner_alternating01::cli::run().
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for more information.
 */
#include "testrunnerAlternating01/Cli/detail/run_single.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include "testRunnerRandom/detail/json_escape.h"
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"
#include "common/O11y/metric.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"
#include "common/Util/detail/watchdog.h"

using crsce::testrunner_random::cli::extract_exit_code;

int main() try {
    // Arm process watchdog (default 10 minutes)
    crsce::common::util::detail::watchdog();
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

    const bool o11y_on = ([](){ if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */) { return (*p!='0'); } return false; })();
    if (o11y_on) {
        ::crsce::o11y::metric("testrunner_begin", 1LL, {{"runner", std::string("testRunnerAlternating01")}});
        ::crsce::o11y::counter("testrunner_runs_attempted");
    }
    for (const auto block_size : block_sizes) {
        const int rc = crsce::testrunner_alternating01::cli::run(out_dir, block_size);
        if (o11y_on) {
            if (rc == 0) { ::crsce::o11y::counter("testrunner_runs_success"); }
            else { ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerAlternating01")}, {"type", std::string("nonzero_rc")}}); ::crsce::o11y::counter("testrunner_runs_failed"); }
            ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerAlternating01")}, {"status", (rc==0?std::string("OK"):std::string("FAIL"))}});
        }
        return rc;
    }

} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerAlternating01")}, {"type", std::string("decompress_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerAlternating01")}, {"status", std::string("FAIL")}});
    }
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
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerAlternating01")}, {"type", std::string("compress_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerAlternating01")}, {"status", std::string("FAIL")}});
    }
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerAlternating01")}, {"type", std::string("std_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerAlternating01")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerAlternating01\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerAlternating01")}, {"type", std::string("unknown_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerAlternating01")}, {"status", std::string("FAIL")}});
    }
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
