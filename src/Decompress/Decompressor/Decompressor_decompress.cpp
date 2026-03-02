/**
 * @file Decompressor_decompress.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Decompressor::decompress -- main decompression pipeline.
 */
#include "decompress/Decompressor/Decompressor.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "common/exceptions/DecompressHeaderInvalid.h"
#include "common/exceptions/DecompressInputOpenError.h"
#include "common/exceptions/DecompressInputReadError.h"
#include "common/exceptions/DecompressOutputOpenError.h"
#include "common/exceptions/DecompressOutputWriteError.h"
#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "common/Format/CompressedPayload/FileHeader.h"
#include "common/O11y/O11y.h"

namespace crsce::decompress {

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
    void Decompressor::decompress(const std::string &inputPath, const std::string &outputPath) {
        // Read the entire input file into a byte vector.
        std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            throw common::exceptions::DecompressInputOpenError("decompress: cannot open input file: " + inputPath);
        }
        const auto fileSize = static_cast<std::size_t>(in.tellg());
        in.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> data(fileSize);
        if (fileSize > 0) {
            if (!in.read(reinterpret_cast<char *>(data.data()), // NOLINT
                         static_cast<std::streamsize>(fileSize))) {
                throw common::exceptions::DecompressInputReadError("decompress: failed to read input file: " + inputPath);
            }
        }
        in.close();

        // Validate minimum size for the file header.
        if (fileSize < common::format::FileHeader::kHeaderBytes) {
            throw common::exceptions::DecompressHeaderInvalid("decompress: input file too small for header: " + inputPath);
        }

        // Deserialize the file header (validates magic and CRC-32).
        const auto header = common::format::FileHeader::deserialize(data.data(), fileSize);

        // Validate file size matches header + block payloads.
        const auto expectedSize = static_cast<std::size_t>(
            common::format::FileHeader::kHeaderBytes +
            (header.blockCount * common::format::CompressedPayload::kBlockPayloadBytes));
        if (fileSize != expectedSize) {
            throw common::exceptions::DecompressHeaderInvalid(
                "decompress: file size mismatch: expected " + std::to_string(expectedSize) +
                " bytes, got " + std::to_string(fileSize));
        }

        // Accumulate output bytes across all blocks.
        std::vector<std::uint8_t> outputBuffer;
        outputBuffer.reserve(static_cast<std::size_t>(header.originalFileSizeBytes));

        ::crsce::o11y::O11y::instance().event("decompress_blocks",
            {{"total", std::to_string(header.blockCount)},
             {"original_bytes", std::to_string(header.originalFileSizeBytes)}});

        // Process each block.
        for (std::uint64_t b = 0; b < header.blockCount; ++b) {
            ::crsce::o11y::O11y::instance().event("decompress_block_start",
                {{"block_id", std::to_string(b)}, {"block_count", std::to_string(header.blockCount)}});

            // Locate the block data in the input buffer.
            const auto blockOffset = static_cast<std::size_t>(
                common::format::FileHeader::kHeaderBytes +
                (b * common::format::CompressedPayload::kBlockPayloadBytes));

            const std::uint8_t *blockData = data.data() + blockOffset; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

            // Deserialize the compressed payload for this block.
            common::format::CompressedPayload payload;
            payload.deserializeBlock(blockData, common::format::CompressedPayload::kBlockPayloadBytes);

            // Reconstruct the original CSM via solver enumeration.
            const auto csm = reconstructBlock(payload);

            ::crsce::o11y::O11y::instance().event("decompress_block_done",
                {{"block_id", std::to_string(b)}, {"block_count", std::to_string(header.blockCount)}});

            // Extract packed bits from the CSM and append to the output buffer.
            const auto bits = csm.vec();
            outputBuffer.insert(outputBuffer.end(), bits.begin(), bits.end());
        }

        // Truncate the output buffer to the original file size.
        const auto originalSize = static_cast<std::size_t>(header.originalFileSizeBytes);
        if (outputBuffer.size() > originalSize) {
            outputBuffer.resize(originalSize);
        }

        // Write the output file.
        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            throw common::exceptions::DecompressOutputOpenError("decompress: cannot open output file: " + outputPath);
        }
        if (!outputBuffer.empty()) {
            out.write(reinterpret_cast<const char *>(outputBuffer.data()), // NOLINT
                      static_cast<std::streamsize>(outputBuffer.size()));
        }
        out.close();
        if (!out.good()) {
            throw common::exceptions::DecompressOutputWriteError("decompress: error writing output file: " + outputPath);
        }
    }

} // namespace crsce::decompress
