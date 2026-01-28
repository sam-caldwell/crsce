/**
 * @file run_decompress_cli.h
 * @brief Invoke the decompress CLI runner with -in/-out arguments.
 */
#pragma once

#include <span>
#include <string>
#include <vector>

#include "decompress/Cli/detail/run.h"

namespace crsce::testhelpers::tlt {
    /**
     * @name run_decompress_cli
     * @brief Call decompress::cli::run with provided container and output paths.
     * @param in Input CRSCE container path.
     * @param out Output reconstructed file path.
     * @return Exit code: 0 on success; non-zero on failure.
     */
    inline int run_decompress_cli(const std::string &in, const std::string &out) {
        std::vector<std::string> av = {"decompress", "-in", in, "-out", out};
        std::vector<char *> argv;
        argv.reserve(av.size());
        for (auto &s : av) { argv.push_back(s.data()); }
        return crsce::decompress::cli::run(std::span<char*>{argv.data(), argv.size()});
    }
} // namespace crsce::testhelpers::tlt
