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

inline std::string run_command_capture(const std::string &cmd, int &exit_code) {
  std::array<char, 8192> buffer{};
  std::string result;
#ifdef _WIN32
  const std::string full = cmd + " 2>&1";
  FILE *pipe = _popen(full.c_str(), "r");
  if (!pipe) { exit_code = -1; return ""; }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result += buffer.data();
  }
  exit_code = _pclose(pipe);
#else
  const std::string full = cmd + " 2>&1";
  FILE *pipe = popen(full.c_str(), "r");
  if (!pipe) { exit_code = -1; return ""; }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    result += buffer.data();
  }
  int status = pclose(pipe);
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else {
    exit_code = -1;
  }
#endif
  return result;
}

inline std::filesystem::path repo_root_from_build_cwd() {
  auto cwd = std::filesystem::current_path();
  if (cwd.filename() == "") {
    cwd = cwd.parent_path();
  }
  return cwd.parent_path().parent_path(); // from build/<preset> to repo root
}

inline std::filesystem::path ensure_plugin_built(std::string &log) {
  auto repo = repo_root_from_build_cwd();
  auto build_root = std::filesystem::current_path();
  int code = 0;
  const std::string cmd = std::string("cmake -D SOURCE_DIR=") + repo.string() +
                          " -D BUILD_DIR=" + build_root.string() +
                          " -P " + (repo / "cmake/pipeline/deps.cmake").string();
  log += run_command_capture(cmd, code);
  if (code != 0) {
    return {};
  }
#ifdef __APPLE__
  const char* libname = "libOneDefinitionPerHeader.dylib";
#else
#ifdef _WIN32
  const char* libname = "OneDefinitionPerHeader.dll";
#else
  const char* libname = "libOneDefinitionPerHeader.so";
#endif
#endif
  auto libpath = build_root / "tools/clang-plugins/OneDefinitionPerHeader" / libname;
  if (!std::filesystem::exists(libpath)) {
    return {};
  }
  return libpath;
}

inline int clang_compile_with_plugin(const std::filesystem::path &fixture_hdr,
                                     const std::filesystem::path &plugin_lib,
                                     std::string &output) {
  auto obj = std::filesystem::current_path() / "_tmp_odph.o";
  const std::string cmd = std::string("clang++ -std=c++23 -x c++ -c ") + fixture_hdr.string() +
                          " -o " + obj.string() +
                          " -Xclang -load -Xclang " + plugin_lib.string() +
                          " -Xclang -plugin -Xclang one-definition-per-header";
  int code = 0;
  output = run_command_capture(cmd, code);
  return code;
}
