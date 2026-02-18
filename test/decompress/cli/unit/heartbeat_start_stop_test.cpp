/**
 * @file heartbeat_start_stop_test.cpp
 * @brief Validate Heartbeat start/stop and logging behavior using a stubbed worker.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <ios>

#include "decompress/Cli/Heartbeat.h"

using crsce::decompress::cli::Heartbeat;

namespace {
    /**
     * @name stub_worker
     * @brief Minimal stub that writes a predictable line at the requested interval,
     *        exits cleanly when the flag is cleared.
     */
    inline std::string &stub_path() {
        static std::string p;
        return p;
    }
    void stub_worker(std::atomic<bool> *run_flag, unsigned interval_ms) {
        std::ofstream os;
        const auto &p = stub_path();
        if (!p.empty()) { os.open(p, std::ios::out | std::ios::app); }
        unsigned n = 0U;
        while (run_flag && run_flag->load(std::memory_order_relaxed) && n < 25U) {
            const std::string line = std::string("stubbeat ") + std::to_string(n++) + "\n";
            if (os.is_open()) { os << line; os.flush(); }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }
}

/**
 * @name Heartbeat_StartStop_WritesOutput
 * @brief Start heartbeat with stubbed worker, wait a bit, stop and verify output lines exist.
 */
TEST(Heartbeat, StartStopWritesOutput) {
    const std::string path = std::string(TEST_BINARY_DIR) + "/o11y_hb_unit_test.log";
    stub_path() = path;

    // Use small interval to speed up test
    Heartbeat hb(5U, &stub_worker);
    hb.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    hb.wait();

    // Read file and expect at least one stub line
    std::ifstream is(path);
    ASSERT_TRUE(is.good());
    std::string all;
    {
        std::string line;
        while (std::getline(is, line)) { all += line; all.push_back('\n'); }
    }
    ASSERT_FALSE(all.empty());
    EXPECT_NE(all.find("stubbeat"), std::string::npos);

    // Idempotent stop (should be a no-op and not crash)
    hb.wait();
}
