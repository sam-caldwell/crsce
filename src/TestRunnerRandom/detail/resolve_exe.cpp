/**
 * @file resolve_exe.cpp
 * @brief Resolve path to an executable across $CRSCE_BIN_DIR, $TEST_BINARY_DIR, and common bin layouts.
 */
#include "testRunnerRandom/detail/resolve_exe.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    bool is_exe_candidate(const fs::path &p) {
        std::error_code ec;
        return fs::exists(p, ec) && fs::is_regular_file(p, ec);
    }
}

namespace crsce::testrunner::detail {

    std::string resolve_exe(const std::string &exe_name) {
        // 1) TEST_BINARY_DIR variants (permits tests to inject wrappers)
        if (const char *tbd = std::getenv("TEST_BINARY_DIR"); tbd && *tbd) { // NOLINT(concurrency-mt-unsafe)
            const fs::path base{tbd};
            const fs::path c1 = base / exe_name;
            if (is_exe_candidate(c1)) { return c1.string(); }
            const fs::path c2 = base / "bin" / exe_name;
            if (is_exe_candidate(c2)) { return c2.string(); }
            const fs::path c3 = base.parent_path() / "bin" / exe_name;
            if (is_exe_candidate(c3)) { return c3.string(); }
            const fs::path c4 = base.parent_path().parent_path() / "bin" / exe_name;
            if (is_exe_candidate(c4)) { return c4.string(); }
        }
        // 2) Hard-coded project bin directory
#ifdef CRSCE_BIN_DIR
        {
            const fs::path hard{CRSCE_BIN_DIR};
            const fs::path p = hard / exe_name;
            if (is_exe_candidate(p)) { return p.string(); }
        }
#endif
        // 3) CWD-relative
        const fs::path cwd = fs::current_path();
        const std::vector<fs::path> tries = {
            cwd / exe_name,
            cwd / "bin" / exe_name,
            cwd.parent_path() / "bin" / exe_name,
            cwd.parent_path().parent_path() / "bin" / exe_name,
        };
        for (const auto &p: tries) {
            if (is_exe_candidate(p)) { return p.string(); }
        }
        // 4) Fallback to name
        return exe_name;
    }
}
