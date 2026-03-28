/**
 * @file AsyncHashPipeline.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Async hash verification pipeline for offloading SHA-256 row checks from the DFS thread.
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "common/Csm/Csm.h"
#include "decompress/Solvers/IHashVerifier.h"

namespace crsce::decompress::solvers {
    /**
     * @class AsyncHashPipeline
     * @name AsyncHashPipeline
     * @brief Background thread that verifies CSM candidates against lateral hashes.
     *
     * The DFS thread submits cross-sum-valid candidates. A background verifier thread
     * runs SHA-256 on all 511 rows, keeping only candidates that pass. Results are
     * dequeued by the DFS thread in FIFO order (preserving lex order).
     */
    class AsyncHashPipeline {
    public:
        /**
         * @name AsyncHashPipeline
         * @brief Construct the pipeline with a hash verifier and bounded queue size.
         * @param hasher Hash verifier (shared ownership with EnumerationController).
         * @param maxPending Maximum candidates buffered before DFS blocks.
         * @throws None
         */
        AsyncHashPipeline(IHashVerifier &hasher, std::uint32_t maxPending);

        /**
         * @name ~AsyncHashPipeline
         * @brief Shut down the verifier thread and drain queues.
         */
        ~AsyncHashPipeline();

        AsyncHashPipeline(const AsyncHashPipeline &) = delete;
        AsyncHashPipeline &operator=(const AsyncHashPipeline &) = delete;
        AsyncHashPipeline(AsyncHashPipeline &&) = delete;
        AsyncHashPipeline &operator=(AsyncHashPipeline &&) = delete;

        /**
         * @name submit
         * @brief Submit a cross-sum-valid candidate for hash verification.
         *
         * Blocks if the pending queue is full (backpressure).
         *
         * @param csm The candidate CSM.
         * @throws None
         */
        void submit(crsce::common::Csm csm);

        /**
         * @name nextVerified
         * @brief Dequeue the next hash-verified solution.
         *
         * Blocks until a verified solution is available or the pipeline is done.
         *
         * @return The next verified CSM, or nullopt if no more solutions.
         * @throws None
         */
        [[nodiscard]] auto nextVerified() -> std::optional<crsce::common::Csm>;

        /**
         * @name tryNextVerified
         * @brief Non-blocking dequeue of the next hash-verified solution.
         *
         * Returns immediately with nullopt if no verified solution is available yet.
         *
         * @return The next verified CSM, or nullopt if none ready.
         * @throws None
         */
        [[nodiscard]] auto tryNextVerified() -> std::optional<crsce::common::Csm>;

        /**
         * @name shutdown
         * @brief Signal the verifier that no more candidates will be submitted.
         *
         * After shutdown, nextVerified() will drain remaining verified solutions
         * then return nullopt.
         * @throws None
         */
        void shutdown();

    private:
        /**
         * @name verifierLoop
         * @brief Background thread function that dequeues candidates and verifies hashes.
         * @throws None
         */
        void verifierLoop();

        /**
         * @name kS
         * @brief Matrix dimension.
         */
        static constexpr std::uint16_t kS = 127;

        /**
         * @name hasher_
         * @brief Reference to the hash verifier.
         */
        IHashVerifier &hasher_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

        /**
         * @name maxPending_
         * @brief Maximum number of candidates buffered in the pending queue.
         */
        std::uint32_t maxPending_;

        /**
         * @name pendingMtx_
         * @brief Mutex protecting the pending candidate queue.
         */
        std::mutex pendingMtx_;

        /**
         * @name pendingNotFull_
         * @brief Condition variable signaled when the pending queue has room.
         */
        std::condition_variable pendingNotFull_;

        /**
         * @name pendingNotEmpty_
         * @brief Condition variable signaled when the pending queue has items.
         */
        std::condition_variable pendingNotEmpty_;

        /**
         * @name pending_
         * @brief Queue of candidates awaiting hash verification.
         */
        std::deque<crsce::common::Csm> pending_;

        /**
         * @name verifiedMtx_
         * @brief Mutex protecting the verified output queue.
         */
        std::mutex verifiedMtx_;

        /**
         * @name verifiedNotEmpty_
         * @brief Condition variable signaled when verified solutions are available.
         */
        std::condition_variable verifiedNotEmpty_;

        /**
         * @name verified_
         * @brief Queue of hash-verified solutions.
         */
        std::deque<crsce::common::Csm> verified_;

        /**
         * @name done_
         * @brief Atomic flag: true when no more candidates will be submitted.
         */
        std::atomic<bool> done_{false};

        /**
         * @name verifierDone_
         * @brief Flag: true when the verifier thread has finished processing.
         */
        bool verifierDone_{false};

        /**
         * @name verifierThread_
         * @brief The background verifier thread.
         */
        std::thread verifierThread_;
    };
} // namespace crsce::decompress::solvers
