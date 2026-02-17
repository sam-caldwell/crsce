/**
 * @file O11y.cpp
 * @brief Unified observability store implementation.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details
 */
#include "common/O11y/O11y.h"

// Pull in the implementation (self-contained includes + namespace)
#include "detail/O11y_impl.inc"

namespace crsce::o11y {
/**
 * @name instance
 * @brief Singleton accessor for O11y store.
 * @return O11y& Reference to the singleton instance.
 */
O11y &O11y::instance() {
    static O11y inst; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables,misc-use-anonymous-namespace)
    return inst;
}
} // namespace crsce::o11y
