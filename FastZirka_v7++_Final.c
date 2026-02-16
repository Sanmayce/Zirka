// v7++ (in addition to v7, sorted progress bar was added; fnv replaced with Pippip; TO-DO: use memcmp on DiskEntry)
// $ clang -O3 -msse4.2 -maes -fopenmp FastZirka_v7++_Final.c -o FastZirka_v7++_Final -DrankmapSERIAL

/*
Zirka v7: Original & Unique Features

- Zero-RAM Architecture (Infinite Memory Model)
Internal RAM: 0 Bytes. The tool completely forgoes `malloc()` for data storage. It utilizes Memory Mapped Files (`mmap`) to treat the Hard Drive/SSD as its working memory.
Capability: This decouples processing limits from physical RAM, allowing a laptop with 8GB RAM to process multi-terabyte datasets without swapping or crashing.
It DeDuplicates (using no RAM) and DeDeDuplicates (using no RAM) using the Fastest hash - 'FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte'.

- 48x "External RAM" Requirement
To achieve this zero-RAM footprint, Zirka v7 leverages massive temporary disk space, effectively using the disk as "External RAM."
The Math: It requires 48x the input file size in free disk space to store the three massive parallel structures:
24x for the Hash Index (`zirka_index.tmp`)
16x for the Nuclear Updates Log (`zirka_updates.tmp`)
8x for the Rank Map (`zirka_rank.tmp`)

- The "Nuclear Option" (Sequential I/O Transformation)
Innovation: Solves the physical limitation of disk thrashing (random writes) by converting them into sequential operations.
Process: Instead of jumping randomly to write duplicate pointers, Zirka Gathers them into a linear log, Sorts them by target address, and Applies them in a single monotonic sweep.
Result: Transforms a bottleneck that would take days on mechanical drives into a streaming operation that completes in minutes.

- Heavily Multi-Threaded Pipeline (64-Core Scalability)
Unlike standard compressors that are often single-threaded, Zirka v7 is designed to saturate every available CPU core.
Stage 1 (Hashing): Uses `#pragma omp parallel for` to hash independent file chunks simultaneously.
Stage 2 (Sorting): Uses a custom Recursive Parallel Tasking system to split the massive index sort across all cores.
Stage 3 (Gathering): Threads scan independent sections of the sorted index to find duplicates without locking.
Stage 4 (Applying): The "Nuclear" application is sharded, allowing multiple threads to stream writes to the Rank Map in parallel regions.

- Forgoing Standard `qsort()`
Custom Engine: Zirka v7 abandons the standard C library `qsort()` because it is single-threaded.
Solution: Implements a custom In-Place Parallel Quicksort (`omp_quicksort`). This algorithm creates OpenMP tasks for partitions, allowing the sorting of billion-entry arrays to be distributed dynamically across all CPU threads.

- Hardware-Accelerated "Pip-Pip" Hashing
Method: Implements the `FNV1A_Pippip_128` algorithm, manually written with SSE4.2 and AES-NI intrinsics.
Speed: Delivers extreme (in fact Fastest) hashing throughput by processing data in 128-bit vector lanes, ensuring the CPU is never waiting on calculation during the I/O stream.

- O(1) Encoder Lookup
Speed: By pre-calculating the Rank Map, the final encoding stage requires zero searching.
Mechanism: It performs a direct array lookup (`rank[position]`), making the compression speed dependent only on sequential read speed, not on the complexity or redundancy of the data.

- "First Occurrence" Strategy (The "Anti-Greedy" Approach)
Concept: Instead of linking duplicates to their nearest neighbor (Greedy), Zirka forces every duplicate to point back to the absolute first occurrence in the file, often gigabytes away.
Strategic Value: This deliberately avoids "eating" the short-range matches that standard LZ compressors (zstd, gzip, 7zip) thrive on.
Synergy: By handling the "Heavy Lifting" (deduplicating matches that are e.g. 25GB apart), Zirka clears the path for the backend LZ compressor to focus entirely on its strength: hyper-efficient bit-packing of local, short-range redundancies.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <immintrin.h>
#include <omp.h> // OPENMP

#define VERSION 7
#define CHUNK_SIZE 4096 //384 //4096 //256
#define MAGIC_BYTE 255
#define SORT_THRESHOLD 4096 // Items below this count use serial qsort
#define NULL_RANK 0xFFFFFFFFFFFFFFFFULL

// --- PIPPIP HASH IMPLEMENTATION ---
#define _PADr_KAZE(x, n) ( ((x) << (n))>>(n) )
#define _PAD_KAZE(x, n) ( ((x) << (n)) )
#define eXdupe
        #ifdef eXdupe
#define SHA1_DIGEST_SIZE 16
        #else
#define SHA1_DIGEST_SIZE 20
        #endif

int g_max_threads_used = 0;
int g_total_tasks = 0;
long long SortedSoFar = 0;
uint64_t entry_count = 0;
uint64_t update_count = 0;

// --- SHA1 IMPLEMENTATION ---
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define F0(b, c, d) (((b) & (c)) | ((~(b)) & (d)))
#define F1(b, c, d) ((b) ^ (c) ^ (d))
#define F2(b, c, d) (((b) & (c)) | ((b) & (d)) | ((c) & (d)))
#define F3(b, c, d) ((b) ^ (c) ^ (d))

static inline uint64_t fold64(uint64_t A, uint64_t B) {
//  #if defined(__GNUC__) || defined(__clang__)
        __uint128_t r = (__uint128_t)A * B;
        return (uint64_t)r ^ (uint64_t)(r >> 64);
//  #else
//      uint64_t hash64 = A ^ B;
//      hash64 *= 1099511628211; //591798841;
//      return hash64;
//  #endif
}

static inline uint32_t fold32(uint32_t A, uint32_t B) {
//  #if defined(__GNUC__) || defined(__clang__)
        uint64_t r = (uint64_t)A * (uint64_t)B;
        return (uint32_t)r ^ (uint32_t)(r >> 32);
//  #else
//      uint32_t hash32 = A ^ B;
//      hash32 *= 591798841;
//      return hash32;
//  #endif
}

// FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte: the 100% FREE lookuper, last update: 2026-Feb-14, Kaze (sanmayce@sanmayce.com).
// Note1: This latest revision was written when Mikayla "saveafox" left this world.
// Note2: Too weak due to ChunkA2 and ChunkB2 not AESed... 2026-Feb-14, strengthened

// "There it now stands for ever. Black on white.
// I can't get away from it. Ahoy, Yorikke, ahoy, hoy, ho!
// Go to hell now if you wish. What do I care? It's all the same now to me.
// I am part of you now. Where you go I go, where you leave I leave, when you go to the devil I go. Married.
// Vanished from the living. Damned and doomed. Of me there is not left a breath in all the vast world.
// Ahoy, Yorikke! Ahoy, hoy, ho!
// I am not buried in the sea,
// The death ship is now part of me
// So far from sunny New Orleans
// So far from lovely Louisiana."
// /An excerpt from 'THE DEATH SHIP - THE STORY OF AN AMERICAN SAILOR' by B.TRAVEN/
// 
// "Walking home to our good old Yorikke, I could not help thinking of this beautiful ship, with a crew on board that had faces as if they were seeing ghosts by day and by night.
// Compared to that gilded Empress, the Yorikke was an honorable old lady with lavender sachets in her drawers.
// Yorikke did not pretend to anything she was not. She lived up to her looks. Honest to her lowest ribs and to the leaks in her bilge.
// Now, what is this? I find myself falling in love with that old jane.
// All right, I cannot pass by you, Yorikke; I have to tell you I love you. Honest, baby, I love you.
// I have six black finger-nails, and four black and green-blue nails on my toes, which you, honey, gave me when necking you.
// Grate-bars have crushed some of my toes. And each finger-nail has its own painful story to tell.
// My chest, my back, my arms, my legs are covered with scars of burns and scorchings.
// Each scar, when it was being created, caused me pains which I shall surely never forget.
// But every outcry of pain was a love-cry for you, honey.
// You are no hypocrite. Your heart does not bleed tears when you do not feel heart-aches deeply and truly.
// You do not dance on the water if you do not feel like being jolly and kicking chasers in the pants.
// Your heart never lies. It is fine and clean like polished gold. Never mind the rags, honey dear.
// When you laugh, your whole soul and all your body is laughing.
// And when you weep, sweety, then you weep so that even the reefs you pass feel like weeping with you.
// I never want to leave you again, honey. I mean it. Not for all the rich and elegant buckets in the world.
// I love you, my gypsy of the sea!"
// /An excerpt from 'THE DEATH SHIP - THE STORY OF AN AMERICAN SAILOR' by B.TRAVEN/
//
// Dedicated to Pippip, the main character in the 'Das Totenschiff' roman, actually the B.Traven himself, his real name was Hermann Albert Otto Maksymilian Feige.
// CAUTION: Add 8 more bytes to the buffer being hashed, usually malloc(...+8) - to prevent out of boundary reads!
// Many thanks go to Yurii 'Hordi' Hordiienko, he lessened with 3 instructions the original 'Pippip', thus:

void FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte (const char *str, size_t wrdlen, uint32_t seed, void *output) {
    __m128i chunkA;
    __m128i chunkA2;
    __m128i chunkB;
    __m128i chunkB2;
    __m128i stateMIX;
    uint64_t hashLH;
    uint64_t hashRH;

    __m128i InterleaveMask = _mm_set_epi8(15,7,14,6,13,5,12,4,11,3,10,2,9,1,8,0);
    stateMIX = _mm_set1_epi32( (uint32_t)wrdlen ^ seed );
    stateMIX = _mm_aesenc_si128(stateMIX, _mm_set_epi64x(0x6c62272e07bb0142, 0x9e3779b97f4a7c15));

    if (wrdlen > 8) {
        __m128i stateA = _mm_set_epi64x(0x6c62272e07bb0142, 0x9e3779b97f4a7c15);
        __m128i stateB = _mm_set_epi64x(0x6c62272e07bb0142, 0x9e3779b97f4a7c15);
        __m128i stateC = _mm_set_epi64x(0x6c62272e07bb0142, 0x9e3779b97f4a7c15);
        size_t Cycles, NDhead;
        if (wrdlen > 16) {
            Cycles = ((wrdlen - 1)>>5) + 1;
            NDhead = wrdlen - (Cycles<<4);
            if (Cycles & 1) {
                #pragma nounroll
                for(; Cycles--; str += 16) {
                    //_mm_prefetch(str+512, _MM_HINT_T0);
                    //_mm_prefetch(str+NDhead+512, _MM_HINT_T0);
                    chunkA = _mm_loadu_si128((__m128i *)(str));
                    chunkA = _mm_xor_si128(chunkA, stateMIX);
                    stateA = _mm_aesenc_si128(stateA, chunkA);

                    chunkB = _mm_loadu_si128((__m128i *)(str+NDhead));
                    chunkB = _mm_xor_si128(chunkB, stateMIX);
                    stateB = _mm_aesenc_si128(stateB, chunkB);

                    stateC = _mm_aesenc_si128(stateC, _mm_shuffle_epi8(chunkA, InterleaveMask));
                    stateC = _mm_aesenc_si128(stateC, _mm_shuffle_epi8(chunkB, InterleaveMask));
                }
                stateMIX = _mm_aesenc_si128(stateMIX, stateA);
                stateMIX = _mm_aesenc_si128(stateMIX, stateB);
                stateMIX = _mm_aesenc_si128(stateMIX, stateC);
            } else {
                Cycles = Cycles>>1;
                // 2. Expanded State (5 Parallel Lanes)
                // Distinct constants help diffusion
                __m128i stateA = _mm_set_epi64x(0x6c62272e07bb0142, 0x9e3779b97f4a7c15);
                __m128i stateB = _mm_set_epi64x(0x1591798841099511, 0x2166136261167776); // Diff const
                __m128i stateC = _mm_set_epi64x(0x3141592653589793, 0x2384626433832795); // Diff const
                __m128i stateD = _mm_set_epi64x(0x0271828182845904, 0x5235360287471352); // Diff const
                __m128i stateE = _mm_set_epi64x(0xc6a4a7935bd1e995, 0x5bd1e9955bd1e995); // Mixer
                #pragma nounroll
                for(; Cycles--; str += 32) {
                    // Load Head (0-31)
                    chunkA = _mm_loadu_si128((__m128i *)(str));
                    chunkA = _mm_xor_si128(chunkA, stateMIX);
                    chunkA2 = _mm_loadu_si128((__m128i *)(str+16));
                    chunkA2 = _mm_xor_si128(chunkA2, stateMIX);

                    // Load Tail/Offset (NDhead..NDhead+31)
                    chunkB = _mm_loadu_si128((__m128i *)(str+NDhead));
                    chunkB = _mm_xor_si128(chunkB, stateMIX);
                    chunkB2 = _mm_loadu_si128((__m128i *)(str+NDhead+16));
                    chunkB2 = _mm_xor_si128(chunkB2, stateMIX);

                    // 3. Parallel Absorption (High ILP)
                    // Use all loaded data (Fixes the bug in the original snippet)
                    stateA = _mm_aesenc_si128(stateA, chunkA);
                    stateB = _mm_aesenc_si128(stateB, chunkA2); // Absorbs 2nd half of Head
                    stateC = _mm_aesenc_si128(stateC, chunkB);
                    stateD = _mm_aesenc_si128(stateD, chunkB2); // Absorbs 2nd half of Tail

                    // 4. Cross-Stream Mixing (The Strength Boost)
                    // XOR Head[1] with Tail[2] and Head[2] with Tail[1] before shuffling.
                    // This entangles the two streams cryptographically.
                    __m128i mix1 = _mm_xor_si128(chunkA, chunkB2);
                    __m128i mix2 = _mm_xor_si128(chunkA2, chunkB);
                    
                    stateE = _mm_aesenc_si128(stateE, _mm_shuffle_epi8(mix1, InterleaveMask));
                    stateE = _mm_aesenc_si128(stateE, _mm_shuffle_epi8(mix2, InterleaveMask));
                }

                // 5. Finalize: Fold all 5 lanes into StateMIX
                stateMIX = _mm_aesenc_si128(stateMIX, stateA);
                stateMIX = _mm_aesenc_si128(stateMIX, stateB);
                stateMIX = _mm_aesenc_si128(stateMIX, stateC);
                stateMIX = _mm_aesenc_si128(stateMIX, stateD);
                stateMIX = _mm_aesenc_si128(stateMIX, stateE);
            } //if (Cycles & 1) {
        } else { // 9..16
            NDhead = wrdlen - (1<<3);
            hashLH = (*(uint64_t *)(str));
            hashRH = (*(uint64_t *)(str+NDhead));

            chunkA = _mm_set_epi64x(hashLH, hashLH);
            chunkA = _mm_xor_si128(chunkA, stateMIX);
            stateA = _mm_aesenc_si128(stateA, chunkA);

            chunkB = _mm_set_epi64x(hashRH, hashRH);
            chunkB = _mm_xor_si128(chunkB, stateMIX);
            stateB = _mm_aesenc_si128(stateB, chunkB);

            stateC = _mm_aesenc_si128(stateC, _mm_shuffle_epi8(chunkA, InterleaveMask));
            stateC = _mm_aesenc_si128(stateC, _mm_shuffle_epi8(chunkB, InterleaveMask));

            stateMIX = _mm_aesenc_si128(stateMIX, stateA);
            stateMIX = _mm_aesenc_si128(stateMIX, stateB);
            stateMIX = _mm_aesenc_si128(stateMIX, stateC);
        } //if (wrdlen > 16) {
    } else {
        hashLH = _PADr_KAZE(*(uint64_t *)(str+0), (8-wrdlen)<<3);
        hashRH = _PAD_KAZE(*(uint64_t *)(str+0), (8-wrdlen)<<3);
        chunkA = _mm_set_epi64x(hashLH, hashLH);
        chunkA = _mm_xor_si128(chunkA, stateMIX);

        chunkB = _mm_set_epi64x(hashRH, hashRH);
        chunkB = _mm_xor_si128(chunkB, stateMIX);

        stateMIX = _mm_aesenc_si128(stateMIX, chunkA);
        stateMIX = _mm_aesenc_si128(stateMIX, chunkB);
    }
    //#ifdef eXdupe
        _mm_storeu_si128((__m128i *)output, stateMIX); // For eXdupe
    //#else
    //    uint64_t result64[2];
    //    _mm_storeu_si128((__m128i *)result64, stateMIX);
    //    uint64_t hash64 = fold64(result64[0], result64[1]);
    //    *(uint32_t*)output = hash64>>32; //hash64;
    //#endif
}

// $ clang_20.1.8 -O3 -msse4.2 -maes -fopenmp FastZirka_v7++_Final.c -o FastZirka_v7++_Final.asm -DrankmapSERIAL -S
/*
FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte:
    .cfi_startproc
# %bb.0:
    movq    %rcx, %rax
    movq    %rsi, %rcx
    xorl    %edx, %esi
    movd    %esi, %xmm0
    pshufd  $0, %xmm0, %xmm0                # xmm0 = xmm0[0,0,0,0]
    aesenc  .LCPI0_0(%rip), %xmm0
    cmpq    $9, %rcx
    jb  .LBB0_10
# %bb.1:
    cmpq    $17, %rcx
    jb  .LBB0_9
# %bb.2:
    leaq    -1(%rcx), %r8
    movq    %r8, %rsi
    shrq    $5, %rsi
    leaq    1(%rsi), %rdx
    testb   $32, %r8b
    jne .LBB0_6
# %bb.3:
    shlq    $4, %rdx
    subq    %rdx, %rcx
    addq    %rdi, %rcx
    shlq    $4, %rsi
    addq    $16, %rsi
    movdqa  .LCPI0_0(%rip), %xmm1           # xmm1 = [11400714819323198485,7809847782465536322]
    xorl    %edx, %edx
    movdqa  .LCPI0_1(%rip), %xmm3           # xmm3 = [0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15]
    movdqa  %xmm1, %xmm2
    movdqa  %xmm1, %xmm4
    .p2align    4
.LBB0_4:                                # =>This Inner Loop Header: Depth=1
    movdqu  (%rdi,%rdx), %xmm5
    pxor    %xmm0, %xmm5
    aesenc  %xmm5, %xmm4
    movdqu  (%rcx,%rdx), %xmm6
    pxor    %xmm0, %xmm6
    aesenc  %xmm6, %xmm2
    pshufb  %xmm3, %xmm5
    aesenc  %xmm5, %xmm1
    pshufb  %xmm3, %xmm6
    aesenc  %xmm6, %xmm1
    addq    $16, %rdx
    cmpq    %rdx, %rsi
    jne .LBB0_4
# %bb.5:
    aesenc  %xmm4, %xmm0
    aesenc  %xmm2, %xmm0
    aesenc  %xmm1, %xmm0
    movdqu  %xmm0, (%rax)
    retq
.LBB0_10:
    movq    (%rdi), %rdx
    shll    $3, %ecx
    negb    %cl
    shlq    %cl, %rdx
    movq    %rdx, %xmm1
                                        # kill: def $cl killed $cl killed $rcx
    shrq    %cl, %rdx
    movq    %rdx, %xmm2
    pshufd  $68, %xmm2, %xmm2               # xmm2 = xmm2[0,1,0,1]
    pxor    %xmm0, %xmm2
    pshufd  $68, %xmm1, %xmm1               # xmm1 = xmm1[0,1,0,1]
    pxor    %xmm0, %xmm1
    aesenc  %xmm2, %xmm0
    aesenc  %xmm1, %xmm0
    movdqu  %xmm0, (%rax)
    retq
.LBB0_9:
    movq    (%rdi), %xmm1                   # xmm1 = mem[0],zero
    pshufd  $68, %xmm1, %xmm1               # xmm1 = xmm1[0,1,0,1]
    pxor    %xmm0, %xmm1
    movdqa  .LCPI0_0(%rip), %xmm2           # xmm2 = [11400714819323198485,7809847782465536322]
    movdqa  %xmm2, %xmm3
    aesenc  %xmm1, %xmm3
    movq    -8(%rdi,%rcx), %xmm4            # xmm4 = mem[0],zero
    pshufd  $68, %xmm4, %xmm4               # xmm4 = xmm4[0,1,0,1]
    pxor    %xmm0, %xmm4
    movdqa  %xmm2, %xmm5
    aesenc  %xmm4, %xmm5
    movdqa  .LCPI0_1(%rip), %xmm6           # xmm6 = [0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15]
    pshufb  %xmm6, %xmm1
    aesenc  %xmm1, %xmm2
    pshufb  %xmm6, %xmm4
    aesenc  %xmm4, %xmm2
    aesenc  %xmm3, %xmm0
    aesenc  %xmm5, %xmm0
    aesenc  %xmm2, %xmm0
    movdqu  %xmm0, (%rax)
    retq
.LBB0_6:
    shrq    %rdx
    shlq    $4, %rsi
    subq    %rsi, %rcx
    movdqa  .LCPI0_0(%rip), %xmm5           # xmm5 = [11400714819323198485,7809847782465536322]
    movdqa  .LCPI0_2(%rip), %xmm4           # xmm4 = [2406632364132693878,1554156972533191953]
    movdqa  .LCPI0_3(%rip), %xmm3           # xmm3 = [2559278670753769365,3549216002486605715]
    movdqa  .LCPI0_4(%rip), %xmm2           # xmm2 = [5923700269363172178,176065353196263684]
    movdqa  .LCPI0_5(%rip), %xmm1           # xmm1 = [6616326155283851669,14313749767032793493]
    movdqa  .LCPI0_1(%rip), %xmm6           # xmm6 = [0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15]
    .p2align    4
.LBB0_7:                                # =>This Inner Loop Header: Depth=1
    movdqu  (%rdi), %xmm7
    movdqu  16(%rdi), %xmm8
    movdqu  (%rdi,%rcx), %xmm9
    movdqa  %xmm9, %xmm10
    pxor    %xmm7, %xmm9
    pxor    %xmm0, %xmm7
    aesenc  %xmm7, %xmm5
    movdqa  %xmm8, %xmm7
    pxor    %xmm0, %xmm7
    aesenc  %xmm7, %xmm4
    movdqu  -16(%rdi,%rcx), %xmm7
    movdqa  %xmm7, %xmm11
    pxor    %xmm8, %xmm7
    pxor    %xmm0, %xmm11
    aesenc  %xmm11, %xmm3
    addq    $32, %rdi
    pxor    %xmm0, %xmm10
    aesenc  %xmm10, %xmm2
    pshufb  %xmm6, %xmm9
    aesenc  %xmm9, %xmm1
    pshufb  %xmm6, %xmm7
    aesenc  %xmm7, %xmm1
    decq    %rdx
    jne .LBB0_7
# %bb.8:
    aesenc  %xmm5, %xmm0
    aesenc  %xmm4, %xmm0
    aesenc  %xmm3, %xmm0
    aesenc  %xmm2, %xmm0
    aesenc  %xmm1, %xmm0
    movdqu  %xmm0, (%rax)
    retq
.Lfunc_end0:
    .size   FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte, .Lfunc_end0-FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte
    .cfi_endproc
*/

