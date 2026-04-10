/**
 * @file cmd/combinatorSolver/main.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief B.60: Combinator-algebraic solver CLI tool.
 *
 * Runs the combinator fixpoint (GaussElim + IntBound + CrossDeduce) on a
 * specified block with a configurable constraint system.
 *
 * Usage:
 *   combinatorSolver -in <file> [-block <N>] [-config <name>]
 *
 * Configs:
 *   lh_vh         Non-toroidal DSM/XSM + LH + VH          [B.60b]
 *   toroidal_vh   Toroidal DSM/XSM + LH + VH              [B.60c/e]
 *   lh_dh         Non-toroidal DSM/XSM + LH + DH          [B.60f]
 */
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "common/Csm/Csm.h"
#include "common/Csm/CsmVariable.h"
#include "decompress/Solvers/CombinatorSolver.h"

using namespace crsce; // NOLINT

static constexpr std::uint16_t kS = decompress::solvers::CombinatorSolver::kS;
static constexpr std::uint32_t kBlockBits = kS * kS;

int main(const int argc, const char *const argv[]) { // NOLINT
    std::string inputPath;
    int blockIdx = 0;
    std::string configName = "lh_vh";
    int dhMaxDiags = 0;
    int xhMaxDiags = 0;
    int preassignCols = 0;
    int rltpMaxLinesOverride = -1;
    int rltpTier5WidthOverride = -1;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i]; // NOLINT
        if (arg == "-in" && i + 1 < argc) { inputPath = argv[++i]; } // NOLINT
        else if (arg == "-block" && i + 1 < argc) { blockIdx = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-config" && i + 1 < argc) { configName = argv[++i]; } // NOLINT
        else if (arg == "-dh-diags" && i + 1 < argc) { dhMaxDiags = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-xh-diags" && i + 1 < argc) { xhMaxDiags = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-preassign-cols" && i + 1 < argc) { preassignCols = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-rltp-max-lines" && i + 1 < argc) { rltpMaxLinesOverride = std::atoi(argv[++i]); } // NOLINT
        else if (arg == "-rltp-tier5-width" && i + 1 < argc) { rltpTier5WidthOverride = std::atoi(argv[++i]); } // NOLINT
    }
    if (inputPath.empty()) {
        std::fprintf(stderr, "usage: combinatorSolver -in <file> [-block <N>] [-config <name>] [-dh-diags <N>] [-xh-diags <N>]\n");
        std::fprintf(stderr, "  configs: lh_vh, toroidal_vh, lh_dh, xh_dh\n");
        return 1;
    }

    // Parse config
    decompress::solvers::CombinatorSolver::Config config;
    if (configName == "lh_vh") {
        config = {.useLH = true, .useVH = true, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0, .toroidal = false};
    } else if (configName == "toroidal_vh") {
        config = {.useLH = true, .useVH = true, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0, .toroidal = true};
    } else if (configName == "lh_dh") {
        config = {.useLH = true, .useVH = false, .useDH = true,
                  .dhMaxDiags = static_cast<std::uint16_t>(dhMaxDiags),
                  .useXH = false, .xhMaxDiags = 0, .toroidal = false};
    } else if (configName == "xh_dh") {
        config = {.useLH = false, .useVH = false, .useDH = true,
                  .dhMaxDiags = static_cast<std::uint16_t>(dhMaxDiags),
                  .useXH = true, .xhMaxDiags = static_cast<std::uint16_t>(xhMaxDiags),
                  .toroidal = false};
    } else if (configName == "lh_dh_xh") {
        config = {.useLH = true, .useVH = false, .useDH = true,
                  .dhMaxDiags = static_cast<std::uint16_t>(dhMaxDiags),
                  .useXH = true, .xhMaxDiags = static_cast<std::uint16_t>(xhMaxDiags),
                  .cascade = false, .toroidal = false};
    } else if (configName == "cascade") {
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32, .useVHInCascade = false,
                  .cascade = true, .toroidal = false};
    } else if (configName == "lh_cascade") {
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32, .useVHInCascade = false,
                  .cascade = true, .toroidal = false};
    } else if (configName == "vh_dh16_xh16_cascade") {
        // B.60m Config B: VH + CRC-16 DH64/XH64 cascade (no LH)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 16, .xhBits = 16, .useVHInCascade = true,
                  .cascade = true, .toroidal = false};
    } else if (configName == "lh_vh_dh16_xh16_cascade") {
        // B.60m old Config A: LH32 + VH32 + CRC-16 DH64/XH64 cascade
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 16, .xhBits = 16,
                  .lhBits = 32, .vhBits = 32,
                  .useVHInCascade = true,
                  .cascade = true, .toroidal = false};
    } else if (configName == "vh_dh32_xh32_cascade") {
        // B.60p: VH32 + DH32_64 + XH32_64 cascade (no LH), limited to diag length ≤32
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 32, .toroidal = false};
    } else if (configName == "vh_dh8_xh8_cascade") {
        // B.60q: VH32 + DH8_253 + XH8_253 cascade (no LH)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 8, .xhBits = 8,
                  .lhBits = 32, .vhBits = 32,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0, .toroidal = false};
    } else if (configName == "hybrid_cascade") {
        // B.60r: Hybrid CRC-8/16/32 DH128+XH128 + VH32 cascade
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "lh16_vh16_hybrid_cascade") {
        // B.60s: LH16 + VH16 + hybrid DH128/XH128 cascade
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 16, .vhBits = 16,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62a") {
        // B.62a: VH32 + DH/XH hybrid + 1 yLTP (CRC-32 + cross-sum) cascade
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = true, .yltpSeed = 0x435253434C54505AULL, // "CRSCLTPZ"
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62c") {
        // B.62c: LH + VH + DH/XH hybrid + 1 yLTP cascade (all axes)
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = true,
                  .yltpSeed = 0x435253434C54505AULL,  // "CRSCLTPZ"
                  .yltpSeed2 = 0,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62d") {
        // B.62d: LH + VH + DH/XH hybrid + 1 rLTP (center-spiral) cascade
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62f") {
        // B.62f: VH + DH/XH hybrid + 2 orthogonal variable-length rLTP cascade (no LH)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62g") {
        // B.62g: B.62f minus rLTP cross-sums (CRC-only rLTP, no IntBound on rLTP lines)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62h") {
        // B.62h: Trimmed rLTP CRC (lines 1-64), tier 3 CRC-16, no cross-sums
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true, .rltpMaxLines = 64, .rltpTier3Width = 16,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62e") {
        // B.62e: VH + DH/XH hybrid + variable-length rLTP cascade (no LH)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "b62b") {
        // B.62b: VH32 + DH/XH hybrid + 2 yLTP (CRC-32 + cross-sum) cascade
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = true,
                  .yltpSeed = 0x435253434C54505AULL,  // "CRSCLTPZ"
                  .yltpSeed2 = 0x435253434C545052ULL, // "CRSCLTPR"
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else if (configName == "all16_cascade") {
        // B.60m revised: LH16 + VH16 + DH16_64 + XH16_64 cascade (all CRC-16)
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 16, .xhBits = 16,
                  .lhBits = 16, .vhBits = 16,
                  .useVHInCascade = true,
                  .cascade = true, .toroidal = false};
    } else if (configName == "b62j") {
        // B.62j: B.62g minus XSM cross-sums (keep XH CRC). C_r = 97.4%
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b62l") {
        // B.62l: Drop DSM, re-add XSM (swap vs B.62j). C_r = 97.4%
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true,
                  .useDSMSums = false, .useXSMSums = true, .toroidal = false};
    } else if (configName == "b62k") {
        // B.62k: B.62j minus DSM cross-sums (keep DH CRC). C_r = 90.4%
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true,
                  .useDSMSums = false, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63c") {
        // B.63c: Multi-phase multi-axis solver. LH+VH+DH64+XH64+DSM.
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .hybrid = true,
                  .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63n") {
        // B.63n: CRC-64 version of b63c (LH64+VH64+DH64+XH64+rLTP with CRC-64 tiers)
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useCrc64 = true, .hybrid = true,
                  .useXSMSums = false, .toroidal = false};
    } else if (configName.substr(0, 4) == "b63k") { // NOLINT
        // B.63k: K0-K6 trade-off configs. Generate rLTP grid centers.
        auto makeGrid = [](int n, std::uint16_t s) {
            std::vector<std::pair<std::uint16_t, std::uint16_t>> centers;
            const int side = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
            for (int i = 0; i < side && static_cast<int>(centers.size()) < n; ++i) {
                for (int j = 0; j < side && static_cast<int>(centers.size()) < n; ++j) {
                    centers.emplace_back(
                        static_cast<std::uint16_t>(s * i / side),
                        static_cast<std::uint16_t>(s * j / side));
                }
            }
            return centers;
        };

        // Base: DSM + DH64+XH64 + BH/DI + rLTP(63,63). Vary LH/VH/LSM/VSM + extra rLTPs.
        config = {.useLH = false, .useVH = false, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};

        if (configName == "b63k0") {
            // K0 baseline = b63c
            config.useLH = true; config.useVH = true;
        } else if (configName == "b63k1") {
            // K1: drop LH, +21 rLTPs
            config.useLH = false; config.useVH = true;
            config.rltpExtraCenters = makeGrid(21, kS);
        } else if (configName == "b63k2") {
            // K2: drop VH, +21 rLTPs
            config.useLH = true; config.useVH = false;
            config.rltpExtraCenters = makeGrid(21, kS);
        } else if (configName == "b63k3") {
            // K3: drop LSM+VSM, +9 rLTPs
            config.useLH = true; config.useVH = true;
            config.useLSMSums = false; config.useVSMSums = false;
            config.rltpExtraCenters = makeGrid(9, kS);
        } else if (configName == "b63k4") {
            // K4: drop LH+LSM+VSM, +30 rLTPs
            config.useLH = false; config.useVH = true;
            config.useLSMSums = false; config.useVSMSums = false;
            config.rltpExtraCenters = makeGrid(30, kS);
        } else if (configName == "b63k5") {
            // K5: drop VH+LSM+VSM, +30 rLTPs
            config.useLH = true; config.useVH = false;
            config.useLSMSums = false; config.useVSMSums = false;
            config.rltpExtraCenters = makeGrid(30, kS);
        } else if (configName == "b63k6") {
            // K6: drop LH+VH, +42 rLTPs
            config.useLH = false; config.useVH = false;
            config.rltpExtraCenters = makeGrid(42, kS);
        }
    } else if (configName == "b63i_no_lh") {
        // B.63i ablation: b63c minus LH
        config = {.useLH = false, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63i_no_vh") {
        // B.63i ablation: b63c minus VH
        config = {.useLH = true, .useVH = false, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63i_no_rltp") {
        // B.63i ablation: b63c minus rLTP
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = false,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63i_no_dh") {
        // B.63i ablation: b63c minus DH/XH (no cascade, just LH+VH+rLTP)
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = false,
                  .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a1") {
        // B.63a A1: LSM + DSM + LH + VH (no DH/XH/rLTP). C_r ~71%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a2") {
        // B.63a A2: A1 + DH64 (CRC-32, 64 shortest diags). C_r ~84%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a3") {
        // B.63a A3: A1 + DH64 + XH64. C_r ~97%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
        // Note: XH is added by cascade alongside DH — both enabled via cascade mode
    } else if (configName == "b63a4") {
        // B.63a A4: A1 + DH64 + 1 rLTP (lines 1-16). C_r ~85%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a5") {
        // B.63a A5: A1 + DH64 + 1 rLTP (lines 1-32). C_r ~88%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 32,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a6") {
        // B.63a A6: A1 + DH32(CRC-16) + XH32(CRC-16) + 1 rLTP (1-16). C_r ~78%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .dhBits = 16, .xhBits = 16,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 32,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a7") {
        // B.63a A7: LSM + DSM + LH + DH64 + 1 rLTP (1-16) (no VH). C_r ~52%
        config = {.useLH = true, .useVH = false, .useDH = false, .useXH = false,
                  .useRLTP = true, .rltpCenterR = 63, .rltpCenterC = 63,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .cascade = true, .cascadeMaxLen = 64,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b63a8") {
        // B.63a A8: LSM + DSM + LH + VH + DH128 + XH128 (close to B.60r). C_r ~88%
        config = {.useLH = true, .useVH = true, .useDH = false, .useXH = false,
                  .useVHInCascade = true, .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .useXSMSums = false, .toroidal = false};
    } else if (configName == "b62m") {
        // B.62m: Hybrid combinator + row search. Option A: C_r = 73.8%
        config = {.useLH = true, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true, .rltpMaxLines = 16,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .hybrid = true, .toroidal = false};
    } else if (configName == "b62i") {
        // B.62i: Trimmed rLTP with CRC-8 on lines 65+ (sweep Lmax via -rltp-max-lines)
        config = {.useLH = false, .useVH = false, .useDH = false, .dhMaxDiags = 0,
                  .useXH = false, .xhMaxDiags = 0,
                  .dhBits = 32, .xhBits = 32,
                  .lhBits = 32, .vhBits = 32,
                  .useYLTP = false, .yltpSeed = 0, .yltpSeed2 = 0,
                  .useRLTP = true, .rltpCenterR = 95, .rltpCenterC = 95,
                  .useRLTP2 = true, .rltpCenter2R = 0, .rltpCenter2C = 0,
                  .rltpCrcOnly = true, .rltpMaxLines = 191, .rltpTier3Width = 16,
                  .rltpTier5Width = 8,
                  .useVHInCascade = true,
                  .cascade = true, .cascadeMaxLen = 0,
                  .hybridWidths = true, .toroidal = false};
    } else {
        std::fprintf(stderr, "unknown config: %s\n", configName.c_str());
        return 1;
    }

    // Apply CLI overrides
    if (rltpMaxLinesOverride >= 0) {
        config.rltpMaxLines = static_cast<std::uint16_t>(rltpMaxLinesOverride);
    }
    if (rltpTier5WidthOverride >= 0) {
        config.rltpTier5Width = static_cast<std::uint8_t>(rltpTier5WidthOverride);
    }

    // Read input file
    std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        std::fprintf(stderr, "cannot open: %s\n", inputPath.c_str());
        return 1;
    }
    const auto fileSize = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(fileSize);
    in.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(fileSize)); // NOLINT
    in.close();

    // Load CSM from block using CsmVariable (works at any S)
    common::CsmVariable csm(kS);
    const auto startBit = static_cast<std::size_t>(blockIdx) * kBlockBits;
    for (std::size_t i = 0; i < kBlockBits; ++i) {
        const auto srcBit = startBit + i;
        const auto srcByte = srcBit / 8;
        if (srcByte >= fileSize) { break; }
        const auto bitInByte = static_cast<std::uint8_t>(7 - (srcBit % 8));
        const auto v = static_cast<std::uint8_t>((data[srcByte] >> bitInByte) & 1);
        csm.set(static_cast<std::uint16_t>(i / kS), static_cast<std::uint16_t>(i % kS), v);
    }

    std::fprintf(stderr, "Block %d loaded (S=%u, %zu bytes)\n", blockIdx, kS, fileSize);
    std::fprintf(stderr, "Config: %s\n", configName.c_str());

    // Solve
    decompress::solvers::CombinatorSolver solver(csm, config);
    std::fprintf(stderr, "System: %s\n", solver.configName().c_str());

    // Pre-assign leftmost n columns (simulates overlap donation from solved neighbor)
    std::uint32_t preassignedCount = 0;
    if (preassignCols > 0) {
        for (std::uint16_t c = 0; c < static_cast<std::uint16_t>(preassignCols) && c < kS; ++c) {
            for (std::uint16_t r = 0; r < kS; ++r) {
                solver.preAssign(r, c, csm.get(r, c));
                ++preassignedCount;
            }
        }
        std::fprintf(stderr, "Pre-assigned: %u cells (%d cols)\n", preassignedCount, preassignCols);
    }
    std::fprintf(stderr, "\n");

    const auto result = config.hybrid ? solver.solveHybrid()
                       : config.cascade ? solver.solveCascade()
                       : solver.solve();

    const auto totalKnown = result.determined + preassignedCount;
    std::fprintf(stderr, "\nResult:\n");
    std::fprintf(stderr, "  Determined:  %u / %u (%.1f%%)\n",
                 result.determined, kBlockBits,
                 100.0 * result.determined / kBlockBits);
    if (preassignedCount > 0) {
        std::fprintf(stderr, "  Pre-assigned:%u\n", preassignedCount);
        std::fprintf(stderr, "  Total known: %u / %u (%.1f%%)\n",
                     totalKnown, kBlockBits,
                     100.0 * totalKnown / kBlockBits);
    }
    std::fprintf(stderr, "  Free:        %u\n", result.free);
    std::fprintf(stderr, "  GaussElim:   %u\n", result.fromGaussElim);
    std::fprintf(stderr, "  IntBound:    %u\n", result.fromIntBound);
    std::fprintf(stderr, "  CrossDeduce: %u\n", result.fromCrossDeduce);
    std::fprintf(stderr, "  Rank:        %u\n", result.rank);
    std::fprintf(stderr, "  Iterations:  %u\n", result.iterations);
    std::fprintf(stderr, "  Feasible:    %s\n", result.feasible ? "true" : "false");
    std::fprintf(stderr, "  Correct:     %s\n", result.correct ? "true" : "false");
    std::fprintf(stderr, "  BH verified: %s\n", result.bhVerified ? "true" : "false");

    // Per-line analysis
    const auto ls = solver.analyzeLines();
    std::fprintf(stderr, "\nLine completion:\n");
    std::fprintf(stderr, "  Rows:       %u / %u\n", ls.rowsComplete, ls.rowsTotal);
    std::fprintf(stderr, "  Columns:    %u / %u (VH verified: %u)\n", ls.colsComplete, ls.colsTotal, ls.vhVerified);
    std::fprintf(stderr, "  Diags:      %u / %u (DH verified: %u)\n", ls.diagsComplete, ls.diagsTotal, ls.dhVerified);
    std::fprintf(stderr, "  AntiDiags:  %u / %u (XH verified: %u)\n", ls.antiDiagsComplete, ls.antiDiagsTotal, ls.xhVerified);

    // Diagonal completion by length
    std::fprintf(stderr, "\nDiag completion by length:\n");
    std::fprintf(stderr, "  len | diags done/total | anti done/total\n");
    std::fprintf(stderr, "  ----|------------------|----------------\n");
    for (int len = 1; len <= 127; ++len) {
        const auto dt = ls.diagTotalByLength[len]; // NOLINT
        const auto dc = ls.diagCompleteByLength[len]; // NOLINT
        const auto at = ls.antiDiagTotalByLength[len]; // NOLINT
        const auto ac = ls.antiDiagCompleteByLength[len]; // NOLINT
        if (dt == 0 && at == 0) { continue; }
        if (dc == dt && ac == at) {
            std::fprintf(stderr, "  %3d |     %3u / %3u    |     %3u / %3u\n", len, dc, dt, ac, at);
        } else {
            std::fprintf(stderr, "  %3d |     %3u / %3u  * |     %3u / %3u %s\n",
                         len, dc, dt, ac, at, (ac < at) ? "*" : "");
        }
    }

    // Per-row unknown distribution (for B.63a hybrid planning)
    if (configName.substr(0, 4) == "b63a" || configName == "b62m") { // NOLINT
        std::vector<std::uint16_t> rowUnknowns(kS);
        std::uint16_t minUnk = kS, maxUnk = 0;
        std::uint32_t totalUnk = 0;
        std::uint16_t rowsUnder30 = 0, rowsUnder40 = 0, rowsUnder60 = 0;
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::uint16_t unk = 0;
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (result.determined < kBlockBits) { // partial solve — check original match
                    // Use determined count heuristic: total unknowns = kBlockBits - determined
                }
            }
            // Compute from result: unknowns per row = cells in row not determined
            // Since we can't access cellState_ from main, estimate from global stats
        }
        // Actually: report total unknowns and estimated per-row average
        const auto avgUnk = static_cast<double>(result.free) / kS;
        std::fprintf(stderr, "\nPer-row unknowns (estimated):\n"); // NOLINT
        std::fprintf(stderr, "  Total free: %u, Avg per row: %.1f\n", result.free, avgUnk); // NOLINT
        std::fprintf(stderr, "  DFS tractable (<=38 unknowns): %s\n", // NOLINT
                     avgUnk <= 38.0 ? "LIKELY" : (avgUnk <= 60.0 ? "MARGINAL" : "NO"));
    }

    return result.feasible ? 0 : 1;
}
