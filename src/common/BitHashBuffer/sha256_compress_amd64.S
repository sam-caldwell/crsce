/**
 * @file sha256_compress_amd64.S
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief AMD64 SHA-256 block compression using x86 SHA-NI extension instructions.
 *
 * Processes one 64-byte block, updating an 8-word state array in place.
 * Uses sha256rnds2, sha256msg1, sha256msg2 for ~2-3x speedup over scalar C++.
 * Requires: SHA-NI, SSE4.1, SSSE3.
 *
 * extern "C" void sha256_compress_amd64(uint32_t state[8], const uint8_t block[64]);
 */

#ifdef __APPLE__
#define FUNC_NAME  _sha256_compress_amd64
#define CONST_SECT .section __TEXT,__const
#else
#define FUNC_NAME  sha256_compress_amd64
#define CONST_SECT .section .rodata
#endif

    .text
    .p2align 4
    .globl FUNC_NAME

/**
 * @name sha256_compress_amd64
 * @brief SHA-256 block compression using x86 SHA-NI hardware extensions.
 * @param rdi Pointer to 8-word (32-byte) state array (in/out).
 * @param rsi Pointer to 64-byte input block.
 *
 * Register allocation:
 *   xmm0       — implicit operand for sha256rnds2 (MSG + K values)
 *   xmm1       — STATE0 (ABEF in SHA-NI format)
 *   xmm2       — STATE1 (CDGH in SHA-NI format)
 *   xmm3..xmm6 — MSG0..MSG3 (message schedule words)
 *   xmm7       — byte-swap mask (constant)
 *   xmm8       — STATE0 save (for epilogue add-back)
 *   xmm9       — STATE1 save
 *   xmm10      — temporary for message schedule
 */
