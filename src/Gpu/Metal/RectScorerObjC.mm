//
// RectScorerObjC.mm
// Objective-C++ shim to run the rectangle scorer kernel via Apple Metal.
// Compiled with AppleClang (OBJCXX) and linked with Metal/Foundation/objc.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstring>

#include "decompress/Gpu/Metal/RectScorerObjC.h"

// Embed the kernel source directly to avoid filesystem/runtime packaging.
static const char *kRectanglesMetalSrc = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Candidate {
    ushort d0, d1, d2, d3;
    ushort x0, x1, x2, x3;
    char   t0, t1, t2, t3;
    char   pad0;
};

struct Params { uint S; uint N; };

kernel void score_rectangles(
    device const ushort *      dcnt   [[ buffer(0) ]],
    device const ushort *      xcnt   [[ buffer(1) ]],
    device const ushort *      vcnt   [[ buffer(2) ]],
    device const ushort *      dsm    [[ buffer(3) ]],
    device const ushort *      xsm    [[ buffer(4) ]],
    device const ushort *      vsm    [[ buffer(5) ]],
    device const Candidate *   cands  [[ buffer(6) ]],
    device int *               out    [[ buffer(7) ]],
    constant Params &          P      [[ buffer(8) ]],
    uint tid                          [[ thread_position_in_grid ]])
{
    if (tid >= P.N) return;
    const Candidate c = cands[tid];

    // Load current counts and targets for affected indices
    const ushort d_idx[4] = { c.d0, c.d1, c.d2, c.d3 };
    const ushort x_idx[4] = { c.x0, c.x1, c.x2, c.x3 };
    const short  t[4]     = { (short)c.t0, (short)c.t1, (short)c.t2, (short)c.t3 };

    int before = 0;
    int after  = 0;

    // DSM family
    ushort d_set[4];
    short  d_acc[4];
    ushort d_count = 0;
    for (ushort i = 0; i < 4; ++i) {
        const ushort idx = d_idx[i];
        bool found = false;
        for (ushort j = 0; j < d_count; ++j) {
            if (d_set[j] == idx) { d_acc[j] += t[i]; found = true; break; }
        }
        if (!found) { d_set[d_count] = idx; d_acc[d_count] = t[i]; d_count++; }
    }
    for (ushort j = 0; j < d_count; ++j) {
        const int cb = (int)dcnt[d_set[j]];
        const int tb = (int)dsm[d_set[j]];
        const int ca = cb + (int)d_acc[j];
        const int w  = max(1, abs(cb - tb));
        before += w * abs(cb - tb);
        after  += w * abs(ca - tb);
    }

    // XSM family
    ushort x_set[4];
    short  x_acc[4];
    ushort x_count = 0;
    for (ushort i = 0; i < 4; ++i) {
        const ushort idx = x_idx[i];
        bool found = false;
        for (ushort j = 0; j < x_count; ++j) {
            if (x_set[j] == idx) { x_acc[j] += t[i]; found = true; break; }
        }
        if (!found) { x_set[x_count] = idx; x_acc[x_count] = t[i]; x_count++; }
    }
    for (ushort j = 0; j < x_count; ++j) {
        const int cb = (int)xcnt[x_set[j]];
        const int tb = (int)xsm[x_set[j]];
        const int ca = cb + (int)x_acc[j];
        const int w  = max(1, abs(cb - tb));
        before += w * abs(cb - tb);
        after  += w * abs(ca - tb);
    }

    // VSM family (columns). Reconstruct (r,c) for corners, accumulate per-column deltas from t[]
    ushort c_set[4];
    short  c_acc[4];
    ushort c_count = 0;
    for (ushort i = 0; i < 4; ++i) {
        const ushort d = d_idx[i];
        const ushort x = x_idx[i];
        // r = (x - d) mod S, c = (r + d) mod S
        const ushort r = (ushort)((x + P.S - d) % P.S);
        const ushort c = (ushort)((r + d) % P.S);
        bool found = false;
        for (ushort j = 0; j < c_count; ++j) {
            if (c_set[j] == c) { c_acc[j] += t[i]; found = true; break; }
        }
        if (!found) { c_set[c_count] = c; c_acc[c_count] = t[i]; c_count++; }
    }
    for (ushort j = 0; j < c_count; ++j) {
        const int cb = (int)vcnt[c_set[j]];
        const int tb = (int)vsm[c_set[j]];
        const int ca = cb + (int)c_acc[j];
        const int w  = max(1, abs(cb - tb));
        before += w * abs(cb - tb);
        after  += w * abs(ca - tb);
    }

    out[tid] = (after - before);
}
)METAL";

