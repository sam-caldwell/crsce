/**
 * @file CrsceViewer/viewer.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Implementation of crsce::viewer::run_viewer.
 */
#include "crsce_viewer/viewer.h"

#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "common/Format/CompressedPayload/CompressedPayload.h"
#include "common/Format/CompressedPayload/FileHeader.h"

using crsce::common::format::CompressedPayload;
using crsce::common::format::FileHeader;

namespace crsce::viewer {

namespace {

/**
 * @brief Format a 32-byte digest as a lowercase hex string.
 */
std::string hexDigest(const std::array<std::uint8_t, 32> &digest) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto b : digest) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

} // namespace

/**
 * @name run_viewer
 * @brief Parse a .crsce file and write human-readable output.
 * @param path Filesystem path to the .crsce container.
 * @param out Output stream for the formatted report.
 * @param err Output stream for error messages.
 * @return 0 on success, 1 on error.
 */
int run_viewer(const std::string &path, std::ostream &out, std::ostream &err) { // NOLINT(misc-use-internal-linkage)
    std::ifstream is(path, std::ios::binary);
    if (!is) {
        err << "error: cannot open '" << path << "'\n";
        return 1;
    }

    // Read header
    std::array<std::uint8_t, FileHeader::kHeaderBytes> hdrBuf{};
    is.read(reinterpret_cast<char *>(hdrBuf.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            static_cast<std::streamsize>(hdrBuf.size()));
    if (is.gcount() != static_cast<std::streamsize>(hdrBuf.size())) {
        err << "error: file too small for header (need 28 bytes)\n";
        return 1;
    }

    FileHeader header;
    try {
        header = FileHeader::deserialize(hdrBuf.data(), hdrBuf.size());
    } catch (const std::exception &e) {
        err << "error: " << e.what() << '\n';
        return 1;
    }

    // Print header
    out << "=== CRSCE Header ===\n"
        << "  original_file_size: " << header.originalFileSizeBytes << " bytes\n"
        << "  block_count:        " << header.blockCount << '\n'
        << '\n';

    // Read and print each block
    for (std::uint64_t b = 0; b < header.blockCount; ++b) {
        std::vector<std::uint8_t> blockBuf(CompressedPayload::kBlockPayloadBytes);
        is.read(reinterpret_cast<char *>(blockBuf.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                static_cast<std::streamsize>(blockBuf.size()));
        if (is.gcount() != static_cast<std::streamsize>(blockBuf.size())) {
            err << "error: short read on block " << b << '\n';
            return 1;
        }

        CompressedPayload payload;
        payload.deserializeBlock(blockBuf.data(), blockBuf.size());

        out << "=== Block " << b << " ===\n";
        out << "  DI: " << static_cast<unsigned>(payload.getDI()) << '\n';

        // LSM (row sums)
        out << "  LSM[0.." << (CompressedPayload::kS - 1) << "]:";
        for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
            out << ' ' << payload.getLSM(k);
        }
        out << '\n';

        // VSM (column sums)
        out << "  VSM[0.." << (CompressedPayload::kS - 1) << "]:";
        for (std::uint16_t k = 0; k < CompressedPayload::kS; ++k) {
            out << ' ' << payload.getVSM(k);
        }
        out << '\n';

        // DSM (diagonal sums)
        out << "  DSM[0.." << (CompressedPayload::kDiagCount - 1) << "]:";
        for (std::uint16_t k = 0; k < CompressedPayload::kDiagCount; ++k) {
            out << ' ' << payload.getDSM(k);
        }
        out << '\n';

        // XSM (anti-diagonal sums)
        out << "  XSM[0.." << (CompressedPayload::kDiagCount - 1) << "]:";
        for (std::uint16_t k = 0; k < CompressedPayload::kDiagCount; ++k) {
            out << ' ' << payload.getXSM(k);
        }
        out << '\n';

        // LH (lateral hashes)
        out << "  LH[0.." << (CompressedPayload::kS - 1) << "]:\n";
        for (std::uint16_t r = 0; r < CompressedPayload::kS; ++r) {
            out << "    [" << std::setw(3) << r << "] " << hexDigest(payload.getLH(r)) << '\n';
        }

        out << '\n';
    }

    return 0;
}

} // namespace crsce::viewer
