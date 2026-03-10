/**
 * @file sha1_compress_arm64.S
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief ARM64 SHA-1 block compression using ARMv8.2-a SHA-2 extension instructions.
 *
 * Processes one 64-byte block, updating a 5-word state array in place.
 * Uses sha1c, sha1p, sha1m, sha1h, sha1su0, sha1su1 for hardware-accelerated SHA-1.
 *
 * extern "C" void sha1_compress_arm64(uint32_t state[5], const uint8_t block[64]);
 */

    .arch armv8.2-a+sha2
    .text
    .p2align 4
    .globl _sha1_compress_arm64

/**
 * @name _sha1_compress_arm64
 * @brief SHA-1 block compression using ARM SHA-1 hardware extensions.
 * @param x0 Pointer to 5-word (20-byte) state array (in/out).
 * @param x1 Pointer to 64-byte input block.
 */
_sha1_compress_arm64:
    // Load current hash state: ABCD from state[0..3], E from state[4]
    ld1     {v0.4s}, [x0]           // v0 = ABCD
    ldr     s1, [x0, #16]           // s1 = E (single 32-bit load)

    // Load 64-byte message block into v2-v5 (16 words)
    ld1     {v2.4s, v3.4s, v4.4s, v5.4s}, [x1]

    // Byte-swap to big-endian
    rev32   v2.16b, v2.16b
    rev32   v3.16b, v3.16b
    rev32   v4.16b, v4.16b
    rev32   v5.16b, v5.16b

    // Save initial state for final add-back
    mov     v6.16b, v0.16b          // ABCD_SAVE
    fmov    s7, s1                  // E_SAVE

    // Load K constants (each duplicated 4x to fill 128-bit vector)
    adrp    x2, _sha1_k_arm64@PAGE
    add     x2, x2, _sha1_k_arm64@PAGEOFF
    ld1     {v20.4s, v21.4s, v22.4s, v23.4s}, [x2]

    // ====================================================================
    // Rounds 0-19: sha1c (Choose), K0
    // ====================================================================

    // Group 0 (rounds 0-3)
    add     v16.4s, v2.4s, v20.4s
    sha1h   s17, s0
    sha1c   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v2.4s, v3.4s, v4.4s
    sha1su1 v2.4s, v5.4s

    // Group 1 (rounds 4-7)
    add     v16.4s, v3.4s, v20.4s
    sha1h   s17, s0
    sha1c   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v3.4s, v4.4s, v5.4s
    sha1su1 v3.4s, v2.4s

    // Group 2 (rounds 8-11)
    add     v16.4s, v4.4s, v20.4s
    sha1h   s17, s0
    sha1c   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v4.4s, v5.4s, v2.4s
    sha1su1 v4.4s, v3.4s

    // Group 3 (rounds 12-15)
    add     v16.4s, v5.4s, v20.4s
    sha1h   s17, s0
    sha1c   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v5.4s, v2.4s, v3.4s
    sha1su1 v5.4s, v4.4s

    // Group 4 (rounds 16-19)
    add     v16.4s, v2.4s, v20.4s
    sha1h   s17, s0
    sha1c   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v2.4s, v3.4s, v4.4s
    sha1su1 v2.4s, v5.4s

    // ====================================================================
    // Rounds 20-39: sha1p (Parity), K1
    // ====================================================================

    // Group 5 (rounds 20-23)
    add     v16.4s, v3.4s, v21.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v3.4s, v4.4s, v5.4s
    sha1su1 v3.4s, v2.4s

    // Group 6 (rounds 24-27)
    add     v16.4s, v4.4s, v21.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v4.4s, v5.4s, v2.4s
    sha1su1 v4.4s, v3.4s

    // Group 7 (rounds 28-31)
    add     v16.4s, v5.4s, v21.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v5.4s, v2.4s, v3.4s
    sha1su1 v5.4s, v4.4s

    // Group 8 (rounds 32-35)
    add     v16.4s, v2.4s, v21.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v2.4s, v3.4s, v4.4s
    sha1su1 v2.4s, v5.4s

    // Group 9 (rounds 36-39)
    add     v16.4s, v3.4s, v21.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v3.4s, v4.4s, v5.4s
    sha1su1 v3.4s, v2.4s

    // ====================================================================
    // Rounds 40-59: sha1m (Majority), K2
    // ====================================================================

    // Group 10 (rounds 40-43)
    add     v16.4s, v4.4s, v22.4s
    sha1h   s17, s0
    sha1m   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v4.4s, v5.4s, v2.4s
    sha1su1 v4.4s, v3.4s

    // Group 11 (rounds 44-47)
    add     v16.4s, v5.4s, v22.4s
    sha1h   s17, s0
    sha1m   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v5.4s, v2.4s, v3.4s
    sha1su1 v5.4s, v4.4s

    // Group 12 (rounds 48-51)
    add     v16.4s, v2.4s, v22.4s
    sha1h   s17, s0
    sha1m   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v2.4s, v3.4s, v4.4s
    sha1su1 v2.4s, v5.4s

    // Group 13 (rounds 52-55)
    add     v16.4s, v3.4s, v22.4s
    sha1h   s17, s0
    sha1m   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v3.4s, v4.4s, v5.4s
    sha1su1 v3.4s, v2.4s

    // Group 14 (rounds 56-59)
    add     v16.4s, v4.4s, v22.4s
    sha1h   s17, s0
    sha1m   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v4.4s, v5.4s, v2.4s
    sha1su1 v4.4s, v3.4s

    // ====================================================================
    // Rounds 60-79: sha1p (Parity), K3
    // ====================================================================

    // Group 15 (rounds 60-63) — last group with schedule update
    add     v16.4s, v5.4s, v23.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17
    sha1su0 v5.4s, v2.4s, v3.4s
    sha1su1 v5.4s, v4.4s

    // Group 16 (rounds 64-67) — no schedule update
    add     v16.4s, v2.4s, v23.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17

    // Group 17 (rounds 68-71) — no schedule update
    add     v16.4s, v3.4s, v23.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17

    // Group 18 (rounds 72-75) — no schedule update
    add     v16.4s, v4.4s, v23.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17

    // Group 19 (rounds 76-79) — no schedule update
    add     v16.4s, v5.4s, v23.4s
    sha1h   s17, s0
    sha1p   q0, s1, v16.4s
    fmov    s1, s17

    // ====================================================================
    // Epilogue: add saved initial state back
    // ====================================================================
    add     v0.4s, v0.4s, v6.4s     // ABCD += ABCD_SAVE

    // E += E_SAVE (scalar integer add via GP registers)
    fmov    w8, s1
    fmov    w9, s7
    add     w8, w8, w9
    fmov    s1, w8

    // Store updated state
    st1     {v0.4s}, [x0]
    str     s1, [x0, #16]
    ret

// SHA-1 K constants (each duplicated 4x to fill 128-bit vector)
    .section __TEXT,__const
    .p2align 4
_sha1_k_arm64:
    .long 0x5A827999, 0x5A827999, 0x5A827999, 0x5A827999
    .long 0x6ED9EBA1, 0x6ED9EBA1, 0x6ED9EBA1, 0x6ED9EBA1
    .long 0x8F1BBCDC, 0x8F1BBCDC, 0x8F1BBCDC, 0x8F1BBCDC
    .long 0xCA62C1D6, 0xCA62C1D6, 0xCA62C1D6, 0xCA62C1D6
