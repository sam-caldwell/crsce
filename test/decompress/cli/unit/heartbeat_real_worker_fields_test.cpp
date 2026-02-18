/**
 * @file heartbeat_real_worker_fields_test.cpp
 * @brief Validate real decompressor heartbeat worker outputs pid/phase/metrics with a controlled snapshot.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <string>
#include <ios>
#include <vector>
#include <cstdint>
#include <span>

#include "decompress/Cli/Heartbeat.h"
#include "decompress/Block/detail/BlockSolveSnapshot.h"
#include "decompress/Phases/DeterministicElimination/ConstraintState.h"
#include "decompress/Block/detail/set_block_solve_snapshot.h"

using crsce::decompress::cli::Heartbeat;

/**
 * @name Heartbeat_RealWorker_ContainsPidPhaseAndSat
 * @brief With a known snapshot (phase=gobp; U vectors zeroed), heartbeat prints pid/phase and sat pct fields.
 */
TEST(DecompressHeartbeat, RealWorkerContainsPidPhaseAndSat) {
    // Build a minimal snapshot: S=4, zero unknowns everywhere (100% sat), phase=gobp
    const crsce::decompress::ConstraintState st; // zero-initialized U_* arrays
    std::vector<std::uint16_t> empty;
    crsce::decompress::BlockSolveSnapshot snap(
        4, st,
        std::span<const std::uint16_t>(empty),
        std::span<const std::uint16_t>(empty),
        std::span<const std::uint16_t>(empty),
        std::span<const std::uint16_t>(empty),
        0ULL
    );
    snap.phase = crsce::decompress::BlockSolveSnapshot::Phase::gobp;
    snap.gobp_status = 1;
    crsce::decompress::set_block_solve_snapshot(snap);

    const std::string path = std::string(TEST_BINARY_DIR) + "/o11y_hb_real_worker_gobp.log";
    ASSERT_TRUE(std::ofstream(path, std::ios::out | std::ios::trunc).good());

    // Route heartbeat to file and run briefly
    ASSERT_EQ(::setenv("CRSCE_HEARTBEAT_PATH", path.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    Heartbeat hb(10U); // default real worker
    hb.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    hb.wait();

    // Validate contents
    std::ifstream is(path);
    ASSERT_TRUE(is.good());
    std::string all;
    {
        std::string line; while (std::getline(is, line)) { all += line; all.push_back('\n'); }
    }
    ASSERT_FALSE(all.empty());
    EXPECT_NE(all.find("pid="), std::string::npos);
    EXPECT_NE(all.find("phase=gobp"), std::string::npos);
    EXPECT_NE(all.find("gobp_status="), std::string::npos);
    // DSM/XSM sat pct fields appear in gobp sub-block
    EXPECT_NE(all.find("dsm_sat_pct="), std::string::npos);
    EXPECT_NE(all.find("xsm_sat_pct="), std::string::npos);
}
