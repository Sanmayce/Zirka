// zirka.c
// $ gcc -O3 zirka.c -o zirka_v2 -static

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#define CHUNK_SIZE 256
#define ORDER 32             // High order for flatter tree (faster disk I/O)
#define MAX_KEYS (ORDER - 1) 
#define NODE_NULL 0          

// --- B-TREE STRUCTURE ---
typedef struct {
    uint8_t  keys[MAX_KEYS][CHUNK_SIZE]; 
    uint64_t offsets[MAX_KEYS];         
    uint64_t children[ORDER];           
    int32_t  n;                          
    int32_t  leaf;                       
} BTreeNode;

// --- GLOBAL STATE ---
FILE* fidx = NULL;
uint64_t root_addr = NODE_NULL;
uint64_t nodes_written = 0;
uint64_t total_keys = 0;
uint64_t duplicates_found = 0;

// --- HASH FUNCTION (Must match Decoder) ---
// Robust FNV-1a Hash to prevent "Zero-Block" Collisions
uint32_t calculate_hashsum(const uint8_t* data) {
    uint32_t hash = 2166136261u; 
    for (int i = 0; i < CHUNK_SIZE; i++) {
        hash ^= data[i];
        hash *= 16777619u; 
    }
    return hash;
}

// --- DISK OPS ---
void read_node(uint64_t addr, BTreeNode* node) {
    if (addr == NODE_NULL) return;
    fseek(fidx, addr, SEEK_SET);
    fread(node, sizeof(BTreeNode), 1, fidx);
}

void write_node(uint64_t addr, BTreeNode* node) {
    fseek(fidx, addr, SEEK_SET);
    fwrite(node, sizeof(BTreeNode), 1, fidx);
}

uint64_t alloc_node() {
    fseek(fidx, 0, SEEK_END);
    uint64_t addr = ftell(fidx);
    BTreeNode empty = {0}; 
    fwrite(&empty, sizeof(BTreeNode), 1, fidx);
    nodes_written++;
    return addr;
}

int compare_chunks(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, CHUNK_SIZE);
}

// --- B-TREE LOGIC ---
typedef struct {
    bool did_split;
    bool already_existed;
    uint8_t promoted_key[CHUNK_SIZE];
    uint64_t promoted_offset;
    uint64_t right_child_addr;
} SplitResult;

SplitResult split_generic(BTreeNode* node, const uint8_t* k, uint64_t offset, uint64_t new_child_addr) {
    SplitResult res = {0};
    uint8_t  t_keys[ORDER][CHUNK_SIZE];
    uint64_t t_offs[ORDER];
    uint64_t t_children[ORDER + 1];

    int i = 0, j = 0;
    bool inserted = false;
    for (i = 0; i < MAX_KEYS; i++) {
        if (!inserted && compare_chunks(k, node->keys[i]) < 0) {
            memcpy(t_keys[j], k, CHUNK_SIZE);
            t_offs[j] = offset;
            t_children[j] = node->children[i];
            t_children[j+1] = new_child_addr;
            j++; inserted = true;
        }
        memcpy(t_keys[j], node->keys[i], CHUNK_SIZE);
        t_offs[j] = node->offsets[i];
        t_children[j + (inserted ? 1 : 0)] = node->children[i+1];
        if (!inserted) t_children[j] = node->children[i];
        j++;
    }
    if (!inserted) {
        memcpy(t_keys[MAX_KEYS], k, CHUNK_SIZE);
        t_offs[MAX_KEYS] = offset;
        t_children[MAX_KEYS] = node->children[MAX_KEYS];
        t_children[ORDER] = new_child_addr;
    }

    int median = ORDER / 2;
    node->n = median;
    for (i = 0; i < median; i++) {
        memcpy(node->keys[i], t_keys[i], CHUNK_SIZE);
        node->offsets[i] = t_offs[i];
        node->children[i] = t_children[i];
    }
    node->children[median] = t_children[median];
    
    uint64_t r_addr = alloc_node();
    BTreeNode r = {0};
    r.leaf = node->leaf;
    r.n = MAX_KEYS - median;
    for (i = 0; i < r.n; i++) {
        memcpy(r.keys[i], t_keys[median + 1 + i], CHUNK_SIZE);
        r.offsets[i] = t_offs[median + 1 + i];
        r.children[i] = t_children[median + 1 + i];
    }
    r.children[r.n] = t_children[ORDER];
    write_node(r_addr, &r);

    res.did_split = true;
    memcpy(res.promoted_key, t_keys[median], CHUNK_SIZE);
    res.promoted_offset = t_offs[median];
    res.right_child_addr = r_addr;
    return res;
}