//  Cycles = ((wrdlen - 1)>>5) + 1;
//  NDhead = wrdlen - (Cycles<<4);
// And some visualization for XMM-WORD:
/*
kl= 33..64 Cycles= (kl-1)/32+1=2; MARGINAL CASES:
                                 2nd head starts at 33-2*16=1 or:
                                        0123456789012345 0123456789012345 0
                                 Head1: [XMM-WORD      ] [XMM-WORD      ]
                                 Head2:  [XMM-WORD      ] [XMM-WORD      ]

                                 2nd head starts at 64-2*16=32 or:
                                        0123456789012345 0123456789012345 0123456789012345 0123456789012345
                                 Head1: [XMM-WORD      ] [XMM-WORD      ]
                                 Head2:                                   [XMM-WORD      ] [XMM-WORD      ]

kl=65..96 Cycles= (kl-1)/32+1=3; MARGINAL CASES:
                                 2nd head starts at 65-3*16=17 or:
                                        0123456789012345 0123456789012345 0123456789012345 0123456789012345 0
                                 Head1: [XMM-WORD      ] [XMM-WORD      ] [XMM-WORD      ]
                                 Head2:                   [XMM-WORD      ] [XMM-WORD      ] [XMM-WORD      ]

                                 2nd head starts at 96-3*16=48 or:
                                        0123456789012345 0123456789012345 0123456789012345 0123456789012345 0123456789012345 0123456789012345
                                 Head1: [XMM-WORD      ] [XMM-WORD      ] [XMM-WORD      ]
                                 Head2:                                                    [XMM-WORD      ] [XMM-WORD      ] [XMM-WORD      ]
*/

