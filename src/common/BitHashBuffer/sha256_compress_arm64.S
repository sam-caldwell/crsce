/**
 * @file sha256_compress_arm64.S
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ARM64 SHA-256 block compression using ARMv8.2-a SHA-2 extension instructions.
 *
 * Processes one 64-byte block, updating an 8-word state array in place.
 * Uses sha256h, sha256h2, sha256su0, sha256su1 for ~2-3x speedup over scalar C++.
 *
 * extern "C" void sha256_compress_arm64(uint32_t state[8], const uint8_t block[64]);
 */

    .arch armv8.2-a+sha2
    .text
    .p2align 4
    .globl _sha256_compress_arm64

/**
 * @name _sha256_compress_arm64
 * @brief SHA-256 block compression using ARM SHA-2 hardware extensions.
 * @param x0 Pointer to 8-word (32-byte) state array (in/out).
 * @param x1 Pointer to 64-byte input block.
 */
_sha256_compress_arm64:
    // Load current hash state: ABCD = state[0..3], EFGH = state[4..7]
    ld1     {v0.4s, v1.4s}, [x0]

    // Load 64-byte message block into v2-v5 (16 words)
    ld1     {v2.4s, v3.4s, v4.4s, v5.4s}, [x1]

    // Byte-swap to big-endian (SHA-256 operates on big-endian words)
    rev32   v2.16b, v2.16b
    rev32   v3.16b, v3.16b
    rev32   v4.16b, v4.16b
    rev32   v5.16b, v5.16b

    // Save initial state for final add-back
    mov     v6.16b, v0.16b          // ABCD_SAVE
    mov     v7.16b, v1.16b          // EFGH_SAVE

    // Load K constants base address
    adrp    x2, _sha256_k_arm64@PAGE
    add     x2, x2, _sha256_k_arm64@PAGEOFF

    // ---- Rounds 0-3 (group 0: MSG0 = v2) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v2.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v2.4s, v3.4s
    sha256su1   v2.4s, v4.4s, v5.4s

    // ---- Rounds 4-7 (group 1: MSG1 = v3) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v3.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v3.4s, v4.4s
    sha256su1   v3.4s, v5.4s, v2.4s

    // ---- Rounds 8-11 (group 2: MSG2 = v4) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v4.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v4.4s, v5.4s
    sha256su1   v4.4s, v2.4s, v3.4s

    // ---- Rounds 12-15 (group 3: MSG3 = v5) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v5.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v5.4s, v2.4s
    sha256su1   v5.4s, v3.4s, v4.4s

    // ---- Rounds 16-19 (group 4: MSG0 = v2) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v2.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v2.4s, v3.4s
    sha256su1   v2.4s, v4.4s, v5.4s

    // ---- Rounds 20-23 (group 5: MSG1 = v3) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v3.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v3.4s, v4.4s
    sha256su1   v3.4s, v5.4s, v2.4s

    // ---- Rounds 24-27 (group 6: MSG2 = v4) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v4.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v4.4s, v5.4s
    sha256su1   v4.4s, v2.4s, v3.4s

    // ---- Rounds 28-31 (group 7: MSG3 = v5) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v5.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v5.4s, v2.4s
    sha256su1   v5.4s, v3.4s, v4.4s

    // ---- Rounds 32-35 (group 8: MSG0 = v2) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v2.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v2.4s, v3.4s
    sha256su1   v2.4s, v4.4s, v5.4s

    // ---- Rounds 36-39 (group 9: MSG1 = v3) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v3.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v3.4s, v4.4s
    sha256su1   v3.4s, v5.4s, v2.4s

    // ---- Rounds 40-43 (group 10: MSG2 = v4) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v4.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v4.4s, v5.4s
    sha256su1   v4.4s, v2.4s, v3.4s

    // ---- Rounds 44-47 (group 11: MSG3 = v5) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v5.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s
    sha256su0   v5.4s, v2.4s
    sha256su1   v5.4s, v3.4s, v4.4s

    // ---- Rounds 48-51 (group 12: MSG0 = v2, no schedule update) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v2.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s

    // ---- Rounds 52-55 (group 13: MSG1 = v3, no schedule update) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v3.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s

    // ---- Rounds 56-59 (group 14: MSG2 = v4, no schedule update) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v4.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s

    // ---- Rounds 60-63 (group 15: MSG3 = v5, no schedule update) ----
    ld1     {v16.4s}, [x2], #16
    add     v16.4s, v5.4s, v16.4s
    mov     v17.16b, v0.16b
    sha256h     q0, q1, v16.4s
    sha256h2    q1, q17, v16.4s

    // Epilogue: add saved initial state back
    add     v0.4s, v0.4s, v6.4s
    add     v1.4s, v1.4s, v7.4s

    // Store updated state
    st1     {v0.4s, v1.4s}, [x0]
    ret

// SHA-256 round constants (K)
    .section __TEXT,__const
    .p2align 4
_sha256_k_arm64:
    .long 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
    .long 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
    .long 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
    .long 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
    .long 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
    .long 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
    .long 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
    .long 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
    .long 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
    .long 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
    .long 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
    .long 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
    .long 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
    .long 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
    .long 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
    .long 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
