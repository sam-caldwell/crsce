/**
 * @file cmd/testRunnerRandom/main.cpp
 * @brief End-to-end random test runner: generate random input, hash with SHA-512,
 *        run external compress/decompress binaries with timing, validate hashes,
 *        and emit structured JSON log lines for each step.
 *
 * Steps:
 *  1) Create a random input file sized in [32 KiB, 1 GiB].
 *  2) Compute SHA-512 of the input and log a {"step":"hashInput",...} record.
 *  3) Execute the "compress" binary to produce a container; log timing and metadata.
 *  4) Execute the "decompress" binary to reconstruct bytes; log timing and metadata.
 *  5) Compare input vs. reconstructed SHA-512; throw on mismatch.
 *  6) Throw on non-zero exit codes from compress/decompress.
 *  7) Enforce 1s/block timeout for compress (based on input size).
 *  8) Enforce 2s/block timeout for decompress (based on input size).
 *  9) Catch exceptions in main(), preserving input/compressed/decompressed files and
 *     captured stdout/stderr from invoked programs.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <algorithm>
#include <cerrno>
#include <iomanip>
#include <vector>
#include <system_error>

// No platform-specific includes required; we use std::system for execution.

namespace fs = std::filesystem;

namespace { // internal linkage for helpers and exceptions

// --- Exceptions ---
struct possibleCollisionException : public std::runtime_error { using std::runtime_error::runtime_error; };
struct compressNonZeroExitException : public std::runtime_error { using std::runtime_error::runtime_error; };
struct decompressNonZeroExitException : public std::runtime_error { using std::runtime_error::runtime_error; };
struct compressTimeoutException : public std::runtime_error { using std::runtime_error::runtime_error; };
struct decompressTimeoutException : public std::runtime_error { using std::runtime_error::runtime_error; };

// --- Time helpers ---
std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// --- JSON helpers ---
std::string json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (const unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    out += oss.str();
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

// --- I/O helpers ---
bool write_file_text(const fs::path &p, const std::string &content) {
    std::ofstream os(p, std::ios::binary);
    if (!os.is_open()) {
        return false;
    }
    os.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(os);
}

// Write a file of random bytes using chunked generation to avoid large allocations
void write_random_file(const fs::path &p, std::uint64_t bytes, std::mt19937_64 &rng) {
    std::ofstream os(p, std::ios::binary);
    if (!os) {
        throw std::runtime_error("failed to open input file for writing: " + p.string());
    }
    std::uniform_int_distribution<int> dist8(0, 255);
    constexpr std::size_t kChunk = 1U << 20U; // 1 MiB
    std::vector<char> buf(kChunk);
    std::uint64_t remaining = bytes;
    while (remaining > 0) {
        const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, kChunk));
        // Fill buffer with byte RNG
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = static_cast<char>(dist8(rng));
        }
        os.write(buf.data(), static_cast<std::streamsize>(n));
        if (!os) {
            throw std::runtime_error("failed writing random input to: " + p.string());
        }
        remaining -= n;
    }
}

// Compute approximate blocks from input byte length based on 511*511 bits per block
std::uint64_t blocks_for_input_bytes(std::uint64_t input_bytes) {
    constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;
    const std::uint64_t bits = input_bytes * 8ULL;
    return (bits + kBitsPerBlock - 1ULL) / kBitsPerBlock; // ceil(bits / kBitsPerBlock)
}

// --- Process execution with output capture and optional timeout ---
struct ProcResult {
    int exit_code{0};
    bool timed_out{false};
    std::int64_t start_ms{0};
    std::int64_t end_ms{0};
    std::string out;
    std::string err;
};

// Read a whole text file into a string (returns empty if missing)
std::string read_file_text(const fs::path &p) {
    std::ifstream is(p, std::ios::binary); // NOLINT(misc-const-correctness)
    if (!is) {
        return {};
    }
    std::ostringstream ss;
    ss << is.rdbuf();
    return ss.str();
}

// Minimal argument quoting for shell execution
std::string quote_arg(const std::string &s) {
#ifdef _WIN32
    std::string out = "\"";
    for (const char ch : s) { out += (ch == '"') ? std::string("\\\"") : std::string(1, ch); }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (const char ch : s) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out += "'";
    return out;
#endif
}

ProcResult run_process(const std::vector<std::string> &argv, std::optional<std::int64_t> /*timeout_ms*/) {
    ProcResult res{};
    if (argv.empty()) {
        res.exit_code = -1;
        return res;
    }
    // Unique temp files for capture
    const auto ts = now_ms();
    const fs::path out_tmp = fs::path(".testrunner_") += std::to_string(static_cast<std::uint64_t>(ts)) + ".stdout.txt";
    const fs::path err_tmp = fs::path(".testrunner_") += std::to_string(static_cast<std::uint64_t>(ts)) + ".stderr.txt";

#ifdef _WIN32
    // Use cmd.exe /c to ensure redirection works
    std::ostringstream full;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) { full << ' '; }
        full << quote_arg(argv[i]);
    }
    full << " 1>" << quote_arg(out_tmp.string()) << " 2>" << quote_arg(err_tmp.string());
    const std::string exe = std::string("cmd /c ") + full.str();
    res.start_ms = now_ms();
    const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
    res.end_ms = now_ms();
    res.exit_code = rc;
