/**
 * @file unit_o11y_metric_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for O11y::metric() and O11y::do_metric().
 */
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib> // NOLINT(misc-include-cleaner) -- setenv/unsetenv on POSIX
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "common/O11y/O11y.h"

namespace {

    using O11y = crsce::o11y::O11y;
    using MK = O11y::MetricKind;

    /**
     * @brief Read all lines from a file into a vector of strings.
     * @param path File path.
     * @return std::vector<std::string> Lines of the file.
     */
    std::vector<std::string> readLines(const std::string &path) {
        std::vector<std::string> lines;
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) { lines.push_back(line); }
        }
        return lines;
    }

    /**
     * @brief Check if a JSONL line contains a given key-value pair (string-valued tag).
     * @param line JSONL line.
     * @param key Tag key.
     * @param value Expected value.
     * @return bool True if found.
     */
    bool hasTag(const std::string &line, const std::string &key, const std::string &value) {
        const std::string needle = "\"" + key + "\":\"" + value + "\"";
        return line.find(needle) != std::string::npos;
    }

    /**
     * @brief Check if a JSONL line contains a given key (tag name).
     * @param line JSONL line.
     * @param key Tag key.
     * @return bool True if found.
     */
    bool hasKey(const std::string &line, const std::string &key) {
        const std::string needle = "\"" + key + "\":";
        return line.find(needle) != std::string::npos;
    }

    /**
     * @brief RAII helper to set CRSCE_EVENTS_PATH to a temp file and clean up on destruction.
     */
    class TempEventsFile {
    public:
        /**
         * @name TempEventsFile
         * @brief Set CRSCE_EVENTS_PATH to a unique temp file.
         */
        TempEventsFile() {
            path_ = std::filesystem::temp_directory_path() / ("o11y_metric_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".jsonl");
            ::setenv("CRSCE_EVENTS_PATH", path_.c_str(), 1); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
        }
        ~TempEventsFile() {
            ::unsetenv("CRSCE_EVENTS_PATH"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
            std::filesystem::remove(path_);
        }
        TempEventsFile(const TempEventsFile &) = delete;
        TempEventsFile &operator=(const TempEventsFile &) = delete;
        TempEventsFile(TempEventsFile &&) = delete;
        TempEventsFile &operator=(TempEventsFile &&) = delete;

        /**
         * @name path
         * @brief Return the temp file path.
         * @return const std::string& Path.
         */
        [[nodiscard]] const std::string &path() const { return path_; }

    private:
        /** @name path_ @brief Temp file path for JSONL output. */
        std::string path_;
    };

    /**
     * @brief A Gauge field is emitted with its raw value and no _per_sec derivative.
     */
    TEST(O11yMetricTest, MetricGaugeEmitsRawValue) {
        const TempEventsFile tmp;
        auto &o = O11y::instance();

        const auto t0 = std::chrono::steady_clock::now();
        o.do_metric("test_gauge_only", {{"depth", 42, MK::Gauge}}, 1000, t0);
        o.flush_events();

        const auto lines = readLines(tmp.path());
        ASSERT_FALSE(lines.empty());
        const auto &last = lines.back();
        EXPECT_TRUE(hasTag(last, "depth", "42")) << last;
        EXPECT_FALSE(hasKey(last, "depth_per_sec")) << last;
        EXPECT_FALSE(hasKey(last, "avg_depth_per_sec")) << last;
    }

    /**
     * @brief The first emission of a Counter produces _per_sec = 0 and avg_..._per_sec = 0.
     */
    TEST(O11yMetricTest, MetricCounterFirstCallEmitsZeroRate) {
        const TempEventsFile tmp;
        auto &o = O11y::instance();

        const auto t0 = std::chrono::steady_clock::now();
        o.do_metric("test_counter_first", {{"ops", 1000, MK::Counter}}, 2000, t0);
        o.flush_events();

        const auto lines = readLines(tmp.path());
        ASSERT_FALSE(lines.empty());
        const auto &last = lines.back();
        EXPECT_TRUE(hasTag(last, "ops", "1000")) << last;
        EXPECT_TRUE(hasTag(last, "ops_per_sec", "0")) << last;
        EXPECT_TRUE(hasTag(last, "avg_ops_per_sec", "0")) << last;
    }

    /**
     * @brief The second emission of a Counter produces a plausible windowed rate.
     *
     * With a 1-second window and delta of 1000, expect ~1000 ops/sec.
     */
    TEST(O11yMetricTest, MetricCounterComputesWindowRate) {
        const TempEventsFile tmp;
        auto &o = O11y::instance();

        const auto t0 = std::chrono::steady_clock::now();
        const auto t1 = t0 + std::chrono::seconds(1);

        o.do_metric("test_counter_rate", {{"ops", 1000, MK::Counter}}, 3000, t0);
        o.do_metric("test_counter_rate", {{"ops", 2000, MK::Counter}}, 4000, t1);
        o.flush_events();

        const auto lines = readLines(tmp.path());
        ASSERT_GE(lines.size(), 2U);
        // The second emission should have a non-zero rate
        const auto &second = lines[lines.size() - 1];
        EXPECT_TRUE(hasTag(second, "ops", "2000")) << second;
        // ops_per_sec should be ~1000 (delta=1000 over 1s)
        EXPECT_FALSE(hasTag(second, "ops_per_sec", "0"))
            << "Expected non-zero rate on second emission: " << second;
    }

    /**
     * @brief Gauge fields never produce _per_sec derivatives, while Counter fields do.
     */
    TEST(O11yMetricTest, MetricMixedGaugeAndCounter) {
        const TempEventsFile tmp;
        auto &o = O11y::instance();

        const auto t0 = std::chrono::steady_clock::now();
        const auto t1 = t0 + std::chrono::seconds(2);

        o.do_metric("test_mixed", {
            {"depth", 55, MK::Gauge},
            {"iters", 500, MK::Counter}
        }, 5000, t0);
        o.do_metric("test_mixed", {
            {"depth", 77, MK::Gauge},
            {"iters", 1500, MK::Counter}
        }, 7000, t1);
        o.flush_events();

        const auto lines = readLines(tmp.path());
        ASSERT_GE(lines.size(), 2U);
        const auto &second = lines[lines.size() - 1];

        // Gauge: raw value present, no rate
        EXPECT_TRUE(hasTag(second, "depth", "77")) << second;
        EXPECT_FALSE(hasKey(second, "depth_per_sec")) << second;

        // Counter: raw value + rate present
        EXPECT_TRUE(hasTag(second, "iters", "1500")) << second;
        EXPECT_TRUE(hasKey(second, "iters_per_sec")) << second;
        EXPECT_TRUE(hasKey(second, "avg_iters_per_sec")) << second;
    }

    /**
     * @brief Two different event names maintain independent windowing state.
     */
    TEST(O11yMetricTest, MetricIndependentWindows) {
        const TempEventsFile tmp;
        auto &o = O11y::instance();

        const auto t0 = std::chrono::steady_clock::now();
        const auto t1 = t0 + std::chrono::seconds(1);

        // First event gets two emissions
        o.do_metric("test_window_a", {{"x", 100, MK::Counter}}, 10000, t0);
        o.do_metric("test_window_a", {{"x", 200, MK::Counter}}, 11000, t1);

        // Second event gets only one emission (should be "first call" with rate=0)
        o.do_metric("test_window_b", {{"x", 500, MK::Counter}}, 11000, t1);
        o.flush_events();

        const auto lines = readLines(tmp.path());
        ASSERT_GE(lines.size(), 3U);

        // Find the test_window_b line -- should have rate=0 (first call)
        bool foundB = false;
        for (const auto &line : lines) {
            if (line.find("\"test_window_b\"") != std::string::npos) {
                EXPECT_TRUE(hasTag(line, "x_per_sec", "0"))
                    << "Expected zero rate for first emission of window_b: " << line;
                foundB = true;
            }
        }
        EXPECT_TRUE(foundB) << "Did not find test_window_b event in output";
    }

} // anonymous namespace