// And some visualization for Q-WORD:
/*
kl= 9..16 Cycles= (kl-1)/16+1=1; MARGINAL CASES:
                                 2nd head starts at 9-1*8=1 or:
                                        012345678
                                 Head1: [Q-WORD]
                                 Head2:  [Q-WORD]

                                 2nd head starts at 16-1*8=8 or:
                                        0123456789012345
                                 Head1: [Q-WORD]
                                 Head2:         [Q-WORD]

kl=17..24 Cycles= (kl-1)/16+1=2; MARGINAL CASES:
                                 2nd head starts at 17-2*8=1 or:
                                        01234567890123456
                                 Head1: [Q-WORD][Q-WORD]
                                 Head2:  [Q-WORD][Q-WORD]

                                 2nd head starts at 24-2*8=8 or:
                                        012345678901234567890123
                                 Head1: [Q-WORD][Q-WORD]
                                 Head2:         [Q-WORD][Q-WORD]

kl=25..32 Cycles= (kl-1)/16+1=2; MARGINAL CASES:
                                 2nd head starts at 25-2*8=9 or:
                                        0123456789012345678901234
                                 Head1: [Q-WORD][Q-WORD]
                                 Head2:          [Q-WORD][Q-WORD]

                                 2nd head starts at 32-2*8=16 or:
                                        01234567890123456789012345678901
                                 Head1: [Q-WORD][Q-WORD]
                                 Head2:                 [Q-WORD][Q-WORD]

kl=33..40 Cycles= (kl-1)/16+1=3; MARGINAL CASES:
                                 2nd head starts at 33-3*8=9 or:
                                        012345678901234567890123456789012
                                 Head1: [Q-WORD][Q-WORD][Q-WORD]
                                 Head2:          [Q-WORD][Q-WORD][Q-WORD]

                                 2nd head starts at 40-3*8=16 or:
                                        0123456789012345678901234567890123456789
                                 Head1: [Q-WORD][Q-WORD][Q-WORD]
                                 Head2:                 [Q-WORD][Q-WORD][Q-WORD]

kl=41..48 Cycles= (kl-1)/16+1=3; MARGINAL CASES:
                                 2nd head starts at 41-3*8=17 or:
                                        01234567890123456789012345678901234567890
                                 Head1: [Q-WORD][Q-WORD][Q-WORD]
                                 Head2:                  [Q-WORD][Q-WORD][Q-WORD]

                                 2nd head starts at 48-3*8=24 or:
                                        012345678901234567890123456789012345678901234567
                                 Head1: [Q-WORD][Q-WORD][Q-WORD]
                                 Head2:                         [Q-WORD][Q-WORD][Q-WORD]
*/

// The more the merrier, therefore I added the 10,000 GitHub stars performer xxhash also:
// https://github.com/Cyan4973/xxHash/issues/1029
// 
// Pippip is not an extremely fast hash, it is the spirit of the author materialized disregarding anything outside the "staying true to oneself", or as one bona fide man Otto/Pippip once said:
// 
// Translate as verbatim as possible:
// In 1926, Traven wrote that the only biography of a writer should be his
// works: «Die Biographie eines schöpferischen Menschen ist ganz und gar unwichtig.
// Wenn der Mensch in seinen Werken nicht zu erkennen ist, dann ist entweder der
// Mensch nichts wert oder seine Werke sind nichts wert. Darum sollte der schöpferische
// Mensch keine andere Biographie haben als seine Werke» (Hauschild, B. Traven: Die
// unbekannten Jahre, op. cit., p. 31.)
// 
// In 1926, Traven wrote that the only biography of a writer should be his works:
// “The biography of a creative person is completely and utterly unimportant.
// If the person is not recognizable in his works, then either the person is worthless or his works are worthless.
// Therefore, the creative person should have no other biography than his works” (Hauschild, B. Traven: Die unbekannten Jahre, op. cit., p. 31.) 

// --- FNV-1a Checksum for Verification ---
uint32_t calc_fnv_off(uint64_t offset) {
    uint8_t data[8]; memcpy(data, &offset, 8);
    uint32_t h = 2166136261u;
    for (int i=0; i<8; i++) { h^=data[i]; h*=16777619u; }
    return h;
}

static const uint32_t K[4] = {
    0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
};

typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} sha1_ctx;

static void sha1_init(sha1_ctx *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

static void sha1_transform(sha1_ctx *ctx, const uint8_t *data) {
    uint32_t a, b, c, d, e;
    uint32_t w[80];
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)data[4 * i + 0] << 24 |
               (uint32_t)data[4 * i + 1] << 16 |
               (uint32_t)data[4 * i + 2] << 8  |
               (uint32_t)data[4 * i + 3];
    }

    for (i = 16; i < 80; i++) {
        w[i] = ROTL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 20; i++) {
        uint32_t temp = ROTL(a, 5) + F0(b, c, d) + e + w[i] + K[0];
        e = d;
        d = c;
        c = ROTL(b, 30);
        b = a;
        a = temp;
    }
    for (i = 20; i < 40; i++) {
        uint32_t temp = ROTL(a, 5) + F1(b, c, d) + e + w[i] + K[1];
        e = d;
        d = c;
        c = ROTL(b, 30);
        b = a;
        a = temp;
    }
    for (i = 40; i < 60; i++) {
        uint32_t temp = ROTL(a, 5) + F2(b, c, d) + e + w[i] + K[2];
        e = d;
        d = c;
        c = ROTL(b, 30);
        b = a;
        a = temp;
    }
    for (i = 60; i < 80; i++) {
        uint32_t temp = ROTL(a, 5) + F3(b, c, d) + e + w[i] + K[3];
        e = d;
        d = c;
        c = ROTL(b, 30);
        b = a;
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

static void sha1_update(sha1_ctx *ctx, const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    size_t pos = (size_t)(ctx->count & 63);
    ctx->count += len;

    while (len > 0) {
        size_t avail = 64 - pos;
        size_t copy = (len < avail) ? len : avail;
        memcpy(&ctx->buffer[pos], bytes, copy);
        pos += copy;
        bytes += copy;
        len -= copy;
        if (pos == 64) {
            sha1_transform(ctx, ctx->buffer);
            pos = 0;
        }
    }
}

static void sha1_final(sha1_ctx *ctx, uint8_t *digest) {
    uint64_t bitlen = ctx->count * 8;
    size_t pos = (size_t)(ctx->count & 63);

    ctx->buffer[pos++] = 0x80;
    if (pos > 64 - 8) {
        memset(&ctx->buffer[pos], 0, 64 - pos);
        sha1_transform(ctx, ctx->buffer);
        pos = 0;
    }
    memset(&ctx->buffer[pos], 0, 64 - 8 - pos);

    for (int i = 7; i >= 0; i--) {
        ctx->buffer[63 - i] = (uint8_t)(bitlen >> (i * 8));
    }

    sha1_transform(ctx, ctx->buffer);

    for (int i = 0; i < 5; i++) {
        digest[4 * i + 0] = (uint8_t)(ctx->state[i] >> 24);
        digest[4 * i + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[4 * i + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[4 * i + 3] = (uint8_t)(ctx->state[i]);
    }
}

void sha1_sum(const void *data, size_t len, uint8_t *out) {
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, out);
}

// --- ENTRY STRUCTURE ---
typedef struct {
    uint64_t h1; // lower 64bits
    uint64_t h2;
    uint64_t offset;
} DiskEntry;

// --- BINARY SEARCH LOGIC ---
// Finds the lowest offset for a given hash that is smaller than current_pos
int64_t find_match_binary(DiskEntry* index, uint64_t total_entries, uint64_t h1, uint64_t h2, uint64_t current_pos) {
    int64_t low = 0, high = (int64_t)total_entries - 1;
    int64_t result_offset = -1;

    while (low <= high) {
        int64_t mid = low + (high - low) / 2;
        if (index[mid].h2 < h2 || (index[mid].h2 == h2 && index[mid].h1 < h1)) {
            low = mid + 1;
        } else if (index[mid].h2 > h2 || (index[mid].h2 == h2 && index[mid].h1 > h1)) {
            high = mid - 1;
        } else {
/*
            // Hash match found! Now find the best (earliest) offset in the sorted block
            // Linear search locally because multiple offsets can have the same hash
            int64_t i = mid;
            while (i >= 0 && index[i].h1 == h1 && index[i].h2 == h2) {
                if (index[i].offset < current_pos) result_offset = index[i].offset;
                i--;
            }
            i = mid + 1;
            while (i < (int64_t)total_entries && index[i].h1 == h1 && index[i].h2 == h2) {
                if (index[i].offset < current_pos) {
                    if (result_offset == -1 || index[i].offset < (uint64_t)result_offset)
                        result_offset = index[i].offset;
                }
                i++;
            }
*/
            int64_t i = mid;
            if (index[i].offset < current_pos) result_offset = index[i].offset;
            break;
        }
    }
    return result_offset;
}

// --- SERIAL COMPARE (For small chunks) ---
int compare_disk_serial(const void* a, const void* b) {
    const DiskEntry* da = (const DiskEntry*)a;
    const DiskEntry* db = (const DiskEntry*)b;
    if (da->h2 < db->h2) return -1;
    if (da->h2 > db->h2) return 1;
    if (da->h1 < db->h1) return -1;
    if (da->h1 > db->h1) return 1;
    if (da->offset < db->offset) return -1;
    if (da->offset > db->offset) return 1;
    return 0;
}

// --- CUSTOM OPENMP QUICKSORT ---
void omp_quicksort(DiskEntry* data, int64_t left, int64_t right) {
    if (left >= right) return;

    // 1. Serial Threshold: If chunk is small, avoid OMP overhead
    if ((right - left) < SORT_THRESHOLD) {
        qsort(data + left, right - left + 1, sizeof(DiskEntry), compare_disk_serial);

    // stats [
        #pragma omp atomic
        SortedSoFar += (right - left + 1);
        #pragma omp critical
        {
            double progress = (double)SortedSoFar / entry_count *100;
            printf ("   Sort Progress = %.1f%%\r", progress);
            fflush(stdout);
        }
    // stats ]

        return;
    }

    // 2. Partitioning (Hoare partition scheme inline for speed)
    int64_t i = left;
    int64_t j = right;
    
    // Pivot Selection (Median of 3 approximation)
    DiskEntry pivot = data[left + (right - left) / 2];

    while (i <= j) {
        // Scan Left: Find element > pivot
        while (1) {
            if (data[i].h2 < pivot.h2) { i++; continue; }
            if (data[i].h2 > pivot.h2) break;
            if (data[i].h1 < pivot.h1) { i++; continue; }
            if (data[i].h1 > pivot.h1) break;
            if (data[i].offset < pivot.offset) { i++; continue; }
            break; 
        }

        // Scan Right: Find element < pivot
        while (1) {
            if (data[j].h2 > pivot.h2) { j--; continue; }
            if (data[j].h2 < pivot.h2) break;
            if (data[j].h1 > pivot.h1) { j--; continue; }
            if (data[j].h1 < pivot.h1) break;
            if (data[j].offset > pivot.offset) { j--; continue; }
            break;
        }

        if (i <= j) {
            // Swap
            DiskEntry temp = data[i];
            data[i] = data[j];
            data[j] = temp;
            i++;
            j--;
        }
    }

    // 3. Recursive Parallel Tasking
    // We create a task for one half, and the current thread handles the other.
    #pragma omp task
    {
        #pragma omp atomic
        g_total_tasks++;
        omp_quicksort(data, left, j);
    }
    // No task needed here (current thread does it)
    omp_quicksort(data, i, right);
}

void* create_mmap_file(const char* filename, size_t size) {
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) { perror("open"); exit(1); }
    if (ftruncate(fd, size) == -1) { perror("truncate"); exit(1); }
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);
    return ptr;
}

