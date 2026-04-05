/**
 * @file CombinatorSolver.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.60: Combinator-algebraic solver implementation.
 */
#include "decompress/Solvers/CombinatorSolver.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "common/BlockHash/BlockHash.h"
#include "common/Util/crc8.h"
#include "common/Util/crc16_ccitt.h"
#include "common/Util/crc32_ieee.h"
#include "decompress/Solvers/Crc32RowCompleter.h"

namespace crsce::decompress::solvers {

    // ── Helpers ─────────────────────────────────────────────────────────

    namespace {
        /**
         * @name diagCells
         * @brief Return flat cell indices for non-toroidal DSM diagonal d.
         */
        std::vector<std::uint32_t> diagCells(const std::uint16_t d) {
            std::vector<std::uint32_t> cells;
            const auto offset = static_cast<std::int32_t>(d) - (CombinatorSolver::kS - 1);
            for (std::uint16_t r = 0; r < CombinatorSolver::kS; ++r) {
                const auto c = static_cast<std::int32_t>(r) + offset;
                if (c >= 0 && c < CombinatorSolver::kS) {
                    cells.push_back(static_cast<std::uint32_t>(r) * CombinatorSolver::kS
                                    + static_cast<std::uint32_t>(c));
                }
            }
            return cells;
        }

        /**
         * @name antiDiagCells
         * @brief Return flat cell indices for non-toroidal XSM anti-diagonal x.
         */
        std::vector<std::uint32_t> antiDiagCells(const std::uint16_t x) {
            std::vector<std::uint32_t> cells;
            for (std::uint16_t r = 0; r < CombinatorSolver::kS; ++r) {
                const auto c = static_cast<std::int32_t>(x) - static_cast<std::int32_t>(r);
                if (c >= 0 && c < CombinatorSolver::kS) {
                    cells.push_back(static_cast<std::uint32_t>(r) * CombinatorSolver::kS
                                    + static_cast<std::uint32_t>(c));
                }
            }
            return cells;
        }

        /**
         * @name crc32OfBits
         * @brief CRC-32 of a variable-length bit vector (len data bits + 1 pad bit).
         */
        std::uint32_t crc32OfBits(const std::uint8_t *bits, const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 32> msg{}; // max 127 bits + 1 pad = 16 bytes
            for (std::uint16_t k = 0; k < len; ++k) {
                if (bits[k]) {
                    msg[k / 8] |= static_cast<std::uint8_t>(1U << (7U - (k % 8U))); // NOLINT
                }
            }
            return common::util::crc32_ieee(msg.data(), totalBytes);
        }

        /**
         * @name buildGenMatrixForLength
         * @brief Build 32 x len GF(2) generator matrix for CRC-32 of len-bit messages.
         * @return (genMatrix, crcZero) — genMatrix[bit][col] and affine constant.
         */
        std::pair<std::vector<std::array<std::uint8_t, 256>>, std::uint32_t>
        buildGenMatrixForLength(const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);

            // CRC-32 of all-zero message
            std::array<std::uint8_t, 32> zeroMsg{};
            const auto c0 = common::util::crc32_ieee(zeroMsg.data(), totalBytes);

            std::vector<std::array<std::uint8_t, 256>> gm(32);
            for (auto &row : gm) { row.fill(0); }

            for (std::uint16_t col = 0; col < len; ++col) {
                std::array<std::uint8_t, 32> msg{};
                msg[col / 8] = static_cast<std::uint8_t>(1U << (7U - (col % 8U))); // NOLINT
                const auto crcUnit = common::util::crc32_ieee(msg.data(), totalBytes);
                const auto colVal = crcUnit ^ c0;
                for (std::uint8_t bit = 0; bit < 32; ++bit) {
                    gm[bit][col] = static_cast<std::uint8_t>((colVal >> bit) & 1U); // NOLINT
                }
            }
            return {gm, c0};
        }

