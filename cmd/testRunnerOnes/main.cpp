/**
 * @file cmd/testRunnerOnes/main.cpp
 * @author Sam Caldwell
 * @brief CLI entry for TestRunnerOnes; delegates to testrunner_ones::cli::run().
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "testrunnerOnes/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include <cstdlib>
#include "testRunnerRandom/detail/json_escape.h"
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"
#include "common/O11y/metric_i64.h"
#include "common/O11y/counter.h"
#include "common/O11y/event.h"
#include "common/Util/detail/watchdog.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"

using crsce::testrunner_random::cli::extract_exit_code;

int main() try {
    // Arm process watchdog (default 10 minutes)
    crsce::common::util::detail::watchdog();
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerOnes";
    const bool o11y_on = ([](){ if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */) { return (*p!='0'); } return false; })();
    if (o11y_on) {
        ::crsce::o11y::metric("testrunner_begin", 1LL, {{"runner", std::string("testRunnerOnes")}});
        ::crsce::o11y::counter("testrunner_runs_attempted");
    }
    const int rc = crsce::testrunner_ones::cli::run(out_dir);
    if (o11y_on) {
        if (rc == 0) { ::crsce::o11y::counter("testrunner_runs_success"); }
        else { ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerOnes")}, {"type", std::string("nonzero_rc")}}); ::crsce::o11y::counter("testrunner_runs_failed"); }
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerOnes")}, {"status", (rc==0?std::string("OK"):std::string("FAIL"))}});
    }
    return rc;
} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerOnes")}, {"type", std::string("decompress_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerOnes")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerOnes")}, {"type", std::string("compress_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerOnes")}, {"status", std::string("FAIL")}});
    }
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerOnes")}, {"type", std::string("std_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerOnes")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    if (const char *p = std::getenv("CRSCE_TESTRUNNER_O11Y") /* NOLINT(concurrency-mt-unsafe) */; p && *p && *p!='0') {
        ::crsce::o11y::event("testrunner_error", {{"runner", std::string("testRunnerOnes")}, {"type", std::string("unknown_exception")}});
        ::crsce::o11y::counter("testrunner_runs_failed");
        ::crsce::o11y::metric("testrunner_end", 1LL, {{"runner", std::string("testRunnerOnes")}, {"status", std::string("FAIL")}});
    }
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerOnes\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
