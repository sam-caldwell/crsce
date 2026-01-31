/**
 * @file process_case.cpp
 * @brief Implement shared compress/decompress/verify flow for a single test case.
 * @author Sam Caldwell
 */
#include "testrunner/Cli/detail/process_case.h"

#include "testrunner/Cli/detail/generated_input.h"
#include "testrunner/Cli/detail/compress_file.h"
#include "testrunner/Cli/detail/decompress_file.h"
#include "testrunner/Cli/detail/evaluate_hashes.h"
#include "testrunner/detail/now_ms.h"

#include <filesystem>
#include <string>
#include <cstdint>

namespace fs = std::filesystem;

namespace crsce::testrunner::cli {
    void process_case(const std::filesystem::path &out_dir,
                      const std::string &prefix,
                      const std::string &suffix,
                      const GeneratedInput &gi,
                      const std::int64_t compress_per_block_ms,
                      const std::int64_t decompress_per_block_ms) {
        const auto ts = crsce::testrunner::detail::now_ms();
        const std::string ts_s = std::to_string(static_cast<std::uint64_t>(ts));

        auto name_with_suffix = [&](const char *kind, const char *ext) -> fs::path {
            std::string name = prefix;
            name += "_";
            name += kind; // output or reconstructed
            name += "_";
            if (!suffix.empty()) {
                name += suffix;
                name += "_";
            }
            name += ts_s;
            name += ext;
            return out_dir / name;
        };

        const fs::path cx_path = name_with_suffix("output", ".crsce");
        const fs::path dx_path = name_with_suffix("reconstructed", ".bin");

        const std::int64_t cx_timeout_ms = static_cast<std::int64_t>(gi.blocks) * compress_per_block_ms;
        const std::int64_t dx_timeout_ms = static_cast<std::int64_t>(gi.blocks) * decompress_per_block_ms;

        (void)compress_file(gi.path, cx_path, gi.sha512, cx_timeout_ms);
        const std::string out_sha = decompress_file(cx_path, dx_path, gi.sha512, dx_timeout_ms);
        evaluate_hashes(gi.sha512, out_sha);
    }
}