SplitResult insert_recursive(uint64_t node_addr, const uint8_t* k, uint64_t offset) {
    BTreeNode node;
    read_node(node_addr, &node);
    SplitResult res = {0};

    for(int i = 0; i < node.n; i++) {
        if(compare_chunks(k, node.keys[i]) == 0) {
            res.already_existed = true;
            return res;
        }
    }

    if (node.leaf) {
        if (node.n < MAX_KEYS) {
            int i = node.n - 1;
            while (i >= 0 && compare_chunks(k, node.keys[i]) < 0) {
                memcpy(node.keys[i+1], node.keys[i], CHUNK_SIZE);
                node.offsets[i+1] = node.offsets[i];
                i--;
            }
            memcpy(node.keys[i+1], k, CHUNK_SIZE);
            node.offsets[i+1] = offset;
            node.n++;
            write_node(node_addr, &node);
            return res;
        } else {
            res = split_generic(&node, k, offset, NODE_NULL);
            write_node(node_addr, &node);
            return res;
        }
    } else {
        int i = 0;
        while (i < node.n && compare_chunks(k, node.keys[i]) > 0) i++;
        SplitResult c_res = insert_recursive(node.children[i], k, offset);
        if (c_res.already_existed || !c_res.did_split) return c_res;

        if (node.n < MAX_KEYS) {
            int j = node.n - 1;
            while (j >= i) {
                memcpy(node.keys[j+1], node.keys[j], CHUNK_SIZE);
                node.offsets[j+1] = node.offsets[j];
                node.children[j+2] = node.children[j+1]; 
                j--;
            }
            memcpy(node.keys[i], c_res.promoted_key, CHUNK_SIZE);
            node.offsets[i] = c_res.promoted_offset;
            node.children[i+1] = c_res.right_child_addr;
            node.n++; 
            write_node(node_addr, &node);
            return (SplitResult){0};
        } else {
            res = split_generic(&node, c_res.promoted_key, c_res.promoted_offset, c_res.right_child_addr);
            write_node(node_addr, &node);
            return res;
        }
    }
}

void insert_root(const uint8_t* k, uint64_t offset) {
    if (root_addr == NODE_NULL) {
        root_addr = alloc_node();
        BTreeNode r = {0}; r.leaf = 1; r.n = 1;
        memcpy(r.keys[0], k, CHUNK_SIZE); r.offsets[0] = offset;
        write_node(root_addr, &r);
        total_keys++;
    } else {
        SplitResult res = insert_recursive(root_addr, k, offset);
        if (!res.already_existed) {
            total_keys++;
            if (res.did_split) {
                uint64_t nr_addr = alloc_node();
                BTreeNode nr = {0}; nr.leaf = 0; nr.n = 1;
                memcpy(nr.keys[0], res.promoted_key, CHUNK_SIZE);
                nr.offsets[0] = res.promoted_offset;
                nr.children[0] = root_addr; nr.children[1] = res.right_child_addr;
                write_node(nr_addr, &nr);
                root_addr = nr_addr;
            }
        }
    }
}

bool find_and_insert(const uint8_t* k, uint64_t current_offset, uint64_t* orig_offset) {
    uint64_t search_addr = root_addr;
    while (search_addr != NODE_NULL) {
        BTreeNode node;
        read_node(search_addr, &node);
        int i = 0;
        while (i < node.n && compare_chunks(k, node.keys[i]) > 0) i++;
        if (i < node.n && compare_chunks(k, node.keys[i]) == 0) {
            *orig_offset = node.offsets[i];
            duplicates_found++;
            return true; 
        }
        if (node.leaf) break;
        search_addr = node.children[i];
    }
    insert_root(k, current_offset);
    return false;
}

