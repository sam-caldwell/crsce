/**
 * @file sha1_compress_amd64.S
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief AMD64 SHA-1 block compression using x86 SHA-NI extension instructions.
 *
 * Processes one 64-byte block, updating a 5-word state array in place.
 * Uses sha1rnds4, sha1nexte, sha1msg1, sha1msg2 for hardware-accelerated SHA-1.
 * Requires: SHA-NI, SSE4.1 (pextrd), SSSE3 (pshufb).
 *
 * extern "C" void sha1_compress_amd64(uint32_t state[5], const uint8_t block[64]);
 */

#ifdef __APPLE__
#define FUNC_NAME  _sha1_compress_amd64
#define CONST_SECT .section __TEXT,__const
#else
#define FUNC_NAME  sha1_compress_amd64
#define CONST_SECT .section .rodata
#endif

    .text
    .p2align 4
    .globl FUNC_NAME

/**
 * @name sha1_compress_amd64
 * @brief SHA-1 block compression using x86 SHA-NI hardware extensions.
 * @param rdi Pointer to 5-word (20-byte) state array (in/out).
 * @param rsi Pointer to 64-byte input block.
 *
 * Register allocation:
 *   xmm0       — ABCD state (A in dword3, D in dword0 — SHA-NI format)
 *   xmm1       — E0 (alternating E register, E in dword3)
 *   xmm2       — E1 (alternating E register)
 *   xmm3..xmm6 — MSG0..MSG3 (message schedule words)
 *   xmm7       — byte-swap mask (constant)
 *   xmm8       — ABCD save (for epilogue add-back)
 *   xmm9       — E save (for epilogue add-back)
 *   xmm10      — temporary
 */
