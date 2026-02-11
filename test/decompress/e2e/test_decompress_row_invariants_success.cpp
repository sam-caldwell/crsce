/**
 * @file test_decompress_row_invariants_success.cpp
 * @brief E2E: decompress real container and assert row-sum invariants (row_avg_pct==100, all rows==1).
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <optional>
#include <iterator>
#include <cstddef>
#include <vector>
#include <system_error>

#include "testRunnerRandom/detail/run_process.h"
#include "testRunnerRandom/detail/resolve_exe.h"

namespace fs = std::filesystem;

namespace {
    // Extract last JSON object from a small pretty-printed log file.
    std::optional<std::string> load_last_object(const fs::path &p) {
        std::ifstream is(p);
        if (!is.good()) { return std::nullopt; }
        std::string data((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        int depth = 0; bool in_str = false; bool esc=false; int start=-1; std::optional<std::string> last;
        for (std::size_t i=0;i<data.size();++i){ const char ch=data[i];
            if (in_str){ if(esc){esc=false;} else if(ch=='\\'){esc=true;} else if(ch=='"'){in_str=false;} }
            else { if (ch=='"'){in_str=true;} else if(ch=='{'){ if(depth==0) { start=static_cast<int>(i);} ++depth; }
                   else if(ch=='}'){ --depth; if(depth==0 && start>=0){ last = data.substr(static_cast<std::size_t>(start), i-static_cast<std::size_t>(start)+1U); start=-1; } } }
        }
        return last;
    }
}

TEST(DecompressE2E, RowSumsRemainIntactOnSuccess) {
    const std::string cx_exe = crsce::testrunner::detail::resolve_exe("compress");
    const std::string dx_exe = crsce::testrunner::detail::resolve_exe("decompress");

    const fs::path repo_root = fs::path(TEST_BINARY_DIR).parent_path().parent_path();
    const fs::path src = repo_root / "docs/testData/useless-machine.mp4";
    ASSERT_TRUE(fs::exists(src)) << "missing test asset: " << src.string();

    const fs::path out_dir = fs::path(TEST_BINARY_DIR).parent_path() / ".e2e_row_invariants";
    std::error_code ec; fs::remove_all(out_dir, ec); fs::create_directories(out_dir, ec);
    const fs::path cx_path = out_dir / "tmp.crsce";
    const fs::path dx_path = out_dir / "tmp.mp4";

    // Compress
    {
        const std::vector<std::string> argv = { cx_exe, "-in", src.string(), "-out", cx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        ASSERT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;
    }
    // Decompress
    {
        const std::vector<std::string> argv = { dx_exe, "-in", cx_path.string(), "-out", dx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        ASSERT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;
        const fs::path rowlog = out_dir / "completion_stats.log";
        ASSERT_TRUE(fs::exists(rowlog));
        const auto last = load_last_object(rowlog);
        ASSERT_TRUE(last.has_value()) << "no completion_stats record";
        const std::string &obj = *last;
        // row_avg_pct should be exactly 100.0 on success
        ASSERT_NE(obj.find("\"row_avg_pct\":100"), std::string::npos)
            << "row_avg_pct not 100 in success record:\n" << obj;
        // rows array must contain only 1s
        const auto p = obj.find("\"rows\":[");
        ASSERT_NE(p, std::string::npos) << obj;
        const auto q = obj.find(']', p);
        ASSERT_NE(q, std::string::npos) << obj;
        const std::string rows = obj.substr(p, q - p);
        ASSERT_EQ(rows.find('0'), std::string::npos) << "rows contain 0 in success record:\n" << obj;
    }
}
