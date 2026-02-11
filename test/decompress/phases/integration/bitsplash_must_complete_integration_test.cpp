/**
 * @file bitsplash_must_complete_integration_test.cpp
 * @brief Integration: run compress+decompress on docs/testData/useless-machine.mp4 and expect BitSplash success.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <optional>
#include <iterator>
#include <system_error>
#include <cstddef>
#include <chrono>

#include "testRunnerRandom/detail/run_process.h"
#include "testRunnerRandom/detail/resolve_exe.h"

namespace fs = std::filesystem;

namespace {
    // Minimal JSON tail extractor (last object) for tiny logs
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

TEST(BitSplashIntegration, CompletesOnUselessMachine) {
    const std::string cx_exe = crsce::testrunner::detail::resolve_exe("compress");
    const std::string dx_exe = crsce::testrunner::detail::resolve_exe("decompress");
    const fs::path repo_root = fs::path(TEST_BINARY_DIR).parent_path().parent_path();
    const fs::path src = repo_root / "docs/testData/useless-machine.mp4";
    ASSERT_TRUE(fs::exists(src)) << "missing test asset: " << src.string();
    const fs::path out_dir = fs::path(TEST_BINARY_DIR).parent_path() / "uselessBitsplash";
    const auto uniq = [](){ using namespace std::chrono; return std::to_string(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()); }();
    const fs::path cx_path = out_dir / ("useless-machine-" + uniq + ".crsce");
    const fs::path dx_path = out_dir / ("useless-machine-" + uniq + ".mp4");
    std::error_code ec; fs::remove_all(out_dir, ec); fs::create_directories(out_dir, ec);

    // Compress (ensure a clean slate)
    fs::remove(cx_path, ec);
    {
        const std::vector<std::string> argv = { cx_exe, "-in", src.string(), "-out", cx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        ASSERT_EQ(res.exit_code, 0) << res.out << "\n" << res.err;
    }
    // Decompress
    fs::remove(dx_path, ec);
    {
        const std::vector<std::string> argv = { dx_exe, "-in", cx_path.string(), "-out", dx_path.string() };
        const auto res = crsce::testrunner::detail::run_process(argv, std::nullopt);
        (void)res;
        // Parse and assert BitSplash success
        const fs::path rowlog = out_dir / "completion_stats.log";
        ASSERT_TRUE(fs::exists(rowlog));
        const auto last = load_last_object(rowlog);
        ASSERT_TRUE(last.has_value()) << "no completion_stats record";
        const std::string &obj = *last;
        ASSERT_NE(obj.find("\"bitsplash_status\":3"), std::string::npos)
            << "BitSplash did not complete. Last record:\n" << obj;
    }
}