/*
Algorithm:

Create a temporary file updates.bin.

In Stage 4.2 loop: Instead of writing to rank, write the pair {duplicate_pos, master_offset} to updates.bin. (This is fast sequential write).

Sort updates.bin by duplicate_pos.

Read updates.bin and write to rank. Since updates.bin is now sorted by position, you will write to rank in perfect order (0, 1, 2...), which is 100x faster on disk.

To implement the "Nuclear Option" (sorting updates to convert random I/O into sequential I/O), we need to modify the code structure slightly.

This approach eliminates the disk thrashing by:

Gathering all duplicate pointers into a temporary linear array (fast sequential write).

Sorting that array by file position (so the target addresses become monotonic).

Applying the updates to the Rank Map in one clean sweep (fast sequential/monotonic write).

Here is the complete solution.

1. Add these Definitions (Top of File)
Place these before main(), alongside the existing DiskEntry and omp_quicksort. This defines the structure for the updates and a specific sorter for them.
*/

// --- NUCLEAR OPTION HELPERS ---
typedef struct {
    uint64_t pos;    // Where in the file the duplicate is (The Random Address)
    uint64_t target; // The Master Offset it should point to (The Value)
} RankUpdate;

// Compare by Position (to linearize the writes later)
int compare_updates(const void* a, const void* b) {
    const RankUpdate* ua = (const RankUpdate*)a;
    const RankUpdate* ub = (const RankUpdate*)b;
    if (ua->pos < ub->pos) return -1;
    if (ua->pos > ub->pos) return 1;
    return 0;
}

// Special Quicksort for the Update List
void omp_quicksort_updates(RankUpdate* data, int64_t left, int64_t right) {
    if (left >= right) return;
    
    // Serial Threshold
    if ((right - left) < SORT_THRESHOLD) {
        qsort(data + left, right - left + 1, sizeof(RankUpdate), compare_updates);

    // stats [
        #pragma omp atomic
        SortedSoFar += (right - left + 1);
        #pragma omp critical
        {
            double progress = (double)SortedSoFar / update_count *100;
            printf ("   Sort Progress = %.1f%%\r", progress);
            fflush(stdout);
        }
    // stats ]

        return;
    }

    int64_t i = left, j = right;
    RankUpdate pivot = data[left + (right - left) / 2];

    while (i <= j) {
        while (data[i].pos < pivot.pos) i++;
        while (data[j].pos > pivot.pos) j--;
        if (i <= j) {
            RankUpdate temp = data[i]; data[i] = data[j]; data[j] = temp;
            i++; j--;
        }
    }

    #pragma omp task
    omp_quicksort_updates(data, left, j);
    omp_quicksort_updates(data, i, right);
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printf("Usage: %s <file>\n", argv[0]); return 1; }

printf ("__________.__        __            \n");
printf ("\\____    /|__|______|  | _______   \n");
printf ("  /     / |  \\_  __ \\  |/ /\\__  \\  \n");
printf (" /     /_ |  ||  | \\/    <  / __ \\_\n");
printf ("/_______ \\|__||__|  |__|_ \\(____  /\n");
printf ("        \\/               \\/     \\/ \n");
printf ("Version %d++, Deduplication granularity %d\n", VERSION, CHUNK_SIZE);

// [sanmayce@djudjeto v7+]$ echo -n Sanmayce> Sanmayce 
// [sanmayce@djudjeto v7+]$ sha1sum Sanmayce 
// 8ecfb9435b758ba69d424f0aa003f6e3dc5943b3  Sanmayce

/*
char Sanmayce[] = "Sanmayce";
uint8_t hash_out20[32];
        sha1_sum(Sanmayce, 8, (uint8_t *)hash_out20);
for (int i=0; i<20; i++) printf("%02x", hash_out20[i]);
printf("\n");
exit (1);
*/

// [sanmayce@djudjeto v7+]$ ./Zirka_v7 q
// Version 7+, Deduplication granularity 384
// 8ecfb9435b758ba69d424f0aa003f6e3dc5943b3

// malloc [
/*
    FILE* fin = fopen(argv[1], "rb");
    if (!fin) { perror("Input error"); return 1; }
    fseek(fin, 0, SEEK_END);
    uint64_t filesize = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    uint64_t entry_count = (filesize >= CHUNK_SIZE) ? (filesize - CHUNK_SIZE + 1) : 0;

    char* filename = argv[1];
    
    printf("[Zirka 1-Pass] File: %s (%.2f GB)\n", filename, filesize / 1024.0 / 1024.0 / 1024.0);
    printf("[Zirka 1-Pass] Hashing to temporary MMAP file (this will use 24x filesize = ~%lu GB disk space)...\n", 
           (entry_count * sizeof(DiskEntry)) / 1024 / 1024 / 1024);
*/
// malloc ]
   
// 1. OPEN FILE & GET SIZE [
    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in == -1) { perror("Open input failed"); return 1; }
    char* filename = argv[1];

    struct stat sb;
    if (fstat(fd_in, &sb) == -1) { perror("Stat failed"); return 1; }
    uint64_t filesize = sb.st_size;
    entry_count = (filesize >= CHUNK_SIZE) ? (filesize - CHUNK_SIZE + 1) : 0;

    printf("[Zirka 1-Pass] File: %s (%.2f GB)\n", argv[1], filesize / 1024.0 / 1024.0 / 1024.0);
    printf("[Zirka 1-Pass] Zero-RAM Mode: Input is memory-mapped (OS manages paging).\n");

    // 2. MMAP THE INPUT (Zero-RAM Magic)
    // PROT_READ: We only read. MAP_PRIVATE: Changes (if any) stay local.
    uint8_t* buffer = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd_in, 0);
    if (buffer == MAP_FAILED) { perror("mmap input failed"); return 1; }
    
    // Hint to OS: We will read this sequentially (speeds up Hashing phase)
    #ifdef __linux__
    madvise(buffer, filesize, MADV_SEQUENTIAL);
    #endif

    // We can close the file descriptor now; the map stays valid.
    close(fd_in);
// 1. OPEN FILE & GET SIZE ]

    // 1. CREATE DISK INDEX
    printf("1. Creating Index (24x Filesize using MMAP, %lu entries)...\n", entry_count);
    DiskEntry* index = create_mmap_file("zirka_index.tmp", entry_count * sizeof(DiskEntry));
        #ifdef eXdupe
    printf("   Hashing (Parallel Pippip, taking 128bits=16bytes)...\n");
        #else
    printf("   Hashing (Parallel SHA1, taking 128bits=16bytes)...\n");
        #endif
    double t_start = omp_get_wtime();
    // OMP Parallel Hashing
    #pragma omp parallel for
    for(uint64_t i=0; i<entry_count; i++) {
        uint64_t hash_out[3]; // 2 for 16 bytes, 3 for 24
        //uint8_t digest[SHA1_DIGEST_SIZE];

        //FNV1A_Pippip_128((char*)buffer + i, CHUNK_SIZE, 0, hash_out);
        #ifdef eXdupe
        //void FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte (const char *str, size_t wrdlen, uint32_t seed, void *output) {
        FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte ((const char *) ((char*)buffer + i), CHUNK_SIZE, 0, hash_out);
        #else
        sha1_sum(((char*)buffer + i), CHUNK_SIZE, (uint8_t *)hash_out);
        #endif

        index[i].h1 = hash_out[0];
        index[i].h2 = hash_out[1];
        index[i].offset = i;
    }
    printf("   Hashed in %.2fs\n", omp_get_wtime() - t_start);

    // 2. PARALLEL DISK SORT
    printf("2. Sorting Disk Index (Parallel Quicksort)...\n");
    t_start = omp_get_wtime();
    SortedSoFar = 0;
    // OMP Parallel Region for Recursion
    #pragma omp parallel
    {
        #pragma omp single nowait
        {
        //g_max_threads_used = omp_get_num_threads();
        omp_quicksort(index, 0, entry_count - 1);
        }
    }
            printf ("   Sort Progress = %.1f%%\n", 100.0);
    printf("   Sorted in %.2fs\n", omp_get_wtime() - t_start);
    //printf("   Max threads executed simultaneously: %d\n", g_max_threads_used);
    printf("   Total parallel tasks generated: %d\n", g_total_tasks);

#ifdef rankmapFIRST
    // --- STAGE 4: BUILDING THE 64-BIT RANK MAP (Dictionary Linker) ---
    printf("3. Building Rank Map (8x Filesize using MMAP)...\n");
    uint64_t* rank = create_mmap_file("zirka_rank.tmp", entry_count * sizeof(uint64_t));
/*
    printf("\nStage 3: Building 64-bit Rank Map (Linking Duplicates to First Occurrence)...\n");
    // Allocate the Rank Map (uint64_t)
    uint64_t* rank = mmap(NULL, filesize * sizeof(uint64_t), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
*/  

    // OPTIMIZATION: Use Huge Pages if available to reduce TLB misses during random writes
    //#ifdef __linux__
    madvise(rank, filesize * sizeof(uint64_t), MADV_HUGEPAGE);
    //#endif
  
// SLOW [

    uint64_t prcnt = 0;
    // 4.1 Initialize Rank Map to NULL
    #pragma omp parallel for
    for(uint64_t i = 0; i < filesize; i++) rank[i] = NULL_RANK;

    // 4.2 Fill Rank Map based on "First Occurrence"
    for (uint64_t i = 0; i < entry_count; i++) {
        uint64_t group_start = i;

        prcnt++;
        if (prcnt == 17*1024) {printf("\rDone: %.1f%%", (double)i/entry_count*100.0); prcnt = 0;}
        
        // Find all records with identical hashes
        while (i + 1 < entry_count && 
               index[i+1].h2 == index[group_start].h2 && 
               index[i+1].h1 == index[group_start].h1) {
            i++;
        }

        uint64_t group_size = (i - group_start) + 1;

        // Logic: If there is more than one, we have duplicates
        if (group_size > 1) {
            // Because our sort tie-breaker was 'offset', index[group_start] is the FIRST appearance
            uint64_t master_offset = index[group_start].offset;

            // Map every subsequent appearance back to the master_offset
            for (uint64_t j = group_start + 1; j <= i; j++) {
                uint64_t duplicate_pos = index[j].offset;
                rank[duplicate_pos] = master_offset;
            }
        }
    }
    printf("\nRank Map linking complete.\n");

