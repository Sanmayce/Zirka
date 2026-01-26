Zirka v.2 - THE B-TREE SLIDING DEDUPLICATOR (JUMPING EDITION) - README.TXT
<br>
<br>
<img width="728" height="1488" alt="transparent" src="https://github.com/user-attachments/assets/4cbbcc53-1eba-42f7-a1cb-b042d54ea136" />
<br>
<br>

THE HIGHLIGHT: ZERO-RAM ARCHITECTURE
Most compression tools are limited by your computer's RAM.
If you want to deduplicate 300GB, they usually require 8++GB of RAM. 
NOT THIS TOOL.
By using a Disk-Based B-Tree, this encoder uses almost ZERO RAM.
It performs operations directly on your NVMe/SSD.
If you can store it, you can dedup it.

THE FOUR EXTREMES (THE HIGHLIGHTS) [

1. THE FASTEST (for given compression ratio) DECOMPRESSOR: 
   Optimized specifically for deduplicated .tar files. The decoder doesn't 
   waste time with B-Trees or complex math; it just copy-pastes data 
   at lightning speeds.

2. THE LOWEST RAM FOOTPRINT: 
   While other tools need gigabytes of physical memory (RAM), this tool 
   stays at nearly 0MB RAM usage. It uses the hard drive as its brain.

3. THE HIGHEST SSD FOOTPRINT: 
   This tool is a "Storage Gladiator." It trades external memory (SSD 
   space) for raw power. The .index file is massive because it catalogs 
   the input data at each position.

4. THE HIGHEST COMPRESSION RATIO: 
   Because we use 256-byte granularity and an "Infinite Lookback" window, 
   we can catch duplicates that ZIP or LZ4 class tools (limited to 32KB-64KB 
   windows) would never even see.

THE FOUR EXTREMES (THE HIGHLIGHTS) ]

In all seriousness, Zirka is a texttoy, for real/practical scenarios, look up Lasse's eXdupe:
https://github.com/rrrlasse/eXdupe

1. WHAT IS THIS?

A FREE open-source console tool (Linux) written in C by Kaze & Gemini Pro AI.
A high-speed deduplicator that uses a B-Tree "Brain" to remember EVERY 
256-byte sequence in a file. It is "Lossless," meaning the restored file 
is a perfect bit-for-bit match of the original.

2. HOW THE ENCODER WORKS

The encoder slides a 256-byte "viewing window" across your file.

Step A: It checks the B-Tree on your disk: "Is this block a repeat?"
Step B: 
- IF UNIQUE: It writes the raw byte and saves the block to the disk-index.
- IF DUPLICATE: It writes a 13-byte "Pointer" and jumps ahead 255 bytes.

3. THE "SANITY CHECK" POINTER

To prevent errors if the original file contains the "Magic Byte" (255), 
we use a 13-byte secure pointer:
[1 Byte Magic] + [8 Bytes Offset] + [4 Bytes Hashsum]

The Decoder will only follow a pointer if the data at the destination 
perfectly matches the 4-byte Hashsum passcode.

4. THE DECODER

The decoder is a lightweight "Copy-Paster." It reads the instructions 
and rebuilds the file by copying from the data it has already restored.

5. PERFORMANCE & STATS

- RAM Usage:   Nearly 0. The disk does the heavy lifting.
- Index Size:  Expect the .index to be significantly larger than the input.
- Lookback:    Infinite. It can find a duplicate from byte 1 even if it's currently at byte 300,000,000,000.

```
Benchmarking 'SUPRAPIG_Intel_Parallel_Studio_XE_2019_Downloadable_Documentation_(29599_files).tar' (451,194,368 bytes), BLAKE3: e05e97740bd199a08dc0da7c8e38fd5e2eadf2fce0d549f1bc08a1428e7ad8ee
+--------------------------+-----------------+-----------------+---------------------+---------------------+----------------------+-------------+---------------+
| Deduplicator/Compressor  | Compressed Size | Compressed Size | I Memory Footprint, | E Memory Footprint, | Memory Footprint,    | Compression | Decompression |
|                          |                 | after "gzip -9" | during Compression  | during Compression  | during Decompression | Time, h:m:s | Time          |
+--------------------------+------[ sorted ]-+-----------------+---------------------+---------------------+----------------------+-------------+---------------+
| Nakamichi 'Satanichi'    |      93,010,010 |            N.A. |       36,088,744 KB |                NONE |           532,264 KB |     1:04:25 |        0.30 s |
| gzip_1.13 -k -9          |     115,820,891 |            N.A. |            2,164 KB |                NONE |             1,932 KB |      8.73 s |        1.42 s |
| Zirka v.2                |     242,505,725 |      92,512,826 |              912 KB |  210.9x = 90,732 MB |               788 KB |     1:15:58 |        1.43 s |
| exdupe_3.0.1_linux_amd64 |     426,111,041 |     103,105,945 |        2,402,048 KB |                NONE |           308,784 KB |      1.15 s |        0.27 s |
+--------------------------+-----------------+-----------------+---------------------+---------------------+----------------------+-------------+---------------+
Note1: The testmachine is my fastest laptop Dell, i7-11850H, 128 GB DDR4, Linux Fedora 42
Note2: The SSD is Corsair MP600 2TB 
Note3: 'I' stands for Internal, 'E' stands for External
```

6. CREDITS

Gemini Pro AI is cool, it is the main "culprit" for this wondertool.
The 'Zirka' naming is to pay tribute to the beloved kidbook 'Дружков Юрий - Приключения Карандаша и Самоделкина (худ. И. Семёнов)'.
One of the two villains was called Zirka - a petty spy. The logo was made from the book's original drawing fed to the Japanese PixAI.

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
зи́ркав (zírkav, “squeezed, squinted”) (of eyes)
Related terms:<br>
зи́ркам (zírkam, “to see through”) (dialectal)
прозо́рец (prozórec, “window”)
https://en.wiktionary.org/wiki/%D0%B7%D0%B8%D1%80%D0%BA%D0%B0

РЕЧНИК НА БЪЛГАРСКИЯ ЕЗИК (ОНЛАЙН)<br>
ЗЍРКА ж. Диал. Цепнатина или отвор в някаква преграда, покрив и под.; пролука, процеп, прозорка, прозирка. Дядо Яначко седеше поприведен в тъмното на одъра; през зирките на вратата светеше – оттатъка в оная, голямата стая. Ил. Волен, НС, 73. Той наблюдаваше през зирките на керемидите как зората се сипваше. Г. Караславов, Избр. съч. Х, 26. През зирките в дъсчената врата на плевнята се виждаше целият двор. В. Андреев, ПП, 8.
https://ibl.bas.bg/rbe/lang/bg/%D0%B7%D0%B8%D1%80%D0%BA%D0%B0/

7. DOWNLOAD

https://github.com/Sanmayce/Zirka

2026-Jan-26
Kaze (sanmayce@sanmayce.com)

