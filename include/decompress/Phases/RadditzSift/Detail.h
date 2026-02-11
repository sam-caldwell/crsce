/**
 * @file Detail.h
 * @brief Internal helpers for Radditz Sift to enable focused unit testing.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt
 */
#pragma once

// Aggregator header for Radditz Sift helpers (no declarations here).
#include "decompress/Phases/RadditzSift/compute_col_counts.h"
#include "decompress/Phases/RadditzSift/compute_deficits.h"
#include "decompress/Phases/RadditzSift/deficit_abs_sum.h"
#include "decompress/Phases/RadditzSift/collect_row_candidates.h"
#include "decompress/Phases/RadditzSift/swap_lateral.h"
#include "decompress/Phases/RadditzSift/greedy_pair_row.h"
#include "decompress/Phases/RadditzSift/all_deficits_zero.h"
#include "decompress/Phases/RadditzSift/verify_row_sums.h"
