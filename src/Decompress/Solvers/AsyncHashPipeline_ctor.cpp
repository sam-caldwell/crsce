/**
 * @file AsyncHashPipeline_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief AsyncHashPipeline constructor: starts the background verifier thread.
 */
#include "decompress/Solvers/AsyncHashPipeline.h"

#include <cstdint>

#include "decompress/Solvers/IHashVerifier.h"

namespace crsce::decompress::solvers {
    /**
     * @name AsyncHashPipeline
     * @brief Construct the pipeline and start the background verifier thread.
     * @param hasher Hash verifier reference.
     * @param maxPending Maximum candidates buffered before DFS blocks.
     * @throws None
     */
    AsyncHashPipeline::AsyncHashPipeline(IHashVerifier &hasher, const std::uint32_t maxPending)
        : hasher_(hasher), maxPending_(maxPending), verifierThread_(&AsyncHashPipeline::verifierLoop, this) {}
} // namespace crsce::decompress::solvers
