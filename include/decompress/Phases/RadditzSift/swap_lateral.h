/**
 * @file swap_lateral.h
 * @author Sam Caldwell
 * @brief Declaration of swap_lateral helper for Radditz Sift.
 */
#pragma once

#include <cstddef>
#include "decompress/Csm/detail/Csm.h"

namespace crsce::decompress::phases::detail {
    /** @brief Swap a single row's bits laterally: 1@cf→0, 0@ct→1. */
    bool swap_lateral(Csm &csm, std::size_t r, std::size_t cf, std::size_t ct);
}