#else
    std::ostringstream full;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) { full << ' '; }
        full << quote_arg(argv[i]);
    }
    full << " 1>" << quote_arg(out_tmp.string()) << " 2>" << quote_arg(err_tmp.string());
    const std::string exe = full.str();
    res.start_ms = now_ms();
    const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
    res.end_ms = now_ms();
    res.exit_code = rc;
#endif
    res.out = read_file_text(out_tmp);
    res.err = read_file_text(err_tmp);
    // Cleanup temp files
    std::error_code ec;
    fs::remove(out_tmp, ec);
    fs::remove(err_tmp, ec);
    return res;
}

// Try to compute SHA-512 using common system utilities (sha512sum or shasum -a 512)
std::string compute_sha512(const fs::path &file) {
    // Prefer sha512sum; fallback to shasum -a 512
    const std::vector<std::vector<std::string> > cmds = {
        {"sha512sum", file.string()},
        {"shasum", "-a", "512", file.string()},
    };
    for (const auto &cmd : cmds) {
        try {
            const auto res = run_process(cmd, std::nullopt);
            if (res.exit_code == 0 && !res.out.empty()) {
                // Parse first hex token
                std::string first;
                for (const char ch : res.out) {
                    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '*') { break; }
                    first.push_back(ch);
                }
                if (!first.empty()) {
                    return first;
                }
            }
        } catch (...) {
            // try next
        }
    }
    throw std::runtime_error("failed to compute SHA-512; neither sha512sum nor shasum found");
}

std::string to_string_u64(std::uint64_t v) {
    return std::to_string(v);
}

std::uint64_t getenv_u64(const char *name, std::uint64_t fallback) {
    if (const char *s = std::getenv(name); s && *s) { // NOLINT(concurrency-mt-unsafe)
        try { return static_cast<std::uint64_t>(std::stoull(s)); }
        catch (...) { return fallback; }
    }
    return fallback;
}

} // namespace

