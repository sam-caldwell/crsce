/**
 * @file Compressor.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor class declaration for CRSCE block compression.
 *
 * Reads an input file, partitions it into kS*kS-bit blocks, computes cross-sums
 * and lateral hashes per block, discovers the disambiguation index (DI) via
 * solver enumeration, and writes a CRSCE compressed output file.
 */
#pragma once

#include <cstdint>
#include <string>

#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"

namespace crsce::compress {

    /**
     * @class Compressor
     * @name Compressor
     * @brief Compresses an input file into CRSCE format using cross-sum constraints, SHA-1 lateral hashes, and SHA-256 block hash.
     * @details
     * For each kS*kS-bit block the compressor:
     *   1. Loads bits into a CSM (row-major, MSB-first per byte)
     *   2. Computes cross-sums (LSM, VSM, DSM, XSM, HSM1, SFC1, HSM2, SFC2)
     *   3. Computes lateral hashes (SHA-1 per row)
     *   4. Computes block hash (SHA-256 of full CSM)
     *   5. Discovers the disambiguation index (DI) via solver enumeration
     *   6. Serializes the block payload and writes to the output file
     *
     * The output file consists of a 28-byte FileHeader followed by concatenated block payloads.
     */
    class Compressor {
    public:
        /**
         * @name kS
         * @brief Matrix dimension (number of rows and columns).
         */
        static constexpr std::uint16_t kS = 511;

        /**
         * @name kBlockBits
         * @brief Number of bits per block (kS * kS = 261,121).
         */
        static constexpr std::uint32_t kBlockBits = kS * kS;

        /**
         * @name Compressor
         * @brief Construct a Compressor, reading MAX_COMPRESSION_TIME from the environment.
         * @throws None
         */
        Compressor();

        /**
         * @name compress
         * @brief Compress an input file and write the CRSCE output.
         * @param inputPath Path to the input file.
         * @param outputPath Path to the output CRSCE file.
         * @return void
         * @throws CompressInputOpenError if the input file cannot be opened.
         * @throws CompressInputReadError if the input file cannot be read.
         * @throws CompressOutputOpenError if the output file cannot be opened.
         * @throws CompressOutputWriteError if the output file write fails.
         * @throws CompressDIOverflow if the DI exceeds 255 for any block.
         * @throws CompressTimeoutException if the compression time limit is exceeded.
         * @throws CompressDINotFound if enumeration exhausts without finding the original CSM.
         */
        void compress(const std::string &inputPath, const std::string &outputPath) const;

    private:
        /**
         * @name loadCsm
         * @brief Load raw bytes into a CSM, row-major, MSB-first per byte.
         * @param data Pointer to the raw byte data.
         * @param bitCount Number of valid bits to load (may be less than kBlockBits for the last block).
         * @return A populated Csm instance.
         * @throws None
         */
        static common::Csm loadCsm(const std::uint8_t *data, std::size_t bitCount);

        /**
         * @name computeCrossSums
         * @brief Compute row, column, diagonal, and anti-diagonal sums from the CSM and fill the payload.
         * @param csm The populated cross-sum matrix.
         * @param payload The CompressedPayload to fill with LSM, VSM, DSM, XSM values.
         * @return void
         * @throws None
         */
        static void computeCrossSums(const common::Csm &csm, common::format::CompressedPayload &payload);

        /**
         * @name computeLH
         * @brief Compute SHA-1 lateral hashes for each CSM row and fill the payload.
         * @param csm The populated cross-sum matrix.
         * @param payload The CompressedPayload to fill with LH digests.
         * @return void
         * @throws None
         */
        static void computeLH(const common::Csm &csm, common::format::CompressedPayload &payload);

        /**
         * @name computeBH
         * @brief Compute the SHA-256 block hash of the full CSM and store it in the payload.
         * @param csm The populated cross-sum matrix.
         * @param payload The CompressedPayload to fill with the BH digest.
         * @return void
         * @throws None
         */
        static void computeBH(const common::Csm &csm, common::format::CompressedPayload &payload);

        /**
         * @name discoverDI
         * @brief Discover the disambiguation index via solver enumeration.
         * @param original The original CSM to match against enumerated solutions.
         * @param payload The CompressedPayload containing cross-sums and lateral hashes.
         * @param maxTimeSeconds Maximum wall-clock time in seconds for enumeration.
         * @return The zero-based DI (0..255).
         * @throws CompressTimeoutException if the time limit is exceeded.
         * @throws CompressDIOverflow if the DI exceeds 255.
         * @throws CompressDINotFound if enumeration exhausts without finding the original CSM.
         */
        static std::uint8_t discoverDI(const common::Csm &original,
                                        const common::format::CompressedPayload &payload,
                                        std::uint64_t maxTimeSeconds);

        /**
         * @name maxTimeSeconds_
         * @brief Maximum wall-clock time in seconds for DI enumeration per block.
         */
        std::uint64_t maxTimeSeconds_{1800};

        /**
         * @name disableDI_
         * @brief When true, skip DI discovery and write DI=0.
         * @details Controlled by the DISABLE_COMPRESS_DI environment variable.
         *          Set to "1" or "true" to disable DI discovery, allowing
         *          compress-only debugging without running the solver.
         */
        bool disableDI_{false};
    };

} // namespace crsce::compress
