/**
 * @file cmd/testCollisions/main.cpp
 * @brief Runs repeated compress/decompress cycles on random inputs to detect collisions and logs results.
 */

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstdlib>

#include "testRunnerRandom/detail/now_ms.h"
#include "testRunnerRandom/detail/sha512.h"
#include "testRunnerRandom/detail/write_random_file.h"
#include "testRunnerRandom/detail/run_process.h"

namespace fs = std::filesystem;

namespace {
    // Compute maximum byte count that does not exceed exactly n CRSCE blocks.
    inline std::uint64_t max_bytes_within_blocks(const std::uint64_t nblocks) {
        constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL; // 261121
        const std::uint64_t bits = nblocks * kBitsPerBlock;
        return bits / 8ULL; // floor
    }

    inline void write_header_if_new(std::ofstream &os) {
        // Header row (TSV) matching requested columns
        os << "cycle_start\t"
           << "cycle_end\t"
           << "random_filename\t"
           << "input_size\t"
           << "input_file_hash\t"
           << "compress_start\t"
           << "compress_end\t"
           << "compressed_file\t"
           << "compressed_hash\t"
           << "compressed_size\t"
           << "decompress_start\t"
           << "decompress_ends\t"
           << "reconstructed_hash\t"
           << "reconstructed_size\t"
           << "result"
           << '\n';
        os.flush();
    }
}

namespace {
    [[nodiscard]] inline int usage() {
        std::cerr
            << "Usage: testCollisions -min_blocks <int> -max_blocks <int> -cycles <int>\n"
            << "  -min_blocks: minimum number of blocks per file (required; min < max)\n"
            << "  -max_blocks: maximum number of blocks per file (required; min < max)\n"
            << "  -cycles    : number of files to process (required)\n";
        return 2;
    }
}

namespace crsce::testCollisions {
}

