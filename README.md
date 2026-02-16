Zirka v.7++ - THE ZER0 RAM DEDUPLICATOR - README.TXT
<br>
<br>
<img width="728" height="1488" alt="transparent" src="https://github.com/user-attachments/assets/4cbbcc53-1eba-42f7-a1cb-b042d54ea136" />
<br>
<br>

THE HIGHLIGHT: ZERO-RAM ARCHITECTURE<br><br>
Most compression tools are limited by your computer's RAM.
If you want to deduplicate 300GB, they usually require 8++GB of RAM. 
NOT THIS TOOL.
This encoder uses almost ZERO RAM.
It performs operations directly on your NVMe/SSD.
If you can store it, you can dedup it.

Zirka v7++: Original & Unique Features

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

In all seriousness, Zirka is a texttoy, for real/practical scenarios, look up Lasse's eXdupe:
https://github.com/rrrlasse/eXdupe

1. WHAT IS THIS?

A FREE open-source console tool (Linux) written in C by Kaze & Gemini Pro AI.

2. HOW THE ENCODER WORKS

The encoder slides a 4096-byte "viewing window" across your file.

Step A: "Is this block a repeat?"
Step B: 
- IF UNIQUE: It writes the raw byte and saves the block to the disk-index.
- IF DUPLICATE: It writes a 13-byte "Pointer" and jumps ahead 4096 bytes.

3. THE "SANITY CHECK" POINTER

To prevent errors if the original file contains the "Magic Byte" (255), 
we use a 13-byte secure pointer:
[1 Byte Magic] + [8 Bytes Offset] + [4 Bytes Hashsum]

The Decoder will only follow a pointer if the data at the destination 
perfectly matches the 4-byte Hashsum passcode.

4. THE DECODER

The decoder is a lightweight "Copy-Paster." It reads the instructions 
and rebuilds the file by copying from the data it has already restored.

5. CREDITS

Gemini Pro AI is cool (often wrongly implemented stuff though), it is the main "culprit" for this wondertool.
The 'Zirka' naming is to pay tribute to the beloved kidbook 'Дружков Юрий - Приключения Карандаша и Самоделкина (худ. И. Семёнов)'.
One of the two villains was called Zirka - a petty spy. The logo was made from the book's original drawing fed to the Japanese PixAI.

6. Benchmarked

The testmachine is DELL Precision 7560 i7-11850H (8 cores, 16 threads), 128GB, SSD: Corsair MP600 2TB (Phison E16 Controller. Toshiba 96-layer 3D TLC):<br>
```
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
```

Sefaria thrown at RAR 7.12, eXdupe-3.0.2. Zirka 7++:<br>
```
[root@djudjeto v7++]# ./rarlinux-x64-712 a -md32g -mcx SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka.rar SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka
[root@djudjeto v7++]# ./rarlinux-x64-712 a -md32g -mcx SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0.rar SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0 
[root@djudjeto v7++]# ./rarlinux-x64-712 a -md32g -mcx SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.rar SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar 

[root@djudjeto v7++]# ls -l SU*
-rw-r--r-- 1 sanmayce sanmayce 27436717568 Jan 28 18:27 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar
-rw-r--r-- 1 root     root      1260838316 Feb 16 01:10 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.rar

-rw-r--r-- 1 root     root     15713574592 Feb 15 23:03 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0
-rw-r--r-- 1 root     root      1372507424 Feb 16 00:19 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0.rar

-rw-r--r-- 1 root     root     13490639033 Feb 15 20:58 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka
-rw-r--r-- 1 root     root      1174686817 Feb 15 23:41 SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka.rar

Zirka+RAR is (1260838316-1174686817)*100/1174686817= 7.33% stronger than RAR
Zirka+RAR is (1372507424-1174686817)*100/1174686817= 16.84% stronger than eXdupe+RAR
```
<br>
The DeDeDuplication = Duplication Showdown - eXdupe is (16.82-14.76)*100/14.76= 13.95% faster than Zirka:
<br>
```
[sanmayce@djudjeto v7++]$ sudo mkdir -p /mnt/ramdisk
[sudo] password for sanmayce: 
[sanmayce@djudjeto v7++]$ sudo mount -t tmpfs -o size=80G tmpfs /mnt/ramdisk/
```
```
[sanmayce@djudjeto v7++]$ cp SUPRAPIG Sefaria-Export-master (62438-folders 82694-files).tar.eXdupe0 /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp SUPRAPIG Sefaria-Export-master (62438-folders 82694-files).tar.zirka /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp FastUnzirka v7++ Final /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cp eXdupe-3.0.2 linux amd64 /mnt/ramdisk/
[sanmayce@djudjeto v7++]$ cd /mnt/ramdisk/
```
```
[sanmayce@djudjeto ramdisk]$ ls -l
-rwxr-xr-x 1 sanmayce sanmayce     4895360 Feb 16 01:35 eXdupe-3.0.2 linux amd64
-rwxr-xr-x 1 sanmayce sanmayce       17328 Feb 15 15:14 FastUnzirka v7++ Final
-rw-r--r-- 1 sanmayce sanmayce 15713574592 Feb 16 01:35 SUPRAPIG Sefaria-Export-master (62438-folders 82694-files).tar.eXdupe0
-rw-r--r-- 1 sanmayce sanmayce 13490639033 Feb 16 01:35 SUPRAPIG Sefaria-Export-master (62438-folders 82694-files).tar.zirka
```
<br>

