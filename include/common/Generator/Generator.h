/**
 * @file Generator.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Minimal C++23 coroutine-based generator template (std::generator polyfill).
 *
 * Provides a lazy, single-pass, pull-based range that suspends after each co_yield
 * and resumes on iterator increment.  Used as a polyfill until the toolchain ships
 * a conforming <generator> header.
 */
#pragma once

#include <coroutine>
#include <exception>
#include <utility>

namespace crsce::common {

    /**
     * @class Generator
     * @name Generator
     * @brief Coroutine-based generator that lazily yields values of type T.
     *
     * Satisfies the range concept (begin/end) so it can be used in range-for loops:
     * @code
     *   Generator<int> count(int n) { for (int i = 0; i < n; ++i) co_yield i; }
     *   for (auto v : count(5)) { ... }
     * @endcode
     *
     * @tparam T The value type yielded by the coroutine.
     */
    template <typename T>
    class Generator {
    public:
        /**
         * @struct promise_type
         * @name promise_type
         * @brief Coroutine promise that stores the most recently yielded value.
         */
        struct promise_type {
            /**
             * @name value_
             * @brief Pointer to the most recently yielded value.
             */
            const T *value_ = nullptr; // NOLINT(misc-non-private-member-variables-in-classes)

            /**
             * @name exception_
             * @brief Captured exception from the coroutine body, if any.
             */
            std::exception_ptr exception_ = nullptr; // NOLINT(misc-non-private-member-variables-in-classes)

            /**
             * @name get_return_object
             * @brief Create the Generator object from this promise.
             * @return A Generator wrapping the coroutine handle.
             * @throws None
             */
            auto get_return_object() -> Generator {
                return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            /**
             * @name initial_suspend
             * @brief Suspend immediately on coroutine entry (lazy start).
             * @return std::suspend_always.
             * @throws None
             */
            static auto initial_suspend() noexcept -> std::suspend_always { return {}; }

            /**
             * @name final_suspend
             * @brief Suspend at coroutine exit so the handle remains valid for destruction.
             * @return std::suspend_always.
             * @throws None
             */
            static auto final_suspend() noexcept -> std::suspend_always { return {}; }

            /**
             * @name yield_value
             * @brief Accept a co_yield expression, storing a pointer to the value.
             * @param value The value being yielded.
             * @return std::suspend_always to return control to the caller.
             * @throws None
             */
            auto yield_value(const T &value) noexcept -> std::suspend_always {
                value_ = &value;
                return {};
            }

            /**
             * @name return_void
             * @brief Called when the coroutine body falls off the end or executes co_return.
             * @throws None
             */
            void return_void() noexcept {}

            /**
             * @name unhandled_exception
             * @brief Capture any exception thrown inside the coroutine body.
             * @throws None
             */
            void unhandled_exception() { exception_ = std::current_exception(); }
        };

        /**
         * @name handle_type
         * @brief Alias for the coroutine handle type.
         */
        using handle_type = std::coroutine_handle<promise_type>;

        /**
         * @name Generator
         * @brief Construct a Generator from a coroutine handle.
         * @param handle The coroutine handle to manage.
         * @throws None
         */
        explicit Generator(handle_type handle) : handle_(handle) {}

        /**
         * @name ~Generator
         * @brief Destroy the generator and its coroutine frame.
         */
        ~Generator() {
            if (handle_) {
                handle_.destroy();
            }
        }

        Generator(const Generator &) = delete;
        Generator &operator=(const Generator &) = delete;

        /**
         * @name Generator
         * @brief Move constructor. Transfers coroutine ownership.
         * @param other The generator to move from.
         * @throws None
         */
        Generator(Generator &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

        /**
         * @name operator=
         * @brief Move assignment. Transfers coroutine ownership.
         * @param other The generator to move from.
         * @return Reference to this generator.
         * @throws None
         */
        Generator &operator=(Generator &&other) noexcept {
            if (this != &other) {
                if (handle_) {
                    handle_.destroy();
                }
                handle_ = std::exchange(other.handle_, nullptr);
            }
            return *this;
        }

        /**
         * @struct sentinel
         * @name sentinel
         * @brief Sentinel type for end-of-range comparison.
         */
        struct sentinel {};

        /**
         * @struct iterator
         * @name iterator
         * @brief Input iterator that resumes the coroutine on each increment.
         */
        struct iterator {
            /**
             * @name handle_
             * @brief The coroutine handle being iterated.
             */
            handle_type handle_; // NOLINT(misc-non-private-member-variables-in-classes)

            /**
             * @name operator==
             * @brief Compare with sentinel to detect end of iteration.
             * @return True if the coroutine is exhausted.
             * @throws None
             */
            auto operator==(sentinel /*unused*/) const -> bool { return !handle_ || handle_.done(); }

            /**
             * @name operator++
             * @brief Advance the coroutine to the next yield point.
             * @return Reference to this iterator.
             * @throws Rethrows any exception captured in the coroutine body.
             */
            auto operator++() -> iterator & {
                handle_.resume();
                if (handle_.promise().exception_) {
                    std::rethrow_exception(handle_.promise().exception_);
                }
                return *this;
            }

            /**
             * @name operator*
             * @brief Access the most recently yielded value.
             * @return Const reference to the yielded value.
             * @throws None
             */
            auto operator*() const -> const T & { return *handle_.promise().value_; }
        };

        /**
         * @name begin
         * @brief Start iteration by resuming the coroutine to its first yield point.
         * @return An iterator positioned at the first yielded value.
         * @throws Rethrows any exception captured during the first resume.
         */
        auto begin() -> iterator {
            if (handle_) {
                handle_.resume();
                if (handle_.promise().exception_) {
                    std::rethrow_exception(handle_.promise().exception_);
                }
            }
            return iterator{handle_};
        }

        /**
         * @name end
         * @brief Return the sentinel marking end of iteration.
         * @return A sentinel value.
         * @throws None
         */
        static auto end() -> sentinel { return {}; }

    private:
        /**
         * @name handle_
         * @brief The coroutine handle managed by this generator.
         */
        handle_type handle_ = nullptr;
    };

} // namespace crsce::common
