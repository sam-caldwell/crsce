/**
 * @file ForEachCellOnLine.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Shared template utility to iterate every cell on a named constraint line.
 *
 * Used by PropagationEngine (forcing) and ConflictAnalyzer (reason-graph traversal)
 * to avoid duplicating the geometry-specific cell enumeration logic.
 *
 * B.20: removed 4 toroidal-slope cases; added LTP3 and LTP4 cases.
 * // B.20 disabled slope cases (retained for reference):
 * // case LineType::Slope256: { constexpr slope=256; ... }
 * // case LineType::Slope255: { constexpr slope=255; ... }
 * // case LineType::Slope2:   { constexpr slope=2;   ... }
 * // case LineType::Slope509: { constexpr slope=509; ... }
 */
#pragma once

#include <algorithm>
#include <cstdint>

#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {

    /**
     * @name forEachCellOnLine
     * @brief Invoke callback(r, c) for every cell on the given constraint line.
     *
     * Dispatches on LineType and computes (r, c) pairs inline with no allocation.
     * The template parameter Func must be callable as void(uint16_t r, uint16_t c).
     *
     * @tparam Func Callable with signature void(uint16_t, uint16_t).
     * @param line The constraint line to enumerate.
     * @param kS   Matrix dimension.
     * @param callback Invoked once per cell on the line.
     */
    template<typename Func>
    void forEachCellOnLine(const LineID line, const std::uint16_t kS, const Func &callback) {
        switch (line.type) {
            case LineType::Row:
                for (std::uint16_t c = 0; c < kS; ++c) {
                    callback(line.index, c);
                }
                break;

            case LineType::Column:
                for (std::uint16_t r = 0; r < kS; ++r) {
                    callback(r, line.index);
                }
                break;

            case LineType::Diagonal: {
                const auto d      = static_cast<std::int32_t>(line.index);
                const auto offset = d - static_cast<std::int32_t>(kS - 1);
                const auto rMin   = static_cast<std::uint16_t>(std::max(0, -offset));
                const auto rMax   = static_cast<std::uint16_t>(
                    std::min(static_cast<int>(kS) - 1, static_cast<int>(kS) - 1 - offset));
                for (auto r = rMin; r <= rMax; ++r) {
                    const auto c = static_cast<std::uint16_t>(r + offset);
                    if (c < kS) {
                        callback(r, c);
                    }
                }
                break;
            }

            case LineType::AntiDiagonal: {
                const auto x    = static_cast<std::int32_t>(line.index);
                const auto rMin = static_cast<std::uint16_t>(
                    std::max(0, x - static_cast<int>(kS) + 1));
                const auto rMax = static_cast<std::uint16_t>(
                    std::min(x, static_cast<int>(kS) - 1));
                for (auto r = rMin; r <= rMax; ++r) {
                    const auto c = static_cast<std::uint16_t>(x - r);
                    callback(r, c);
                }
                break;
            }

            case LineType::LTP1: {
                const auto &cells = ltp1CellsForLine(line.index);
                for (const auto &cell : cells) {
                    callback(cell.r, cell.c);
                }
                break;
            }

            case LineType::LTP2: {
                const auto &cells = ltp2CellsForLine(line.index);
                for (const auto &cell : cells) {
                    callback(cell.r, cell.c);
                }
                break;
            }

            case LineType::LTP3: {
                const auto &cells = ltp3CellsForLine(line.index);
                for (const auto &cell : cells) {
                    callback(cell.r, cell.c);
                }
                break;
            }

            case LineType::LTP4: {
                const auto &cells = ltp4CellsForLine(line.index);
                for (const auto &cell : cells) {
                    callback(cell.r, cell.c);
                }
                break;
            }
        }
    }

} // namespace crsce::decompress::solvers
