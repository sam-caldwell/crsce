/**
 * @file CrossSum_ctor.cpp
 */
#include "decompress/CrossSum/CrossSum.h"

namespace crsce::decompress {
    CrossSum::CrossSum(const CrossSumFamily fam, const Vec &targets) : fam_(fam), tgt_(targets) {}
}

