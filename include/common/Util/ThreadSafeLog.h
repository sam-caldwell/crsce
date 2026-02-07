/**
 * @file ThreadSafeLog.h
 * @brief Mutex-protected logging helpers for atomic line writes.
 */
#pragma once

#include <mutex>
#include <string>
#include <cstdint>
#include <iostream>
#include <chrono>

namespace crsce::common::util {
    class ThreadSafeLog {
    public:
        static std::uint64_t now_ms() {
            using namespace std::chrono;
            return static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        }
        static void out_line(const std::string &s) {
            const std::scoped_lock lk(g_mutex);
            try { std::cout << s << '\n'; std::cout.flush(); } catch (...) {}
        }
        static void err_line(const std::string &s) {
            const std::scoped_lock lk(g_mutex);
            try { std::cerr << s << '\n'; std::cerr.flush(); } catch (...) {}
        }
        static void out_ts(const std::string &s) {
            const std::scoped_lock lk(g_mutex);
            try { std::cout << "[ts=" << now_ms() << "] " << s << '\n'; std::cout.flush(); } catch (...) {}
        }
    private:
        inline static std::mutex g_mutex{};
    };
}
