/**
 * @file run_process.cpp
 * @brief Subprocess execution with capture using std::system and temp files.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testrunner/detail/run_process.h"
#include "testrunner/detail/proc_result.h"
#include "testrunner/detail/read_file_text.h"
#include "testrunner/detail/now_ms.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sstream>
#include <vector>
#include <optional>
#include <cstdint>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {
    /**
     * @name run_process
     * @brief Execute a command, capture stdout/stderr to temp files, and return results.
     * @param argv Executable and arguments vector.
     * @param timeout_ms Optional timeout (unused in this implementation).
     * @return ProcResult with exit status, timings, and captured output.
     */
    ProcResult run_process(const std::vector<std::string> &argv, std::optional<std::int64_t> timeout_ms) {
        ProcResult res{};
        (void)timeout_ms; // timeout not enforced in this implementation
        if (argv.empty()) { res.exit_code = -1; return res; }
        const auto ts = now_ms();
        const fs::path tmp_dir = fs::path(".testrunner_");
        std::error_code ec_dir; fs::create_directories(tmp_dir, ec_dir);
        const fs::path out_tmp = tmp_dir / (std::to_string(ts) + ".stdout.txt");
        const fs::path err_tmp = tmp_dir / (std::to_string(ts) + ".stderr.txt");

    #ifdef _WIN32
        std::ostringstream full;
        for (std::size_t i = 0; i < argv.size(); ++i) {
            if (i) { full << ' '; }
            full << '"';
            for (const char ch: argv[i]) {
                if (ch == '"') { full << "\\\""; } else { full << ch; }
            }
            full << '"';
        }
        // Redirect stdout/stderr to temp files with quoting
        full << " 1>\"" << out_tmp.string() << "\" 2>\"" << err_tmp.string() << "\"";
        const std::string exe = std::string("cmd /c ") + full.str();
        res.start_ms = now_ms();
        const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
        res.end_ms = now_ms();
        res.exit_code = rc;
    #else
        std::ostringstream full;
        for (std::size_t i = 0; i < argv.size(); ++i) {
            if (i) { full << ' '; }
            full << '\'';
            for (const char ch: argv[i]) {
                if (ch == '\'') { full << "'\\''"; } else { full << ch; }
            }
            full << '\'';
        }
        // Redirect stdout/stderr to temp files with quoting
        full << " 1>'" << out_tmp.string() << "' 2>'" << err_tmp.string() << "'";
        const std::string exe = full.str();
        res.start_ms = now_ms();
        const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
        res.end_ms = now_ms();
        res.exit_code = rc;
    #endif
        res.out = read_file_text(out_tmp);
        res.err = read_file_text(err_tmp);
        std::error_code ec; fs::remove(out_tmp, ec); fs::remove(err_tmp, ec);
        return res;
    }
}