FUNC_NAME:
    /* Load initial hash state */
    movdqu  (%rdi), %xmm3          /* xmm3 = [A, B, C, D] */
    movdqu  16(%rdi), %xmm4        /* xmm4 = [E, F, G, H] */

    /* Rearrange for SHA-NI state layout:                     */
    /*   STATE0 = ABEF (A in dword3, F in dword0)             */
    /*   STATE1 = CDGH (C in dword3, H in dword0)             */
    pshufd  $0xB1, %xmm3, %xmm3   /* [B, A, D, C] */
    pshufd  $0x1B, %xmm4, %xmm4   /* [H, G, F, E] */
    movdqa  %xmm3, %xmm1
    palignr $8, %xmm4, %xmm1      /* xmm1 = STATE0 (ABEF) */
    pblendw $0xF0, %xmm3, %xmm4   /* xmm4 = STATE1 (CDGH) */
    movdqa  %xmm4, %xmm2          /* xmm2 = STATE1 */

    /* Save initial state for epilogue add-back */
    movdqa  %xmm1, %xmm8          /* ABEF_SAVE */
    movdqa  %xmm2, %xmm9          /* CDGH_SAVE */

    /* Load byte-swap mask for big-endian conversion */
    movdqa  .Lshuf_mask(%rip), %xmm7

    /* Load and byte-swap 64-byte message block */
    movdqu  (%rsi), %xmm3
    pshufb  %xmm7, %xmm3          /* MSG0 = W[0..3] */
    movdqu  16(%rsi), %xmm4
    pshufb  %xmm7, %xmm4          /* MSG1 = W[4..7] */
    movdqu  32(%rsi), %xmm5
    pshufb  %xmm7, %xmm5          /* MSG2 = W[8..11] */
    movdqu  48(%rsi), %xmm6
    pshufb  %xmm7, %xmm6          /* MSG3 = W[12..15] */

    /* ==================================================================== */
    /* Rounds 0-3 (MSG0, schedule update for W[16..19])                     */
    /* ==================================================================== */
    movdqa  .Lk256+0*16(%rip), %xmm0
    paddd   %xmm3, %xmm0
    sha256rnds2 %xmm1, %xmm2      /* STATE1 updated (rounds 0-1) */
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1      /* STATE0 updated (rounds 2-3) */
    sha256msg1  %xmm4, %xmm3
    movdqa  %xmm6, %xmm10
    palignr $4, %xmm5, %xmm10
    paddd   %xmm10, %xmm3
    sha256msg2  %xmm6, %xmm3

    /* ==================================================================== */
    /* Rounds 4-7 (MSG1, schedule update for W[20..23])                     */
    /* ==================================================================== */
    movdqa  .Lk256+1*16(%rip), %xmm0
    paddd   %xmm4, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm5, %xmm4
    movdqa  %xmm3, %xmm10
    palignr $4, %xmm6, %xmm10
    paddd   %xmm10, %xmm4
    sha256msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 8-11 (MSG2, schedule update for W[24..27])                    */
    /* ==================================================================== */
    movdqa  .Lk256+2*16(%rip), %xmm0
    paddd   %xmm5, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm6, %xmm5
    movdqa  %xmm4, %xmm10
    palignr $4, %xmm3, %xmm10
    paddd   %xmm10, %xmm5
    sha256msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 12-15 (MSG3, schedule update for W[28..31])                   */
    /* ==================================================================== */
    movdqa  .Lk256+3*16(%rip), %xmm0
    paddd   %xmm6, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm3, %xmm6
    movdqa  %xmm5, %xmm10
    palignr $4, %xmm4, %xmm10
    paddd   %xmm10, %xmm6
    sha256msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 16-19 (MSG0, schedule update for W[32..35])                   */
    /* ==================================================================== */
    movdqa  .Lk256+4*16(%rip), %xmm0
    paddd   %xmm3, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm4, %xmm3
    movdqa  %xmm6, %xmm10
    palignr $4, %xmm5, %xmm10
    paddd   %xmm10, %xmm3
    sha256msg2  %xmm6, %xmm3

    /* ==================================================================== */
    /* Rounds 20-23 (MSG1, schedule update for W[36..39])                   */
    /* ==================================================================== */
    movdqa  .Lk256+5*16(%rip), %xmm0
    paddd   %xmm4, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm5, %xmm4
    movdqa  %xmm3, %xmm10
    palignr $4, %xmm6, %xmm10
    paddd   %xmm10, %xmm4
    sha256msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 24-27 (MSG2, schedule update for W[40..43])                   */
    /* ==================================================================== */
    movdqa  .Lk256+6*16(%rip), %xmm0
    paddd   %xmm5, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm6, %xmm5
    movdqa  %xmm4, %xmm10
    palignr $4, %xmm3, %xmm10
    paddd   %xmm10, %xmm5
    sha256msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 28-31 (MSG3, schedule update for W[44..47])                   */
    /* ==================================================================== */
    movdqa  .Lk256+7*16(%rip), %xmm0
    paddd   %xmm6, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm3, %xmm6
    movdqa  %xmm5, %xmm10
    palignr $4, %xmm4, %xmm10
    paddd   %xmm10, %xmm6
    sha256msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 32-35 (MSG0, schedule update for W[48..51])                   */
    /* ==================================================================== */
    movdqa  .Lk256+8*16(%rip), %xmm0
    paddd   %xmm3, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm4, %xmm3
    movdqa  %xmm6, %xmm10
    palignr $4, %xmm5, %xmm10
    paddd   %xmm10, %xmm3
    sha256msg2  %xmm6, %xmm3

    /* ==================================================================== */
    /* Rounds 36-39 (MSG1, schedule update for W[52..55])                   */
    /* ==================================================================== */
    movdqa  .Lk256+9*16(%rip), %xmm0
    paddd   %xmm4, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm5, %xmm4
    movdqa  %xmm3, %xmm10
    palignr $4, %xmm6, %xmm10
    paddd   %xmm10, %xmm4
    sha256msg2  %xmm3, %xmm4

    /* ==================================================================== */
    /* Rounds 40-43 (MSG2, schedule update for W[56..59])                   */
    /* ==================================================================== */
    movdqa  .Lk256+10*16(%rip), %xmm0
    paddd   %xmm5, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm6, %xmm5
    movdqa  %xmm4, %xmm10
    palignr $4, %xmm3, %xmm10
    paddd   %xmm10, %xmm5
    sha256msg2  %xmm4, %xmm5

    /* ==================================================================== */
    /* Rounds 44-47 (MSG3, schedule update for W[60..63])                   */
    /* ==================================================================== */
    movdqa  .Lk256+11*16(%rip), %xmm0
    paddd   %xmm6, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1
    sha256msg1  %xmm3, %xmm6
    movdqa  %xmm5, %xmm10
    palignr $4, %xmm4, %xmm10
    paddd   %xmm10, %xmm6
    sha256msg2  %xmm5, %xmm6

    /* ==================================================================== */
    /* Rounds 48-51 (MSG0, no schedule update)                              */
    /* ==================================================================== */
    movdqa  .Lk256+12*16(%rip), %xmm0
    paddd   %xmm3, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1

    /* ==================================================================== */
    /* Rounds 52-55 (MSG1, no schedule update)                              */
    /* ==================================================================== */
    movdqa  .Lk256+13*16(%rip), %xmm0
    paddd   %xmm4, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1

    /* ==================================================================== */
    /* Rounds 56-59 (MSG2, no schedule update)                              */
    /* ==================================================================== */
    movdqa  .Lk256+14*16(%rip), %xmm0
    paddd   %xmm5, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1

    /* ==================================================================== */
    /* Rounds 60-63 (MSG3, no schedule update)                              */
    /* ==================================================================== */
    movdqa  .Lk256+15*16(%rip), %xmm0
    paddd   %xmm6, %xmm0
    sha256rnds2 %xmm1, %xmm2
    pshufd  $0x0E, %xmm0, %xmm0
    sha256rnds2 %xmm2, %xmm1

    /* ==================================================================== */
    /* Epilogue: add saved initial state back                               */
    /* ==================================================================== */
    paddd   %xmm8, %xmm1          /* STATE0 += ABEF_SAVE */
    paddd   %xmm9, %xmm2          /* STATE1 += CDGH_SAVE */

    /* Reverse the SHA-NI state rearrangement back to standard layout:      */
    /*   STATE0 = [F(0), E(1), B(2), A(3)]                                 */
    /*   STATE1 = [H(0), G(1), D(2), C(3)]                                 */
    /* Target: state[0..3] = [A, B, C, D], state[4..7] = [E, F, G, H]     */
    movdqa  %xmm1, %xmm10
    movdqa  %xmm1, %xmm11
    shufps  $0xBB, %xmm2, %xmm10  /* [A, B, C, D] */
    shufps  $0x11, %xmm2, %xmm11  /* [E, F, G, H] */

    /* Store updated state */
    movdqu  %xmm10, (%rdi)
    movdqu  %xmm11, 16(%rdi)
    ret

/* ====================================================================== */
/* Read-only data: byte-swap mask and SHA-256 round constants (K)         */
/* ====================================================================== */
CONST_SECT
    .p2align 4

/* PSHUFB mask to convert little-endian dwords to big-endian */
.Lshuf_mask:
    .byte 3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12

/* SHA-256 round constants (K) — 64 words, 16 groups of 4 */
.Lk256:
    .long 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5  /*  0- 3 */
    .long 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5  /*  4- 7 */
    .long 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3  /*  8-11 */
    .long 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174  /* 12-15 */
    .long 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc  /* 16-19 */
    .long 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da  /* 20-23 */
    .long 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7  /* 24-27 */
    .long 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967  /* 28-31 */
    .long 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13  /* 32-35 */
    .long 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85  /* 36-39 */
    .long 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3  /* 40-43 */
    .long 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070  /* 44-47 */
    .long 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5  /* 48-51 */
    .long 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3  /* 52-55 */
    .long 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208  /* 56-59 */
    .long 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2  /* 60-63 */

#if defined(__linux__) && defined(__ELF__)
    .section .note.GNU-stack,"",@progbits
#endif
