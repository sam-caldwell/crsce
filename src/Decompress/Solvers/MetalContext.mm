/**
 * @file MetalContext.mm
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Objective-C++ implementation of MetalContext PIMPL for Metal GPU acceleration.
 */
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "decompress/Solvers/MetalContext.h"
#include "decompress/Solvers/MetalLineStatAudit.h"

#ifndef CRSCE_BIN_DIR
#define CRSCE_BIN_DIR "."
#endif

#ifndef CRSCE_SRC_DIR
#define CRSCE_SRC_DIR "."
#endif

namespace crsce::decompress::solvers {

    /**
     * @struct MetalContext::Impl
     * @name Impl
     * @brief PIMPL body holding Objective-C Metal objects.
     */
    struct MetalContext::Impl {
        /**
         * @name device
         * @brief The Metal GPU device.
         */
        id<MTLDevice> device = nil;

        /**
         * @name commandQueue
         * @brief The Metal command queue.
         */
        id<MTLCommandQueue> commandQueue = nil;

        /**
         * @name pipelineState
         * @brief The compiled compute pipeline state for the line-stat audit kernel.
         */
        id<MTLComputePipelineState> pipelineState = nil;

        /**
         * @name assignmentBuffer
         * @brief GPU buffer for assignment bits (511 rows x 16 uint32).
         */
        id<MTLBuffer> assignmentBuffer = nil;

        /**
         * @name knownBuffer
         * @brief GPU buffer for known bits (511 rows x 16 uint32).
         */
        id<MTLBuffer> knownBuffer = nil;

        /**
         * @name lineStatsBuffer
         * @brief GPU buffer for output line statistics.
         */
        id<MTLBuffer> lineStatsBuffer = nil;

        /**
         * @name targetsBuffer
         * @brief GPU buffer for cross-sum target values.
         */
        id<MTLBuffer> targetsBuffer = nil;

        /**
         * @name proposalCountBuffer
         * @brief GPU buffer for atomic proposal count.
         */
        id<MTLBuffer> proposalCountBuffer = nil;

        /**
         * @name proposalsBuffer
         * @brief GPU buffer for force proposals array.
         */
        id<MTLBuffer> proposalsBuffer = nil;

        /**
         * @name constantsBuffer
         * @brief GPU buffer for constant kS value.
         */
        id<MTLBuffer> constantsBuffer = nil;

        /**
         * @name available
         * @brief Whether the Metal pipeline was successfully initialized.
         */
        bool available = false;
    };

    /**
     * @name MetalContext
     * @brief Construct the Metal context: discover device, load shaders, create buffers.
     * @throws std::runtime_error if Metal is not available or shader library cannot be loaded.
     */
    MetalContext::MetalContext() : impl_(std::make_unique<Impl>()) {
        @autoreleasepool {
            impl_->device = MTLCreateSystemDefaultDevice();
            if (!impl_->device) {
                return; // No Metal device; isAvailable() returns false
            }

            impl_->commandQueue = [impl_->device newCommandQueue];
            if (!impl_->commandQueue) {
                return;
            }

            // Try loading pre-compiled .metallib, then fall back to runtime source compilation
            NSError *error = nil;
            id<MTLLibrary> library = nil;

            // Attempt 1: pre-compiled .metallib from bin directory
            NSString *libPathStr = [NSString stringWithFormat:@"%s/crsce_shaders.metallib",
                                    CRSCE_BIN_DIR];
            NSURL *libURL = [NSURL fileURLWithPath:libPathStr];
            if ([[NSFileManager defaultManager] fileExistsAtPath:libPathStr]) {
                library = [impl_->device newLibraryWithURL:libURL error:&error];
            }

            // Attempt 2: compile from .metal source at runtime
            if (!library) {
                NSString *srcPath = [NSString stringWithFormat:
                    @"%s/Decompress/Solvers/Metal/line_stat_audit.metal", CRSCE_SRC_DIR];
                NSData *srcData = [NSData dataWithContentsOfFile:srcPath];
                if (srcData) {
                    NSString *srcStr = [[NSString alloc] initWithData:srcData encoding:NSUTF8StringEncoding];
                    MTLCompileOptions *opts = [[MTLCompileOptions alloc] init];
                    opts.mathMode = MTLMathModeFast;
                    library = [impl_->device newLibraryWithSource:srcStr options:opts error:&error];
                }
            }

            if (!library) {
                return;
            }

            id<MTLFunction> kernelFunction = [library newFunctionWithName:@"line_stat_audit"];
            if (!kernelFunction) {
                return;
            }

            impl_->pipelineState = [impl_->device newComputePipelineStateWithFunction:kernelFunction
                                                                                error:&error];
            if (!impl_->pipelineState) {
                return;
            }

            // Allocate shared-memory buffers (StorageModeShared = zero-copy on Apple Silicon)
            const auto assignBufSize = static_cast<NSUInteger>(kMetalS * kRowWords * sizeof(std::uint32_t));
            impl_->assignmentBuffer = [impl_->device newBufferWithLength:assignBufSize
                                                                 options:MTLResourceStorageModeShared];
            impl_->knownBuffer = [impl_->device newBufferWithLength:assignBufSize
                                                            options:MTLResourceStorageModeShared];

            const auto lineStatsBufSize = static_cast<NSUInteger>(kTotalLines * sizeof(GpuLineStat));
            impl_->lineStatsBuffer = [impl_->device newBufferWithLength:lineStatsBufSize
                                                                options:MTLResourceStorageModeShared];

            const auto targetsBufSize = static_cast<NSUInteger>(kTotalLines * sizeof(std::uint16_t));
            impl_->targetsBuffer = [impl_->device newBufferWithLength:targetsBufSize
                                                              options:MTLResourceStorageModeShared];

            impl_->proposalCountBuffer = [impl_->device newBufferWithLength:sizeof(std::uint32_t)
                                                                    options:MTLResourceStorageModeShared];

            const auto proposalsBufSize = static_cast<NSUInteger>(kMaxForceProposals * sizeof(GpuForceProposal));
            impl_->proposalsBuffer = [impl_->device newBufferWithLength:proposalsBufSize
                                                                options:MTLResourceStorageModeShared];

            const std::uint16_t sConst = kMetalS;
            impl_->constantsBuffer = [impl_->device newBufferWithBytes:&sConst
                                                                length:sizeof(std::uint16_t)
                                                               options:MTLResourceStorageModeShared];

            impl_->available = true;
        }
    }

