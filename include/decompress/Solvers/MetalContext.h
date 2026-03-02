/**
 * @file MetalContext.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief PIMPL wrapper for Apple Metal device, command queue, buffers, and pipeline state.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "decompress/Solvers/MetalLineStatAudit.h"

namespace crsce::decompress::solvers {
    /**
     * @class MetalContext
     * @name MetalContext
     * @brief Manages the Metal device, compute pipeline, and shared-memory buffers for GPU-accelerated
     *        line-stat auditing. Pure C++ header; Objective-C types are hidden behind the PIMPL.
     */
    class MetalContext {
    public:
        /**
         * @name MetalContext
         * @brief Construct a Metal context, discovering the default GPU and loading the shader library.
         * @throws std::runtime_error if Metal device or shader library cannot be initialized.
         */
        MetalContext();

        /**
         * @name ~MetalContext
         * @brief Destructor.
         */
        ~MetalContext();

        MetalContext(const MetalContext &) = delete;
        MetalContext &operator=(const MetalContext &) = delete;
        MetalContext(MetalContext &&) noexcept;
        MetalContext &operator=(MetalContext &&) noexcept;

        /**
         * @name isAvailable
         * @brief Check whether a Metal GPU device was successfully acquired.
         * @return True if the Metal pipeline is ready for dispatch.
         */
        [[nodiscard]] bool isAvailable() const;

        /**
         * @name uploadCellState
         * @brief Upload assignment and known bit masks to GPU shared-memory buffers.
         * @param assignmentBits Per-row assignment bits (511 rows x 16 uint32 words each).
         * @param knownBits Per-row known bits (511 rows x 16 uint32 words each).
         */
        void uploadCellState(const std::vector<std::array<std::uint32_t, kRowWords>> &assignmentBits,
                             const std::vector<std::array<std::uint32_t, kRowWords>> &knownBits);

        /**
         * @name uploadTargets
         * @brief Upload cross-sum target values to the GPU buffer. Called once per block.
         * @param targets Flat array of kTotalLines target sums in line order (row, col, diag, anti-diag).
         */
        void uploadTargets(const std::vector<std::uint16_t> &targets);

        /**
         * @name dispatchAudit
         * @brief Launch the line-stat audit kernel on the GPU and wait for completion.
         */
        void dispatchAudit();

        /**
         * @name readLineStats
         * @brief Read back computed line statistics from the GPU.
         * @return Vector of kTotalLines GpuLineStat structs.
         */
        [[nodiscard]] std::vector<GpuLineStat> readLineStats() const;

        /**
         * @name readForceProposals
         * @brief Read back force proposals emitted by the GPU kernel.
         * @return Vector of GpuForceProposal structs.
         */
        [[nodiscard]] std::vector<GpuForceProposal> readForceProposals() const;

    private:
        /**
         * @name Impl
         * @brief Forward-declared PIMPL holding Objective-C Metal objects.
         */
        struct Impl;

        /**
         * @name impl_
         * @brief Pointer to the Objective-C++ implementation.
         */
        std::unique_ptr<Impl> impl_;
    };
} // namespace crsce::decompress::solvers
