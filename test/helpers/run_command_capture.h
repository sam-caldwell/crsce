/**
 * @file run_command_capture.h
 * @brief Execute a command, capture combined stdout+stderr, and return output text.
 */
#pragma once

#include <array>
#include <cstdio>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

/**
 * @brief Run a command (shell interpolation allowed for redirection) and capture output.
 * @param cmd Command to execute; stderr is redirected to stdout.
 * @param exit_code Filled with process exit status (0 on success; -1 on failure to spawn).
 * @return Captured output (stdout+stderr).
 */
inline std::string run_command_capture(const std::string &cmd, int &exit_code) {
#ifdef _WIN32
  const std::string full = cmd + " 2>&1";
  std::array<char, 4096> buf{};
  std::string out;
  if (FILE *pipe = _popen(full.c_str(), "r")) {
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
      out += buf.data();
    }
    exit_code = _pclose(pipe);
  } else {
    exit_code = -1;
  }
  return out;
#else
  const std::string full = cmd + " 2>&1";
  std::array<char, 4096> buf{};
  std::string out;
  if (FILE *pipe = popen(full.c_str(), "r")) {
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
      out += buf.data();
    }
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
      exit_code = WEXITSTATUS(status);
    } else {
      exit_code = -1;
    }
  } else {
    exit_code = -1;
  }
  return out;
#endif
}
