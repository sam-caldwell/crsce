/**
 * @file RunSha256Stdin.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/HasherUtils/RunSha256Stdin.h"
#include "common/HasherUtils/detail/extract_first_hex64.h"

#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

#if defined(__unix__) || defined(__APPLE__)
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace crsce::common::hasher {
    /**
     * @name run_sha256_stdin
     * @brief Execute a command, pipe data to stdin, capture stdout, and extract hex digest.
     * @param cmd Executable and arguments vector.
     * @param data Bytes to send to the command's stdin.
     * @param hex_out Output string to receive 64-char lowercase hex digest.
     * @return bool True on success; false on I/O or process failure.
     */
    bool run_sha256_stdin(const std::vector<std::string> &cmd,
                          const std::vector<std::uint8_t> &data,
                          std::string &hex_out) {
#if defined(__unix__) || defined(__APPLE__)
        std::array<int, 2> in_pipe{};
        std::array<int, 2> out_pipe{};
        if (pipe(in_pipe.data()) != 0) {
            return false;
        }
        if (pipe(out_pipe.data()) != 0) {
            close(in_pipe[0]);
            close(in_pipe[1]);
            return false;
        }
        const auto pid = fork();
        if (pid == 0) {
            (void) dup2(in_pipe[0], STDIN_FILENO);
            (void) dup2(out_pipe[1], STDOUT_FILENO);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(in_pipe[0]);
            close(out_pipe[1]);

            std::vector<char *> argv;
            argv.reserve(cmd.size() + 1);
            for (const auto &s: cmd) {
                argv.push_back(const_cast<char *>(s.c_str())); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }
            argv.push_back(nullptr);

            (void) execv(cmd[0].c_str(), argv.data());
            _exit(127);
        }
        if (pid < 0) {
            close(in_pipe[0]);
            close(in_pipe[1]);
            close(out_pipe[0]);
            close(out_pipe[1]);
            return false;
        }
        close(in_pipe[0]);
        close(out_pipe[1]);

        // Write all input
        std::size_t written = 0;
        while (written < data.size()) {
            const auto w = write(in_pipe[1], &data[written], data.size() - written);
            if (w <= 0) {
                break;
            }
            written += static_cast<std::size_t>(w);
        }
        close(in_pipe[1]);

        // Read output
        std::string out;
        std::array<char, 4096> buf{};
        for (;;) {
            const auto n = read(out_pipe[0], buf.data(), buf.size());
            if (n <= 0) {
                break;
            }
            out.append(buf.data(), static_cast<std::size_t>(n));
        }
        close(out_pipe[0]);

        int status = 0;
        (void) waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            return false;
        }

        return extract_first_hex64(out, hex_out);
#else
        (void) cmd;
        (void) data;
        (void) hex_out;
        return false;
#endif
    }
} // namespace crsce::common::hasher
