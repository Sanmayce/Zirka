// Compile: clang -O3 -msse4.2 -maes  FastUnzirka_v7++_Final.c -o FastUnzirka_v7++_Final

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <immintrin.h>

#define CHUNK_SIZE 4096 //384 
#define MAGIC_BYTE 255
#define INITIAL_OUTPUT_SIZE (1024ULL * 1024ULL * 1024ULL) 

#define _PADr_KAZE(x, n) ( ((x) << (n))>>(n) )
#define _PAD_KAZE(x, n) ( ((x) << (n)) )

// Too weak due to ChunkA2 and ChunkB2 not AESed... 2026-Feb-14, strengthened
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

// --- THE FIX: MATCHING ENCODER HASH ---
// This hashes the 8-byte offset value, exactly like Zirka_v7.c does
uint32_t calc_fnv_off(uint64_t offset) {
    uint8_t data[8]; 
    memcpy(data, &offset, 8);
    uint32_t h = 2166136261u;
    for (int i=0; i<8; i++) { 
        h ^= data[i]; 
        h *= 16777619u; 
    }
    return h;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printf("Usage: %s <file.zirka>\n", argv[0]); return 1; }

    // 1. Open and Map Input
    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in < 0) { perror("Input error"); return 1; }
    struct stat sb;
    fstat(fd_in, &sb);
    uint8_t* in_map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd_in, 0);

    // 2. Prepare Output
    char out_name[512];
    snprintf(out_name, 512, "%s.restored", argv[1]);
    int fd_out = open(out_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    
    // Initial 1GB allocation (will grow if needed)
    uint64_t out_cap = INITIAL_OUTPUT_SIZE;
    ftruncate(fd_out, out_cap);
    uint8_t* out_map = mmap(NULL, out_cap, PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);

    uint64_t ipos = 0, opos = 0, hits = 0;
    uint32_t chk[4];

    printf("[Zirka v7 Restorer] Processing %s...\n", argv[1]);

    while (ipos < sb.st_size) {
        // Resize check
        if (opos + CHUNK_SIZE >= out_cap) {
            uint64_t old_cap = out_cap;
            out_cap *= 2;
            ftruncate(fd_out, out_cap);
            munmap(out_map, old_cap);
            out_map = mmap(NULL, out_cap, PROT_READ | PROT_WRITE, MAP_SHARED, fd_out, 0);
        }

        // Check for Magic Tag
        if (in_map[ipos] == MAGIC_BYTE && ipos + 13 <= sb.st_size) {
            uint64_t match_off;
            uint32_t expected_hash;
            
            // Read the 8-byte offset and 4-byte hash from the .zirka file
            memcpy(&match_off, &in_map[ipos + 1], 8);
            memcpy(&expected_hash, &in_map[ipos + 9], 4);

            // VERIFICATION: Hash the OFFSET, not the data
            //if (calc_fnv_off(match_off) == expected_hash) {
            FNV1A_Pippip_Yurii_OOO_128bit_AES_TriXZi_Mikayla_forte((const char *)&match_off, 8, 0, chk);
            if (chk[0] == expected_hash) {
                // Success! Restore the chunk from the previous output data
                memcpy(&out_map[opos], &out_map[match_off], CHUNK_SIZE);
                opos += CHUNK_SIZE;
                ipos += 13;
                hits++;
                continue;
            }
        }

        // Literal Byte
        out_map[opos++] = in_map[ipos++];
    }

    printf("\nRestoration Complete.\n");
    printf("Dedup Tags Processed: %lu\n", hits);
    printf("Final File Size: %lu bytes\n", opos);

    // Finalize file size on disk
    ftruncate(fd_out, opos);
    
    munmap(in_map, sb.st_size);
    munmap(out_map, out_cap);
    close(fd_in);
    close(fd_out);

    return 0;
}