FUNC_NAME:
    /* Load initial hash state: ABCD from state[0..3], E from state[4] */
    movdqu  (%rdi), %xmm0          /* xmm0 = [A(0), B(1), C(2), D(3)] */
    pshufd  $0x1B, %xmm0, %xmm0   /* Reverse to SHA-NI format: [D, C, B, A] */

    movd    16(%rdi), %xmm1        /* xmm1.dword0 = E */
    pshufd  $0x39, %xmm1, %xmm1   /* Move E to dword3: [0, 0, 0, E] */

    /* Save initial state for epilogue add-back */
    movdqa  %xmm0, %xmm8          /* ABCD_SAVE */
    movdqa  %xmm1, %xmm9          /* E_SAVE */

    /* Load byte-swap mask */
    movdqa  .Lsha1_shuf_mask(%rip), %xmm7

    /* Load and byte-swap 64-byte message block */
    movdqu  (%rsi), %xmm3
    pshufb  %xmm7, %xmm3          /* MSG0 */
    movdqu  16(%rsi), %xmm4
    pshufb  %xmm7, %xmm4          /* MSG1 */
    movdqu  32(%rsi), %xmm5
    pshufb  %xmm7, %xmm5          /* MSG2 */
    movdqu  48(%rsi), %xmm6
    pshufb  %xmm7, %xmm6          /* MSG3 */

    /* ==================================================================== */
    /* Rounds 0-3: sha1c (Choose), K0                                       */
    /* Group 0 — sha1msg1 on MSG0; no completion yet                        */
    /* ==================================================================== */
    sha1nexte %xmm3, %xmm1        /* E0 = nexte(E0, MSG0) */
    movdqa  %xmm0, %xmm2          /* E1 = ABCD */
    sha1rnds4 $0, %xmm1, %xmm0    /* ABCD = rnds4(ABCD, E0, fn=0) */
    sha1msg1  %xmm4, %xmm3        /* MSG0 partial schedule */

    /* ==================================================================== */
    /* Rounds 4-7: sha1c (Choose), K0                                       */
    /* Group 1 — sha1msg1 on MSG1; complete MSG0                            */
    /* ==================================================================== */
    sha1nexte %xmm4, %xmm2        /* E1 = nexte(E1, MSG1) */
    movdqa  %xmm0, %xmm1          /* E0 = ABCD */
    sha1rnds4 $0, %xmm2, %xmm0
    sha1msg1  %xmm5, %xmm4
    pxor    %xmm5, %xmm3          /* MSG0 ^= MSG2 */
    sha1msg2  %xmm6, %xmm3        /* MSG0 = sha1msg2(MSG0, MSG3) */

    /* ==================================================================== */
    /* Rounds 8-11: sha1c (Choose), K0                                      */
    /* Group 2 — sha1msg1 on MSG2; complete MSG1                            */
    /* ==================================================================== */
    sha1nexte %xmm5, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $0, %xmm1, %xmm0
    sha1msg1  %xmm6, %xmm5
    pxor    %xmm6, %xmm4
    sha1msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 12-15: sha1c (Choose), K0                                     */
    /* Group 3 — sha1msg1 on MSG3; complete MSG2                            */
    /* ==================================================================== */
    sha1nexte %xmm6, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $0, %xmm2, %xmm0
    sha1msg1  %xmm3, %xmm6
    pxor    %xmm3, %xmm5
    sha1msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 16-19: sha1c (Choose), K0                                     */
    /* Group 4 — sha1msg1 on MSG0; complete MSG3                            */
    /* ==================================================================== */
    sha1nexte %xmm3, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $0, %xmm1, %xmm0
    sha1msg1  %xmm4, %xmm3
    pxor    %xmm4, %xmm6
    sha1msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 20-23: sha1p (Parity), K1                                     */
    /* Group 5 — sha1msg1 on MSG1; complete MSG0                            */
    /* ==================================================================== */
    sha1nexte %xmm4, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $1, %xmm2, %xmm0
    sha1msg1  %xmm5, %xmm4
    pxor    %xmm5, %xmm3
    sha1msg2  %xmm6, %xmm3

    /* ==================================================================== */
    /* Rounds 24-27: sha1p (Parity), K1                                     */
    /* Group 6 — sha1msg1 on MSG2; complete MSG1                            */
    /* ==================================================================== */
    sha1nexte %xmm5, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $1, %xmm1, %xmm0
    sha1msg1  %xmm6, %xmm5
    pxor    %xmm6, %xmm4
    sha1msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 28-31: sha1p (Parity), K1                                     */
    /* Group 7 — sha1msg1 on MSG3; complete MSG2                            */
    /* ==================================================================== */
    sha1nexte %xmm6, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $1, %xmm2, %xmm0
    sha1msg1  %xmm3, %xmm6
    pxor    %xmm3, %xmm5
    sha1msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 32-35: sha1p (Parity), K1                                     */
    /* Group 8 — sha1msg1 on MSG0; complete MSG3                            */
    /* ==================================================================== */
    sha1nexte %xmm3, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $1, %xmm1, %xmm0
    sha1msg1  %xmm4, %xmm3
    pxor    %xmm4, %xmm6
    sha1msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 36-39: sha1p (Parity), K1                                     */
    /* Group 9 — sha1msg1 on MSG1; complete MSG0                            */
    /* ==================================================================== */
    sha1nexte %xmm4, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $1, %xmm2, %xmm0
    sha1msg1  %xmm5, %xmm4
    pxor    %xmm5, %xmm3
    sha1msg2  %xmm6, %xmm3

    /* ==================================================================== */
    /* Rounds 40-43: sha1m (Majority), K2                                   */
    /* Group 10 — sha1msg1 on MSG2; complete MSG1                           */
    /* ==================================================================== */
    sha1nexte %xmm5, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $2, %xmm1, %xmm0
    sha1msg1  %xmm6, %xmm5
    pxor    %xmm6, %xmm4
    sha1msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 44-47: sha1m (Majority), K2                                   */
    /* Group 11 — sha1msg1 on MSG3; complete MSG2                           */
    /* ==================================================================== */
    sha1nexte %xmm6, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $2, %xmm2, %xmm0
    sha1msg1  %xmm3, %xmm6
    pxor    %xmm3, %xmm5
    sha1msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 48-51: sha1m (Majority), K2                                   */
    /* Group 12 — sha1msg1 on MSG0; complete MSG3                           */
    /* ==================================================================== */
    sha1nexte %xmm3, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $2, %xmm1, %xmm0
    sha1msg1  %xmm4, %xmm3
    pxor    %xmm4, %xmm6
    sha1msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 52-55: sha1m (Majority), K2                                   */
    /* Group 13 — sha1msg1 on MSG1; complete MSG0                           */
    /* ==================================================================== */
    sha1nexte %xmm4, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $2, %xmm2, %xmm0
    sha1msg1  %xmm5, %xmm4
    pxor    %xmm5, %xmm3
    sha1msg2  %xmm6, %xmm3

    /* ==================================================================== */
    /* Rounds 56-59: sha1m (Majority), K2                                   */
    /* Group 14 — sha1msg1 on MSG2; complete MSG1                           */
    /* ==================================================================== */
    sha1nexte %xmm5, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $2, %xmm1, %xmm0
    sha1msg1  %xmm6, %xmm5
    pxor    %xmm6, %xmm4
    sha1msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 60-63: sha1p (Parity), K3                                     */
    /* Group 15 — sha1msg1 on MSG3; complete MSG2                           */
    /* ==================================================================== */
    sha1nexte %xmm6, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $3, %xmm2, %xmm0
    sha1msg1  %xmm3, %xmm6
    pxor    %xmm3, %xmm5
    sha1msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 64-67: sha1p (Parity), K3                                     */
    /* Group 16 — no sha1msg1; complete MSG3                                */
    /* ==================================================================== */
    sha1nexte %xmm3, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $3, %xmm1, %xmm0
    pxor    %xmm4, %xmm6
    sha1msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 68-71: sha1p (Parity), K3                                     */
    /* Group 17 — no schedule                                               */
    /* ==================================================================== */
    sha1nexte %xmm4, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $3, %xmm2, %xmm0

    /* ==================================================================== */
    /* Rounds 72-75: sha1p (Parity), K3                                     */
    /* Group 18 — no schedule                                               */
    /* ==================================================================== */
    sha1nexte %xmm5, %xmm1
    movdqa  %xmm0, %xmm2
    sha1rnds4 $3, %xmm1, %xmm0

    /* ==================================================================== */
    /* Rounds 76-79: sha1p (Parity), K3                                     */
    /* Group 19 — no schedule                                               */
    /* ==================================================================== */
    sha1nexte %xmm6, %xmm2
    movdqa  %xmm0, %xmm1
    sha1rnds4 $3, %xmm2, %xmm0

    /* ==================================================================== */
    /* Epilogue: add saved initial state back                               */
    /* ==================================================================== */
    paddd   %xmm8, %xmm0          /* ABCD += ABCD_SAVE */

    /* Extract final E: after the last group (odd, group 19),               */
    /* xmm1 = old ABCD (saved before rnds4). sha1nexte(xmm1, 0) extracts   */
    /* the final E value via ROL32(dword3, 30).                             */
    pxor    %xmm10, %xmm10
    sha1nexte %xmm10, %xmm1       /* xmm1.dword3 = final E */
    paddd   %xmm9, %xmm1          /* E += E_SAVE */

    /* Store updated state */
    pshufd  $0x1B, %xmm0, %xmm0   /* Reverse ABCD back to [A, B, C, D] */
    movdqu  %xmm0, (%rdi)          /* state[0..3] */
    pextrd  $3, %xmm1, 16(%rdi)    /* state[4] = E (from dword3) */
    ret

/* ====================================================================== */
/* Read-only data: byte-swap mask                                         */
/* ====================================================================== */
CONST_SECT
    .p2align 4

/* PSHUFB mask to convert little-endian dwords to big-endian */
.Lsha1_shuf_mask:
    .byte 3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12

#if defined(__linux__) && defined(__ELF__)
    .section .note.GNU-stack,"",@progbits
#endif
