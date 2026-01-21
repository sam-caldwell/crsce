/**
 * @file solver_dummy_impl_smoke_test.cpp
 */
#include <gtest/gtest.h>

#include <cstddef>

#include "decompress/Solver/Solver.h"

using crsce::decompress::Solver;

namespace {
class DummySolver final : public Solver {
public:
    DummySolver() = default;
    std::size_t solve_step() override {
        if (steps_ < 2) {
            ++steps_;
            return 1; // solved one bit
        }
        solved_ = true;
        return 0; // no more progress
    }
    [[nodiscard]] bool solved() const override { return solved_; }
    void reset() override {
        steps_ = 0;
        solved_ = false;
    }

private:
    std::size_t steps_{0};
    bool solved_{false};
};
} // namespace

TEST(SolverDummyImplSmoke, CallsThroughBasePointer) { // NOLINT
    DummySolver impl;
    Solver *s = &impl; // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    EXPECT_FALSE(s->solved());
    EXPECT_EQ(s->solve_step(), 1U);
    EXPECT_EQ(s->solve_step(), 1U);
    EXPECT_EQ(s->solve_step(), 0U);
    EXPECT_TRUE(s->solved());
    s->reset();
    EXPECT_FALSE(s->solved());
}
