// unzirka.c
// $ gcc -O3 unzirka.c -o unzirka_v2 -static

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define CHUNK_SIZE 256

// Robust FNV-1a Hash to prevent "Zero-Block" Collisions
uint32_t calculate_hashsum(const uint8_t* data) {
    uint32_t hash = 2166136261u; 
    for (int i = 0; i < CHUNK_SIZE; i++) {
        hash ^= data[i];
        hash *= 16777619u; 
    }
    return hash;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printf("Usage: %s <file.deduplicatedfile>\n", argv[0]); return 1; }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin) { perror("Input error"); return 1; }

    char out_name[512];
    snprintf(out_name, sizeof(out_name), "%s.restored", argv[1]);
    FILE* fout = fopen(out_name, "w+b");
    if (!fout) { perror("Output error"); fclose(fin); return 1; }

    int c;
    uint8_t buffer[CHUNK_SIZE];
    uint64_t current_out_pos = 0;

    printf("Decoding: %s\n", argv[1]);

    while ((c = fgetc(fin)) != EOF) {
        if (c == 255) {
            uint64_t source_offset;
            uint32_t expected_hash;
            
            long pointer_start = ftell(fin); // Current position is just after '255'

            // Try to read pointer args
            if (fread(&source_offset, 8, 1, fin) == 1 && fread(&expected_hash, 4, 1, fin) == 1) {
                
                bool match = false;
                if (source_offset < current_out_pos) {
                    long saved_pos = ftell(fout);
                    fseek(fout, source_offset, SEEK_SET);
                    if (fread(buffer, 1, CHUNK_SIZE, fout) == CHUNK_SIZE) {
                        if (calculate_hashsum(buffer) == expected_hash) {
                            match = true;
                        }
                    }
                    fseek(fout, saved_pos, SEEK_SET); // Always return to write head
                }

                if (match) {
                    // It was a valid pointer!
                    fwrite(buffer, 1, CHUNK_SIZE, fout);
                    current_out_pos += CHUNK_SIZE;
                    continue; // Done with this '255'
                }
            }
            
            // --- FALSE ALARM ---
            // If we are here, the '255' was just a literal.
            // We must Rewind input to just after the '255' (pointer_start)
            // But wait! We read 12 bytes that were NOT a pointer.
            // Those 12 bytes are actually LITERALS that come after the 255.
            
            fputc(255, fout); // Write the literal 255
            current_out_pos++;
            
            // Rewind the input file to process the bytes *after* 255 as normal data
            fseek(fin, pointer_start, SEEK_SET);
            
        } else {
            // Standard literal
            fputc((uint8_t)c, fout);
            current_out_pos++;
        }

        if (current_out_pos % 10485760 == 0) {
            fprintf(stderr, "\rRestored: %lu MB", current_out_pos / 1048576);
        }
    }

    printf("\nDone. Final Size: %lu bytes\n", current_out_pos);
    fclose(fin); fclose(fout);
    return 0;
}