// SLOW ]

/*
// --- 4.2 Optimised Parallel Rank Filling --- [
    printf("\n   [Boost] Linking duplicates (Parallel)...\n");

    // OPTIMIZATION: Use Huge Pages if available to reduce TLB misses during random writes
    #ifdef __linux__
    madvise(rank, filesize * sizeof(uint64_t), MADV_HUGEPAGE);
    #endif

    // We use a counter to track progress across threads
    uint64_t processed_count = 0;

    #pragma omp parallel for schedule(dynamic, 2048)
    for (uint64_t i = 0; i < entry_count; i++) {
        
        // 1. EFFICIENT SKIP: 
        // We only want to process the "Group Start". 
        // If 'i' is NOT the first element of a group, skip it immediately.
        // (Comparison with i-1 is safe in parallel read-only)
        if (i > 0) {
            if (index[i].h2 == index[i-1].h2 && index[i].h1 == index[i-1].h1) {
                continue; 
            }
        }

        // 2. SCAN FORWARD:
        // Since we are the "Group Leader", we scan strictly forward to find our duplicates.
        uint64_t group_start = i;
        uint64_t master_offset = index[group_start].offset; // First Occurrence

        // Look ahead
        uint64_t look = group_start + 1;
        
        // Note: This inner while loop is fast because sorted identical hashes are adjacent.
        while (look < entry_count && 
               index[look].h2 == index[group_start].h2 && 
               index[look].h1 == index[group_start].h1) {
            
            // 3. WRITE TO RANK MAP
            // 'duplicate_pos' is random, so this is the bottleneck. 
            // Parallelism helps saturate the I/O queue here.
            uint64_t duplicate_pos = index[look].offset;
            rank[duplicate_pos] = master_offset;
            
            look++;
        }

        // 4. PROGRESS BAR (Atomic update to avoid race condition)
        if (i % 4096 == 0) {
            #pragma omp atomic
            processed_count += 4096;
            
            // Only one thread prints
            if (omp_get_thread_num() == 0) {
                printf("\r   Linked: %.1f%%", (double)processed_count / entry_count * 100.0);
                fflush(stdout);
            }
        }
    }
    printf("\n   [Boost] Parallel Linking Complete.\n");
// --- 4.2 Optimised Parallel Rank Filling --- ]
*/

    // Clear index to free up memory before the final pass
    //munmap(index, entry_count * sizeof(DiskEntry));

    // --- STAGE 5: ENCODING (O(1) Pass) ---
    printf("Stage 4: Encoding with O(1) Rank Lookups...\n");
    char out_name[512]; snprintf(out_name, 512, "%s.zirka", argv[1]);
    FILE* fout = fopen(out_name, "wb");
    uint64_t pos = 0;
    
    while(pos < filesize) {
        // Instant check: Does this position have a link to a first occurrence?
        uint64_t match_off = rank[pos];

        if (match_off != NULL_RANK) {
            // master_offset is always < pos because we linked j = group_start + 1 onwards
            if (memcmp(buffer + pos, buffer + match_off, CHUNK_SIZE) == 0) {
                fputc(MAGIC_BYTE, fout);
                fwrite(&match_off, 8, 1, fout); // Point back to master
                uint32_t chk = calc_fnv_off(match_off);
                fwrite(&chk, 4, 1, fout);
                pos += CHUNK_SIZE;
                continue;
            }
        }
        
        // Literal byte
        fputc(buffer[pos], fout);
        pos++;
    }

    printf("\nDone.\n");
    fclose(fout);
    free(buffer);
    
    munmap(index, entry_count * sizeof(DiskEntry));
    munmap(rank, entry_count * sizeof(uint64_t));
    //unlink("zirka_index.tmp");
    //unlink("zirka_rank.tmp");
    return 0;
#endif

#ifdef rankmap
    // 3. BUILD RANK MAP (Parallel)
    printf("3. Building Rank Map (8x Filesize using MMAP)...\n");
    uint64_t* rank = create_mmap_file("zirka_rank.tmp", entry_count * sizeof(uint64_t));
    
    #pragma omp parallel for
    for(uint64_t i=0; i<entry_count; i++) {
        rank[index[i].offset] = (uint64_t)i;
    }

    // 4. ENCODE (Serial Dependency on Window Slide)
    printf("4. Encoding...\n");
    char out_name[512]; snprintf(out_name, 512, "%s.zirka", argv[1]);
    FILE* fout = fopen(out_name, "wb");

    uint64_t pos = 0;
    uint64_t prcnt = 0;
    while(pos < filesize) {
        if (pos > entry_count - 1) {
            fwrite(buffer + pos, 1, filesize - pos, fout);
            break;
        }

        uint64_t my_rank = rank[pos];
        bool found = false;
        uint64_t match_offset = 0;

        if (my_rank > 0) {
            DiskEntry candidate = index[my_rank - 1];
            DiskEntry me = index[my_rank];

            // Hash Check
            if (candidate.h1 == me.h1 && candidate.h2 == me.h2) {
                // Content Verify
                if (memcmp(buffer + pos, buffer + candidate.offset, CHUNK_SIZE) == 0) {
                     if (candidate.offset < pos) {
                        found = true;
                        match_offset = candidate.offset;
                     }
                }
            }
        }

        prcnt++;
        if (found) {
            fputc(MAGIC_BYTE, fout);
            fwrite(&match_offset, 8, 1, fout);
            uint32_t h = calc_fnv_off(match_offset); 
            fwrite(&h, 4, 1, fout);
            pos += CHUNK_SIZE;
        } else {
            fputc(buffer[pos], fout);
            pos++;
        }
        
        if (prcnt == 7*1024) {printf("\r   Encoded: %.1f%%", (double)pos/filesize*100.0); prcnt = 0;}
    }

    printf("\nDone.\n");
    fclose(fout);
    free(buffer);
    
    munmap(index, entry_count * sizeof(DiskEntry));
    munmap(rank, entry_count * sizeof(uint64_t));
    //unlink("zirka_index.tmp");
    //unlink("zirka_rank.tmp");
    
    return 0;
#endif

#ifdef rankmapSERIAL
    // --- STAGE 3: BUILD RANK MAP (NUCLEAR OPTION) ---
    printf("3. Building Rank Map (8x Filesize using MMAP, Nuclear Mode: Sequential I/O)...\n");
    
    // 1. Create the Rank Map (initially empty)
    uint64_t* rank = create_mmap_file("zirka_rank.tmp", filesize * sizeof(uint64_t));
    
    // OPTIMIZATION: Huge Pages for random access speed later
    #ifdef __linux__
    madvise(rank, filesize * sizeof(uint64_t), MADV_HUGEPAGE);
    #endif

    // Initialize to NULL
    #pragma omp parallel for
    for(uint64_t i = 0; i < filesize; i++) rank[i] = NULL_RANK;

    // --- NUCLEAR PHASE 1: GATHER UPDATES (Sequential Write) ---
    printf("   [Nuclear] Gathering duplicates (16x Filesize using MMAP)...\n");
    
    // Create a temporary buffer for updates. 
    // Max possible updates = entry_count (worst case).
    RankUpdate* updates = create_mmap_file("zirka_updates.tmp", entry_count * sizeof(RankUpdate));
    update_count = 0;

    #pragma omp parallel for schedule(dynamic, 4096)
    for (uint64_t i = 0; i < entry_count; i++) {
        // Skip if not the "Group Leader" (First of identical hashes)
        if (i > 0) {
            if (index[i].h2 == index[i-1].h2 && index[i].h1 == index[i-1].h1) continue;
        }

        uint64_t group_start = i;
        // Since we sorted by Hash+Offset, this is the absolute first occurrence
        uint64_t master_offset = index[group_start].offset; 

        // Scan the group to find duplicates
        uint64_t look = group_start + 1;
        uint64_t local_dupes = 0;
        
        // Count first to reserve space atomically
        uint64_t temp_look = look;
        while (temp_look < entry_count && 
               index[temp_look].h2 == index[group_start].h2 && 
               index[temp_look].h1 == index[group_start].h1) {
            local_dupes++;
            temp_look++;
        }

        if (local_dupes > 0) {
            uint64_t my_write_idx;
            #pragma omp atomic capture
            { my_write_idx = update_count; update_count += local_dupes; }

            // Write the updates: "At position [duplicate], point to [master]"
            while (look < entry_count && 
                   index[look].h2 == index[group_start].h2 && 
                   index[look].h1 == index[group_start].h1) {
                
                updates[my_write_idx].pos = index[look].offset; 
                updates[my_write_idx].target = master_offset;   
                
                my_write_idx++;
                look++;
            }
        }
    }
    printf("   [Nuclear] Found %lu duplicates to link.\n", update_count);

    // --- NUCLEAR PHASE 2: SORT UPDATES (Transforms Random I/O to Sequential) ---
    if (update_count > 0) {
        printf("   [Nuclear] Sorting updates by file position...\n");
        SortedSoFar = 0;
        #pragma omp parallel
        {
            #pragma omp single nowait
            omp_quicksort_updates(updates, 0, update_count - 1);
        }
            printf ("   Sort Progress = %.1f%%\n", 100.0);

        // --- NUCLEAR PHASE 3: APPLY UPDATES (Monotonic Write) ---
        printf("   [Nuclear] Applying updates to Rank Map...\n");
        #pragma omp parallel for schedule(static)
        for (uint64_t i = 0; i < update_count; i++) {
            rank[updates[i].pos] = updates[i].target;
        }
    }

    // Clean up temporary updates file
    munmap(updates, entry_count * sizeof(RankUpdate));
    unlink("zirka_updates.tmp"); // Uncomment to delete temp file

    // Free the Index (We don't need it anymore for Encoding!)
    munmap(index, entry_count * sizeof(DiskEntry));
    unlink("zirka_index.tmp");

    // --- STAGE 4: ENCODER (CORRECT "FIRST OCCURRENCE" LOGIC) ---
    printf("4. Encoding (Direct Rank Lookup)...\n");
    char out_name[512]; snprintf(out_name, 512, "%s.zirka", argv[1]);
    FILE* fout = fopen(out_name, "wb");
    uint64_t pos = 0;
    uint64_t prcnt = 0;
    uint32_t chk[4];
    
    while(pos < filesize) {
        // Direct Lookup: rank[pos] contains the OFFSET of the duplicate
        uint64_t match_off = rank[pos];

        if (match_off != NULL_RANK) {
            // Safety: It must be a backward reference
            if (match_off + CHUNK_SIZE <= pos) {
                // Verify content (Paranoia check)
                if (memcmp(buffer + pos, buffer + match_off, CHUNK_SIZE) == 0) {
                    fputc(MAGIC_BYTE, fout);
                    fwrite(&match_off, 8, 1, fout);
                    //uint32_t chk = calc_fnv_off(match_off);
                    FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte((const char *)&match_off, 8, 0, chk);

                    fwrite(&chk, 4, 1, fout);
                    pos += CHUNK_SIZE;
                    
                    // Progress Update
                    prcnt += CHUNK_SIZE;
                    if (prcnt >= 1*1024*1024) { 
                        printf("\r   Encoded: %.1f%%", (double)pos/filesize*100.0); 
                        prcnt = 0; 
                    }
                    continue;
                }
            }
        }
        
        // Literal
        fputc(buffer[pos], fout);
        pos++;
        
        // Progress Update (for literals)
        prcnt++;
        if (prcnt >= 1*1024*1024) { 
            printf("\r   Encoded: %.1f%%", (double)pos/filesize*100.0); 
            prcnt = 0; 
        }
    }

    printf("\r   Encoded: %.1f%%\n", 100.0); 
    printf("Done.\n");
    fclose(fout);
    //free(buffer);
    munmap(buffer, filesize);

    munmap(rank, filesize * sizeof(uint64_t));
    unlink("zirka_rank.tmp");
    return 0;
