/**
 * @file plugin_runner.h
 * @brief Helper to build and run the OneDefinitionPerHeader plugin for tests.
 */
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <array>
#include <thread>
#include <chrono>
#include <fstream>
#ifndef _WIN32
#include <sys/wait.h>
#include <spawn.h>
#include <unistd.h>
extern char **environ; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

inline std::string run_command_capture(const std::string &cmd, int &exit_code) {
#ifdef _WIN32
    std::array<char, 8192> buffer{};
    std::string result;
    const std::string full = cmd + " 2>&1";
    FILE *pipe = _popen(full.c_str(), "r");
    if (!pipe) {
        exit_code = -1;
        return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    exit_code = _pclose(pipe);
    return result;
#else
    // Fallback shell-based execution (not used by clang++ path below)
    std::array<char, 8192> buffer{};
    std::string result;
    const std::string full = cmd + " 2>&1";
    FILE *pipe = popen(full.c_str(), "r");
    if (!pipe) {
        exit_code = -1;
        return "";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1;
    }
    return result;
#endif
}

#ifndef _WIN32
inline std::string run_exec_capture(const std::vector<std::string> &args, int &exit_code) {
    std::string result;
    std::array<int, 2> pipefd{};
    if (pipe(pipefd.data()) != 0) {
        exit_code = -1;
        return "";
    }
    posix_spawn_file_actions_t fa{};
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, pipefd[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, pipefd[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&fa, pipefd[0]);
    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &s: args) {
        argv.push_back(const_cast<char *>(s.c_str())); // NOLINT(cppcoreguidelines-pro-type-const-cast)
    }
    argv.push_back(nullptr);
    pid_t pid{};
    const int rc = posix_spawn(&pid, args[0].c_str(), &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    close(pipefd[1]);
    if (rc != 0) {
        close(pipefd[0]);
        exit_code = -1;
        return "";
    }
    std::array<char, 4096> buf{};
    ssize_t n = 0;
    while ((n = read(pipefd[0], buf.data(), buf.size())) > 0) {
        result.append(buf.data(), static_cast<size_t>(n));
    }
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1;
    }
    return result;
}
#endif

inline std::filesystem::path repo_root_from_build_cwd() {
    auto p = std::filesystem::current_path();
    for (int i = 0; i < 8; ++i) {
        if (std::filesystem::exists(p / "CMakePresets.json")) {
            return p;
        }
        if (p.has_parent_path()) {
            p = p.parent_path();
        } else {
            break;
        }
    }
    return p;
}

inline std::filesystem::path ensure_plugin_built(std::string &log) {
    auto repo = repo_root_from_build_cwd();
    auto build_root = std::filesystem::current_path();
    // If a library already exists, skip invoking deps.cmake
#ifdef __APPLE__
    const char *libname = "libOneDefinitionPerHeader.dylib";
#else
#ifdef _WIN32
    const char *libname = "OneDefinitionPerHeader.dll";
#else
    const char *libname = "libOneDefinitionPerHeader.so";
#endif
#endif
    auto libpath = build_root / "tools/clang-plugins/OneDefinitionPerHeader" / libname;
    if (std::filesystem::exists(libpath)) {
        return libpath;
    }
    // Simple cross-process lock to avoid concurrent deps.cmake runs under CTest -j
    const auto lock = build_root / ".deps.lock";
    for (int i = 0; i < 300; ++i) {
        if (!std::filesystem::exists(lock)) {
            if (std::ofstream f(lock, std::ios::out | std::ios::trunc); f.is_open()) {
                f.close();
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    int code = 0;
    const std::string cmd = std::string("cmake -D SOURCE_DIR=") + repo.string() +
                            " -D BUILD_DIR=" + build_root.string() +
                            " -DONLY_GROUP=clang-plugins/OneDefinitionPerHeader" +
                            " -P " + (repo / "cmake/pipeline/deps.cmake").string();
    log += run_command_capture(cmd, code);
    // Release lock
    std::error_code ec;
    std::filesystem::remove(lock, ec);
    if (code != 0) {
        return {};
    }
    // Resolve output after build
    if (!std::filesystem::exists(libpath)) {
        return {};
    }
    return libpath;
}

inline int clang_compile_with_plugin(const std::filesystem::path &fixture_hdr,
                                     const std::filesystem::path &plugin_lib,
                                     std::string &output) {
    auto obj = std::filesystem::current_path() / "_tmp_odph.o";
    int code = 0;
#ifdef _WIN32
    const std::string cmd = std::string("/opt/homebrew/opt/llvm/bin/clang++ -std=c++23 -x c++ -c ") + fixture_hdr.
                            string() +
                            " -o " + obj.string() +
                            " -Xclang -load -Xclang " + plugin_lib.string() +
                            " -Xclang -plugin -Xclang one-definition-per-header";
    output = run_command_capture(cmd, code);
    return code;
#else
    const std::vector<std::string> args{
        "/opt/homebrew/opt/llvm/bin/clang++",
        "-std=c++23",
        "-x", "c++",
        "-c", fixture_hdr.string(),
        "-o", obj.string(),
        "-Xclang", "-load", "-Xclang", plugin_lib.string(),
        "-Xclang", "-plugin", "-Xclang", "one-definition-per-header"
    };
    output = run_exec_capture(args, code);
    return code;
#endif
}

#ifndef _WIN32
inline bool plugin_sanity_check(const std::filesystem::path &plugin_lib) {
    // Use a known sad fixture that should trigger a header doc error.
    const auto repo = repo_root_from_build_cwd();
    const auto fx =
            repo /
            "test/tools/clang-plugins/OneDefinitionPerHeader/fixtures/sad/missing_header/include/MissingHeader.h";
    if (!std::filesystem::exists(fx)) { return false; }
    int code = 0;
    const std::vector<std::string> args{
        "/opt/homebrew/opt/llvm/bin/clang++",
        "-std=c++23",
        "-x", "c++",
        "-c", fx.string(),
        "-o", (std::filesystem::current_path() / "_tmp_odph_probe.o").string(),
        "-Xclang", "-load", "-Xclang", plugin_lib.string(),
        "-Xclang", "-plugin", "-Xclang", "one-definition-per-header"
    };
    const std::string out = run_exec_capture(args, code);
    return out.find("header must start with Doxygen block") != std::string::npos;
}
#else
inline bool plugin_sanity_check(const std::filesystem::path & /*plugin_lib*/) { return true; }
#endif
