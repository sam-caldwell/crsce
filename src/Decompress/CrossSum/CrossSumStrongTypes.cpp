/**
 * @file CrossSumStrongTypes.cpp
 * @brief Sentinel TU for strong-typed cross-sum wrappers.
 */
#include <span>
#include <cstdint>
#include "decompress/CrossSum/CrossSumBase.h"
#include "decompress/CrossSum/LsmVsmBase.h"
#include "decompress/CrossSum/DiagonalSumBase.h"
#include "decompress/CrossSum/LateralSumMatrix.h"
#include "decompress/CrossSum/VerticalSumMatrix.h"
#include "decompress/CrossSum/DiagonalSumMatrix.h"
#include "decompress/CrossSum/AntiDiagonalSumMatrix.h"

namespace crsce::decompress::xsum {
    namespace {
        void odpcpp_sentinel_crosssum_strongtypes() {
            // Touch types to satisfy include-cleaner
            const std::span<const std::uint16_t> z{};
            const LateralSumMatrix a{z};
            const VerticalSumMatrix b{z};
            const DiagonalSumMatrix c{z};
            const AntiDiagonalSumMatrix d{z};
            const auto *ba = static_cast<const CrossSumBase*>(&a);
            const auto *bb = static_cast<const LsmVsmBase*>(&b);
            const auto *bc = static_cast<const DiagonalSumBase*>(&c);
            (void)ba; (void)bb; (void)bc; (void)d.size();
        }
    }
}