static inline bool crsce_metal_compile_library(id<MTLDevice> device,
                                               id<MTLLibrary> __strong *outLib) {
    NSError *err = nil;
    NSString *src = [[NSString alloc] initWithUTF8String:kRectanglesMetalSrc];
    if (!src) return false;
    id<MTLLibrary> lib = [device newLibraryWithSource:src options:nil error:&err];
    if (!lib) {
        return false;
    }
    *outLib = lib;
    return true;
}

bool crsce_metal_score_rects(const std::uint16_t *dcnt,
                             const std::uint16_t *xcnt,
                             const std::uint16_t *vcnt,
                             const std::uint16_t *dsm,
                             const std::uint16_t *xsm,
                             const std::uint16_t *vsm,
                             const RectCandidateC *cands,
                             std::size_t S,
                             std::size_t N,
                             std::int32_t *out_dphi) {
    @autoreleasepool {
        if (!dcnt || !xcnt || !dsm || !xsm || !cands || !out_dphi) return false;
        if (S == 0 || N == 0) return false;

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) return false;

        id<MTLLibrary> lib = nil;
        if (!crsce_metal_compile_library(device, &lib)) return false;

        id<MTLFunction> func = [lib newFunctionWithName:@"score_rectangles"];
        if (!func) return false;

        NSError *psoErr = nil;
        id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:func error:&psoErr];
        if (!pso) return false;

        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!queue) return false;

        // Buffers
        id<MTLBuffer> b_dcnt = [device newBufferWithBytes:dcnt length:S*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_xcnt = [device newBufferWithBytes:xcnt length:S*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_vcnt = [device newBufferWithBytes:vcnt length:S*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_dsm  = [device newBufferWithBytes:dsm  length:S*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_xsm  = [device newBufferWithBytes:xsm  length:S*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_vsm  = [device newBufferWithBytes:vsm  length:S*sizeof(uint16_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_cand = [device newBufferWithBytes:cands length:N*sizeof(RectCandidateC) options:MTLResourceStorageModeShared];
        id<MTLBuffer> b_out  = [device newBufferWithLength:N*sizeof(int32_t) options:MTLResourceStorageModeShared];
        struct Params { uint32_t S; uint32_t N; } params = { (uint32_t)S, (uint32_t)N };
        id<MTLBuffer> b_par  = [device newBufferWithBytes:&params length:sizeof(params) options:MTLResourceStorageModeShared];
        if (!b_dcnt || !b_xcnt || !b_vcnt || !b_dsm || !b_xsm || !b_vsm || !b_cand || !b_out || !b_par) return false;

        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setComputePipelineState:pso];
        [enc setBuffer:b_dcnt offset:0 atIndex:0];
        [enc setBuffer:b_xcnt offset:0 atIndex:1];
        [enc setBuffer:b_vcnt offset:0 atIndex:2];
        [enc setBuffer:b_dsm  offset:0 atIndex:3];
        [enc setBuffer:b_xsm  offset:0 atIndex:4];
        [enc setBuffer:b_vsm  offset:0 atIndex:5];
        [enc setBuffer:b_cand offset:0 atIndex:6];
        [enc setBuffer:b_out  offset:0 atIndex:7];
        [enc setBuffer:b_par  offset:0 atIndex:8];

        NSUInteger tg = MIN((NSUInteger)64, pso.maxTotalThreadsPerThreadgroup);
        if (tg == 0) tg = 64;
        MTLSize grid = MTLSizeMake((NSUInteger)N, 1, 1);
        MTLSize tgs  = MTLSizeMake(tg, 1, 1);
        if ([enc respondsToSelector:@selector(dispatchThreads:threadsPerThreadgroup:)]) {
            [enc dispatchThreads:grid threadsPerThreadgroup:tgs];
        } else {
            // Fallback to threadgroups API
            NSUInteger groups = ((NSUInteger)N + tg - 1) / tg;
            [enc dispatchThreadgroups:MTLSizeMake(groups,1,1) threadsPerThreadgroup:tgs];
        }
        [enc endEncoding];

        [cb commit];
        [cb waitUntilCompleted];

        std::memcpy(out_dphi, [b_out contents], N*sizeof(std::int32_t));
        return true;
    }
}
