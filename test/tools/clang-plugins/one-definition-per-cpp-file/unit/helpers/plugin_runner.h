/**
 * @file plugin_runner.h
 * @brief Helper to build and run the OneDefinitionPerCppFile plugin for tests.
 */
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <array>
#ifndef _WIN32
#include <sys/wait.h>
#endif

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
  // Assume current working directory is <repo>/build/<preset>
  auto cwd = std::filesystem::current_path();
  if (cwd.filename() == "") {
    cwd = cwd.parent_path();
  }
  auto maybe_build_dir = cwd.parent_path(); // <repo>/build
  auto maybe_repo = maybe_build_dir.parent_path(); // <repo>
  return maybe_repo;
}

inline std::filesystem::path ensure_plugin_built(std::string &log) {
  auto repo = repo_root_from_build_cwd();
  auto build_root = std::filesystem::current_path();

  int code = 0;
  // Build all tools via the existing deps script
  const std::string cmd = std::string("cmake -D SOURCE_DIR=") + repo.string() +
                    " -D BUILD_DIR=" + build_root.string() +
                    " -P " + (repo / "cmake/pipeline/deps.cmake").string();
  log += run_command_capture(cmd, code);
  if (code != 0) {
    return {};
  }

#ifdef __APPLE__
  const char* libname = "libOneDefinitionPerCppFile.dylib";
#else
#ifdef _WIN32
  const char* libname = "OneDefinitionPerCppFile.dll";
#else
  const char* libname = "libOneDefinitionPerCppFile.so";
#endif
#endif
  auto libpath = build_root / "tools/clang-plugins/OneDefinitionPerCppFile" / libname;
  if (!std::filesystem::exists(libpath)) {
    return {};
  }
  return libpath;
}

inline int clang_compile_with_plugin(const std::filesystem::path &fixture_cpp,
                                     const std::filesystem::path &plugin_lib,
                                     std::string &output) {
  auto obj = std::filesystem::current_path() / "_tmp_one_def_per_cpp_file.o";
  const std::string cmd = std::string("clang++ -std=c++23 -c ") + fixture_cpp.string() +
                    " -o " + obj.string() +
                    " -Xclang -load -Xclang " + plugin_lib.string() +
                    " -Xclang -plugin -Xclang one-definition-per-cpp-file";
  int code = 0;
  output = run_command_capture(cmd, code);
  return code;
}
