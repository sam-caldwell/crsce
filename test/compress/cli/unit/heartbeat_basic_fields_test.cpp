/**
 * @file heartbeat_basic_fields_test.cpp
 * @brief Validate compressor heartbeat outputs pid and phase label.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <string>
#include <ios>
#include <cstdlib>

#include "compress/Cli/Heartbeat.h"

using crsce::compress::cli::Heartbeat;

TEST(CompressHeartbeat, BasicPidAndPhase) {
    const std::string path = std::string(TEST_BINARY_DIR) + "/o11y_hb_compress_basic.log";
    ASSERT_TRUE(std::ofstream(path, std::ios::out | std::ios::trunc).good());
    ASSERT_EQ(::setenv("CRSCE_HEARTBEAT_PATH", path.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

    Heartbeat hb(10U);
    hb.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    hb.wait();

    std::ifstream is(path);
    ASSERT_TRUE(is.good());
    std::string all;
    {
        std::string line; while (std::getline(is, line)) { all += line; all.push_back('\n'); }
    }
    ASSERT_FALSE(all.empty());
    EXPECT_NE(all.find("pid="), std::string::npos);
    EXPECT_NE(all.find("phase=compress"), std::string::npos);
}

