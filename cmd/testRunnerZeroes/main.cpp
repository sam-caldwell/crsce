/**
 * @file cmd/testRunnerZeroes/main.cpp
 * @author Sam Caldwell
 * @brief CLI entry for TestRunnerZeroes; delegates to testrunner_zeroes::cli::run().
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
*/
#include "testrunnerZeroes/Cli/detail/run.h"
#include <exception>
#include <iostream>
#include <filesystem>
#include <string>
#include <cstdint>
#include <cstdlib>
#include "testRunnerRandom/detail/json_escape.h"
#include "common/O11y/O11y.h"
#include "testRunnerRandom/Cli/detail/extract_exit_code.h"
#include "common/Util/detail/watchdog.h"
#include "common/exceptions/DecompressNonZeroExitException.h"
#include "common/exceptions/CompressNonZeroExitException.h"
#include "common/O11y/O11y.h"

using crsce::testrunner_random::cli::extract_exit_code;

int main() try {
    ::crsce::o11y::O11y::instance().start();
    // Arm process watchdog (default 10 minutes)
    crsce::common::util::detail::watchdog();
    namespace fs = std::filesystem;
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path out_dir = root / "testRunnerZeroes";
    ::crsce::o11y::O11y::instance().metric("testrunner_begin", static_cast<std::int64_t>(1), {{"runner", std::string("testRunnerZeroes")}});
    ::crsce::o11y::O11y::instance().counter("testrunner_runs_attempted");
    const int rc = crsce::testrunner_zeroes::cli::run(out_dir);
    if (rc == 0) { ::crsce::o11y::O11y::instance().counter("testrunner_runs_success"); }
    else {
        ::crsce::o11y::O11y::instance().event("testrunner_error", {{"runner", std::string("testRunnerZeroes")}, {"type", std::string("nonzero_rc")}});
        ::crsce::o11y::O11y::instance().counter("testrunner_runs_failed");
    }
    ::crsce::o11y::O11y::instance().metric("testrunner_end", static_cast<std::int64_t>(1), {{"runner", std::string("testRunnerZeroes")}, {"status", (rc==0?std::string("OK"):std::string("FAIL"))}});
    return rc;
} catch (const crsce::common::exceptions::DecompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 4);
    ::crsce::o11y::O11y::instance().event("testrunner_error", {{"runner", std::string("testRunnerZeroes")}, {"type", std::string("decompress_exception")}});
    ::crsce::o11y::O11y::instance().counter("testrunner_runs_failed");
    ::crsce::o11y::O11y::instance().metric("testrunner_end", static_cast<std::int64_t>(1), {{"runner", std::string("testRunnerZeroes")}, {"status", std::string("FAIL")}});
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerZeroes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const crsce::common::exceptions::CompressNonZeroExitException &e) {
    const int code = extract_exit_code(e.what(), 3);
    ::crsce::o11y::O11y::instance().event("testrunner_error", {{"runner", std::string("testRunnerZeroes")}, {"type", std::string("compress_exception")}});
    ::crsce::o11y::O11y::instance().counter("testrunner_runs_failed");
    ::crsce::o11y::O11y::instance().metric("testrunner_end", static_cast<std::int64_t>(1), {{"runner", std::string("testRunnerZeroes")}, {"status", std::string("FAIL")}});
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerZeroes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\",\n"
              << "  \"exitCode\":" << code << "\n"
              << "}\n";
    std::cout.flush();
    return code;
} catch (const std::exception &e) {
    ::crsce::o11y::O11y::instance().event("testrunner_error", {{"runner", std::string("testRunnerZeroes")}, {"type", std::string("unknown_exception")}});
    ::crsce::o11y::O11y::instance().counter("testrunner_runs_failed");
    ::crsce::o11y::O11y::instance().metric("testrunner_end", static_cast<std::int64_t>(1), {{"runner", std::string("testRunnerZeroes")}, {"status", std::string("FAIL")}});
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerZeroes\",\n"
              << "  \"message\":\"" << crsce::testrunner::detail::json_escape(std::string(e.what())) << "\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
} catch (...) {
    std::cout << "{\n"
              << "  \"step\":\"error\",\n"
              << "  \"runner\":\"testRunnerZeroes\",\n"
              << "  \"message\":\"unknown exception\"\n"
              << "}\n";
    std::cout.flush();
    return 1;
}
