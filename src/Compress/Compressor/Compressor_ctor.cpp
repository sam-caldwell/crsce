/**
 * @file Compressor_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor constructor: reads MAX_COMPRESSION_TIME and DISABLE_COMPRESS_DI from the environment.
 */
#include "compress/Compressor/Compressor.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace crsce::compress {

    /**
     * @name Compressor
     * @brief Construct a Compressor, reading MAX_COMPRESSION_TIME and DISABLE_COMPRESS_DI from the environment.
     * @details If the environment variable MAX_COMPRESSION_TIME is set and is a valid
     *          positive integer, it is used as the per-block time limit in seconds.
     *          Otherwise the default of 1,800 seconds (30 minutes) is used.
     *
     *          If the environment variable DISABLE_COMPRESS_DI is set to "1" or "true",
     *          the compressor skips DI discovery and writes DI=0 for every block.
     *          This allows debugging/testing compress functionality without running the solver.
     * @throws None
     */
    Compressor::Compressor() {
        const char *env = std::getenv("MAX_COMPRESSION_TIME"); // NOLINT(concurrency-mt-unsafe)
        if (env != nullptr) {
            try {
                const auto val = std::stoull(std::string(env));
                if (val > 0) {
                    maxTimeSeconds_ = val;
                }
            } catch (...) {
                // Ignore invalid values; keep the default.
            }
        }

        const char *diEnv = std::getenv("DISABLE_COMPRESS_DI"); // NOLINT(concurrency-mt-unsafe)
        if (diEnv != nullptr) {
            disableDI_ = (std::strcmp(diEnv, "1") == 0 || std::strcmp(diEnv, "true") == 0);
        }
    }

} // namespace crsce::compress
