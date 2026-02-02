/**
 * @file exe_path.h
 * @brief Resolve path to a repo-built executable in a future-proof way for tests.
 */
#pragma once

#include <filesystem>
#include <string>
#include <cstdlib>

namespace crsce::test {
    inline std::string exe_path(const std::string &name) {
        namespace fs = std::filesystem;
        // Prefer hard-coded repo bin if available (compile-time define propagated via crsce_static)
#ifdef CRSCE_BIN_DIR
        {
            const fs::path hard{CRSCE_BIN_DIR};
            const fs::path p = hard / name;
            if (fs::exists(p)) {
                return p.string();
            }
        }
#endif
        // Try TEST_BINARY_DIR based layouts provided by CTest presets
        if (const char *tbd = std::getenv("TEST_BINARY_DIR"); tbd && *tbd) { // NOLINT(concurrency-mt-unsafe)
            const fs::path base{tbd};
            const fs::path p1 = base.parent_path().parent_path() / "bin" / name; // repo_root/bin
            const fs::path p2 = base / "bin" / name;
            const fs::path p3 = base / name;
            if (fs::exists(p1)) {
                return p1.string();
            }
            if (fs::exists(p2)) {
                return p2.string();
            }
            if (fs::exists(p3)) {
                return p3.string();
            }
        }
        // Fallback to current working directory patterns and to PATH
        const fs::path cwd = fs::current_path();
        const fs::path c1 = cwd / "bin" / name;
        const fs::path c2 = cwd.parent_path() / "bin" / name;
        if (fs::exists(c1)) {
            return c1.string();
        }
        if (fs::exists(c2)) {
            return c2.string();
        }
        return name; // PATH
    }
}