    MetalContext::~MetalContext() = default;
    MetalContext::MetalContext(MetalContext &&) noexcept = default;
    MetalContext &MetalContext::operator=(MetalContext &&) noexcept = default;

    /**
     * @name isAvailable
     * @brief Check whether the Metal pipeline is ready.
     * @return True if device, pipeline, and buffers are initialized.
     */
    bool MetalContext::isAvailable() const {
        return impl_ && impl_->available;
    }

    /**
     * @name uploadCellState
     * @brief Copy assignment and known bit masks into GPU shared-memory buffers.
     * @param assignmentBits Per-row assignment bits.
     * @param knownBits Per-row known bits.
     */
    void MetalContext::uploadCellState(
        const std::vector<std::array<std::uint32_t, kRowWords>> &assignmentBits,
        const std::vector<std::array<std::uint32_t, kRowWords>> &knownBits) {
        std::memcpy(impl_->assignmentBuffer.contents, assignmentBits.data(),
                    kMetalS * kRowWords * sizeof(std::uint32_t));
        std::memcpy(impl_->knownBuffer.contents, knownBits.data(),
                    kMetalS * kRowWords * sizeof(std::uint32_t));
    }

    /**
     * @name uploadTargets
     * @brief Copy cross-sum targets into the GPU buffer.
     * @param targets Flat array of kTotalLines target values.
     */
    void MetalContext::uploadTargets(const std::vector<std::uint16_t> &targets) {
        std::memcpy(impl_->targetsBuffer.contents, targets.data(),
                    kTotalLines * sizeof(std::uint16_t));
    }

    /**
     * @name dispatchAudit
     * @brief Encode and submit the line-stat audit kernel, then wait for completion.
     */
    void MetalContext::dispatchAudit() {
        @autoreleasepool {
            // Reset atomic proposal count to 0
            std::memset(impl_->proposalCountBuffer.contents, 0, sizeof(std::uint32_t));

            id<MTLCommandBuffer> commandBuffer = [impl_->commandQueue commandBuffer];
            id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];

            [encoder setComputePipelineState:impl_->pipelineState];
            [encoder setBuffer:impl_->assignmentBuffer offset:0 atIndex:0];
            [encoder setBuffer:impl_->knownBuffer offset:0 atIndex:1];
            [encoder setBuffer:impl_->lineStatsBuffer offset:0 atIndex:2];
            [encoder setBuffer:impl_->targetsBuffer offset:0 atIndex:3];
            [encoder setBuffer:impl_->proposalCountBuffer offset:0 atIndex:4];
            [encoder setBuffer:impl_->proposalsBuffer offset:0 atIndex:5];
            [encoder setBuffer:impl_->constantsBuffer offset:0 atIndex:6];

            // Dispatch one thread per line (kTotalLines = 3064)
            const auto threadgroupSize = static_cast<NSUInteger>(
                std::min(static_cast<NSUInteger>(64),
                         impl_->pipelineState.maxTotalThreadsPerThreadgroup));
            const MTLSize threads = MTLSizeMake(kTotalLines, 1, 1);
            const MTLSize groups = MTLSizeMake(threadgroupSize, 1, 1);

            [encoder dispatchThreads:threads threadsPerThreadgroup:groups];
            [encoder endEncoding];
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
        }
    }

    /**
     * @name readLineStats
     * @brief Read back line statistics from the GPU buffer.
     * @return Vector of GpuLineStat structs.
     */
    std::vector<GpuLineStat> MetalContext::readLineStats() const {
        std::vector<GpuLineStat> stats(kTotalLines);
        std::memcpy(stats.data(), impl_->lineStatsBuffer.contents,
                    kTotalLines * sizeof(GpuLineStat));
        return stats;
    }

    /**
     * @name readForceProposals
     * @brief Read back force proposals from the GPU buffers.
     * @return Vector of GpuForceProposal structs.
     */
    std::vector<GpuForceProposal> MetalContext::readForceProposals() const {
        std::uint32_t count = 0;
        std::memcpy(&count, impl_->proposalCountBuffer.contents, sizeof(std::uint32_t));

        if (count > kMaxForceProposals) {
            count = kMaxForceProposals;
        }

        std::vector<GpuForceProposal> proposals(count);
        if (count > 0) {
            std::memcpy(proposals.data(), impl_->proposalsBuffer.contents,
                        count * sizeof(GpuForceProposal));
        }
        return proposals;
    }

} // namespace crsce::decompress::solvers
