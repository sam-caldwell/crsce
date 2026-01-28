/**
 * @file run_compress_cli.h
 * @brief Invoke the compress CLI runner with -in/-out arguments.
 */
#pragma once

#include <span>
#include <string>
#include <vector>

#include "compress/Cli/detail/run.h"

namespace crsce::testhelpers::tlt {
    /**
     * @name run_compress_cli
     * @brief Call compress::cli::run with provided input and output file paths.
     * @param in Input file path.
     * @param out Output CRSCE container path.
     * @return Exit code: 0 on success; non-zero on failure.
     */
    inline int run_compress_cli(const std::string &in, const std::string &out) {
        std::vector<std::string> av = {"compress", "-in", in, "-out", out};
        std::vector<char *> argv;
        argv.reserve(av.size());
        for (auto &s : av) { argv.push_back(s.data()); }
        return crsce::compress::cli::run(std::span<char*>{argv.data(), argv.size()});
    }
} // namespace crsce::testhelpers::tlt