```
[sanmayce@djudjeto ramdisk]$ perf stat -d ./FastUnzirka_v7++_Final SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.zirka
[Zirka v7 Restorer] Processing SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka...

Restoration Complete.
Dedup Tags Processed: 3415645
Final File Size: 27436717568 bytes

 Performance counter stats for ./FastUnzirka_v7++_Final SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.zirka:

         16,803.59 msec task-clock:u                         0.999 CPUs utilized             
                 0      context-switches:u                   0.000 /sec                      
                 0      cpu-migrations:u                     0.000 /sec                      
         6,961,591      page-faults:u                      414.292 K/sec                     
   175,007,954,055      instructions:u                       4.12  insn per cycle              (90.91%)
    42,444,050,519      cycles:u                             2.526 GHz                         (90.90%)
    40,381,973,147      branches:u                           2.403 G/sec                       (90.92%)
         3,814,208      branch-misses:u                      0.01% of all branches             (81.81%)
                        TopdownL1                       0.4 %  tma_backend_bound      
                                                       96.8 %  tma_bad_speculation    
                                                        0.7 %  tma_frontend_bound     
                                                        2.2 %  tma_retiring             (90.90%)
    40,362,366,308      L1-dcache-loads:u                    2.402 G/sec                       (90.92%)
       445,287,152      L1-dcache-load-misses:u              1.10% of all L1-dcache accesses   (90.91%)
        85,120,545      LLC-loads:u                          5.066 M/sec                       (90.92%)
        57,574,197      LLC-load-misses:u                   67.64% of all LL-cache accesses    (81.80%)

      16.823698295 seconds time elapsed

       8.214037000 seconds user
       8.461439000 seconds sys

[sanmayce@djudjeto ramdisk]$ perf stat -d ./eXdupe-3.0.2_linux_amd64 -R SUPRAPIG_Sefaria-Export-master_\(62438-folders_82694-files\).tar.eXdupe0 q
Wrote 27,436,717,568 bytes in 1 files                                                                                                                         

 Performance counter stats for ./eXdupe-3.0.2_linux_amd64 -R SUPRAPIG_Sefaria-Export-master_(62438-folders_82694-files).tar.eXdupe0 q:

         14,701.96 msec task-clock:u                          0.996 CPUs utilized             
                 0      context-switches:u                    0.000 /sec                      
                 0      cpu-migrations:u                      0.000 /sec                      
           102,988      page-faults:u                         7.005 K/sec                     
    85,919,414,851      instructions:u                        2.44  insn per cycle              (90.90%)
    35,163,744,366      cycles:u                              2.392 GHz                         (90.91%)
    12,567,674,765      branches:u                          854.830 M/sec                       (90.91%)
        90,587,068      branch-misses:u                       0.72% of all branches             (81.82%)
                        TopdownL1                        5.0 %  tma_backend_bound      
                                                        94.2 %  tma_bad_speculation    
                                                         0.2 %  tma_frontend_bound     
                                                         0.5 %  tma_retiring             (90.92%)
    13,338,600,761      L1-dcache-loads:u                   907.267 M/sec                       (90.91%)
     4,523,671,736      L1-dcache-load-misses:u              33.91% of all L1-dcache accesses   (90.92%)
       147,757,520      LLC-loads:u                          10.050 M/sec                       (90.91%)
        72,831,024      LLC-load-misses:u                    49.29% of all LL-cache accesses    (81.81%)

      14.760860445 seconds time elapsed

       7.429165000 seconds user
       7.156169000 seconds sys

[sanmayce@djudjeto ramdisk]$ 
```

зирка<br>
Bulgarian
Etymology: By surface analysis, зи́ра (zíra, “transparent substance”) +‎ -ка (-ka)
Pronunciation: IPA: [ˈzirkɐ]
Noun
зи́рка • (zírka) m (dialectal)
slit, leak, gap for seeing through<br>
Derived terms:<br>
взи́рка (vzírka) (dialectal)
прози́рка (prozírka) (dialectal)
зи́ркав (zírkav, “squeezed, squinted”) (of eyes)<br>
Related terms:<br>
зи́ркам (zírkam, “to see through”) (dialectal)
прозо́рец (prozórec, “window”)
https://en.wiktionary.org/wiki/%D0%B7%D0%B8%D1%80%D0%BA%D0%B0

РЕЧНИК НА БЪЛГАРСКИЯ ЕЗИК (ОНЛАЙН)<br>
ЗЍРКА ж. Диал. Цепнатина или отвор в някаква преграда, покрив и под.; пролука, процеп, прозорка, прозирка. Дядо Яначко седеше поприведен в тъмното на одъра; през зирките на вратата светеше – оттатъка в оная, голямата стая. Ил. Волен, НС, 73. Той наблюдаваше през зирките на керемидите как зората се сипваше. Г. Караславов, Избр. съч. Х, 26. През зирките в дъсчената врата на плевнята се виждаше целият двор. В. Андреев, ПП, 8.
https://ibl.bas.bg/rbe/lang/bg/%D0%B7%D0%B8%D1%80%D0%BA%D0%B0/

8. DOWNLOAD

https://github.com/Sanmayce/Zirka

2026-Feb-15
Kaze (sanmayce@sanmayce.com)

