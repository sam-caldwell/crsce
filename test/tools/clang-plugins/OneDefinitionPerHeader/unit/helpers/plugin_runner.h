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
#endif

inline std::string run_command_capture(const std::string &cmd, int &exit_code) {
#ifdef _WIN32
  std::array<char, 8192> buffer{};
  std::string result;
  const std::string full = cmd + " 2>&1";
  FILE *pipe = _popen(full.c_str(), "r");
  if (!pipe) { exit_code = -1; return ""; }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result += buffer.data();
  }
  exit_code = _pclose(pipe);
  return result;
#else
  const auto tmp = std::filesystem::current_path() / ".cmd_out.txt";
  const std::string full = cmd + " > " + tmp.string() + " 2>&1";
  int status = std::system(full.c_str());
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else {
    exit_code = -1;
  }
  std::ifstream in(tmp);
  std::string result((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  std::error_code ec;
  std::filesystem::remove(tmp, ec);
  return result;
#endif
}

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
  auto libname = "libOneDefinitionPerHeader.dylib";
#else
#ifdef _WIN32
  const char* libname = "OneDefinitionPerHeader.dll";
#else
  const char* libname = "libOneDefinitionPerHeader.so";
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
  const std::string cmd = std::string("/opt/homebrew/opt/llvm/bin/clang++ -std=c++23 -x c++ -c ") + fixture_hdr.string() +
                          " -o " + obj.string() +
                          " -Xclang -load -Xclang " + plugin_lib.string() +
                          " -Xclang -plugin -Xclang one-definition-per-header";
  int code = 0;
  output = run_command_capture(cmd, code);
  return code;
}