int main(int argc, char *argv[]) try {
    using crsce::testrunner::detail::now_ms;

    // Resolve output directory similar to other tools: <repo_root>/<tool>
    const fs::path bin_dir{TEST_BINARY_DIR};
    const fs::path out_dir = bin_dir / "testCollisions";
    // Ensure output directory exists; if a file is in the way, remove it.
    if (fs::exists(out_dir) && !fs::is_directory(out_dir)) {
        std::error_code ec_rm; fs::remove(out_dir, ec_rm);
    }
    std::error_code ec_mk; fs::create_directories(out_dir, ec_mk);

    // No process-env mutation here; child binaries are resolved via bin_base below.

    // Parse required args: -min_blocks -max_blocks -cycles
    int min_blocks = -1;
    int max_blocks = -1;
    int cycles = -1;
    const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
    for (std::size_t i = 1; i + 1 < args.size(); ++i) {
        const std::string key = args[i];
        const std::string val = args[i + 1];
        if (key == "-min_blocks") { min_blocks = std::stoi(val); ++i; }
        else if (key == "-max_blocks") { max_blocks = std::stoi(val); ++i; }
        else if (key == "-cycles") { cycles = std::stoi(val); ++i; }
    }
    if (min_blocks < 1 || max_blocks < 1 || cycles < 1 || !(min_blocks < max_blocks)) {
        return usage();
    }

    // Select a block count for this run (inclusive range)
    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<int> dist(min_blocks, max_blocks);
    const int number_of_blocks = dist(rng);

    // Prepare log file with start timestamp in name
    const auto run_start_ms = now_ms();
    const fs::path log_path = out_dir / (std::string("testCollisions_") + std::to_string(static_cast<std::uint64_t>(run_start_ms)) + ".log");
    const bool new_file = !fs::exists(log_path);
    std::ofstream log(log_path, std::ios::app);
    if (!log) {
        std::cerr << "failed to open log file: " << log_path << "\n";
        return 1;
    }
    if (new_file) { write_header_if_new(log); }

    // Precompute per-block timeouts similar to testRunnerRandom
    constexpr auto kCompressPerBlockMs = 1000ULL;
    constexpr auto kDecompressPerBlockMs = 20000ULL;

    // Resolve binaries from project root ./bin
    const fs::path root = bin_dir.has_parent_path() ? bin_dir.parent_path() : bin_dir;
    const fs::path compress_exe = root / "bin" / "compress";
    const fs::path decompress_exe = root / "bin" / "decompress";

    // Execute cycles
    for (int i = 0; i < cycles; ++i) {
        const auto cycle_start = now_ms();

        // Build unique base name
        const std::string ts = std::to_string(static_cast<std::uint64_t>(cycle_start));
        const std::string base = std::string("random_input_") + ts + "_" + std::to_string(i) + "_" + std::to_string(number_of_blocks);
        const fs::path in_path = out_dir / (base + ".bin");
        const fs::path cx_path = out_dir / (base + ".crsce");
        const fs::path dx_path = out_dir / (base + ".reconstructed");

        std::string input_hash;
        std::string compressed_hash;
        std::string recon_hash;
        std::uint64_t input_size = 0;
        std::uint64_t compressed_size = 0;
        std::uint64_t reconstructed_size = 0;
        std::string result = "fail"; // pessimistic default

        const auto input_bytes = max_bytes_within_blocks(static_cast<std::uint64_t>(number_of_blocks));
        // Legacy timeouts kept for context (not enforced here)

        std::int64_t cx_start = 0;
        std::int64_t cx_end = 0;
        std::int64_t dx_start = 0;
        std::int64_t dx_end = 0;

        try {
            // Generate random input file of size within block boundary
            crsce::testrunner::detail::write_random_file(in_path, input_bytes, rng);
            input_size = input_bytes;
            input_hash = crsce::testrunner::detail::compute_sha512(in_path);

            // Compress
            cx_start = now_ms();
            {
                const std::vector<std::string> argv = { compress_exe.string(), "-in", in_path.string(), "-out", cx_path.string() };
                const auto cx_res = crsce::testrunner::detail::run_process(argv, std::nullopt);
                if (cx_res.exit_code != 0) { throw std::runtime_error("compress failed"); }
            }
            cx_end = now_ms();

            // Hash compressed file
            {
                std::error_code ec_sz; const auto sz = fs::file_size(cx_path, ec_sz);
                if (!ec_sz) { compressed_size = static_cast<std::uint64_t>(sz); }
            }
            compressed_hash = crsce::testrunner::detail::compute_sha512(cx_path);

            // Decompress
            dx_start = now_ms();
            {
                const std::vector<std::string> argv = { decompress_exe.string(), "-in", cx_path.string(), "-out", dx_path.string() };
                const auto dx_res = crsce::testrunner::detail::run_process(argv, std::nullopt);
                if (dx_res.exit_code != 0) { throw std::runtime_error("decompress failed"); }
            }
            dx_end = now_ms();
            {
                std::error_code ec_sz; const auto sz = fs::file_size(dx_path, ec_sz);
                if (!ec_sz) { reconstructed_size = static_cast<std::uint64_t>(sz); }
            }
            recon_hash = crsce::testrunner::detail::compute_sha512(dx_path);
            // Compare
            if (recon_hash == input_hash) {
                result = "pass";
                // On pass, remove artifacts as requested
                std::error_code ec_rm; fs::remove(cx_path, ec_rm); fs::remove(dx_path, ec_rm);
            } else {
                result = "fail";
            }
        } catch (const std::exception &e) {
            // Leave result as 'fail'; emit minimal context to stderr for debugging.
            std::cerr << "cycle " << i << " error: " << e.what() << "\n";
            if (cx_end == 0 && cx_start != 0) { cx_end = now_ms(); }
            if (dx_end == 0 && dx_start != 0) { dx_end = now_ms(); }
        } catch (...) {
            std::cerr << "cycle " << i << " unknown error\n";
            if (cx_end == 0 && cx_start != 0) { cx_end = now_ms(); }
            if (dx_end == 0 && dx_start != 0) { dx_end = now_ms(); }
        }

        const auto cycle_end = now_ms();

        // Emit TSV log record
        log << cycle_start << '\t'
            << cycle_end << '\t'
            << in_path.string() << '\t'
            << input_size << '\t'
            << input_hash << '\t'
            << cx_start << '\t'
            << cx_end << '\t'
            << cx_path.string() << '\t'
            << compressed_hash << '\t'
            << compressed_size << '\t'
            << dx_start << '\t'
            << dx_end << '\t'
            << recon_hash << '\t'
            << reconstructed_size << '\t'
            << result
            << '\n';
        log.flush();
    }

    return 0;
} catch (const std::exception &e) {
    std::cerr << "testCollisions: " << e.what() << "\n";
    return 1;
} catch (...) {
    std::cerr << "testCollisions: unknown exception\n";
    return 1;
}
