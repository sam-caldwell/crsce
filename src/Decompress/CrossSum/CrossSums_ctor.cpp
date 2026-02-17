/**
 * @file CrossSums_ctor.cpp
 * @brief Constructor for CrossSums aggregate type.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "decompress/CrossSum/CrossSum.h"        // CrossSums
#include "decompress/CrossSum/CrossSumType.h"    // CrossSum

namespace crsce::decompress {
    /**
     * @name CrossSums
     * @brief Initialize CrossSums from individual family CrossSum instances.
     * @param lsm Row family CrossSum.
     * @param vsm Column family CrossSum.
     * @param dsm Diagonal family CrossSum.
     * @param xsm Anti-diagonal family CrossSum.
     */
    CrossSums::CrossSums(const CrossSum &lsm, const CrossSum &vsm, const CrossSum &dsm, const CrossSum &xsm)
        : lsm_(lsm), vsm_(vsm), dsm_(dsm), xsm_(xsm) {}
}