#endif

#ifdef BS
    // 3. ENCODE (Using Binary Search instead of Rank Map)
    printf("3. Encoding (Binary Search Mode)...\n");
    char out_name[512]; snprintf(out_name, 512, "%s.zirka", argv[1]);
    FILE* fout = fopen(out_name, "wb");


// Make all duplicates to point to the first of them [
    uint64_t prcnt = 0;
    // 4.1 Initialize Rank Map to NULL

    // 4.2 Fill Rank Map based on "First Occurrence"
    for (uint64_t i = 0; i < entry_count; i++) {
        uint64_t group_start = i;

        prcnt++;
        if (prcnt == 17*1024) {printf("\rDone: %.1f%%", (double)i/entry_count*100.0); prcnt = 0;}
        
        // Find all records with identical hashes
        while (i + 1 < entry_count && 
               index[i+1].h2 == index[group_start].h2 && 
               index[i+1].h1 == index[group_start].h1) {
            i++;
        }

        uint64_t group_size = (i - group_start) + 1;

        // Logic: If there is more than one, we have duplicates
        if (group_size > 1) {
            // Because our sort tie-breaker was 'offset', index[group_start] is the FIRST appearance
            uint64_t master_offset = index[group_start].offset;

            // Map every subsequent appearance back to the master_offset
            for (uint64_t j = group_start + 1; j <= i; j++) {
                index[j].offset = master_offset;
            }
        }
    }
printf("\n");
// Make all duplicates to point to the first of them ]


    uint64_t pos = 0;
    prcnt = 0;
    while(pos < filesize) {
        bool found = false;
        uint64_t match_offset = 0;

        if (pos < entry_count) {
            uint64_t h[2];
            FNV1A_Pippip_128((char*)buffer + pos, CHUNK_SIZE, 0, h);
            
            int64_t best_off = find_match_binary(index, entry_count, h[0], h[1], pos);
            
            if (best_off != -1) {
                // Verify content
                if (memcmp(buffer + pos, buffer + best_off, CHUNK_SIZE) == 0) {
                    found = true;
                    match_offset = (uint64_t)best_off;
                }
            }
        }

        prcnt++;
        if (found) {
            fputc(MAGIC_BYTE, fout);
            fwrite(&match_offset, 8, 1, fout);
            uint32_t h_chk = calc_fnv_off(match_offset); 
            fwrite(&h_chk, 4, 1, fout);
            pos += CHUNK_SIZE;
        } else {
            fputc(buffer[pos], fout);
            pos++;
        }
        
        if (prcnt == 7*1024) {printf("\r   Encoded: %.1f%%", (double)pos/filesize*100.0); prcnt = 0;}
    }

    printf("\nDone.\n");
    fclose(fout);
    free(buffer);
    munmap(index, entry_count * sizeof(DiskEntry));
    //unlink("zirka_index.tmp");
    return 0;
#endif
}

/*
This breakdown highlights the architectural achievements of --Zirka v7--, emphasizing the original approaches that allow a consumer laptop to process terabyte-scale datasets without crashing.

### --High-Performance Highlights & Original Approaches--

- --Zero-RAM Architecture (The "Infinite" Memory Model)--
- --Concept:-- Zirka v7 never calls `malloc()` for data buffers. Instead, it relies entirely on --Memory Mapped Files (`mmap`)--.
- --Benefit:-- The program's memory limit is decoupled from physical RAM. You can process a --10TB file on a laptop with 8GB RAM--, provided you have disk space.
- --Mechanism:-- The OS treats the SSD/HDD as "slow RAM," paging in only the tiny 4KB chunks needed at any specific millisecond.

- --The "Nuclear Option" (Sequential I/O Transformation)--
- --Problem Solved:-- Building a deduplication map usually requires billions of --Random Writes-- (jumping around the file), which destroys performance on mechanical drives and saturates SSD controllers.
- --Original Approach:-- Zirka v7 defers these writes.
1. --Gather:-- It writes intended updates to a temporary log sequentially.
2. --Sort:-- It sorts this log by the -target address-.
3. --Apply:-- It applies the updates in a single, monotonic sweep (0% to 100%).

- --Result:-- A process that used to take days (due to disk thrashing) now finishes in minutes.

- --Parallel Pip-Pip Hashing (Hardware Acceleration)--
- --Method:-- Utilizes the --`FNV1A_Pippip_128`-- algorithm, custom-written to exploit --SSE4.2-- and --AES-NI-- CPU instructions.
- --Parallelism:-- The file is sliced into segments, allowing all CPU cores to compute 128-bit fingerprints simultaneously without locking.

- --"First Occurrence" Deduplication Strategy--
- --Method:-- Instead of linking duplicates to their nearest neighbor (a chain like `C -> B -> A`), Zirka v7 forces every duplicate to point directly to the --Master Copy-- (`C -> A`, `B -> A`).
- --Benefit:-- This simplifies the decoder and allows for the "Nuclear" sorting optimization, as the target value is constant for the entire hash group.

- --64-Bit Rank Map (O(1) Lookup Speed)--
- --Performance:-- Replaces the traditional  Binary Search with a direct array lookup ().
- --Impact:-- Checks for duplicates instantly. The encoder speed is limited only by how fast the CPU can read the file, not by searching through index trees.

---

### --Deep Dive: The Rank Map Stage--

The --Rank Map-- is the brain of Zirka v7. It is a massive array of 64-bit integers, exactly the same length as the input file.

#### --1. The Goal--

If your file has a duplicate chunk at byte `1,000,000` that is identical to byte `500`, the Rank Map at index `1,000,000` simply stores the number `500`.

- --Encoder Logic:-- `if (rank[current_pos] != NULL) { Replace with Pointer to rank[current_pos]; }`
- This single instruction replaces complex hash table lookups or binary tree traversals.

#### --2. The Challenge (The "Random Write" Wall)--

To build this map, we have a sorted list of hashes. We know that `Position A` and `Position Z` are identical.

- --Naive Approach:-- We iterate through the sorted hashes. We see `Z` is a duplicate of `A`. We jump to index `Z` in the Rank Map and write `A`.
- --The Crash:-- Since hashes are sorted by -value-, their positions (`Z`) are random. Writing to `rank[10]`, then `rank[9,000,000]`, then `rank[50]` causes the hard drive head to seek wildly (thrashing), slowing speeds to ~500KB/s.

#### --3. The Zirka v7 Solution (The Nuclear Workflow)--

The Rank Map is built in three distinct, optimized phases that strictly enforce --Sequential I/O--:

- --Phase A: The Parallel Gather--
- Zirka reads the sorted Hash Index (which is sequential in memory).
- It identifies groups: "Hashes at `Pos 100`, `Pos 5000`, `Pos 800` are identical."
- It selects the Master: `Pos 100` (Smallest offset).
- It writes --Updates-- to a temporary buffer:
- `{Target: 5000, Value: 100}`
- `{Target: 800, Value: 100}`

- -Key Highlight:- This writing is purely sequential (appending), so the disk writes at maximum speed (500MB/s+).

- --Phase B: The Parallel Sort--
- Zirka takes the temporary buffer of updates and sorts them by --Target Position--.
- Result: `{Target: 800, Value: 100}`, `{Target: 5000, Value: 100}`.
- -Key Highlight:- Sorting happens in memory chunks using OpenMP Parallel Quicksort, utilizing all CPU cores.

- --Phase C: The Monotonic Apply--
- Zirka opens the massive Rank Map file.
- Because the updates are now sorted (`800` comes before `5000`), it writes to the file linearly.
- -Key Highlight:- The disk head sweeps smoothly from the beginning to the end of the file once. No seeking. No thrashing.

This architecture transforms a task that is traditionally --I/O Bound-- (waiting for the disk) into one that is --CPU Bound-- (waiting for the sort), allowing Zirka v7 to utilize the full power of modern multi-core processors.
*/

/*
The Math: Calculating the "X"
In Zirka v7, we are dealing with three temporary workspace files.
Their sizes are determined by the size of the structures they store.
zirka_index.tmp ($24X$):
    Stores DiskEntry: h1 (8) + h2 (8) + offset (8) = 24 bytes per entry.
    One entry per byte of the input file.Cost: $24X$
zirka_rank.tmp ($8X$):Stores a uint64_t (8 bytes) for every byte of the input file.Cost: $8X$
zirka_updates.tmp ($16X$):
    Stores RankUpdate: pos (8) + target (8) = 16 bytes per entry.
    Since we allocate space for the "worst case" (where every byte is a duplicate), we pre-allocate entry_count * 16.
    Cost: $16X$
The Total "X"
When Stage 3 (Rank Building) is running, all three files exist at once:
    $24X$ (Index) + $8X$ (Rank) + $16X$ (Updates) = $48X$ overhead.
    Including the input file ($1X$), the total disk pressure is $49X$.
Example:If you process a 1GB file, you need 49GB of free disk space at the peak of Stage 3.
*/

// Testdatafile: 'SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar' (5,694,801,920 bytes)
// +-------------------------+-----------------+---------------+--------------------------+---------------------------+
// | Compressor/Deduplicator | CPU utilization | Encoding Time |         Memory Footprint | Deduplication Granularity |
// +-------------------------+-----------------+---------------+--------------------------+---------------------------+
// | eXdupe 3.0.2 -x0        |             96% |          0:02 |             2,405,360 KB |                      4 KB |
// | Zirka v7_rankmapSERIAL  |            852% |         38:52 | None, but 123,946,696 KB |                    384 B  |
// | Zirka v7_rankmapSERIAL  |            849% |         32:56 | None, but 123,937,092 KB |                      4 KB |
// +-------------------------+-----------------+---------------+--------------------------+---------------------------+
// Note: The testmachine is DELL Precision 7560 i7-11850H (8 cores, 16 threads), 128GB, SSD: Corsair MP600 2TB (Phison E16 Controller. Toshiba 96-layer 3D TLC)

// Benchmarking 'SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar' (5,694,801,920 bytes), BLAKE3: f2cf49584c4db1df8c22576dbd23b2ac69ada63248aecf862ca5f42e896d50e1
// +--------------------------+-----------------+-----------------+---------------------+---------------------+----------------------+-------------+---------------+
// | Deduplicator/Compressor  | Compressed Size | Compressed Size | I Memory Footprint, | E Memory Footprint, | Memory Footprint,    | Compression | Decompression |
// | 4 KB granularity         |                 | after "gzip -9" | during Compression  | during Compression  | during Decompression | Time, m:s   | Time, m:s     |
// +--------------------------+------[ sorted ]-+-----------------+---------------------+---------------------+----------------------+-------------+---------------+
// | gzip_1.13 -k -9          |   1,438,042,263 |            N.A. |            2,164 KB |                NONE |             1,932 KB |       19:35 |        0:21 s |
// | Zirka v.7                |   3,032,947,232 |     774,051,763 |                NONE |        48x = 255 GB |               840 KB |       38:52 |        0:17 s |
// | exdupe_3.0.2_linux_amd64 |   3,517,329,611 |     911,399,664 |        2,405,360 KB |                NONE |           310,088 KB |        0:02 |        0:02 s |
// +--------------------------+-----------------+-----------------+---------------------+---------------------+----------------------+-------------+---------------+
// Note1: The testmachine is my fastest laptop Dell, i7-11850H, 128 GB DDR4, Linux Fedora 42
// Note2: The SSD is Corsair MP600 2TB 
// Note3: 'I' stands for Internal, 'E' stands for External

