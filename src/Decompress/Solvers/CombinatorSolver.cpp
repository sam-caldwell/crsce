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
#include <string>
#include <utility>
#include <vector>

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
            std::array<std::uint8_t, 16> msg{}; // max 127 bits + 1 pad = 16 bytes
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
        std::pair<std::vector<std::array<std::uint8_t, 128>>, std::uint32_t>
        buildGenMatrixForLength(const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);

            // CRC-32 of all-zero message
            std::array<std::uint8_t, 16> zeroMsg{};
            const auto c0 = common::util::crc32_ieee(zeroMsg.data(), totalBytes);

            std::vector<std::array<std::uint8_t, 128>> gm(32);
            for (auto &row : gm) { row.fill(0); }

            for (std::uint16_t col = 0; col < len; ++col) {
                std::array<std::uint8_t, 16> msg{};
                msg[col / 8] = static_cast<std::uint8_t>(1U << (7U - (col % 8U))); // NOLINT
                const auto crcUnit = common::util::crc32_ieee(msg.data(), totalBytes);
                const auto colVal = crcUnit ^ c0;
                for (std::uint8_t bit = 0; bit < 32; ++bit) {
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

        // Store original for verification
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                original_[static_cast<std::uint32_t>(r) * kS + c] = csm.get(r, c);
            }
        }

        buildSystem(csm);
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

    void CombinatorSolver::buildSystem(const common::Csm &csm) {
        addGeometricParity(csm);
        if (config_.useLH) { addLH(csm); }
        if (config_.useVH) { addVH(csm); }
        if (config_.useDH) { addDH(csm); }
        if (config_.useXH) { addXH(csm); }
    }

    void CombinatorSolver::addGeometricParity(const common::Csm &csm) {
        // LSM (rows)
        for (std::uint16_t r = 0; r < kS; ++r) {
            IntLine line;
            GF2Row gf2row{};
            std::uint8_t parity = 0;
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto flat = static_cast<std::uint32_t>(r) * kS + c;
                line.cells.push_back(flat);
                gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                if (csm.get(r, c)) { parity ^= 1; line.target++; }
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
                if (csm.get(r, c)) { parity ^= 1; line.target++; }
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
                    if (csm.get(r, c)) { parity ^= 1; line.target++; }
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
                    if (csm.get(r, cc)) { parity ^= 1; line.target++; }
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
            // Non-toroidal DSM
            for (std::uint16_t d = 0; d < kDiagCount; ++d) {
                const auto cells = diagCells(d);
                IntLine line;
                GF2Row gf2row{};
                std::uint8_t parity = 0;
                for (const auto flat : cells) {
                    line.cells.push_back(flat);
                    gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    const auto r = static_cast<std::uint16_t>(flat / kS);
                    const auto c = static_cast<std::uint16_t>(flat % kS);
                    if (csm.get(r, c)) { parity ^= 1; line.target++; }
                }
                line.rho = line.target;
                line.u = static_cast<std::int32_t>(cells.size());
                const auto li = static_cast<std::uint32_t>(intLines_.size());
                intLines_.push_back(std::move(line));
                for (const auto f : intLines_[li].cells) { cellToLines_[f].push_back(li); }
                gf2Rows_.push_back(gf2row);
                gf2Target_.push_back(parity);
            }
            // Non-toroidal XSM
            for (std::uint16_t x = 0; x < kDiagCount; ++x) {
                const auto cells = antiDiagCells(x);
                IntLine line;
                GF2Row gf2row{};
                std::uint8_t parity = 0;
                for (const auto flat : cells) {
                    line.cells.push_back(flat);
                    gf2row[flat / 64] |= (std::uint64_t{1} << (flat % 64)); // NOLINT
                    const auto r = static_cast<std::uint16_t>(flat / kS);
                    const auto c = static_cast<std::uint16_t>(flat % kS);
                    if (csm.get(r, c)) { parity ^= 1; line.target++; }
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

    void CombinatorSolver::addLH(const common::Csm &csm) {
        const auto &gm = detail::kGenMatrix;
        const auto c0 = detail::kCrcZero;

        for (std::uint16_t r = 0; r < kS; ++r) {
            // Compute row CRC-32
            std::array<std::uint8_t, 16> msg{};
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (csm.get(r, c)) {
                    msg[c / 8] |= static_cast<std::uint8_t>(1U << (7U - (c % 8U))); // NOLINT
                }
            }
            const auto rowCrc = common::util::crc32_ieee(msg.data(), msg.size());

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                const auto baseCol = static_cast<std::uint32_t>(r) * kS;
                for (std::uint16_t c = 0; c < kS; ++c) {
                    if ((gm[bit][c / 64] >> (c % 64)) & 1U) { // NOLINT
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

    void CombinatorSolver::addVH(const common::Csm &csm) {
        const auto &gm = detail::kGenMatrix;
        const auto c0 = detail::kCrcZero;

        for (std::uint16_t c = 0; c < kS; ++c) {
            // Compute column CRC-32
            std::array<std::uint8_t, 16> msg{};
            for (std::uint16_t r = 0; r < kS; ++r) {
                if (csm.get(r, c)) {
                    msg[r / 8] |= static_cast<std::uint8_t>(1U << (7U - (r % 8U))); // NOLINT
                }
            }
            const auto colCrc = common::util::crc32_ieee(msg.data(), msg.size());

            for (std::uint8_t bit = 0; bit < 32; ++bit) {
                GF2Row gf2row{};
                for (std::uint16_t r = 0; r < kS; ++r) {
                    if ((gm[bit][r / 64] >> (r % 64)) & 1U) { // NOLINT
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

    void CombinatorSolver::addDH(const common::Csm &csm) {
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
            std::vector<std::array<std::uint8_t, 128>> gm;
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
                diagBits[j] = csm.get(r, c);
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

    // ── XH (anti-diagonal CRC-32) ─────────────────────────────────────

    void CombinatorSolver::addXH(const common::Csm &csm) {
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
            std::vector<std::array<std::uint8_t, 128>> gm;
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
                xBits[j] = csm.get(r, c);
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

    void CombinatorSolver::addDHRange(const common::Csm &csm,
                                       const std::uint16_t minLen,
                                       const std::uint16_t maxLen) {
        struct GenInfo {
            std::vector<std::array<std::uint8_t, 128>> gm;
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
                diagBits[j] = csm.get(r, c);
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

    void CombinatorSolver::addXHRange(const common::Csm &csm,
                                       const std::uint16_t minLen,
                                       const std::uint16_t maxLen) {
        struct GenInfo {
            std::vector<std::array<std::uint8_t, 128>> gm;
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
                xBits[j] = csm.get(r, c);
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

    // ── CRC-16 helpers ────────────────────────────────────────────────

    namespace {
        std::uint16_t crc16OfBits(const std::uint8_t *bits, const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 16> msg{};
            for (std::uint16_t k = 0; k < len; ++k) {
                if (bits[k]) {
                    msg[k / 8] |= static_cast<std::uint8_t>(1U << (7U - (k % 8U))); // NOLINT
                }
            }
            return common::util::crc16_ccitt(msg.data(), totalBytes);
        }

        std::pair<std::vector<std::array<std::uint8_t, 128>>, std::uint16_t>
        buildGenMatrix16ForLength(const std::uint16_t len) {
            const auto totalBytes = static_cast<std::size_t>((len + 1 + 7) / 8);
            std::array<std::uint8_t, 16> zeroMsg{};
            const auto c0 = common::util::crc16_ccitt(zeroMsg.data(), totalBytes);
            std::vector<std::array<std::uint8_t, 128>> gm(16);
            for (auto &row : gm) { row.fill(0); }
            for (std::uint16_t col = 0; col < len; ++col) {
                std::array<std::uint8_t, 16> msg{};
                msg[col / 8] = static_cast<std::uint8_t>(1U << (7U - (col % 8U))); // NOLINT
                const auto crcUnit = common::util::crc16_ccitt(msg.data(), totalBytes);
                const auto colVal = static_cast<std::uint16_t>(crcUnit ^ c0);
                for (std::uint8_t bit = 0; bit < 16; ++bit) {
                    gm[bit][col] = static_cast<std::uint8_t>((colVal >> bit) & 1U); // NOLINT
                }
            }
            return {gm, c0};
        }
    } // anonymous namespace

    // ── CRC-16 DH/XH range methods ─────────────────────────────────────

    void CombinatorSolver::addDH16Range(const common::Csm &csm,
                                         const std::uint16_t minLen,
                                         const std::uint16_t maxLen) {
        struct GenInfo16 {
            std::vector<std::array<std::uint8_t, 128>> gm;
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
                diagBits[j] = csm.get(r, c);
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

    void CombinatorSolver::addXH16Range(const common::Csm &csm,
                                         const std::uint16_t minLen,
                                         const std::uint16_t maxLen) {
        struct GenInfo16 {
            std::vector<std::array<std::uint8_t, 128>> gm;
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
                xBits[j] = csm.get(r, c);
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

    // ── Multi-phase cascade solve ───────────────────────────────────────

    auto CombinatorSolver::solveCascade(const common::Csm &csm) -> Result {
        // Phase boundaries depend on hash width
        const bool use16 = (config_.dhBits == 16);
        // CRC-16: fully determines diags of length ≤16; CRC-32: ≤32
        static constexpr std::array<std::pair<std::uint16_t, std::uint16_t>, 4> phases32 = {{
            {1, 32}, {33, 64}, {65, 96}, {97, 127}
        }};
        static constexpr std::array<std::pair<std::uint16_t, std::uint16_t>, 4> phases16 = {{
            {1, 16}, {17, 32}, {33, 64}, {65, 127}
        }};
        const auto &phases = use16 ? phases16 : phases32;

        // Add VH from the start if configured
        if (config_.useVHInCascade) {
            addVH(csm);
            std::fprintf(stderr, "  VH added: +%u equations\n", static_cast<unsigned>(kS * 32));
        }

        Result result;
        result.free = kN;

        for (std::size_t phase = 0; phase < phases.size(); ++phase) {
            const auto [minLen, maxLen] = phases[phase]; // NOLINT
            const auto prevDet = result.determined;

            // Add DH/XH equations for this phase's length range
            const auto rowsBefore = gf2Rows_.size();
            if (use16) {
                addDH16Range(csm, minLen, maxLen);
                addXH16Range(csm, minLen, maxLen);
            } else {
                addDHRange(csm, minLen, maxLen);
                addXHRange(csm, minLen, maxLen);
            }
            const auto rowsAdded = gf2Rows_.size() - rowsBefore;

            std::fprintf(stderr, "\n  Phase %zu (diag len %u-%u): +%zu GF2 equations (%zu total)\n",
                         phase + 1, minLen, maxLen, rowsAdded, gf2Rows_.size());

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

        // Verify
        result.correct = true;
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            if (cellState_[flat] >= 0) {
                if (static_cast<std::uint8_t>(cellState_[flat]) != original_[flat]) {
                    result.correct = false;
                    break;
                }
            }
        }

        return result;
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

        // Verify correctness
        result.correct = true;
        for (std::uint32_t flat = 0; flat < kN; ++flat) {
            if (cellState_[flat] >= 0) {
                if (static_cast<std::uint8_t>(cellState_[flat]) != original_[flat]) {
                    result.correct = false;
                    break;
                }
            }
        }

        return result;
    }

} // namespace crsce::decompress::solvers
