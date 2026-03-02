/**
 * @file MetalLineStatAudit.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Shared GPU/CPU data structures for the Metal line-stat audit kernel.
 */
#pragma once

#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @struct GpuLineStat
     * @name GpuLineStat
     * @brief Per-line statistics computed by the Metal kernel (8 bytes, GPU-aligned).
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct GpuLineStat {
        /**
         * @name unknown
         * @brief Count of unassigned cells u(L).
         */
        std::uint16_t unknown;

        /**
         * @name assigned
         * @brief Count of assigned-one cells a(L).
         */
        std::uint16_t assigned;

        /**
         * @name residual
         * @brief Residual rho(L) = S(L) - a(L).
         */
        std::int16_t residual;

        /**
         * @name pad
         * @brief Padding for 8-byte alignment.
         */
        std::uint16_t pad;
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @struct GpuForceProposal
     * @name GpuForceProposal
     * @brief A forced-assignment proposal emitted by the Metal kernel (8 bytes, GPU-aligned).
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct GpuForceProposal {
        /**
         * @name r
         * @brief Row index.
         */
        std::uint16_t r;

        /**
         * @name c
         * @brief Column index.
         */
        std::uint16_t c;

        /**
         * @name value
         * @brief Forced value (0 or 1).
         */
        std::uint8_t value;

        /**
         * @name pad
         * @brief Padding for 8-byte alignment.
         */
        std::uint8_t pad[3]; // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    /**
     * @name kS
     * @brief Matrix dimension constant.
     */
    static constexpr std::uint16_t kMetalS = 511;

    /**
     * @name kTotalLines
     * @brief Total number of constraint lines: s rows + s cols + (2s-1) diags + (2s-1) anti-diags.
     */
    static constexpr std::uint32_t kTotalLines = kMetalS + kMetalS + (2 * kMetalS - 1) + (2 * kMetalS - 1);

    /**
     * @name kRowWords
     * @brief Number of uint32 words per row for GPU buffers (511 bits -> 16 x 32-bit words).
     */
    static constexpr std::uint32_t kRowWords = 16;

    /**
     * @name kMaxForceProposals
     * @brief Maximum number of force proposals the GPU can emit per audit dispatch.
     */
    static constexpr std::uint32_t kMaxForceProposals = kMetalS * kMetalS;
} // namespace crsce::decompress::solvers