// -rwxr-xr-x 1 sanmayce sanmayce  5,694,801,920  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar
// -rwxr-xr-x 1 sanmayce sanmayce  1,438,042,263  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.gz          ! -9 !
// -rw-r--r-- 1 sanmayce sanmayce    526,521,023  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.rar         ! -md6g -mcx !

// -rw-r--r-- 1 sanmayce sanmayce  3,517,329,611  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.eXdupe0     ! 4KB !
// -rw-r--r-- 1 sanmayce sanmayce    911,399,664  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.eXdupe0.gz  ! -9 !
// -rw-r--r-- 1 sanmayce sanmayce    556,486,845  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.eXdupe0.rar ! -md6g -mcx !

// -rw-r--r-- 1 sanmayce sanmayce  3,032,947,232  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.zirka       ! 4KB !
// -rw-r--r-- 1 sanmayce sanmayce    774,051,763  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.zirka.gz    ! -9 !
// -rw-r--r-- 1 sanmayce sanmayce    518,760,581  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.zirka.rar   ! -md6g -mcx !

// -rw-r--r-- 1 sanmayce sanmayce  3,011,751,067  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.zirka       ! 384 !
// -rw-r--r-- 1 sanmayce sanmayce    815,037,185  SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.zirka.gz    ! -9 !

// [sanmayce@djudjeto Zirka_v7_sourcecode_binary]$ /bin/time -v ./eXdupe-3.0.2_linux_amd64 -k -x0 SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar.eXdupe0
//  Percent of CPU this job got: 289%
//  Elapsed (wall clock) time (h:mm:ss or m:ss): 0:02.38
//  Maximum resident set size (kbytes): 2405360

// [sanmayce@djudjeto Zirka_v7_sourcecode_binary]$ /bin/time -v ./Zirka_v7 SUPRAPIG_github.com_Sefaria-Project_2024-Jun-01_16833-TXTs.tar
//  Percent of CPU this job got: 852%
//  Elapsed (wall clock) time (h:mm:ss or m:ss): 38:52.92
//  Maximum resident set size (kbytes): 124229544

/*
Yes, replacing the single-threaded qsort with a Parallel Quicksort is the single best way to accelerate Stage 2. Since mmap treats the disk file as an array in memory, we can divide the sorting work across all CPU cores.

We will use a Parallel In-Place Quicksort logic:

Partitioning: One thread picks a pivot and organizes the array into "Lower" and "Higher" halves.

Tasking: It then spawns a new OpenMP task to sort the "Lower" half while it sorts the "Higher" half itself.

Threshold: When a chunk becomes small (e.g., < 64KB), it switches to the standard serial sort to avoid the overhead of managing threads.

Key Changes for v7:
omp_quicksort: This is a custom recursive function. It partitions the array and uses #pragma omp task to hand off one half of the work to another available thread. This allows sorting to scale with the number of CPU cores.

Sort Threshold: Recursion stops creating tasks when the sub-array is smaller than SORT_THRESHOLD (set to 4096). This prevents the "overhead storm" of creating millions of tiny threads for small arrays.

Parallel Hashing: The Pippip hashing loop is now decorated with #pragma omp parallel for, utilizing all cores to generate signatures rapidly.

Parallel Rank: Building the rank map is also fully parallelized.
*/

/*
[sanmayce@djudjeto Zirka_v7_sourcecode_binary]$ /bin/time -v ./Zirka_v7 SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar 
__________.__        __            
\____    /|__|______|  | _______   
  /     / |  \_  __ \  |/ /\__  \  
 /     /_ |  ||  | \/    <  / __ \_
/_______ \|__||__|  |__|_ \(____  /
        \/               \/     \/ 
Version 7, Deduplication granularity 384
[Zirka 1-Pass] File: SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar (25.55 GB)
[Zirka 1-Pass] Zero-RAM Mode: Input is memory-mapped (OS manages paging).
1. Creating Index (24x Filesize using MMAP, 27436717185 entries)...
   Hashing (Parallel Pippip)...
   Hashed in 703.84s
2. Sorting Disk Index (Parallel Quicksort)...
   Sorted in 12635.66s
3. Building Rank Map (8x Filesize using MMAP, Nuclear Mode: Sequential I/O)...
   [Nuclear] Gathering duplicates (16x Filesize using MMAP)...
   [Nuclear] Found 18054515447 duplicates to link.
   [Nuclear] Sorting updates by file position...
   [Nuclear] Applying updates to Rank Map...
4. Encoding (Direct Rank Lookup)...
   Encoded: 100.0%
Done.

    Percent of CPU this job got: 909%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 5:30:47
    Maximum resident set size (kbytes): 122,084,012
    File system inputs: 14,136,404,256
    File system outputs: 19,937,472,096

$ /bin/time -v ./unzirka_v7 SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar
    Percent of CPU this job got: 80%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 1:34.27
    Maximum resident set size (kbytes): 896
    File system inputs: 48,306,224
    File system outputs: 53,587,344

$ b3sum SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar
fe1bf9b7f83bcc8b8edead02e4eef0e8f8d8ee4f1e9a8c0f14591c8c50528b40  SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar
$ b3sum SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.restored 
fe1bf9b7f83bcc8b8edead02e4eef0e8f8d8ee4f1e9a8c0f14591c8c50528b40  SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.restored

$ /bin/time -v ./eXdupe-3.0.2_linux_amd64 -k -x0 SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0
    Percent of CPU this job got: 384%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 0:07.86
    Maximum resident set size (kbytes): 2,458,800

-rw-r--r-- 1 sanmayce sanmayce 27436717568  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar'

-rw-r--r-- 1 sanmayce sanmayce 15717218725  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0'    ! -x0 !
-rw-r--r-- 1 sanmayce sanmayce  3834037524  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0.gz' ! gzip -9 !
-rw-r--r-- 1 sanmayce sanmayce  1996591818  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0.rc' ! bwtsatan_static -20e9 !

-rw-r--r-- 1 sanmayce sanmayce  8789920329  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka'
-rw-r--r-- 1 sanmayce sanmayce  2379701060  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka.gz'   ! gzip -9 !
-rw-r--r-- 1 sanmayce sanmayce  1526645078  'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka.rc'   ! bwtsatan_static -20e9 !
*/


/*
### 1. The Struct Layout Must Match the Priority

`memcmp` reads sequentially. If your sorting priority is `h2`, then `h1`, then `offset`, they must exist in memory in that exact physical order.

Your current struct is `[h1][h2][offset]`. You need to flip it:

```c
// The memcmp-optimized struct
typedef struct {
    uint64_t h2;     // Highest priority
    uint64_t h1;     // Middle priority
    uint64_t offset; // Lowest priority (tie-breaker)
} DiskEntry;

```

### 2. The Big-Endian Byte-Swap Trick

As we discussed, `memcmp` fails on Little-Endian systems because the least significant byte comes first. However, if you store the integers in Big-Endian format, the most significant byte comes first.

In Big-Endian, a memory comparison perfectly mimics a mathematical magnitude comparison. You can flip your integers to Big-Endian using a lightning-fast hardware instruction (`BSWAP` on x86) via the compiler intrinsic `__builtin_bswap64()`.

### The Complete "memcmp" Solution

Here is how you implement your idea. You pay a tiny, 1-cycle penalty during the hashing phase to byte-swap the data, and in exchange, your sorting phase gets to use a raw, vectorized 24-byte memory comparison.

Step 1: Swap bytes during index creation (in `main`)

```c
    // ... inside your hashing loop ...
    #pragma omp parallel for
    for(uint64_t i=0; i<entry_count; i++) {
        uint64_t hash_out[2];
        FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte ((const char *)(buffer + i), CHUNK_SIZE, 0, hash_out);
        
        // FLIP TO BIG-ENDIAN FOR MEMCMP SORTING
        index[i].h2 = __builtin_bswap64(hash_out[1]);
        index[i].h1 = __builtin_bswap64(hash_out[0]);
        index[i].offset = __builtin_bswap64(i);
    }

```

Step 2: The blazingly fast comparator

```c
// --- ULTRA-FAST VECTORIZED COMPARE ---
int compare_disk_serial(const void* a, const void* b) {
    // memcmp will now correctly compare h2, then h1, then offset
    // because they are physically in that order, and in Big-Endian format.
    return memcmp(a, b, 24); 
}

```

Step 3: Un-swap the offset when you actually use it
Later in the code (Stage 4), when you need to actually read the offset to build your `zirka_updates.tmp`, you just flip it back:

```c
uint64_t master_offset = __builtin_bswap64(index[group_start].offset); 

```
*/

