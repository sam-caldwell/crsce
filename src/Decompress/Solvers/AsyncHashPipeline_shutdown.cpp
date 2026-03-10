/**
 * @file AsyncHashPipeline_shutdown.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief AsyncHashPipeline::shutdown and destructor.
 */
#include "decompress/Solvers/AsyncHashPipeline.h"

namespace crsce::decompress::solvers {
    /**
     * @name shutdown
     * @brief Signal that no more candidates will be submitted.
     * @throws None
     */
    void AsyncHashPipeline::shutdown() {
        done_.store(true);
        pendingNotEmpty_.notify_one();
    }

    /**
     * @name ~AsyncHashPipeline
     * @brief Shut down the verifier thread and drain queues.
     */
    AsyncHashPipeline::~AsyncHashPipeline() {
        shutdown();
        if (verifierThread_.joinable()) {
            verifierThread_.join();
        }
    }
} // namespace crsce::decompress::solvers