// --- MAIN ENCODER LOOP ---
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin) { perror("Input file error"); return 1; }

    // --- Stats Prep ---
    fseek(fin, 0, SEEK_END);
    double input_size_mb = ftell(fin) / 1048576.0;
    uint64_t input_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    char out_name[512];
    snprintf(out_name, sizeof(out_name), "%s.deduplicatedfile", argv[1]);
    FILE* fout = fopen(out_name, "wb");
    
    // Open Index
    fidx = fopen("dedup.index", "w+b");
    uint64_t h_ptr = 0;
    fwrite(&h_ptr, 8, 1, fidx);

    uint8_t window[CHUNK_SIZE];
    uint64_t pos = 0;
    uint8_t magic = 255;
    size_t in_buffer = 0;

    // --- INITIAL LOAD ---
    // We try to fill the window completely to start.
    in_buffer = fread(window, 1, CHUNK_SIZE, fin);

    printf("Starting Deduplication on: %s\n", argv[1]);

    // --- MAIN LOOP ---
    // Only run the deduplication logic if we have a FULL CHUNK to compare.
    while (in_buffer == CHUNK_SIZE) {
        uint64_t first_seen_at = 0;
        
        // 1. Check B-Tree for current window
        if (find_and_insert(window, pos, &first_seen_at)) {
            // --- MATCH FOUND ---
            // A. Write Magic Pointer (13 bytes)
            uint32_t hash = calculate_hashsum(window);
            fputc(magic, fout);
            fwrite(&first_seen_at, 8, 1, fout); // Offset
            fwrite(&hash, 4, 1, fout);          // Checksum

            // B. JUMP! 
            // We have encoded 256 bytes instantly. 
            pos += CHUNK_SIZE;
            
            // C. Refill Window Completely
            // We discard current buffer and load the NEXT 256 bytes from disk.
            in_buffer = fread(window, 1, CHUNK_SIZE, fin);

        } else {
            // --- NO MATCH (UNIQUE) ---
            // A. Write ONLY the first byte (Head-Writing)
            fputc(window[0], fout);
            
            // B. SLIDE!
            // Shift window left by 1
            memmove(window, window + 1, CHUNK_SIZE - 1);
            
            // C. Read 1 new byte to fill the end
            int next_byte = fgetc(fin);
            if (next_byte != EOF) {
                window[CHUNK_SIZE - 1] = (uint8_t)next_byte;
                // in_buffer remains CHUNK_SIZE
            } else {
                in_buffer--; // We are entering the "Tail" phase
            }
            pos++;
        }

        if (pos % 1048576 == 0) {
            fprintf(stderr, "\rDone: %lu%%", (pos*100)/input_size);
        }
    }

    // --- TAIL FLUSH ---
    // If the file didn't end on a perfect 256 boundary, or we hit EOF while sliding,
    // write the remaining bytes in the buffer as literals.
    for (size_t i = 0; i < in_buffer; i++) {
        fputc(window[i], fout);
        pos++;
    }

    // --- FINISH INDEX ---
    fseek(fidx, 0, SEEK_SET);
    fwrite(&root_addr, 8, 1, fidx); // Save root address to header
    
    // --- FINAL REPORT ---
    fseek(fidx, 0, SEEK_END);
    double index_size_mb = ftell(fidx) / 1048576.0;

            fprintf(stderr, "\nDone: %lu%%\n", 100);
    printf("Duplicates Found: %lu\n", duplicates_found);
    if (input_size_mb > 0) {
        printf("Index Inflation:  %.1fx (Cost of infinite lookback)\n", 
                index_size_mb / input_size_mb);
    }

    fclose(fin);
    fclose(fout);
    fclose(fidx);
    return 0;
}

