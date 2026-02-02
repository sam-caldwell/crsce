/**
 * @file resolve_exe.cpp
 * @brief Resolve path to an executable across $CRSCE_BIN_DIR, $TEST_BINARY_DIR, and common bin layouts.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "testRunnerRandom/detail/resolve_exe.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

namespace crsce::testrunner::detail {

    /**
     * @name resolve_exe
     * @brief Resolve an executable name to a concrete path, trying common locations.
     * @param exe_name Executable file name to resolve.
     * @return Resolved path string or the original name for PATH lookup.
     */
    std::string resolve_exe(const std::string &exe_name) {
        // 1) TEST_BINARY_DIR variants (permits tests to inject wrappers)
        if (const char *tbd = std::getenv("TEST_BINARY_DIR"); tbd && *tbd) { // NOLINT(concurrency-mt-unsafe)
            const fs::path base{tbd};
            const fs::path c1 = base / exe_name;
            {
                std::error_code ec;
                if (fs::exists(c1, ec) && fs::is_regular_file(c1, ec)) { return c1.string(); }
            }
            const fs::path c2 = base / "bin" / exe_name;
            {
                std::error_code ec;
                if (fs::exists(c2, ec) && fs::is_regular_file(c2, ec)) { return c2.string(); }
            }
            const fs::path c3 = base.parent_path() / "bin" / exe_name;
            {
                std::error_code ec;
                if (fs::exists(c3, ec) && fs::is_regular_file(c3, ec)) { return c3.string(); }
            }
            const fs::path c4 = base.parent_path().parent_path() / "bin" / exe_name;
            {
                std::error_code ec;
                if (fs::exists(c4, ec) && fs::is_regular_file(c4, ec)) { return c4.string(); }
            }
        }
        // 2) Hard-coded project bin directory
#ifdef CRSCE_BIN_DIR
        {
            const fs::path hard{CRSCE_BIN_DIR};
            const fs::path p = hard / exe_name;
            std::error_code ec;
            if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) { return p.string(); }
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
            std::error_code ec;
            if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) { return p.string(); }
        }
        // 4) Fallback to name
        return exe_name;
    }
}
