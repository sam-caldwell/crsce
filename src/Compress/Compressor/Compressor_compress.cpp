/**
 * @file Compressor_compress.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Compressor::compress -- main compression pipeline.
 */
#include "compress/Compressor/Compressor.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "common/exceptions/CompressInputOpenError.h"
#include "common/exceptions/CompressInputReadError.h"
#include "common/exceptions/CompressOutputOpenError.h"
#include "common/exceptions/CompressOutputWriteError.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "common/Format/CompressedPayload/FileHeader.h"
#include "common/O11y/O11y.h"

namespace crsce::compress {

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
    void Compressor::compress(const std::string &inputPath, const std::string &outputPath) const {
        // Read the entire input file into a byte vector.
        std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            throw common::exceptions::CompressInputOpenError("compress: cannot open input file: " + inputPath);
        }
        const auto fileSize = static_cast<std::size_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> data(fileSize);
        if (fileSize > 0) {
            if (!in.read(reinterpret_cast<char *>(data.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                         static_cast<std::streamsize>(fileSize))) {
                throw common::exceptions::CompressInputReadError("compress: failed to read input file: " + inputPath);
            }
        }
        in.close();

        // Compute the number of blocks: ceil(fileSizeBits / kBlockBits).
        const std::uint64_t fileSizeBits = static_cast<std::uint64_t>(fileSize) * 8;
        const std::uint64_t blockCount = (fileSizeBits == 0) ? 0 : (fileSizeBits + kBlockBits - 1) / kBlockBits;

        // Open the output file.
        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            throw common::exceptions::CompressOutputOpenError("compress: cannot open output file: " + outputPath);
        }

        // Write the file header.
        common::format::FileHeader header;
        header.originalFileSizeBytes = fileSize;
        header.blockCount = blockCount;
        const auto headerBytes = header.serialize();
        out.write(reinterpret_cast<const char *>(headerBytes.data()), // NOLINT
                  static_cast<std::streamsize>(headerBytes.size()));

        ::crsce::o11y::O11y::instance().event("compress_blocks",
            {{"total", std::to_string(blockCount)}, {"file_bytes", std::to_string(fileSize)}});

        // Process each block.
        for (std::uint64_t b = 0; b < blockCount; ++b) {
            ::crsce::o11y::O11y::instance().event("compress_block_start",
                {{"block_id", std::to_string(b)}, {"block_count", std::to_string(blockCount)}});
            // Determine the bit range for this block.
            const std::uint64_t startBit = b * kBlockBits;
            const std::uint64_t remainingBits = fileSizeBits - startBit;
            const auto blockBitCount = static_cast<std::size_t>(
                remainingBits < kBlockBits ? remainingBits : kBlockBits);

            // Compute the byte range covering the needed bits.
            // Extract bits from the source buffer, handling non-byte-aligned block starts.
            // kBlockBits = 511*511 = 261121, and 261121 mod 8 = 1, so blocks after the
            // first are NOT byte-aligned. We extract bits individually.
            const auto neededBytes = (blockBitCount + 7) / 8;
            std::vector<std::uint8_t> blockData(neededBytes, 0);

            for (std::size_t i = 0; i < blockBitCount; ++i) {
                const std::size_t srcBitPos = static_cast<std::size_t>(startBit) + i;
                const std::size_t srcByteIdx = srcBitPos / 8;
                const std::uint8_t srcBitInByte = 7 - static_cast<std::uint8_t>(srcBitPos % 8);

                const std::size_t dstByteIdx = i / 8;
                const std::uint8_t dstBitInByte = 7 - static_cast<std::uint8_t>(i % 8);

                if (srcByteIdx < data.size()) {
                    const std::uint8_t bit = (data[srcByteIdx] >> srcBitInByte) & 1;
                    blockData[dstByteIdx] |= static_cast<std::uint8_t>(bit << dstBitInByte);
                }
            }

            // Load the CSM from the block data.
            auto csm = loadCsm(blockData.data(), blockBitCount);

            // Create the compressed payload and fill it.
            common::format::CompressedPayload payload;
            computeCrossSums(csm, payload);
            computeLH(csm, payload);
            computeBH(csm, payload);

            // Discover the disambiguation index (or skip if disabled).
            const std::uint8_t di = disableDI_ ? std::uint8_t{0}
                                                : discoverDI(csm, payload, maxTimeSeconds_);
            payload.setDI(di);
            ::crsce::o11y::O11y::instance().event("compress_block_done",
                {{"block_id", std::to_string(b)}, {"block_count", std::to_string(blockCount)},
                 {"di", std::to_string(di)}});

            // Serialize and write the block.
            const auto blockBytes = payload.serializeBlock();
            out.write(reinterpret_cast<const char *>(blockBytes.data()), // NOLINT
                      static_cast<std::streamsize>(blockBytes.size()));
        }

        out.close();
        if (!out.good()) {
            throw common::exceptions::CompressOutputWriteError("compress: error writing output file: " + outputPath);
        }
    }

} // namespace crsce::compress