// Note: The testmachine is DELL Precision 7560 i7-11850H (8 cores, 16 threads), 128GB, SSD: Corsair MP600 2TB (Phison E16 Controller. Toshiba 96-layer 3D TLC):
/*
[sanmayce@djudjeto v7++]$ ls -l
-rwxr-xr-x 1 sanmayce sanmayce       17328 Feb 15 15:14  FastUnzirka_v7++_Final
-rwxrwxrwx 1 sanmayce sanmayce        9339 Feb 15 15:12  FastUnzirka_v7++_Final.c
-rwxr-xr-x 1 sanmayce sanmayce       27768 Feb 15 15:13  FastZirka_v7++_Final
-rwxrwxrwx 1 sanmayce sanmayce       63058 Feb 15 15:13  FastZirka_v7++_Final.c
-rwxrwxrwx 1 sanmayce sanmayce        2408 Feb 15 15:36  Report_READ-n-WRITE_usage.sh
-rw-r--r-- 1 sanmayce sanmayce 27436717568 Jan 28 18:27 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar'

[sanmayce@djudjeto v7++]$ su
Password: 

[root@djudjeto v7++]# sh Report_READ-n-WRITE_usage.sh SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar 
-----------------------------------------------------------
 [Snapshot 1] Reading SMART counters from /dev/nvme1n1p2...
   Start Read  : 492894157 units
   Start Write : 676606741 units
-----------------------------------------------------------

 [Running] ./FastZirka_v7++_Final SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar
__________.__        __            
\____    /|__|______|  | _______   
  /     / |  \_  __ \  |/ /\__  \  
 /     /_ |  ||  | \/    <  / __ \_
/_______ \|__||__|  |__|_ \(____  /
        \/               \/     \/ 
Version 7++, Deduplication granularity 4096
[Zirka 1-Pass] File: SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar (25.55 GB)
[Zirka 1-Pass] Zero-RAM Mode: Input is memory-mapped (OS manages paging).
1. Creating Index (24x Filesize using MMAP, 27436713473 entries)...
   Hashing (Parallel Pippip, taking 128bits=16bytes)...
   Hashed in 947.18s
2. Sorting Disk Index (Parallel Quicksort)...
   Sort Progress = 100.0%
   Sorted in 12588.56s
   Total parallel tasks generated: 13391930
3. Building Rank Map (8x Filesize using MMAP, Nuclear Mode: Sequential I/O)...
   [Nuclear] Gathering duplicates (16x Filesize using MMAP)...
   [Nuclear] Found 13381855880 duplicates to link.
   [Nuclear] Sorting updates by file position...
   Sort Progress = 100.0%
   [Nuclear] Applying updates to Rank Map...
4. Encoding (Direct Rank Lookup)...
   Encoded: 100.0%
Done.
    Command being timed: "./FastZirka_v7++_Final SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar"
    User time (seconds): 174177.42
    System time (seconds): 14244.59
    Percent of CPU this job got: 985%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 5:18:32
    Average shared text size (kbytes): 0
    Average unshared data size (kbytes): 0
    Average stack size (kbytes): 0
    Average total size (kbytes): 0
    Maximum resident set size (kbytes): 122111568
    Average resident set size (kbytes): 0
    Major (requiring I/O) page faults: 65663683
    Minor (reclaiming a frame) page faults: 2851395316
    Voluntary context switches: 368228845
    Involuntary context switches: 25781338
    Swaps: 0
    File system inputs: 12291134792
    File system outputs: 17860163088
    Socket messages sent: 0
    Socket messages received: 0
    Signals delivered: 0
    Page size (bytes): 4096
    Exit status: 0

-----------------------------------------------------------
 [Snapshot 2] Reading SMART counters...
   End Read    : 505188270 units
   End Write   : 694467239 units
-----------------------------------------------------------

=====================================================
 DATA USAGE REPORT
=====================================================
 Data Read    : 12294113 units (~5862.29 GB)
 Data Written : 17860498 units (~8516.55 GB)
=====================================================

[root@djudjeto v7++]# ls -l SU*
-rw-r--r-- 1 sanmayce sanmayce 27436717568 Jan 28 18:27 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar'
-rw-r--r-- 1 root     root     13490639033 Feb 15 20:58 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka'

[root@djudjeto v7++]# /bin/time -v ./FastUnzirka_v7++_Final SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka 
[Zirka v7 Restorer] Processing SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka...

Restoration Complete.
Dedup Tags Processed: 3415645
Final File Size: 27436717568 bytes
    Command being timed: "./FastUnzirka_v7++_Final SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka"
    User time (seconds): 8.51
    System time (seconds): 10.13
    Percent of CPU this job got: 71%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 0:26.10
    Average shared text size (kbytes): 0
    Average unshared data size (kbytes): 0
    Average stack size (kbytes): 0
    Average total size (kbytes): 0
    Maximum resident set size (kbytes): 24260928
    Average resident set size (kbytes): 0
    Major (requiring I/O) page faults: 8
    Minor (reclaiming a frame) page faults: 7059498
    Voluntary context switches: 7382
    Involuntary context switches: 218
    Swaps: 0
    File system inputs: 25068968
    File system outputs: 53587456
    Socket messages sent: 0
    Socket messages received: 0
    Signals delivered: 0
    Page size (bytes): 4096
    Exit status: 0

[root@djudjeto v7++]# b3sum SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar
fe1bf9b7f83bcc8b8edead02e4eef0e8f8d8ee4f1e9a8c0f14591c8c50528b40  SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar
[root@djudjeto v7++]# b3sum SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka.restored 
fe1bf9b7f83bcc8b8edead02e4eef0e8f8d8ee4f1e9a8c0f14591c8c50528b40  SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka.restored

[root@djudjeto v7++]# EmptyBuffersCache.sh 
[root@djudjeto v7++]# /bin/time -v ./eXdupe-3.0.2_linux_amd64 -k -x0 SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0
Input:                       27,436,717,568 B in 1 files                                                                                                                    
Output:                      15,713,574,592 B (57%)
Speed:                       1,730 MB/s
Speed w/o init overhead:     1,850 MB/s
Stored as duplicated files:  0 B in 0 files
Stored as duplicated blocks: 11.1 GB (5.66 GB large, 5.50 GB small)
Stored as literals:          14.3 GB (14.3 GB compressed)
Overheads:                   383 B meta, 11.9 MB refs, 231 MB hashtable, 5.27 MB misc
Unhashed due to congestion:  1.63 GB large, 2.76 GB small
Unhashed anomalies:          133 MB large, 277 MB small
High entropy files:          0 B in 0 files
Hashtable fillratio:         10% small, 0% large
    Command being timed: "./eXdupe-3.0.2_linux_amd64 -k -x0 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0"
    User time (seconds): 21.41
    System time (seconds): 8.62
    Percent of CPU this job got: 196%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 0:15.30
    Average shared text size (kbytes): 0
    Average unshared data size (kbytes): 0
    Average stack size (kbytes): 0
    Average total size (kbytes): 0
    Maximum resident set size (kbytes): 2458556
    Average resident set size (kbytes): 0
    Major (requiring I/O) page faults: 35
    Minor (reclaiming a frame) page faults: 626495
    Voluntary context switches: 262556
    Involuntary context switches: 93
    Swaps: 0
    File system inputs: 53596048
    File system outputs: 30690576
    Socket messages sent: 0
    Socket messages received: 0
    Signals delivered: 0
    Page size (bytes): 4096
    Exit status: 0
[root@djudjeto v7++]# /bin/time -v ./eXdupe-3.0.2_linux_amd64 -R SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0 q
Wrote 27,436,717,568 bytes in 1 files                                                                                                                                       
    Command being timed: "./eXdupe-3.0.2_linux_amd64 -R SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0 q"
    User time (seconds): 7.20
    System time (seconds): 6.21
    Percent of CPU this job got: 97%
    Elapsed (wall clock) time (h:mm:ss or m:ss): 0:13.69
    Average shared text size (kbytes): 0
    Average unshared data size (kbytes): 0
    Average stack size (kbytes): 0
    Average total size (kbytes): 0
    Maximum resident set size (kbytes): 362020
    Average resident set size (kbytes): 0
    Major (requiring I/O) page faults: 0
    Minor (reclaiming a frame) page faults: 102964
    Voluntary context switches: 103
    Involuntary context switches: 148
    Swaps: 0
    File system inputs: 16
    File system outputs: 53587344
    Socket messages sent: 0
    Socket messages received: 0
    Signals delivered: 0
    Page size (bytes): 4096
    Exit status: 0
*/

/*
'Sefaria' thrown at RAR 7.12, eXdupe-3.0.2. Zirka 7++:

[root@djudjeto v7++]# ./rarlinux-x64-712 a -md32g -mcx SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka.rar SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka
[root@djudjeto v7++]# ./rarlinux-x64-712 a -md32g -mcx SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0.rar SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0 
[root@djudjeto v7++]# ./rarlinux-x64-712 a -md32g -mcx SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.rar SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar 

[root@djudjeto v7++]# ls -l SU*
-rw-r--r-- 1 sanmayce sanmayce 27436717568 Jan 28 18:27 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar'
-rw-r--r-- 1 root     root      1260838316 Feb 16 01:10 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.rar'

-rw-r--r-- 1 root     root     15713574592 Feb 15 23:03 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0'
-rw-r--r-- 1 root     root      1372507424 Feb 16 00:19 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0.rar'

-rw-r--r-- 1 root     root     13490639033 Feb 15 20:58 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka'
-rw-r--r-- 1 root     root      1174686817 Feb 15 23:41 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka.rar'

Zirka+RAR is (1260838316-1174686817)*100/1174686817= 7.33% stronger than RAR
Zirka+RAR is (1372507424-1174686817)*100/1174686817= 16.84% stronger than eXdupe+RAR
*/

// The DeDeDuplication = Duplication Showdown - eXdupe is (16.82-14.76)*100/14.76= 13.95% faster than Zirka:
/*
[sanmayce@djudjeto v7++]$ sudo mkdir -p /mnt/ramdisk
[sudo] password for sanmayce: 
[sanmayce@djudjeto v7++]$ sudo mount -t tmpfs -o size=80G tmpfs /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0 /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp FastUnzirka_v7++_Final /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp eXdupe-3.0.2_linux_amd64 /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cd /mnt/ramdisk/
[sanmayce@djudjeto ramdisk]$ ls -l
-rwxr-xr-x 1 sanmayce sanmayce     4895360 Feb 16 01:35  eXdupe-3.0.2_linux_amd64
-rwxr-xr-x 1 sanmayce sanmayce       17328 Feb 15 15:14  FastUnzirka_v7++_Final
-rw-r--r-- 1 sanmayce sanmayce 15713574592 Feb 16 01:35 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0'
-rw-r--r-- 1 sanmayce sanmayce 13490639033 Feb 16 01:35 'SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka'

[sanmayce@djudjeto ramdisk]$ perf stat -d ./FastUnzirka_v7++_Final SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka
[Zirka v7 Restorer] Processing SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka...

Restoration Complete.
Dedup Tags Processed: 3415645
Final File Size: 27436717568 bytes

 Performance counter stats for './FastUnzirka_v7++_Final SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka':

         16,803.59 msec task-clock:u                     #    0.999 CPUs utilized             
                 0      context-switches:u               #    0.000 /sec                      
                 0      cpu-migrations:u                 #    0.000 /sec                      
         6,961,591      page-faults:u                    #  414.292 K/sec                     
   175,007,954,055      instructions:u                   #    4.12  insn per cycle              (90.91%)
    42,444,050,519      cycles:u                         #    2.526 GHz                         (90.90%)
    40,381,973,147      branches:u                       #    2.403 G/sec                       (90.92%)
         3,814,208      branch-misses:u                  #    0.01% of all branches             (81.81%)
                        TopdownL1                 #      0.4 %  tma_backend_bound      
                                                  #     96.8 %  tma_bad_speculation    
                                                  #      0.7 %  tma_frontend_bound     
                                                  #      2.2 %  tma_retiring             (90.90%)
    40,362,366,308      L1-dcache-loads:u                #    2.402 G/sec                       (90.92%)
       445,287,152      L1-dcache-load-misses:u          #    1.10% of all L1-dcache accesses   (90.91%)
        85,120,545      LLC-loads:u                      #    5.066 M/sec                       (90.92%)
        57,574,197      LLC-load-misses:u                #   67.64% of all LL-cache accesses    (81.80%)

      16.823698295 seconds time elapsed

       8.214037000 seconds user
       8.461439000 seconds sys

[sanmayce@djudjeto ramdisk]$ perf stat -d ./eXdupe-3.0.2_linux_amd64 -R SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0 q
Wrote 27,436,717,568 bytes in 1 files                                                                                                                                       

 Performance counter stats for './eXdupe-3.0.2_linux_amd64 -R SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0 q':

         14,701.96 msec task-clock:u                     #    0.996 CPUs utilized             
                 0      context-switches:u               #    0.000 /sec                      
                 0      cpu-migrations:u                 #    0.000 /sec                      
           102,988      page-faults:u                    #    7.005 K/sec                     
    85,919,414,851      instructions:u                   #    2.44  insn per cycle              (90.90%)
    35,163,744,366      cycles:u                         #    2.392 GHz                         (90.91%)
    12,567,674,765      branches:u                       #  854.830 M/sec                       (90.91%)
        90,587,068      branch-misses:u                  #    0.72% of all branches             (81.82%)
                        TopdownL1                 #      5.0 %  tma_backend_bound      
                                                  #     94.2 %  tma_bad_speculation    
                                                  #      0.2 %  tma_frontend_bound     
                                                  #      0.5 %  tma_retiring             (90.92%)
    13,338,600,761      L1-dcache-loads:u                #  907.267 M/sec                       (90.91%)
     4,523,671,736      L1-dcache-load-misses:u          #   33.91% of all L1-dcache accesses   (90.92%)
       147,757,520      LLC-loads:u                      #   10.050 M/sec                       (90.91%)
        72,831,024      LLC-load-misses:u                #   49.29% of all LL-cache accesses    (81.81%)

      14.760860445 seconds time elapsed

       7.429165000 seconds user
       7.156169000 seconds sys

[sanmayce@djudjeto ramdisk]$ 
*/

