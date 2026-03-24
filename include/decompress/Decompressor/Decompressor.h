/**
 * @file Decompressor.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Decompressor class declaration for CRSCE block decompression.
 *
 * Reads a CRSCE compressed file, reconstructs each block's CSM via solver
 * enumeration to the stored disambiguation index (DI), and writes the
 * original uncompressed bitstream.
 */
#pragma once

#include <cstdint>
#include <string>

#include "common/Csm/Csm.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"

namespace crsce::decompress {

    /**
     * @class Decompressor
     * @name Decompressor
     * @brief Decompresses a CRSCE file by reconstructing CSM blocks via constraint-based enumeration.
     * @details
     * For each block the decompressor:
     *   1. Deserializes the CompressedPayload from the block's 19,549 bytes
     *   2. Builds solver components from the payload's cross-sums and lateral hashes
     *   3. Enumerates solutions until reaching the DI-th (0-based) match
     *   4. Extracts bits from the reconstructed CSM in row-major MSB-first order
     *
     * The output file is truncated to the original file size stored in the header.
     */
    class Decompressor {
    public:
        /**
         * @name kS
         * @brief Matrix dimension (number of rows and columns).
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name kBlockBits
         * @brief Number of bits per block (kS * kS = 261,121).
         */
        static constexpr std::uint32_t kBlockBits = kS * kS;

        /**
         * @name Decompressor
         * @brief Construct a default Decompressor.
         * @throws None
         */
        Decompressor();

        /**
         * @name decompress
         * @brief Decompress a CRSCE input file and write the original output.
         * @param inputPath Path to the CRSCE compressed input file.
         * @param outputPath Path to the output (decompressed) file.
         * @return void
         * @throws DecompressInputOpenError if the input file cannot be opened.
         * @throws DecompressInputReadError if the input file cannot be read.
         * @throws DecompressHeaderInvalid if the header is invalid (too small, bad magic, CRC, or size mismatch).
         * @throws DecompressOutputOpenError if the output file cannot be opened.
         * @throws DecompressOutputWriteError if the output file write fails.
         * @throws DecompressDIOutOfRange if a block cannot be reconstructed.
         */
        void decompress(const std::string &inputPath, const std::string &outputPath);

    private:
        /**
         * @name reconstructBlock
         * @brief Reconstruct the original CSM for a single block from its compressed payload.
         * @param payload The deserialized CompressedPayload for the block.
         * @return The reconstructed Csm matching the DI-th enumerated solution.
         * @throws DecompressDIOutOfRange if enumeration does not reach the DI-th solution.
         */
        static common::Csm reconstructBlock(const common::format::CompressedPayload &payload);
    };

} // namespace crsce::decompress