int main() try {
    // Random engine
    std::random_device rd;
    std::mt19937_64 rng(rd());

    // Choose size in [32KiB, 1GiB] (overridable via env vars)
    const std::uint64_t kMin = std::max<std::uint64_t>(32ULL * 1024ULL, getenv_u64("CRSCE_TESTRUNNER_MIN_BYTES", 32ULL * 1024ULL));
    const std::uint64_t kMax = std::max<std::uint64_t>(kMin, getenv_u64("CRSCE_TESTRUNNER_MAX_BYTES", 1ULL * 1024ULL * 1024ULL * 1024ULL));
    std::uniform_int_distribution<std::uint64_t> dist(kMin, kMax);
    const std::uint64_t in_bytes = dist(rng);

    // Derive filenames with timestamp suffix
    const std::int64_t ts = now_ms();
    const fs::path in_path = fs::path("random_input_") += to_string_u64(static_cast<std::uint64_t>(ts)) + ".bin";
    const fs::path cx_path = fs::path("random_output_") += to_string_u64(static_cast<std::uint64_t>(ts)) + ".crsce";
    const fs::path dx_path = fs::path("random_reconstructed_") += to_string_u64(static_cast<std::uint64_t>(ts)) + ".bin";

    // Determine blocks for timeout policy
    const std::uint64_t nblocks = std::max<std::uint64_t>(1, blocks_for_input_bytes(in_bytes));
    const std::int64_t compress_timeout_ms = static_cast<std::int64_t>(nblocks) * 1000;
    const std::int64_t decompress_timeout_ms = static_cast<std::int64_t>(nblocks) * 2000;

    // 1) Write random input file
    write_random_file(in_path, in_bytes, rng);

    // 2) SHA-512 of input
    const std::int64_t hash_start = now_ms();
    const std::string input_sha512 = compute_sha512(in_path);
    const std::int64_t hash_end = now_ms();
    {
        std::cout << "{\n"
                  << "  \"step\":\"hashInput\",\n"
                  << "  \"start\":" << hash_start << ",\n"
                  << "  \"end\":" << hash_end << ",\n"
                  << "  \"input\":\"" << json_escape(in_path.string()) << "\",\n"
                  << "  \"hash\":\"" << input_sha512 << "\"\n"
                  << "}\n";
        std::cout.flush();
    }

    // 3) Run compress: compress -in <in_path> -out <cx_path>
    ProcResult cx_res{};
    try {
        cx_res = run_process({"compress", "-in", in_path.string(), "-out", cx_path.string()}, compress_timeout_ms);
    } catch (const std::exception &e) {
        // Preserve what we can and rethrow
        write_file_text(fs::path(cx_path).replace_extension(".compress.stdout.txt"), cx_res.out);
        write_file_text(fs::path(cx_path).replace_extension(".compress.stderr.txt"), cx_res.err);
        throw;
    }
    // Persist child output logs regardless of outcome
    write_file_text(fs::path(cx_path).replace_extension(".compress.stdout.txt"), cx_res.out);
    write_file_text(fs::path(cx_path).replace_extension(".compress.stderr.txt"), cx_res.err);

    {
        const auto elapsed = cx_res.end_ms - cx_res.start_ms;
        std::cout << "{\n"
                  << "  \"step\":\"compress\",\n"
                  << "  \"start\":" << cx_res.start_ms << ",\n"
                  << "  \"end\":" << cx_res.end_ms << ",\n"
                  << "  \"input\":\"" << json_escape(in_path.string()) << "\",\n"
                  << "  \"hash\":\"" << input_sha512 << "\",\n"
                  << "  \"output\":\"" << json_escape(cx_path.string()) << "\",\n"
                  << "  \"compressTime\":" << elapsed << "\n"
                  << "}\n";
        std::cout.flush();
    }

    if ((cx_res.end_ms - cx_res.start_ms) > compress_timeout_ms) {
        throw compressTimeoutException("compress timed out (exceeded 1s per block)");
    }
    if (cx_res.exit_code != 0) {
        std::ostringstream oss; oss << "compress exited with code " << cx_res.exit_code;
        throw compressNonZeroExitException(oss.str());
    }

    // 4) Run decompress: decompress -in <cx_path> -out <dx_path>
    ProcResult dx_res{};
    try {
        dx_res = run_process({"decompress", "-in", cx_path.string(), "-out", dx_path.string()}, decompress_timeout_ms);
    } catch (const std::exception &e) {
        write_file_text(fs::path(cx_path).replace_extension(".decompress.stdout.txt"), dx_res.out);
        write_file_text(fs::path(cx_path).replace_extension(".decompress.stderr.txt"), dx_res.err);
        throw;
    }
    write_file_text(fs::path(cx_path).replace_extension(".decompress.stdout.txt"), dx_res.out);
    write_file_text(fs::path(cx_path).replace_extension(".decompress.stderr.txt"), dx_res.err);

    const std::string output_sha512 = compute_sha512(dx_path);
    {
        const auto elapsed = dx_res.end_ms - dx_res.start_ms;
        std::cout << "{\n"
                  << "  \"step\":\"decompress\",\n"
                  << "  \"start\":" << dx_res.start_ms << ",\n"
                  << "  \"end\":" << dx_res.end_ms << ",\n"
                  << "  \"input\":\"" << json_escape(cx_path.string()) << "\",\n"
                  << "  \"output\":\"" << json_escape(dx_path.string()) << "\",\n"
                  << "  \"decompressTime\":" << elapsed << ",\n"
                  << "  \"hash\":\"" << output_sha512 << "\"\n"
                  << "}\n";
        std::cout.flush();
    }

    if ((dx_res.end_ms - dx_res.start_ms) > decompress_timeout_ms) {
        throw decompressTimeoutException("decompress timed out (exceeded 2s per block)");
    }
    if (dx_res.exit_code != 0) {
        std::ostringstream oss; oss << "decompress exited with code " << dx_res.exit_code;
        throw decompressNonZeroExitException(oss.str());
    }

    // 5) Compare hashes
    if (output_sha512 != input_sha512) {
        throw possibleCollisionException("sha512 mismatch between input and decompressed output");
    }

    return 0;

} catch (const std::exception &e) {
    // Per requirement (9): catch any exceptions and preserve all artifacts.
    // No cleanup here; simply report and return non-zero.
    std::cerr << "testRunnerRandom error: " << e.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "testRunnerRandom error: unknown exception" << '\n';
    return 1;
}