        /**
         * @name crc16OfBits
         * @brief CRC-16 of a variable-length bit vector.
         */
        std::uint16_t crc16OfBits(const std::uint8_t *bits, const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t k = 0; k < len; ++k) {
                if (bits[k]) {
                    msg[k / 8] |= static_cast<std::uint8_t>(1U << (7U - (k % 8U))); // NOLINT
                }
            }
            return common::util::crc16_ccitt(msg.data(), totalBytes);
        }

        /**
         * @name buildGenMatrix16ForLength
         * @brief Build 16 x len GF(2) generator matrix for CRC-16.
         */
        std::pair<std::vector<std::array<std::uint8_t, 256>>, std::uint16_t>
        buildGenMatrix16ForLength(const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 16> zeroMsg{};
            const auto c0 = common::util::crc16_ccitt(zeroMsg.data(), totalBytes);
            std::vector<std::array<std::uint8_t, 256>> gm(16);
            for (auto &row : gm) { row.fill(0); }
            for (std::uint16_t col = 0; col < len; ++col) {
                std::array<std::uint8_t, 32> msg{};
                msg[col / 8] = static_cast<std::uint8_t>(1U << (7U - (col % 8U))); // NOLINT
                const auto crcUnit = common::util::crc16_ccitt(msg.data(), totalBytes);
                const auto colVal = static_cast<std::uint16_t>(crcUnit ^ c0);
                for (std::uint8_t bit = 0; bit < 16; ++bit) {
                    gm[bit][col] = static_cast<std::uint8_t>((colVal >> bit) & 1U); // NOLINT
                }
            }
            return {gm, c0};
        }

        /**
         * @name crc8OfBits
         * @brief CRC-8 of a variable-length bit vector.
         */
        std::uint8_t crc8OfBits(const std::uint8_t *bits, const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t k = 0; k < len; ++k) {
                if (bits[k]) {
                    msg[k / 8] |= static_cast<std::uint8_t>(1U << (7U - (k % 8U))); // NOLINT
                }
            }
            return common::util::crc8(msg.data(), totalBytes);
        }

        /**
         * @name buildGenMatrix8ForLength
         * @brief Build 8 x len GF(2) generator matrix for CRC-8.
         */
        std::pair<std::vector<std::array<std::uint8_t, 256>>, std::uint8_t>
        buildGenMatrix8ForLength(const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 16> zeroMsg{};
            const auto c0 = common::util::crc8(zeroMsg.data(), totalBytes);
            std::vector<std::array<std::uint8_t, 256>> gm(8);
            for (auto &row : gm) { row.fill(0); }
            for (std::uint16_t col = 0; col < len; ++col) {
                std::array<std::uint8_t, 32> msg{};
                msg[col / 8] = static_cast<std::uint8_t>(1U << (7U - (col % 8U))); // NOLINT
                const auto crcUnit = common::util::crc8(msg.data(), totalBytes);
                const auto colVal = static_cast<std::uint8_t>(crcUnit ^ c0);
                for (std::uint8_t bit = 0; bit < 8; ++bit) {
                    gm[bit][col] = static_cast<std::uint8_t>((colVal >> bit) & 1U); // NOLINT
                }
            }
            return {gm, c0};
        }

    } // anonymous namespace

    // ── Constructor ─────────────────────────────────────────────────────

    CombinatorSolver::CombinatorSolver(const common::Csm &csm, const Config config)
        : config_(config) {
        cellState_.fill(-1);
        cellToLines_.resize(kN);

        // Store original for verification (must use csm.get, not getOrig — original_ not yet populated)
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                original_[static_cast<std::uint32_t>(r) * kS + c] = csm.get(r, c);
            }
        }

        expectedBH_ = common::BlockHash::compute(csm);
        buildSystem();
    }

    CombinatorSolver::CombinatorSolver(const common::CsmVariable &csm, const Config config)
        : config_(config) {
        cellState_.fill(-1);
        cellToLines_.resize(kN);

        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                original_[static_cast<std::uint32_t>(r) * kS + c] = csm.get(r, c);
            }
        }

        // Compute per-row CRC-32 (LH) for hybrid Phase II verification
        expectedLH_.resize(kS);
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (original_[(static_cast<std::uint32_t>(r) * kS) + c]) { // NOLINT
                    msg[c / 8] |= static_cast<std::uint8_t>(1U << (7U - (c % 8U))); // NOLINT
                }
            }
            expectedLH_[r] = common::util::crc32_ieee(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));
        }

        // Compute per-column CRC-32 (VH) for Phase III verification
        expectedVH_.resize(kS);
        for (std::uint16_t c = 0; c < kS; ++c) {
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (original_[(static_cast<std::uint32_t>(r) * kS) + c]) { // NOLINT
                    msg[r / 8] |= static_cast<std::uint8_t>(1U << (7U - (r % 8U))); // NOLINT
                }
            }
            expectedVH_[c] = common::util::crc32_ieee(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));
        }

        // Compute per-diagonal and per-anti-diagonal CRCs for Phase IV/V verification
        for (std::uint16_t d = 0; d < kDiagCount; ++d) {
            const auto cells = diagCells(d);
            const auto lineLen = static_cast<std::uint16_t>(cells.size());
            std::uint8_t crcW = 32;
            if (lineLen <= 8) { crcW = 8; }
            else if (lineLen <= 16) { crcW = 16; }
            const auto msgBytes = static_cast<std::size_t>((lineLen + 1 + 7) / 8);
            std::vector<std::uint8_t> msg(msgBytes, 0);
            for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
                const auto flat = cells[slot]; // NOLINT
                const auto r = static_cast<std::uint16_t>(flat / kS);
                const auto cc = static_cast<std::uint16_t>(flat % kS);
                if (original_[(static_cast<std::uint32_t>(r) * kS) + cc]) { // NOLINT
                    msg[slot / 8] |= static_cast<std::uint8_t>(1U << (7U - (slot % 8U))); // NOLINT
                }
            }
            std::uint32_t crc = 0;
            if (crcW == 8) { crc = common::util::crc8(msg.data(), msgBytes); }
            else if (crcW == 16) { crc = common::util::crc16_ccitt(msg.data(), msgBytes); }
            else { crc = common::util::crc32_ieee(msg.data(), msgBytes); }
            diagAxes_.push_back({cells, crc, crcW});
        }
        for (std::uint16_t x = 0; x < kDiagCount; ++x) {
            const auto cells = antiDiagCells(x);
            const auto lineLen = static_cast<std::uint16_t>(cells.size());
            std::uint8_t crcW = 32;
            if (lineLen <= 8) { crcW = 8; }
            else if (lineLen <= 16) { crcW = 16; }
            const auto msgBytes = static_cast<std::size_t>((lineLen + 1 + 7) / 8);
            std::vector<std::uint8_t> msg(msgBytes, 0);
            for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
                const auto flat = cells[slot]; // NOLINT
                const auto r = static_cast<std::uint16_t>(flat / kS);
                const auto cc = static_cast<std::uint16_t>(flat % kS);
                if (original_[(static_cast<std::uint32_t>(r) * kS) + cc]) { // NOLINT
                    msg[slot / 8] |= static_cast<std::uint8_t>(1U << (7U - (slot % 8U))); // NOLINT
                }
            }
            std::uint32_t crc = 0;
            if (crcW == 8) { crc = common::util::crc8(msg.data(), msgBytes); }
            else if (crcW == 16) { crc = common::util::crc16_ccitt(msg.data(), msgBytes); }
            else { crc = common::util::crc32_ieee(msg.data(), msgBytes); }
            antiDiagAxes_.push_back({cells, crc, crcW});
        }

        // Compute BH from CsmVariable
        if constexpr (kS <= 127) {
            // Convert to fixed Csm for BlockHash
            common::Csm fixedCsm;
            for (std::uint16_t r = 0; r < kS; ++r) {
                for (std::uint16_t c = 0; c < kS; ++c) {
                    fixedCsm.set(r, c, original_[static_cast<std::uint32_t>(r) * kS + c]);
                }
            }
            expectedBH_ = common::BlockHash::compute(fixedCsm);
        } else {
            const auto buf = csm.serialize();
            expectedBH_ = common::BlockHash::compute(buf.data(), buf.size());
        }

        buildSystem();
    }

    // ── Config name ─────────────────────────────────────────────────────

    std::string CombinatorSolver::configName() const {
        std::string name;
        if (config_.toroidal) { name += "toroidal_"; } else { name += "straight_"; }
        name += "LH";
        if (config_.useVH) { name += "+VH"; }
        if (config_.useDH) { name += "+DH"; }
        return name;
    }

    // ── System construction ─────────────────────────────────────────────

    void CombinatorSolver::buildSystem() {
        addGeometricParity();
        if (config_.useLH) {
            if (config_.lhBits == 16) { addLH16(); } else { addLH(); }
        }
        if (config_.useVH) {
            if (config_.vhBits == 16) { addVH16(); } else { addVH(); }
        }
        if (config_.useDH) { addDH(); }
        if (config_.useXH) { addXH(); }
        if (config_.useYLTP) { addYLTP(); }
        if (config_.useRLTP) { addRLTP(); }
    }

    void CombinatorSolver::addGeometricParity() {
        // LSM (rows)
        for (std::uint16_t r = 0; r < kS; ++r) {
            IntLine line;
            GF2Row gf2row{};
            std::uint8_t parity = 0;
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                line.cells.push_back(flat);
                gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                if (getOrig(r, c)) { parity ^= 1; line.target++; }
            }
            line.rho = line.target;
            line.u = kS;
            const auto li = static_cast<std::uint32_t>(intLines_.size());
            intLines_.push_back(std::move(line));
            for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
            gf2Rows_.push_back(gf2row);
            gf2Target_.push_back(parity);
        }

        // VSM (columns)
        for (std::uint16_t c = 0; c < kS; ++c) {
            IntLine line;
            GF2Row gf2row{};
            std::uint8_t parity = 0;
            for (std::uint16_t r = 0; r < kS; ++r) {
                const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                line.cells.push_back(flat);
                gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                if (getOrig(r, c)) { parity ^= 1; line.target++; }
            }
            line.rho = line.target;
            line.u = kS;
            const auto li = static_cast<std::uint32_t>(intLines_.size());
            intLines_.push_back(std::move(line));
            for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
            gf2Rows_.push_back(gf2row);
            gf2Target_.push_back(parity);
        }

        if (config_.toroidal) {
            // tDSM slope=1: k = (c - r) mod S
            for (std::uint16_t k = 0; k < kS; ++k) {
                IntLine line;
                GF2Row gf2row{};
                std::uint8_t parity = 0;
                for (std::uint16_t r = 0; r < kS; ++r) {
                    const auto c = static_cast<std::uint16_t>((k + r) % kS);
                    const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                    line.cells.push_back(flat);
                    gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    if (getOrig(r, c)) { parity ^= 1; line.target++; }
                }
                line.rho = line.target;
                line.u = kS;
                const auto li = static_cast<std::uint32_t>(intLines_.size());
                intLines_.push_back(std::move(line));
                for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(parity);
            }
            // tXSM slope=126: k = (c + r) mod S
            for (std::uint16_t k = 0; k < kS; ++k) {
                IntLine line;
                GF2Row gf2row{};
                std::uint8_t parity = 0;
                for (std::uint16_t r = 0; r < kS; ++r) {
                    auto c = static_cast<std::int32_t>(k) - static_cast<std::int32_t>(r);
                    if (c < 0) { c += kS; }
                    const auto cc = static_cast<std::uint16_t>(c);
                    const auto flat = static_cast<std::uint32_t>(r) * kS + cc;
                    line.cells.push_back(flat);
                    gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    if (getOrig(r, cc)) { parity ^= 1; line.target++; }
                }
                line.rho = line.target;
                line.u = kS;
                const auto li = static_cast<std::uint32_t>(intLines_.size());
                intLines_.push_back(std::move(line));
                for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(parity);
            }
        } else {
            // Non-toroidal DSM (skip IntBound lines when useDSMSums=false; parity GF2 always added)
            for (std::uint16_t d = 0; d < kDiagCount; ++d) {
                const auto cells = diagCells(d);
                GF2Row gf2row{};
                std::uint8_t parity = 0;
                for (const auto flat : cells) {
                    gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    const auto r = static_cast<std::uint16_t>(flat / kS);
                    const auto c = static_cast<std::uint16_t>(flat % kS);
                    if (getOrig(r, c)) { parity ^= 1; }
                }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(parity);
                if (!config_.useDSMSums) { continue; }
                IntLine line;
                for (const auto flat : cells) {
                    line.cells.push_back(flat);
                    const auto r = static_cast<std::uint16_t>(flat / kS);
                    const auto c = static_cast<std::uint16_t>(flat % kS);
                    if (getOrig(r, c)) { line.target++; }
                }
                line.rho = line.target;
                line.u = static_cast<std::int32_t>(cells.size());
                const auto li = static_cast<std::uint32_t>(intLines_.size());
                intLines_.push_back(std::move(line));
                for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
            }
            // Non-toroidal XSM (skip IntBound lines when useXSMSums=false; parity GF2 always added)
            for (std::uint16_t x = 0; x < kDiagCount; ++x) {
                const auto cells = antiDiagCells(x);
                GF2Row gf2row{};
                std::uint8_t parity = 0;
                for (const auto flat : cells) {
                    gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    const auto r = static_cast<std::uint16_t>(flat / kS);
                    const auto c = static_cast<std::uint16_t>(flat % kS);
                    if (getOrig(r, c)) { parity ^= 1; }
                }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(parity);
                if (!config_.useXSMSums) { continue; }
                IntLine line;
                for (const auto flat : cells) {
                    line.cells.push_back(flat);
                    const auto r = static_cast<std::uint16_t>(flat / kS);
                    const auto c = static_cast<std::uint16_t>(flat % kS);
                    if (getOrig(r, c)) { line.target++; }
                }
                line.rho = line.target;
                line.u = static_cast<std::int32_t>(cells.size());
                const auto li = static_cast<std::uint32_t>(intLines_.size());
                intLines_.push_back(std::move(line));
                for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(parity);
            }
        }
    }

    void CombinatorSolver::addLH() {
        // Compute generator matrix for current kS (runtime, not constexpr)
        auto [gmVec, c0_32] = buildGenMatrixForLength(kS);
        const auto c0 = c0_32;

        for (std::uint16_t r = 0; r < kS; ++r) {
            // Compute row CRC-32
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (getOrig(r, c)) {
                    msg[c / 8] |= static_cast<std::uint8_t>(1U << (7U - (c % 8U))); // NOLINT
                }
            }
            const auto rowCrc = common::util::crc32_ieee(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                const auto baseCol = static_cast<std::uint32_t>(r) * kS;
                for (std::uint16_t c = 0; c < kS; ++c) {
                    if (gmVec[bit][c]) { // NOLINT
                        const auto flat = baseCol + c;
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(((rowCrc >> bit) & 1U) ^ ((c0 >> bit) & 1U));
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    void CombinatorSolver::addVH() {
        std::fprintf(stderr, "  addVH: kS=%u msgBytes=%zu\n", kS, static_cast<std::size_t>((kS+1+7)/8));
        auto [gmVec, c0_32] = buildGenMatrixForLength(kS);
        const auto c0 = c0_32;
        std::fprintf(stderr, "  addVH: c0=0x%08X\n", c0);

        for (std::uint16_t c = 0; c < kS; ++c) {
            // Compute column CRC-32
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (getOrig(r, c)) {
                    msg[r / 8] |= static_cast<std::uint8_t>(1U << (7U - (r % 8U))); // NOLINT
                }
            }
            const auto colCrc = common::util::crc32_ieee(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                for (std::uint16_t r = 0; r < kS; ++r) {
                    if (gmVec[bit][r]) { // NOLINT
                        const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(((colCrc >> bit) & 1U) ^ ((c0 >> bit) & 1U));


                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    void CombinatorSolver::addDH() {
        // Sort diagonals by length (shortest first) for selective inclusion
        std::vector<std::uint16_t> diagOrder(kDiagCount);
        for (std::uint16_t i = 0; i < kDiagCount; ++i) { diagOrder[i] = i; }
        std::sort(diagOrder.begin(), diagOrder.end(), [](std::uint16_t a, std::uint16_t b) {
            return diagCells(a).size() < diagCells(b).size();
        });

        const auto maxDiags = (config_.dhMaxDiags > 0)
            ? std::min(static_cast<std::uint16_t>(config_.dhMaxDiags), kDiagCount)
            : kDiagCount;

        // Cache generator matrices by diagonal length
        struct GenInfo {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint32_t c0;
        };
        std::array<GenInfo, 128> cache{}; // indexed by length (1..127)
        std::array<bool, 128> cached{};
        cached.fill(false);

        std::uint16_t diagsIncluded = 0;
        for (const auto d : diagOrder) {
            if (diagsIncluded >= maxDiags) { break; }
            const auto cells = diagCells(d);
            const auto dlen = static_cast<std::uint16_t>(cells.size());
            if (dlen == 0) { continue; }
            ++diagsIncluded;

            if (!cached[dlen]) {
                auto [gm, c0] = buildGenMatrixForLength(dlen);
                cache[dlen] = {std::move(gm), c0};
                cached[dlen] = true;
            }
            const auto &gi = cache[dlen];

            // Compute diagonal CRC-32
            std::vector<std::uint8_t> diagBits(dlen);
            for (std::uint16_t j = 0; j < dlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                diagBits[j] = getOrig(r, c);
            }
            const auto diagCrc = crc32OfBits(diagBits.data(), dlen);

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    if (gi.gm[bit][j]) { // NOLINT
                        const auto flat = cells[j];
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(
                    ((diagCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    // ── CRC-16 LH and VH ─────────────────────────────────────────────

    void CombinatorSolver::addLH16() {
        const auto [gm, c0] = buildGenMatrix16ForLength(kS);
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (getOrig(r, c)) {
                    msg[c / 8] |= static_cast<std::uint8_t>(1U << (7U - (c % 8U))); // NOLINT
                }
            }
            const auto rowCrc = common::util::crc16_ccitt(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));
            for (std::uint8_t bit = 0; bit < 16; ++bit) {
                GF2Row gf2row{};
                const auto baseCol = static_cast<std::uint32_t>(r) * kS;
                for (std::uint16_t c = 0; c < kS; ++c) {
                    if (gm[bit][c]) { // NOLINT
                        const auto flat = baseCol + c;
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(((rowCrc >> bit) & 1U) ^ ((c0 >> bit) & 1U));
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    void CombinatorSolver::addVH16() {
        const auto [gm, c0] = buildGenMatrix16ForLength(kS);
        for (std::uint16_t c = 0; c < kS; ++c) {
            std::array<std::uint8_t, 32> msg{};
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (getOrig(r, c)) {
                    msg[r / 8] |= static_cast<std::uint8_t>(1U << (7U - (r % 8U))); // NOLINT
                }
            }
            const auto colCrc = common::util::crc16_ccitt(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));
            for (std::uint8_t bit = 0; bit < 16; ++bit) {
                GF2Row gf2row{};
                for (std::uint16_t r = 0; r < kS; ++r) {
                    if (gm[bit][r]) { // NOLINT
                        const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(((colCrc >> bit) & 1U) ^ ((c0 >> bit) & 1U));
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    // ── XH (anti-diagonal CRC-32) ─────────────────────────────────────

    void CombinatorSolver::addXH() {
        // Sort anti-diagonals by length (shortest first)
        std::vector<std::uint16_t> xOrder(kDiagCount);
        for (std::uint16_t i = 0; i < kDiagCount; ++i) { xOrder[i] = i; }
        std::sort(xOrder.begin(), xOrder.end(), [](std::uint16_t a, std::uint16_t b) {
            return antiDiagCells(a).size() < antiDiagCells(b).size();
        });

        const auto maxDiags = (config_.xhMaxDiags > 0)
            ? std::min(static_cast<std::uint16_t>(config_.xhMaxDiags), kDiagCount)
            : kDiagCount;

        struct GenInfo {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint32_t c0;
        };
        std::array<GenInfo, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        std::uint16_t diagsIncluded = 0;
        for (const auto x : xOrder) {
            if (diagsIncluded >= maxDiags) { break; }
            const auto cells = antiDiagCells(x);
            const auto xlen = static_cast<std::uint16_t>(cells.size());
            if (xlen == 0) { continue; }
            ++diagsIncluded;

            if (!cached[xlen]) {
                auto [gm, c0] = buildGenMatrixForLength(xlen);
                cache[xlen] = {std::move(gm), c0};
                cached[xlen] = true;
            }
            const auto &gi = cache[xlen];

            // Compute anti-diagonal CRC-32
            std::vector<std::uint8_t> xBits(xlen);
            for (std::uint16_t j = 0; j < xlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                xBits[j] = getOrig(r, c);
            }
            const auto xCrc = crc32OfBits(xBits.data(), xlen);

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                for (std::uint16_t j = 0; j < xlen; ++j) {
                    if (gi.gm[bit][j]) { // NOLINT
                        const auto flat = cells[j];
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(
                    ((xCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    // ── Range-based DH/XH addition ────────────────────────────────────

    void CombinatorSolver::addDHRange(
                                       const std::uint16_t minLen,
                                       const std::uint16_t maxLen) {
        struct GenInfo {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint32_t c0;
        };
        std::array<GenInfo, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        for (std::uint16_t d = 0; d < kDiagCount; ++d) {
            const auto cells = diagCells(d);
            const auto dlen = static_cast<std::uint16_t>(cells.size());
            if (dlen < minLen || dlen > maxLen) { continue; }

            if (!cached[dlen]) {
                auto [gm, c0] = buildGenMatrixForLength(dlen);
                cache[dlen] = {std::move(gm), c0};
                cached[dlen] = true;
            }
            const auto &gi = cache[dlen];

            std::vector<std::uint8_t> diagBits(dlen);
            for (std::uint16_t j = 0; j < dlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                diagBits[j] = getOrig(r, c);
            }
            const auto diagCrc = crc32OfBits(diagBits.data(), dlen);

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    if (gi.gm[bit][j]) { // NOLINT
                        const auto flat = cells[j];
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                const auto t = static_cast<std::uint8_t>(
                    ((diagCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));

                // Substitute already-determined cells
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0) {
                        if (gf2row[flat / 64] & (std::uint64_t{1} << (flat % 64))) { // NOLINT
                            gf2row[flat / 64] &= ~(std::uint64_t{1} << (flat % 64)); // NOLINT
                            if (cellState_[flat] == 1) {
                                // XOR target with 1 (since this cell is 1 and in the equation)
                                // We need a mutable target, handled below
                            }
                        }
                    }
                }

                // Recompute target with determined cells substituted
                std::uint8_t adjT = t;
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0 && cellState_[flat] == 1) {
                        if (gi.gm[bit][j]) { adjT ^= 1; } // NOLINT
                    }
                }

                // Zero out determined cell columns
                GF2Row adjRow{};
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] < 0 && gi.gm[bit][j]) { // NOLINT
                        adjRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }

                gf2Rows_.push_back(adjRow);
                gf2Target_.push_back(adjT);
            }
        }
    }

    void CombinatorSolver::addXHRange(
                                       const std::uint16_t minLen,
                                       const std::uint16_t maxLen) {
        struct GenInfo {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint32_t c0;
        };
        std::array<GenInfo, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        for (std::uint16_t x = 0; x < kDiagCount; ++x) {
            const auto cells = antiDiagCells(x);
            const auto xlen = static_cast<std::uint16_t>(cells.size());
            if (xlen < minLen || xlen > maxLen) { continue; }

            if (!cached[xlen]) {
                auto [gm, c0] = buildGenMatrixForLength(xlen);
                cache[xlen] = {std::move(gm), c0};
                cached[xlen] = true;
            }
            const auto &gi = cache[xlen];

            std::vector<std::uint8_t> xBits(xlen);
            for (std::uint16_t j = 0; j < xlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                xBits[j] = getOrig(r, c);
            }
            const auto xCrc = crc32OfBits(xBits.data(), xlen);

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                std::uint8_t adjT = static_cast<std::uint8_t>(
                    ((xCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                GF2Row adjRow{};

                for (std::uint16_t j = 0; j < xlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0) {
                        if (cellState_[flat] == 1 && gi.gm[bit][j]) { adjT ^= 1; } // NOLINT
                    } else if (gi.gm[bit][j]) { // NOLINT
                        adjRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }

                gf2Rows_.push_back(adjRow);
                gf2Target_.push_back(adjT);
            }
        }
    }

    // ── CRC-16 DH/XH range methods ─────────────────────────────────────

    void CombinatorSolver::addDH16Range(
                                         const std::uint16_t minLen,
                                         const std::uint16_t maxLen) {
        struct GenInfo16 {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint16_t c0;
        };
        std::array<GenInfo16, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        for (std::uint16_t d = 0; d < kDiagCount; ++d) {
            const auto cells = diagCells(d);
            const auto dlen = static_cast<std::uint16_t>(cells.size());
            if (dlen < minLen || dlen > maxLen) { continue; }

            if (!cached[dlen]) {
                auto [gm, c0] = buildGenMatrix16ForLength(dlen);
                cache[dlen] = {std::move(gm), c0};
                cached[dlen] = true;
            }
            const auto &gi = cache[dlen];

            std::vector<std::uint8_t> diagBits(dlen);
            for (std::uint16_t j = 0; j < dlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                diagBits[j] = getOrig(r, c);
            }
            const auto diagCrc = crc16OfBits(diagBits.data(), dlen);

            for (std::uint8_t bit = 0; bit < 16; ++bit) {
                std::uint8_t adjT = static_cast<std::uint8_t>(
                    ((diagCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                GF2Row adjRow{};
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0) {
                        if (cellState_[flat] == 1 && gi.gm[bit][j]) { adjT ^= 1; } // NOLINT
                    } else if (gi.gm[bit][j]) { // NOLINT
                        adjRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                gf2Rows_.push_back(adjRow);
                gf2Target_.push_back(adjT);
            }
        }
    }

    void CombinatorSolver::addXH16Range(
                                         const std::uint16_t minLen,
                                         const std::uint16_t maxLen) {
        struct GenInfo16 {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint16_t c0;
        };
        std::array<GenInfo16, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        for (std::uint16_t x = 0; x < kDiagCount; ++x) {
            const auto cells = antiDiagCells(x);
            const auto xlen = static_cast<std::uint16_t>(cells.size());
            if (xlen < minLen || xlen > maxLen) { continue; }

            if (!cached[xlen]) {
                auto [gm, c0] = buildGenMatrix16ForLength(xlen);
                cache[xlen] = {std::move(gm), c0};
                cached[xlen] = true;
            }
            const auto &gi = cache[xlen];

            std::vector<std::uint8_t> xBits(xlen);
            for (std::uint16_t j = 0; j < xlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                xBits[j] = getOrig(r, c);
            }
            const auto xCrc = crc16OfBits(xBits.data(), xlen);

            for (std::uint8_t bit = 0; bit < 16; ++bit) {
                std::uint8_t adjT = static_cast<std::uint8_t>(
                    ((xCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                GF2Row adjRow{};
                for (std::uint16_t j = 0; j < xlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0) {
                        if (cellState_[flat] == 1 && gi.gm[bit][j]) { adjT ^= 1; } // NOLINT
                    } else if (gi.gm[bit][j]) { // NOLINT
                        adjRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                gf2Rows_.push_back(adjRow);
                gf2Target_.push_back(adjT);
            }
        }
    }

    // ── yLTP (Fisher-Yates random partition + CRC-32 + cross-sum) ─────

    void CombinatorSolver::addYLTP() {
        if (config_.yltpSeed != 0) { addYLTPWithSeed(config_.yltpSeed); }
        if (config_.yltpSeed2 != 0) { addYLTPWithSeed(config_.yltpSeed2); }
    }

    void CombinatorSolver::addYLTPWithSeed(const std::uint64_t seed) {

        // Fisher-Yates LCG partition: assigns each of kN cells to one of kS lines
        static constexpr std::uint64_t LCG_A = 6364136223846793005ULL;
        static constexpr std::uint64_t LCG_C = 1442695040888963407ULL;

        std::vector<std::uint32_t> pool(kN);
        for (std::uint32_t i = 0; i < kN; ++i) { pool[i] = i; }
        auto state = seed;
        for (std::uint32_t i = kN - 1; i > 0; --i) {
            state = state * LCG_A + LCG_C;
            const auto j = static_cast<std::uint32_t>(state % (i + 1));
            std::swap(pool[i], pool[j]); // NOLINT
        }

        // Build membership: membership[flat] = line index
        std::vector<std::uint16_t> membership(kN);
        for (std::uint16_t line = 0; line < kS; ++line) {
            for (std::uint16_t slot = 0; slot < kS; ++slot) {
                membership[pool[static_cast<std::uint32_t>(line) * kS + slot]] = line; // NOLINT
            }
        }

        // Build per-line cell lists
        std::vector<std::vector<std::uint32_t>> lineCells(kS);
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            lineCells[membership[flat]].push_back(flat); // NOLINT
        }

        // Add integer constraint lines (cross-sum)
        for (std::uint16_t line = 0; line < kS; ++line) {
            IntLine il;
            GF2Row gf2row{};
            std::uint8_t parity = 0;
            for (const auto flat : lineCells[line]) {
                il.cells.push_back(flat);
                gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                const auto r = static_cast<std::uint16_t>(flat / kS);
                const auto c = static_cast<std::uint16_t>(flat % kS);
                if (getOrig(r, c)) { parity ^= 1; il.target++; }
            }
            il.rho = il.target;
            il.u = static_cast<std::int32_t>(lineCells[line].size());
            const auto li = static_cast<std::uint32_t>(intLines_.size());
            intLines_.push_back(std::move(il));
            for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
            // Parity equation
            gf2Rows_.push_back(gf2row);
            gf2Target_.push_back(parity);
        }

        // Add CRC-32 per yLTP line
        // Each line has kS cells. Build generator matrix for message length kS.
        auto [gmVec, c0] = buildGenMatrixForLength(kS);
        // Convert to array-of-arrays for indexed access
        std::array<std::vector<std::uint8_t>, 32> gm;
        for (std::uint8_t b = 0; b < 32; ++b) {
            gm[b].resize(kS);
            for (std::uint16_t k = 0; k < kS; ++k) { gm[b][k] = gmVec[b][k]; } // NOLINT
        }

        for (std::uint16_t line = 0; line < kS; ++line) {
            // Compute CRC-32 of this line's cells
            const auto msgBytes = static_cast<std::size_t>((kS + 1 + 7) / 8);
            std::vector<std::uint8_t> msg(msgBytes, 0);
            const auto &cells = lineCells[line];
            for (std::uint16_t slot = 0; slot < kS; ++slot) {
                const auto flat = cells[slot]; // NOLINT
                const auto r = static_cast<std::uint16_t>(flat / kS);
                const auto c = static_cast<std::uint16_t>(flat % kS);
                if (getOrig(r, c)) {
                    msg[slot / 8] |= static_cast<std::uint8_t>(1U << (7U - (slot % 8U))); // NOLINT
                }
            }
            const auto lineCrc = common::util::crc32_ieee(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                auto t = static_cast<std::uint8_t>(((lineCrc >> bit) & 1U) ^ ((c0 >> bit) & 1U));
                for (std::uint16_t slot = 0; slot < kS; ++slot) {
                    const auto flat = cells[slot]; // NOLINT
                    if (gm[bit][slot]) { // NOLINT — unpacked byte format
                        gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(t);
            }
        }
    }

    // ── rLTP (center-spiral partition + CRC-32 + cross-sum) ───────────

    void CombinatorSolver::addRLTP() {
        if (!config_.useRLTP) { return; }
        addRLTPAt(config_.rltpCenterR, config_.rltpCenterC);
        if (config_.useRLTP2) {
            addRLTPAt(config_.rltpCenter2R, config_.rltpCenter2C);
        }
    }

    void CombinatorSolver::addRLTPAt(const std::uint16_t cr, const std::uint16_t cc) {

        struct CellDist {
            std::uint32_t flat;
            std::uint32_t distSq;
        };
        std::vector<CellDist> sorted(kN);
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                const auto dr = static_cast<std::int32_t>(r) - static_cast<std::int32_t>(cr);
                const auto dc = static_cast<std::int32_t>(c) - static_cast<std::int32_t>(cc);
                sorted[flat] = {flat, static_cast<std::uint32_t>(dr * dr + dc * dc)};
            }
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const CellDist &a, const CellDist &b) { return a.distSq < b.distSq; });

        // Variable-length spiral: line k has k+1 cells (1, 2, 3, ..., s)
        // When rltpMaxLines > 0, stop after that many graduated lines (no uniform tail).
        const auto maxGrad = (config_.rltpMaxLines > 0)
            ? std::min(config_.rltpMaxLines, kS)
            : kS;

        std::vector<std::vector<std::uint32_t>> lineCells;
        std::uint32_t cellIdx = 0;

        // Phase 1: graduated lines (length 1, 2, 3, ..., maxGrad)
        for (std::uint16_t lineLen = 1; lineLen <= maxGrad && cellIdx < kN; ++lineLen) {
            std::vector<std::uint32_t> lc;
            for (std::uint16_t j = 0; j < lineLen && cellIdx < kN; ++j) {
                lc.push_back(sorted[cellIdx++].flat);
            }
            lineCells.push_back(std::move(lc));
        }

        // Phase 2: remaining cells in uniform-s lines (skip when rltpMaxLines > 0)
        if (config_.rltpMaxLines == 0) {
            while (cellIdx < kN) {
                std::vector<std::uint32_t> lc;
                for (std::uint16_t j = 0; j < kS && cellIdx < kN; ++j) {
                    lc.push_back(sorted[cellIdx++].flat);
                }
                lineCells.push_back(std::move(lc));
            }
        }

        const auto totalLines = static_cast<std::uint16_t>(lineCells.size());
        const auto uniformLines = (config_.rltpMaxLines == 0)
            ? static_cast<unsigned>(totalLines > kS ? totalLines - kS : 0)
            : 0U;
        std::fprintf(stderr, "  rLTP: %u lines (graduated 1..%u + %u uniform), center (%u,%u)\n",
                     totalLines, static_cast<unsigned>(maxGrad), uniformLines,
                     cr, cc);

        // Cache generator matrices by line length
        struct GenCache {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint32_t c0;
        };
        std::vector<GenCache> gmCache(kS + 1); // indexed by length
        std::vector<bool> gmCached(kS + 1, false);

        // CRC-16 cache
        struct GenCache16 {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint16_t c0;
        };
        std::vector<GenCache16> gm16Cache(kS + 1);
        std::vector<bool> gm16Cached(kS + 1, false);

        // CRC-8 cache
        struct GenCache8 {
            std::vector<std::array<std::uint8_t, 256>> gm;
            std::uint8_t c0;
        };
        std::vector<GenCache8> gm8Cache(kS + 1);
        std::vector<bool> gm8Cached(kS + 1, false);

        for (std::uint16_t line = 0; line < totalLines; ++line) {
            const auto &lc = lineCells[line];
            const auto lineLen = static_cast<std::uint16_t>(lc.size());

            // Parity GF(2) row (always added — 1 bit per line, negligible payload)
            GF2Row parityRow{};
            std::uint8_t parity = 0;
            for (const auto flat : lc) {
                parityRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                if (original_[flat]) { parity ^= 1; }
            }
            gf2Rows_.push_back(parityRow);
            gf2Target_.push_back(parity);

            // Integer cross-sum line (IntBound) — skip when rltpCrcOnly
            if (!config_.rltpCrcOnly) {
                IntLine il;
                for (const auto flat : lc) {
                    il.cells.push_back(flat);
                    if (original_[flat]) { il.target++; }
                }
                il.rho = il.target;
                il.u = static_cast<std::int32_t>(lineLen);
                const auto li = static_cast<std::uint32_t>(intLines_.size());
                intLines_.push_back(std::move(il));
                for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
            }

            // Graduated rLTPh: CRC width by line length (5-tier scheme)
            std::uint8_t crcBits;
            if (lineLen <= 8) { crcBits = 8; }             // tier 1
            else if (lineLen <= 16) { crcBits = 16; }      // tier 2
            else if (lineLen <= 32) { crcBits = config_.rltpTier3Width; } // tier 3
            else if (lineLen <= 64) { crcBits = 32; }      // tier 4
            else { crcBits = config_.rltpTier5Width; }      // tier 5

            // Build CRC message
            const auto msgBytes = static_cast<std::size_t>((lineLen + 1 + 7) / 8);
            std::vector<std::uint8_t> msg(msgBytes, 0);
            for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
                if (original_[lc[slot]]) { // NOLINT
                    msg[slot / 8] |= static_cast<std::uint8_t>(1U << (7U - (slot % 8U))); // NOLINT
                }
            }

            if (crcBits == 8) {
                if (!gm8Cached[lineLen]) {
                    auto [gm, c0] = buildGenMatrix8ForLength(lineLen);
                    gm8Cache[lineLen] = {std::move(gm), c0};
                    gm8Cached[lineLen] = true;
                }
                const auto &gc = gm8Cache[lineLen];
                const auto lineCrc = common::util::crc8(msg.data(), msgBytes);
                for (std::uint8_t bit = 0; bit < 8; ++bit) {
                    GF2Row gf2row{};
                    auto t = static_cast<std::uint8_t>(((lineCrc >> bit) & 1U) ^ ((gc.c0 >> bit) & 1U));
                    for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
                        if (gc.gm[bit][slot]) { // NOLINT
                            gf2row[lc[slot] / 64] |= (std::uint64_t{1} << (lc[slot] % 64)); // NOLINT
                        }
                    }
                    gf2Rows_.push_back(gf2row);
                    gf2Target_.push_back(t);
                }
            } else if (crcBits == 16) {
                if (!gm16Cached[lineLen]) {
                    auto [gm, c0] = buildGenMatrix16ForLength(lineLen);
                    gm16Cache[lineLen] = {std::move(gm), c0};
                    gm16Cached[lineLen] = true;
                }
                const auto &gc = gm16Cache[lineLen];
                const auto lineCrc = common::util::crc16_ccitt(msg.data(), msgBytes);
                for (std::uint8_t bit = 0; bit < 16; ++bit) {
                    GF2Row gf2row{};
                    auto t = static_cast<std::uint8_t>(((lineCrc >> bit) & 1U) ^ ((gc.c0 >> bit) & 1U));
                    for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
                        if (gc.gm[bit][slot]) { // NOLINT
                            gf2row[lc[slot] / 64] |= (std::uint64_t{1} << (lc[slot] % 64)); // NOLINT
                        }
                    }
                    gf2Rows_.push_back(gf2row);
                    gf2Target_.push_back(t);
                }
            } else {
                if (!gmCached[lineLen]) {
                    auto [gm, c0] = buildGenMatrixForLength(lineLen);
                    gmCache[lineLen] = {std::move(gm), c0};
                    gmCached[lineLen] = true;
                }
                const auto &gc = gmCache[lineLen];
                const auto lineCrc = common::util::crc32_ieee(msg.data(), msgBytes);
                for (std::uint8_t bit = 0; bit < 32; ++bit) {
                    GF2Row gf2row{};
                    auto t = static_cast<std::uint8_t>(((lineCrc >> bit) & 1U) ^ ((gc.c0 >> bit) & 1U));
                    for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
                        if (gc.gm[bit][slot]) { // NOLINT
                            gf2row[lc[slot] / 64] |= (std::uint64_t{1} << (lc[slot] % 64)); // NOLINT
                        }
                    }
                    gf2Rows_.push_back(gf2row);
                    gf2Target_.push_back(t);
                }
            }
        }
    }

    // ── CRC-8 DH/XH range methods ────────────────────────────────────

    void CombinatorSolver::addDH8Range(
                                        const std::uint16_t minLen,
                                        const std::uint16_t maxLen) {
        struct GenInfo8 { std::vector<std::array<std::uint8_t, 256>> gm; std::uint8_t c0; };
        std::array<GenInfo8, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        for (std::uint16_t d = 0; d < kDiagCount; ++d) {
            const auto cells = diagCells(d);
            const auto dlen = static_cast<std::uint16_t>(cells.size());
            if (dlen < minLen || dlen > maxLen) { continue; }
            if (!cached[dlen]) {
                auto [gm, c0] = buildGenMatrix8ForLength(dlen);
                cache[dlen] = {std::move(gm), c0};
                cached[dlen] = true;
            }
            const auto &gi = cache[dlen];
            std::vector<std::uint8_t> diagBits(dlen);
            for (std::uint16_t j = 0; j < dlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                diagBits[j] = getOrig(r, c);
            }
            const auto diagCrc = crc8OfBits(diagBits.data(), dlen);
            for (std::uint8_t bit = 0; bit < 8; ++bit) {
                std::uint8_t adjT = static_cast<std::uint8_t>(
                    ((diagCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                GF2Row adjRow{};
                for (std::uint16_t j = 0; j < dlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0) {
                        if (cellState_[flat] == 1 && gi.gm[bit][j]) { adjT ^= 1; } // NOLINT
                    } else if (gi.gm[bit][j]) { // NOLINT
                        adjRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                gf2Rows_.push_back(adjRow);
                gf2Target_.push_back(adjT);
            }
        }
    }

    void CombinatorSolver::addXH8Range(
                                        const std::uint16_t minLen,
                                        const std::uint16_t maxLen) {
        struct GenInfo8 { std::vector<std::array<std::uint8_t, 256>> gm; std::uint8_t c0; };
        std::array<GenInfo8, 128> cache{};
        std::array<bool, 128> cached{};
        cached.fill(false);

        for (std::uint16_t x = 0; x < kDiagCount; ++x) {
            const auto cells = antiDiagCells(x);
            const auto xlen = static_cast<std::uint16_t>(cells.size());
            if (xlen < minLen || xlen > maxLen) { continue; }
            if (!cached[xlen]) {
                auto [gm, c0] = buildGenMatrix8ForLength(xlen);
                cache[xlen] = {std::move(gm), c0};
                cached[xlen] = true;
            }
            const auto &gi = cache[xlen];
            std::vector<std::uint8_t> xBits(xlen);
            for (std::uint16_t j = 0; j < xlen; ++j) {
                const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                xBits[j] = getOrig(r, c);
            }
            const auto xCrc = crc8OfBits(xBits.data(), xlen);
            for (std::uint8_t bit = 0; bit < 8; ++bit) {
                std::uint8_t adjT = static_cast<std::uint8_t>(
                    ((xCrc >> bit) & 1U) ^ ((gi.c0 >> bit) & 1U));
                GF2Row adjRow{};
                for (std::uint16_t j = 0; j < xlen; ++j) {
                    const auto flat = cells[j];
                    if (cellState_[flat] >= 0) {
                        if (cellState_[flat] == 1 && gi.gm[bit][j]) { adjT ^= 1; } // NOLINT
                    } else if (gi.gm[bit][j]) { // NOLINT
                        adjRow[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    }
                }
                gf2Rows_.push_back(adjRow);
                gf2Target_.push_back(adjT);
            }
        }
    }

    // ── Multi-phase cascade solve ───────────────────────────────────────

    auto CombinatorSolver::solveCascade() -> Result {
        // Hybrid phases: each phase uses the minimum CRC width that fully determines its length range
        struct PhaseSpec {
            std::uint16_t minLen;
            std::uint16_t maxLen;
            std::uint8_t hashBits;  // 8, 16, or 32
        };
        const auto maxS = static_cast<std::uint16_t>(kS);
        std::vector<PhaseSpec> phases;
        if (config_.hybridWidths) {
            phases = {{1, 8, 8}, {9, 16, 16}, {17, 32, 32}, {33, 64, 16}, {65, 96, 8}};
        } else if (config_.dhBits == 8) {
            phases = {{1, 8, 8}, {9, 16, 8}, {17, 32, 8}, {33, maxS, 8}};
        } else if (config_.dhBits == 16) {
            phases = {{1, 16, 16}, {17, 32, 16}, {33, 64, 16}, {65, maxS, 16}};
        } else {
            phases = {{1, 32, 32}, {33, 64, 32}, {65, 96, 32}, {97, maxS, 32}};
        }

        // Add VH from the start if configured
        if (config_.useVHInCascade) {
            if (config_.vhBits == 16) { addVH16(); } else { addVH(); }
            std::fprintf(stderr, "  VH%u added: +%u equations\n",
                         config_.vhBits, static_cast<unsigned>(kS * config_.vhBits));

        }

        // Add yLTP from the start if configured
        if (config_.useYLTP) {
            addYLTP();
            std::fprintf(stderr, "  yLTP added: +%u GF2 eq + %u integer lines\n",
                         static_cast<unsigned>(kS * 32), kS);
        }

        // Add rLTP from the start if configured
        if (config_.useRLTP) {
            addRLTP();
            std::fprintf(stderr, "  rLTP added: +%u GF2 eq + %u integer lines (center %u,%u)\n",
                         static_cast<unsigned>(kS * 32), kS, config_.rltpCenterR, config_.rltpCenterC);
        }

        Result result;
        result.free = kN;

        const auto cascadeLimit = (config_.cascadeMaxLen > 0)
            ? config_.cascadeMaxLen : static_cast<std::uint16_t>(kS);

        for (std::size_t phase = 0; phase < phases.size(); ++phase) {
            auto minLen = phases[phase].minLen; // NOLINT
            auto maxLen = phases[phase].maxLen; // NOLINT
            const auto phaseBits = phases[phase].hashBits; // NOLINT
            if (minLen > cascadeLimit) { break; }
            if (maxLen > cascadeLimit) { maxLen = cascadeLimit; }
            const auto prevDet = result.determined;

            // Add DH/XH equations for this phase's length range using phase-specific width
            const auto rowsBefore = gf2Rows_.size();
            if (phaseBits == 8) {
                addDH8Range(minLen, maxLen);
                addXH8Range(minLen, maxLen);
            } else if (phaseBits == 16) {
                addDH16Range(minLen, maxLen);
                addXH16Range(minLen, maxLen);
            } else {
                addDHRange(minLen, maxLen);
                addXHRange(minLen, maxLen);
            }
            const auto rowsAdded = gf2Rows_.size() - rowsBefore;

            std::fprintf(stderr, "\n  Phase %zu (diag len %u-%u, CRC-%u): +%zu GF2 equations (%zu total)\n",
                         phase + 1, minLen, maxLen, phaseBits, rowsAdded, gf2Rows_.size());

            // Run fixpoint to convergence
            for (std::uint32_t iter = 1; iter <= 200; ++iter) {
                const auto iterPrev = result.determined;

                auto [rank, detGF2] = gaussElim();
                result.rank = rank;
                std::vector<std::pair<std::uint32_t, std::uint8_t>> newGF2;
                for (const auto &[f, v] : detGF2) {
                    if (cellState_[f] < 0) {
                        assignCell(f, v);
                        newGF2.emplace_back(f, v);
                    }
                }
                result.fromGaussElim += static_cast<std::uint32_t>(newGF2.size());
                result.determined += static_cast<std::uint32_t>(newGF2.size());
                if (!newGF2.empty()) { propagateGF2(newGF2); }

                auto [detIB, inconIB] = intBound();
                if (inconIB) { result.feasible = false; return result; }
                result.fromIntBound += static_cast<std::uint32_t>(detIB.size());
                result.determined += static_cast<std::uint32_t>(detIB.size());
                if (!detIB.empty()) { propagateGF2(detIB); }

                auto [detCD, inconCD] = crossDeduce();
                if (inconCD) { result.feasible = false; return result; }
                result.fromCrossDeduce += static_cast<std::uint32_t>(detCD.size());
                result.determined += static_cast<std::uint32_t>(detCD.size());
                if (!detCD.empty()) { propagateGF2(detCD); }

                result.free = kN - result.determined;
                result.iterations++;

                std::fprintf(stderr,
                    "    Iter %u: |D|=%u (%.1f%%) [+%zu GF2, +%zu IB, +%zu CD] rank=%u\n",
                    result.iterations, result.determined,
                    100.0 * result.determined / kN,
                    newGF2.size(), detIB.size(), detCD.size(), rank);

                if (result.determined == iterPrev || result.determined == kN) { break; }
            }

            const auto phaseGain = result.determined - prevDet;
            std::fprintf(stderr, "  Phase %zu result: +%u cells (total %u = %.1f%%)\n",
                         phase + 1, phaseGain, result.determined,
                         100.0 * result.determined / kN);

            if (result.determined == kN) { break; }
            if (phaseGain == 0 && phase >= 1) {
                std::fprintf(stderr, "  Cascade stalled at phase %zu\n", phase + 1);
                break;
            }
        }

        // Verify cell values against original
        result.correct = true;
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            if (cellState_[flat] >= 0) {
                if (static_cast<std::uint8_t>(cellState_[flat]) != original_[flat]) {
                    result.correct = false;
                    break;
                }
            }
        }

        // SHA-256 block hash verification (only if all cells assigned)
        {
            bool allAssigned = true;
            for (std::uint32_t flat = 0; flat < kN; ++flat) {
                if (cellState_[flat] < 0) { allAssigned = false; break; } // NOLINT
            }
            if (allAssigned) {
                if constexpr (kS <= 127) {
                    common::Csm reconstructed;
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            reconstructed.set(r, c, static_cast<std::uint8_t>(
                                cellState_[(static_cast<std::uint32_t>(r) * kS) + c])); // NOLINT
                        }
                    }
                    result.bhVerified = common::BlockHash::verify(reconstructed, expectedBH_);
                } else {
                    common::CsmVariable reconstructed(kS);
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            reconstructed.set(r, c, static_cast<std::uint8_t>(
                                cellState_[(static_cast<std::uint32_t>(r) * kS) + c])); // NOLINT
                        }
                    }
                    const auto buf = reconstructed.serialize();
                    result.bhVerified = common::BlockHash::verify(buf.data(), buf.size(), expectedBH_);
                }
            }
        }

        return result;
    }

    // ── Pre-assignment (overlap donation) ─────────────────────────────

    void CombinatorSolver::preAssign(const std::uint16_t r, const std::uint16_t c,
                                      const std::uint8_t v) {
        const auto flat = static_cast<std::uint32_t>(r) * kS + c;
        if (cellState_[flat] >= 0) { return; } // already assigned
        assignCell(flat, v);
        // Propagate into GF(2) system
        const auto word = flat / 64;
        const auto bit = std::uint64_t{1} << (flat % 64);
        for (std::size_t i = 0; i < gf2Rows_.size(); ++i) {
            if (gf2Rows_[i][word] & bit) { // NOLINT
                if (v == 1) { gf2Target_[i] ^= 1; }
                gf2Rows_[i][word] &= ~bit; // NOLINT
            }
        }
    }

    // ── Per-line analysis ────────────────────────────────────────────────

    auto CombinatorSolver::analyzeLines() const -> LineStats {
        LineStats ls;

        // Rows
        for (std::uint16_t r = 0; r < kS; ++r) {
            bool complete = true;
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (cellState_[static_cast<std::uint32_t>(r) * kS + c] < 0) {
                    complete = false;
                    break;
                }
            }
            if (complete) { ++ls.rowsComplete; }
        }

        // Columns + VH verification
        for (std::uint16_t c = 0; c < kS; ++c) {
            bool complete = true;
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (cellState_[static_cast<std::uint32_t>(r) * kS + c] < 0) {
                    complete = false;
                    break;
                }
            }
            if (complete) {
                ++ls.colsComplete;
                // VH32 verification
                std::array<std::uint8_t, 32> msg{};
                for (std::uint16_t r = 0; r < kS; ++r) {
                    if (cellState_[static_cast<std::uint32_t>(r) * kS + c] == 1) {
                        msg[r / 8] |= static_cast<std::uint8_t>(1U << (7U - (r % 8U))); // NOLINT
                    }
                }
                const auto solvedCrc = common::util::crc32_ieee(msg.data(), static_cast<std::size_t>((kS + 1 + 7) / 8));
                // Compute expected from original
                std::array<std::uint8_t, 16> origMsg{};
                for (std::uint16_t r = 0; r < kS; ++r) {
                    if (getOrig(r, c)) {
                        origMsg[r / 8] |= static_cast<std::uint8_t>(1U << (7U - (r % 8U))); // NOLINT
                    }
                }
                const auto expectedCrc = common::util::crc32_ieee(origMsg.data(), origMsg.size());
                if (solvedCrc == expectedCrc) { ++ls.vhVerified; }
            }
        }

        // DSM diagonals + DH verification
        for (std::uint16_t d = 0; d < kDiagCount; ++d) {
            const auto cells = diagCells(d);
            const auto dlen = static_cast<std::uint16_t>(cells.size());
            ls.diagTotalByLength[dlen]++; // NOLINT

            bool complete = true;
            for (const auto flat : cells) {
                if (cellState_[flat] < 0) { complete = false; break; }
            }
            if (complete) {
                ++ls.diagsComplete;
                ls.diagCompleteByLength[dlen]++; // NOLINT

                // DH16 verification (if dlen <= 64, i.e., in DH64)
                if (dlen <= 64) {
                    std::vector<std::uint8_t> bits(dlen);
                    std::vector<std::uint8_t> origBits(dlen);
                    for (std::uint16_t j = 0; j < dlen; ++j) {
                        bits[j] = static_cast<std::uint8_t>(cellState_[cells[j]]);
                        const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                        const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                        origBits[j] = getOrig(r, c);
                    }
                    const auto solvedCrc = crc16OfBits(bits.data(), dlen);
                    const auto expectedCrc = crc16OfBits(origBits.data(), dlen);
                    if (solvedCrc == expectedCrc) { ++ls.dhVerified; }
                }
            }
        }

        // XSM anti-diagonals + XH verification
        for (std::uint16_t x = 0; x < kDiagCount; ++x) {
            const auto cells = antiDiagCells(x);
            const auto xlen = static_cast<std::uint16_t>(cells.size());
            ls.antiDiagTotalByLength[xlen]++; // NOLINT

            bool complete = true;
            for (const auto flat : cells) {
                if (cellState_[flat] < 0) { complete = false; break; }
            }
            if (complete) {
                ++ls.antiDiagsComplete;
                ls.antiDiagCompleteByLength[xlen]++; // NOLINT

                if (xlen <= 64) {
                    std::vector<std::uint8_t> bits(xlen);
                    std::vector<std::uint8_t> origBits(xlen);
                    for (std::uint16_t j = 0; j < xlen; ++j) {
                        bits[j] = static_cast<std::uint8_t>(cellState_[cells[j]]);
                        const auto r = static_cast<std::uint16_t>(cells[j] / kS);
                        const auto c = static_cast<std::uint16_t>(cells[j] % kS);
                        origBits[j] = getOrig(r, c);
                    }
                    const auto solvedCrc = crc16OfBits(bits.data(), xlen);
                    const auto expectedCrc = crc16OfBits(origBits.data(), xlen);
                    if (solvedCrc == expectedCrc) { ++ls.xhVerified; }
                }
            }
        }

        return ls;
    }

    // ── GaussElim ───────────────────────────────────────────────────────

    auto CombinatorSolver::gaussElim()
        -> std::pair<std::uint32_t, std::vector<std::pair<std::uint32_t, std::uint8_t>>> {

        const auto m = static_cast<std::uint32_t>(gf2Rows_.size());

        // Work on copies
        auto rows = gf2Rows_;
        auto target = gf2Target_;

        std::vector<std::int32_t> pivotCol(m, -1);
        std::uint32_t pivotRow = 0;

        for (std::uint32_t col = 0; col < kN && pivotRow < m; ++col) {
            // Skip already-determined cells
            if (cellState_[col] >= 0) { continue; }

            const auto word = col / 64;
            const auto bit = std::uint64_t{1} << (col % 64);

            // Find pivot
            std::int32_t found = -1;
            for (std::uint32_t r = pivotRow; r < m; ++r) {
                if (rows[r][word] & bit) { // NOLINT
                    found = static_cast<std::int32_t>(r);
                    break;
                }
            }
            if (found < 0) { continue; }

            // Swap
            if (static_cast<std::uint32_t>(found) != pivotRow) {
                std::swap(rows[pivotRow], rows[found]); // NOLINT
                std::swap(target[pivotRow], target[found]); // NOLINT
            }

            // Eliminate
            for (std::uint32_t r = 0; r < m; ++r) {
                if (r != pivotRow && (rows[r][word] & bit)) { // NOLINT
                    for (std::uint32_t w = 0; w < kWordsPerRow; ++w) {
                        rows[r][w] ^= rows[pivotRow][w]; // NOLINT
                    }
                    target[r] ^= target[pivotRow]; // NOLINT
                }
            }

            pivotCol[pivotRow] = static_cast<std::int32_t>(col); // NOLINT
            ++pivotRow;
        }

        const auto rank = pivotRow;

        // Identify free columns (undetermined cells that are not pivots)
        std::vector<bool> isPivot(kN, false);
        for (std::uint32_t i = 0; i < rank; ++i) {
            if (pivotCol[i] >= 0) {
                isPivot[static_cast<std::uint32_t>(pivotCol[i])] = true;
            }
        }

        // Extract determined cells: pivot rows with no free-column dependencies
        std::vector<std::pair<std::uint32_t, std::uint8_t>> determined;
        for (std::uint32_t i = 0; i < rank; ++i) {
            if (pivotCol[i] < 0) { continue; }
            const auto pcol = static_cast<std::uint32_t>(pivotCol[i]);
            bool hasFree = false;
            for (std::uint32_t col = 0; col < kN; ++col) {
                if (col == pcol) { continue; }
                if (cellState_[col] >= 0) { continue; } // already determined
                if (isPivot[col]) { continue; } // another pivot, not free
                // Wait — this isn't right. Free columns are non-pivot undetermined columns.
                // But we need to check if THIS row has a 1 in any free column.
                if (!isPivot[col] && (rows[i][col / 64] >> (col % 64)) & 1U) { // NOLINT
                    hasFree = true;
                    break;
                }
            }
            if (!hasFree && cellState_[pcol] < 0) {
                determined.emplace_back(pcol, target[i]); // NOLINT
            }
        }

        return {rank, determined};
    }

    // ── IntBound ────────────────────────────────────────────────────────

    auto CombinatorSolver::intBound()
        -> std::pair<std::vector<std::pair<std::uint32_t, std::uint8_t>>, bool> {

        std::vector<std::pair<std::uint32_t, std::uint8_t>> determined;
        const auto nLines = static_cast<std::uint32_t>(intLines_.size());

        std::vector<std::uint32_t> queue;
        queue.reserve(nLines);
        std::vector<bool> inQueue(nLines, false);
        for (std::uint32_t i = 0; i < nLines; ++i) {
            queue.push_back(i);
            inQueue[i] = true;
        }

        std::size_t head = 0;
        while (head < queue.size()) {
            const auto li = queue[head++];
            inQueue[li] = false;

            auto &line = intLines_[li];
            if (line.u == 0) { continue; }
            if (line.rho < 0 || line.rho > line.u) { return {{}, true}; }

            std::int8_t fv = -1;
            if (line.rho == 0) { fv = 0; }
            else if (line.rho == line.u) { fv = 1; }
            if (fv < 0) { continue; }

            for (const auto flat : line.cells) {
                if (cellState_[flat] >= 0) { continue; }
                assignCell(flat, static_cast<std::uint8_t>(fv));
                determined.emplace_back(flat, static_cast<std::uint8_t>(fv));
                for (const auto li2 : cellToLines_[flat]) {
                    if (!inQueue[li2]) {
                        queue.push_back(li2);
                        inQueue[li2] = true;
                    }
                }
            }
        }

        return {determined, false};
    }

    // ── CrossDeduce ─────────────────────────────────────────────────────

    auto CombinatorSolver::crossDeduce()
        -> std::pair<std::vector<std::pair<std::uint32_t, std::uint8_t>>, bool> {

        std::vector<std::pair<std::uint32_t, std::uint8_t>> determined;

        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            if (cellState_[flat] >= 0) { continue; }

            std::int8_t forced = -1;
            for (std::uint8_t v = 0; v <= 1; ++v) {
                bool feasible = true;
                for (const auto li : cellToLines_[flat]) {
                    const auto newRho = intLines_[li].rho - v;
                    const auto newU = intLines_[li].u - 1;
                    if (newRho < 0 || newRho > newU) {
                        feasible = false;
                        break;
                    }
                }
                if (!feasible) {
                    if (forced >= 0) { return {{}, true}; } // both infeasible
                    forced = static_cast<std::int8_t>(1 - v);
                }
            }
            if (forced >= 0) {
                determined.emplace_back(flat, static_cast<std::uint8_t>(forced));
            }
        }

        // Apply
        for (const auto &[flat, v] : determined) {
            if (cellState_[flat] < 0) {
                assignCell(flat, v);
            }
        }

        return {determined, false};
    }

    // ── Cell assignment ─────────────────────────────────────────────────

    void CombinatorSolver::assignCell(const std::uint32_t flat, const std::uint8_t v) {
        cellState_[flat] = static_cast<std::int8_t>(v);
        for (const auto li : cellToLines_[flat]) {
            intLines_[li].u -= 1;
            intLines_[li].rho -= v;
        }
    }

    // ── GF(2) propagation ───────────────────────────────────────────────

    void CombinatorSolver::propagateGF2(
        const std::vector<std::pair<std::uint32_t, std::uint8_t>> &determined) {
        for (const auto &[flat, v] : determined) {
            const auto word = flat / 64;
            const auto bit = std::uint64_t{1} << (flat % 64);
            for (std::size_t i = 0; i < gf2Rows_.size(); ++i) {
                if (gf2Rows_[i][word] & bit) { // NOLINT
                    if (v == 1) { gf2Target_[i] ^= 1; }
                    gf2Rows_[i][word] &= ~bit; // NOLINT
                }
            }
        }
    }

    // ── Fixpoint ────────────────────────────────────────────────────────

    auto CombinatorSolver::solve() -> Result {
        Result result;
        result.free = kN;

        for (std::uint32_t iter = 1; iter <= 200; ++iter) {
            const auto prevDet = result.determined;

            // GaussElim
            auto [rank, detGF2] = gaussElim();
            result.rank = rank;
            // Filter already-determined
            std::vector<std::pair<std::uint32_t, std::uint8_t>> newGF2;
            for (const auto &[f, v] : detGF2) {
                if (cellState_[f] < 0) {
                    assignCell(f, v);
                    newGF2.emplace_back(f, v);
                }
            }
            result.fromGaussElim += static_cast<std::uint32_t>(newGF2.size());
            result.determined += static_cast<std::uint32_t>(newGF2.size());
            if (!newGF2.empty()) { propagateGF2(newGF2); }

            // IntBound
            auto [detIB, inconIB] = intBound();
            if (inconIB) {
                result.feasible = false;
                std::fprintf(stderr, "  Iter %u: INCONSISTENT in IntBound\n", iter);
                break;
            }
            result.fromIntBound += static_cast<std::uint32_t>(detIB.size());
            result.determined += static_cast<std::uint32_t>(detIB.size());
            if (!detIB.empty()) { propagateGF2(detIB); }

            // CrossDeduce
            auto [detCD, inconCD] = crossDeduce();
            if (inconCD) {
                result.feasible = false;
                std::fprintf(stderr, "  Iter %u: INCONSISTENT in CrossDeduce\n", iter);
                break;
            }
            result.fromCrossDeduce += static_cast<std::uint32_t>(detCD.size());
            result.determined += static_cast<std::uint32_t>(detCD.size());
            if (!detCD.empty()) { propagateGF2(detCD); }

            result.free = kN - result.determined;
            result.iterations = iter;

            std::fprintf(stderr,
                "  Iter %u: |D|=%u (%.1f%%) |F|=%u "
                "[+%zu GF2, +%zu IB, +%zu CD] rank=%u\n",
                iter, result.determined,
                100.0 * result.determined / kN,
                result.free,
                newGF2.size(), detIB.size(), detCD.size(),
                rank);

            if (result.determined == prevDet || result.determined == kN) { break; }
        }

        // Verify cell values against original
        result.correct = true;
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            if (cellState_[flat] >= 0) { // NOLINT
                if (static_cast<std::uint8_t>(cellState_[flat]) != original_[flat]) { // NOLINT
                    result.correct = false;
                    break;
                }
            }
        }

        // SHA-256 block hash verification (only if all cells assigned)
        {
            bool allAssigned = true;
            for (std::uint32_t flat = 0; flat < kN; ++flat) {
                if (cellState_[flat] < 0) { allAssigned = false; break; } // NOLINT
            }
            if (allAssigned) {
                if constexpr (kS <= 127) {
                    common::Csm reconstructed;
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            reconstructed.set(r, c, static_cast<std::uint8_t>(
                                cellState_[(static_cast<std::uint32_t>(r) * kS) + c])); // NOLINT
                        }
                    }
                    result.bhVerified = common::BlockHash::verify(reconstructed, expectedBH_);
                } else {
                    common::CsmVariable reconstructed(kS);
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            reconstructed.set(r, c, static_cast<std::uint8_t>(
                                cellState_[(static_cast<std::uint32_t>(r) * kS) + c])); // NOLINT
                        }
                    }
                    const auto buf = reconstructed.serialize();
                    result.bhVerified = common::BlockHash::verify(buf.data(), buf.size(), expectedBH_);
                }
            }
        }

        return result;
    }

    // ── Hybrid solver (Phase II) helpers ─────────────────────────────

    void CombinatorSolver::unassignCell(const std::uint32_t flat) {
        const auto v = static_cast<std::uint8_t>(cellState_[flat]); // NOLINT
        cellState_[flat] = -1; // NOLINT
        for (const auto li : cellToLines_[flat]) { // NOLINT
            intLines_[li].u += 1; // NOLINT
            intLines_[li].rho += v; // NOLINT
        }
    }

    bool CombinatorSolver::verifyRowLH(const std::uint16_t r) const {
        const auto baseFlat = static_cast<std::uint32_t>(r) * kS;
        std::array<std::uint8_t, 32> msg{};
        for (std::uint16_t c = 0; c < kS; ++c) {
            if (cellState_[baseFlat + c] == 1) { // NOLINT
                msg[c / 8] |= static_cast<std::uint8_t>(1U << (7U - (c % 8U))); // NOLINT
            }
        }
        const auto crc = common::util::crc32_ieee(msg.data(),
            static_cast<std::size_t>((kS + 1 + 7) / 8));
        return crc == expectedLH_[r]; // NOLINT
    }

    std::vector<CombinatorSolver::IntLineState> CombinatorSolver::snapshotIntLines() const {
        std::vector<IntLineState> snap(intLines_.size());
        for (std::size_t i = 0; i < intLines_.size(); ++i) {
            snap[i] = {intLines_[i].rho, intLines_[i].u}; // NOLINT
        }
        return snap;
    }

    void CombinatorSolver::restoreIntLines(const std::vector<IntLineState> &snap) {
        for (std::size_t i = 0; i < intLines_.size(); ++i) {
            intLines_[i].rho = snap[i].rho; // NOLINT
            intLines_[i].u = snap[i].u; // NOLINT
        }
    }

    void CombinatorSolver::runFixpoint(Result &result) {
        for (std::uint32_t iter = 1; iter <= 200; ++iter) {
            const auto prevDet = result.determined;

            auto [rank, detGF2] = gaussElim();
            result.rank = rank;
            std::vector<std::pair<std::uint32_t, std::uint8_t>> newGF2;
            for (const auto &[f, v] : detGF2) {
                if (cellState_[f] < 0) { // NOLINT
                    assignCell(f, v);
                    newGF2.emplace_back(f, v);
                }
            }
            result.fromGaussElim += static_cast<std::uint32_t>(newGF2.size());
            result.determined += static_cast<std::uint32_t>(newGF2.size());
            if (!newGF2.empty()) { propagateGF2(newGF2); }

            auto [detIB, inconIB] = intBound();
            if (inconIB) { result.feasible = false; return; }
            result.fromIntBound += static_cast<std::uint32_t>(detIB.size());
            result.determined += static_cast<std::uint32_t>(detIB.size());
            if (!detIB.empty()) { propagateGF2(detIB); }

            auto [detCD, inconCD] = crossDeduce();
            if (inconCD) { result.feasible = false; return; }
            result.fromCrossDeduce += static_cast<std::uint32_t>(detCD.size());
            result.determined += static_cast<std::uint32_t>(detCD.size());
            if (!detCD.empty()) { propagateGF2(detCD); }

            result.free = kN - result.determined;
            if (result.determined == prevDet || result.determined == kN) { break; }
        }
    }

    bool CombinatorSolver::dfsRow(const std::vector<std::uint32_t> &unknowns,
                                   const std::uint32_t idx, const std::uint16_t r) {
        if (idx == static_cast<std::uint32_t>(unknowns.size())) {
            return verifyRowLH(r);
        }
        const auto flat = unknowns[idx]; // NOLINT
        for (std::uint8_t v = 0; v <= 1; ++v) {
            bool feasible = true;
            for (const auto li : cellToLines_[flat]) { // NOLINT
                const auto newRho = intLines_[li].rho - static_cast<std::int32_t>(v); // NOLINT
                const auto newU = intLines_[li].u - 1; // NOLINT
                if (newRho < 0 || newRho > newU) { feasible = false; break; }
            }
            if (!feasible) { continue; }
            assignCell(flat, v);
            if (dfsRow(unknowns, idx + 1, r)) { return true; }
            unassignCell(flat);
        }
        return false;
    }

    // ── Generic line CRC verification ───────────────────────────────

    bool CombinatorSolver::verifyLineCrc(const std::vector<std::uint32_t> &cells,
                                          const std::uint32_t expectedCrc,
                                          const std::uint8_t crcWidth) const {
        const auto lineLen = static_cast<std::uint16_t>(cells.size());
        const auto msgBytes = static_cast<std::size_t>((lineLen + 1 + 7) / 8);
        std::vector<std::uint8_t> msg(msgBytes, 0);
        for (std::uint16_t slot = 0; slot < lineLen; ++slot) {
            if (cellState_[cells[slot]] == 1) { // NOLINT
                msg[slot / 8] |= static_cast<std::uint8_t>(1U << (7U - (slot % 8U))); // NOLINT
            }
        }
        std::uint32_t crc = 0;
        if (crcWidth == 8) { crc = common::util::crc8(msg.data(), msgBytes); }
        else if (crcWidth == 16) { crc = common::util::crc16_ccitt(msg.data(), msgBytes); }
        else { crc = common::util::crc32_ieee(msg.data(), msgBytes); }
        return crc == expectedCrc;
    }

    // ── Generic line DFS with CRC verification ────────────────────────

    bool CombinatorSolver::dfsLine(const std::vector<std::uint32_t> &unknowns,
                                    const std::uint32_t idx,
                                    const std::vector<std::uint32_t> &allCells,
                                    const std::uint32_t expectedCrc,
                                    const std::uint8_t crcWidth) {
        // Per-line node limit: abort if DFS explores too many nodes
        static thread_local std::uint64_t dfsNodes_; // NOLINT
        static constexpr std::uint64_t kMaxDfsNodes = 50'000'000;
        if (idx == 0) { dfsNodes_ = 0; }
        if (dfsNodes_ > kMaxDfsNodes) { return false; }

        if (idx == static_cast<std::uint32_t>(unknowns.size())) {
            return verifyLineCrc(allCells, expectedCrc, crcWidth);
        }
        const auto flat = unknowns[idx]; // NOLINT
        for (std::uint8_t v = 0; v <= 1; ++v) {
            ++dfsNodes_;
            if (dfsNodes_ > kMaxDfsNodes) { return false; }
            bool feasible = true;
            for (const auto li : cellToLines_[flat]) { // NOLINT
                const auto newRho = intLines_[li].rho - static_cast<std::int32_t>(v); // NOLINT
                const auto newU = intLines_[li].u - 1; // NOLINT
                if (newRho < 0 || newRho > newU) { feasible = false; break; }
            }
            if (!feasible) { continue; }
            assignCell(flat, v);
            if (dfsLine(unknowns, idx + 1, allCells, expectedCrc, crcWidth)) { return true; }
            unassignCell(flat);
        }
        return false;
    }

    // ── Multi-phase multi-axis hybrid solver ──────────────────────────

    auto CombinatorSolver::solveHybrid() -> Result {
        // Phase I: combinator algebra (initial)
        Result result = solveCascade();
        if (result.determined == kN || !result.feasible) { return result; }

        std::fprintf(stderr, "\n=== B.63c Multi-Phase Multi-Axis Solver ===\n"); // NOLINT

        static constexpr std::uint16_t kMaxDfsUnknowns = 127;

        // Helper: run one axis DFS phase. Returns number of lines solved.
        auto runAxisPhase = [&](const char *axisName,
                                const auto &lineSource, // callable: lineSource(idx) -> {cells, crc, width}
                                std::uint32_t lineCount) -> std::uint32_t {
            // Collect lines sorted by unknown count ascending
            struct LineEntry { std::uint32_t idx; std::uint16_t unknowns; };
            std::vector<LineEntry> candidates;
            for (std::uint32_t i = 0; i < lineCount; ++i) {
                const auto &[cells, crc, width] = lineSource(i);
                std::uint16_t unk = 0;
                for (const auto flat : cells) {
                    if (cellState_[flat] < 0) { ++unk; } // NOLINT
                }
                if (unk > 0 && unk <= kMaxDfsUnknowns) {
                    candidates.push_back({i, unk});
                }
            }
            std::sort(candidates.begin(), candidates.end(),
                      [](const LineEntry &a, const LineEntry &b) { return a.unknowns < b.unknowns; });

            std::uint32_t solved = 0;
            for (const auto &[li, initUnk] : candidates) {
                const auto &[cells, expectedCrc, crcWidth] = lineSource(li);
                // Recompute unknowns (may have changed from prior solves in this phase)
                std::vector<std::uint32_t> unknowns;
                for (const auto flat : cells) {
                    if (cellState_[flat] < 0) { unknowns.push_back(flat); } // NOLINT
                }
                if (unknowns.empty()) { ++solved; continue; } // already complete
                if (unknowns.size() > kMaxDfsUnknowns) { continue; }

                if (dfsLine(unknowns, 0, cells, expectedCrc, crcWidth)) {
                    ++solved;
                    result.determined += static_cast<std::uint32_t>(unknowns.size());
                    result.free = kN - result.determined;
                }
            }
            return solved;
        };

        // Line accessors for each axis
        auto rowLine = [&](std::uint32_t r) -> std::tuple<
                const std::vector<std::uint32_t>&, std::uint32_t, std::uint8_t> {
            // Build row cells on the fly (lightweight)
            static thread_local std::vector<std::uint32_t> rowCells; // NOLINT
            rowCells.resize(kS);
            for (std::uint16_t c = 0; c < kS; ++c) {
                rowCells[c] = static_cast<std::uint32_t>(r) * kS + c;
            }
            return {rowCells, expectedLH_[r], 32}; // NOLINT
        };

        auto colLine = [&](std::uint32_t c) -> std::tuple<
                const std::vector<std::uint32_t>&, std::uint32_t, std::uint8_t> {
            static thread_local std::vector<std::uint32_t> colCells; // NOLINT
            colCells.resize(kS);
            for (std::uint16_t r = 0; r < kS; ++r) {
                colCells[r] = static_cast<std::uint32_t>(r) * kS + static_cast<std::uint32_t>(c);
            }
            return {colCells, expectedVH_[c], 32}; // NOLINT
        };

        auto diagLine = [&](std::uint32_t d) -> std::tuple<
                const std::vector<std::uint32_t>&, std::uint32_t, std::uint8_t> {
            return {diagAxes_[d].cells, diagAxes_[d].expectedCrc, diagAxes_[d].crcWidth}; // NOLINT
        };

        auto antiDiagLine = [&](std::uint32_t x) -> std::tuple<
                const std::vector<std::uint32_t>&, std::uint32_t, std::uint8_t> {
            return {antiDiagAxes_[x].cells, antiDiagAxes_[x].expectedCrc, antiDiagAxes_[x].crcWidth}; // NOLINT
        };

        // Outer loop: cycle through all phases until no progress
        for (std::uint32_t round = 1; round <= 100; ++round) {
            const auto detBefore = result.determined;
            std::fprintf(stderr, "\n--- Round %u (determined: %u / %u = %.1f%%) ---\n", // NOLINT
                         round, result.determined, kN, 100.0 * result.determined / kN);

            // Phase I: combinator fixpoint
            {
                const auto before = result.determined;
                runFixpoint(result);
                const auto gained = result.determined - before;
                std::fprintf(stderr, "  Phase I (combinator):     +%u cells (total %u = %.1f%%)\n", // NOLINT
                             gained, result.determined, 100.0 * result.determined / kN);
                if (result.determined == kN) { break; }
            }

            // Phase II: row DFS (LH)
            {
                const auto before = result.determined;
                const auto solved = runAxisPhase("row", rowLine, kS);
                const auto gained = result.determined - before;
                std::fprintf(stderr, "  Phase II (rows, LH):      %u lines solved, +%u cells\n", // NOLINT
                             solved, gained);
                if (gained > 0) { continue; } // restart from Phase I
                if (result.determined == kN) { break; }
            }

            // Phase III: column DFS (VH)
            {
                const auto before = result.determined;
                const auto solved = runAxisPhase("column", colLine, kS);
                const auto gained = result.determined - before;
                std::fprintf(stderr, "  Phase III (cols, VH):     %u lines solved, +%u cells\n", // NOLINT
                             solved, gained);
                if (gained > 0) { continue; }
                if (result.determined == kN) { break; }
            }

            // Phase IV: diagonal DFS (DH)
            {
                const auto before = result.determined;
                const auto solved = runAxisPhase("diagonal", diagLine,
                    static_cast<std::uint32_t>(diagAxes_.size()));
                const auto gained = result.determined - before;
                std::fprintf(stderr, "  Phase IV (diags, DH):     %u lines solved, +%u cells\n", // NOLINT
                             solved, gained);
                if (gained > 0) { continue; }
                if (result.determined == kN) { break; }
            }

            // Phase V: anti-diagonal DFS (XH)
            {
                const auto before = result.determined;
                const auto solved = runAxisPhase("anti-diagonal", antiDiagLine,
                    static_cast<std::uint32_t>(antiDiagAxes_.size()));
                const auto gained = result.determined - before;
                std::fprintf(stderr, "  Phase V (anti-diags, XH): %u lines solved, +%u cells\n", // NOLINT
                             solved, gained);
                if (gained > 0) { continue; }
                if (result.determined == kN) { break; }
            }

            // No phase made progress — stalled
            if (result.determined == detBefore) {
                std::fprintf(stderr, "  No progress in round %u. Stalled.\n", round); // NOLINT
                break;
            }
        }


        // ── Phase VI: Random restart census (B.63h) ─────────────────────
        if (result.determined < kN) {
            const auto baseCellState = cellState_;
            const auto baseIntLines = snapshotIntLines();
            const auto baseDetermined = result.determined;

            std::fprintf(stderr, "\n--- Phase VI: Random restart census (B.63h) ---\n"); // NOLINT
            std::fprintf(stderr, "Baseline: %u / %u determined (%.1f%%)\n", // NOLINT
                         baseDetermined, kN, 100.0 * baseDetermined / kN);

            // IntBound propagation helper
            auto propagateIB = [&]() -> std::pair<std::uint32_t, bool> {
                std::uint32_t forced = 0;
                bool changed = true;
                while (changed) {
                    changed = false;
                    for (auto &il : intLines_) {
                        if (il.u <= 0) { continue; }
                        if (il.rho < 0 || il.rho > il.u) { return {forced, false}; }
                        if (il.rho == 0) {
                            for (const auto f : il.cells) {
                                if (cellState_[f] < 0) { assignCell(f, 0); ++forced; changed = true; } // NOLINT
                            }
                        } else if (il.rho == il.u) {
                            for (const auto f : il.cells) {
                                if (cellState_[f] < 0) { assignCell(f, 1); ++forced; changed = true; } // NOLINT
                            }
                        }
                    }
                }
                for (const auto &il : intLines_) {
                    if (il.rho < 0 || il.rho > il.u) { return {forced, false}; }
                }
                return {forced, true};
            };

            static constexpr std::uint32_t kMaxDecPerRestart = 5'000'000;
            static constexpr std::uint32_t kNumRestarts = 10'000;

            // Run one restart, return peak determination
            auto runRestart = [&](const std::vector<std::uint32_t> &order)
                -> std::tuple<std::uint32_t, std::uint32_t, std::uint64_t, std::uint64_t, bool> {
                cellState_ = baseCellState;
                restoreIntLines(baseIntLines);

                struct Dec { std::uint32_t flat; std::uint8_t value;
                    std::array<std::int8_t, kN> cs; std::vector<IntLineState> is; };
                std::vector<Dec> stack;
                std::uint64_t decisions = 0, backtracks = 0;
                std::uint32_t peakDet = baseDetermined, orderIdx = 0;

                while (decisions < kMaxDecPerRestart) {
                    std::uint32_t det = 0;
                    for (std::uint32_t f = 0; f < kN; ++f) { if (cellState_[f] >= 0) { ++det; } } // NOLINT
                    if (det > peakDet) { peakDet = det; }
                    if (det == kN) { return {peakDet, static_cast<std::uint32_t>(stack.size()), decisions, backtracks, true}; }

                    while (orderIdx < static_cast<std::uint32_t>(order.size()) &&
                           cellState_[order[orderIdx]] >= 0) { ++orderIdx; } // NOLINT
                    if (orderIdx >= static_cast<std::uint32_t>(order.size())) { break; }
                    const auto flat = order[orderIdx]; // NOLINT

                    ++decisions;
                    stack.push_back({flat, 0, cellState_, snapshotIntLines()});
                    assignCell(flat, 0);
                    auto [f1, ok1] = propagateIB();
                    if (ok1) {
                        const auto r = static_cast<std::uint16_t>(flat / kS);
                        bool rc = true;
                        const auto base = static_cast<std::uint32_t>(r) * kS;
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            if (cellState_[base + c] < 0) { rc = false; break; } // NOLINT
                        }
                        if (rc && !verifyRowLH(r)) { ok1 = false; }
                    }
                    if (!ok1) {
                        cellState_ = stack.back().cs; restoreIntLines(stack.back().is);
                        stack.back().value = 1;
                        assignCell(flat, 1);
                        auto [f2, ok2] = propagateIB();
                        if (ok2) {
                            const auto r = static_cast<std::uint16_t>(flat / kS);
                            bool rc = true;
                            const auto base = static_cast<std::uint32_t>(r) * kS;
                            for (std::uint16_t c = 0; c < kS; ++c) {
                                if (cellState_[base + c] < 0) { rc = false; break; } // NOLINT
                            }
                            if (rc && !verifyRowLH(r)) { ok2 = false; }
                        }
                        if (!ok2) {
                            ++backtracks;
                            cellState_ = stack.back().cs; restoreIntLines(stack.back().is);
                            stack.pop_back();
                            while (!stack.empty()) {
                                auto &prev = stack.back();
                                if (prev.value == 0) {
                                    cellState_ = prev.cs; restoreIntLines(prev.is);
                                    prev.value = 1;
                                    assignCell(prev.flat, 1);
                                    auto [f3, ok3] = propagateIB();
                                    if (ok3) {
                                        const auto rr = static_cast<std::uint16_t>(prev.flat / kS);
                                        bool rc = true;
                                        const auto bb = static_cast<std::uint32_t>(rr) * kS;
                                        for (std::uint16_t c = 0; c < kS; ++c) {
                                            if (cellState_[bb + c] < 0) { rc = false; break; } // NOLINT
                                        }
                                        if (rc && !verifyRowLH(rr)) { ok3 = false; }
                                    }
                                    if (ok3) { break; }
                                }
                                ++backtracks;
                                cellState_ = prev.cs; restoreIntLines(prev.is);
                                stack.pop_back();
                            }
                            if (stack.empty()) { break; }
                        }
                    }
                }
                return {peakDet, static_cast<std::uint32_t>(stack.size()), decisions, backtracks, false};
            };

            // Collect unknowns
            std::vector<std::uint32_t> unknowns;
            for (std::uint32_t f = 0; f < kN; ++f) {
                if (baseCellState[f] < 0) { unknowns.push_back(f); } // NOLINT
            }

            std::uint32_t bestPeak = 0, bestRestart = 0;
            std::uint64_t seed = 77777;

            for (std::uint32_t restart = 0; restart < kNumRestarts; ++restart) {
                // Shuffle
                auto order = unknowns;
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                for (std::size_t i = order.size() - 1; i > 0; --i) {
                    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                    const auto j = static_cast<std::size_t>(seed >> 33) % (i + 1); // NOLINT
                    std::swap(order[i], order[j]); // NOLINT
                }

                auto [peak, depth, decisions, backtracks, solved] = runRestart(order);
                if (peak > bestPeak) { bestPeak = peak; bestRestart = restart; }

                std::fprintf(stderr, "  restart=%u peak=%u (%.1f%%) depth=%u decisions=%llu backtracks=%llu solved=%s\n", // NOLINT
                             restart, peak, 100.0 * peak / kN, depth,
                             static_cast<unsigned long long>(decisions),
                             static_cast<unsigned long long>(backtracks),
                             solved ? "true" : "false");

                if (solved) {
                    result.determined = kN; result.free = 0;
                    std::fprintf(stderr, "  SOLVED on restart %u!\n", restart); // NOLINT
                    break;
                }
            }

            std::fprintf(stderr, "\nCensus: best restart=%u peak=%u (%.1f%%)\n", // NOLINT
                         bestRestart, bestPeak, 100.0 * bestPeak / kN);

            if (result.determined < kN) {
                cellState_ = baseCellState;
                restoreIntLines(baseIntLines);
                result.determined = baseDetermined;
                result.free = kN - baseDetermined;
            }
        }

        std::fprintf(stderr, "\nFinal: %u / %u determined (%.1f%%)\n", // NOLINT
                     result.determined, kN, 100.0 * result.determined / kN);

        // Verify correctness and BH
        result.correct = true;
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            if (cellState_[flat] >= 0) { // NOLINT
                if (static_cast<std::uint8_t>(cellState_[flat]) != original_[flat]) { // NOLINT
                    result.correct = false; break;
                }
            }
        }
        {
            bool allAssigned = true;
            for (std::uint32_t flat = 0; flat < kN; ++flat) {
                if (cellState_[flat] < 0) { allAssigned = false; break; } // NOLINT
            }
            if (allAssigned) {
                if constexpr (kS <= 127) {
                    common::Csm reconstructed;
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            reconstructed.set(r, c, static_cast<std::uint8_t>(
                                cellState_[(static_cast<std::uint32_t>(r) * kS) + c])); // NOLINT
                        }
                    }
                    result.bhVerified = common::BlockHash::verify(reconstructed, expectedBH_);
                } else {
                    common::CsmVariable reconstructed(kS);
                    for (std::uint16_t r = 0; r < kS; ++r) {
                        for (std::uint16_t c = 0; c < kS; ++c) {
                            reconstructed.set(r, c, static_cast<std::uint8_t>(
                                cellState_[(static_cast<std::uint32_t>(r) * kS) + c])); // NOLINT
                        }
                    }
                    const auto buf = reconstructed.serialize();
                    result.bhVerified = common::BlockHash::verify(buf.data(), buf.size(), expectedBH_);
                }
            }
        }
        return result;
    }


} // namespace crsce::decompress::solvers
