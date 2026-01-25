Zirka v.1 - THE B-TREE SLIDING DEDUPLICATOR (JUMPING EDITION) - README.TXT
<br>
<br>
<img width="776" height="796" alt="Zirka_logo_text" src="https://github.com/user-attachments/assets/63c625d4-5e86-4f79-85c6-2752b3e50f6a" />
<br>
<br>
THE HIGHLIGHT: ZERO-RAM ARCHITECTURE
<br>
Most compression tools are limited by your computer's RAM. If you want to 
deduplicate 300GB, they usually require 8++GB of RAM. 
NOT THIS TOOL. By using a Disk-Based B-Tree, this encoder uses almost 
ZERO RAM. It performs "Search & Rescue" operations directly on your 
NVMe/SSD. If you can store it, you can dedup it.

In all seriousness, Zirka is a texttoy, for a true practical deduplicator look up Lasse's eXdupe:  
https://github.com/rrrlasse/eXdupe

THE FOUR EXTREMES (THE HIGHLIGHTS) [

1. THE FASTEST DECOMPRESSOR: 
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

1. WHAT IS THIS?
    
A FREE open-source console tool (Linux) written in C by Kaze & Gemini AI.
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
- Lookback:    Infinite. It can find a duplicate from byte 1 even if it's 
               currently at byte 300,000,000,000.

```
$ sh zrk.sh Andrzej\ Sapkowski\ -\ The\ Witcher\ Series\ -\ 2007-2018.tar
Deduplicating with 256 bytes granularity...
Uniques: 5887604 | Duplicates: 104
The need for index file is: 2226.08MB / 5.63MB =~ 395.1x

Command being timed: "./zirka Andrzej Sapkowski - The Witcher Series - 2007-2018.tar"
Elapsed (wall clock) time (h:mm:ss or m:ss): 33:16.09
Maximum resident set size (kbytes): 1456
File system inputs: 127,970,104
File system outputs: 115,155,272

Command being timed: "./unzirka Andrzej Sapkowski - The Witcher Series - 2007-2018.tar"
Elapsed (wall clock) time (h:mm:ss or m:ss): 0:00.12
Maximum resident set size (kbytes): 1336
File system inputs: 11,480
File system outputs: 11,544
```

```
    5,908,480 'Andrzej Sapkowski - The Witcher Series - 2007-2018.tar'
    5,883,364 'Andrzej Sapkowski - The Witcher Series - 2007-2018.tar.deduplicatedfile'
    5,908,480 'Andrzej Sapkowski - The Witcher Series - 2007-2018.tar.restored'
2,334,216,200 dedup.index
```

6. CREDITS
    
Gemini AI is cool, it is the main "culprit" for this wondertool.
The 'Zirka' naming is to pay tribute to the beloved kidbook 'Дружков Ю. - Приключения Карандаша и Самоделкина (худ. И. Семёнов)'.
One of the two villains was called Zirka - a petty spy.

зирка  
Bulgarian  
Etymology: By surface analysis, зи́ра (zíra, “transparent substance”) +‎ -ка (-ka)  
Pronunciation: IPA: [ˈzirkɐ]  
Noun  
зи́рка • (zírka) m (dialectal)
slit, leak, gap for seeing through  
Derived terms:  
взи́рка (vzírka) (dialectal)
прози́рка (prozírka) (dialectal)
зи́ркав (zírkav, “squeezed, squinted”) (of eyes)  
Related terms:  
зи́ркам (zírkam, “to see through”) (dialectal)
прозо́рец (prozórec, “window”)    
https://en.wiktionary.org/wiki/%D0%B7%D0%B8%D1%80%D0%BA%D0%B0

РЕЧНИК НА БЪЛГАРСКИЯ ЕЗИК (ОНЛАЙН)
ЗЍРКА ж. Диал. Цепнатина или отвор в някаква преграда, покрив и под.; пролука, процеп, прозорка, прозирка. Дядо Яначко седеше поприведен в тъмното на одъра; през зирките на вратата светеше – оттатъка в оная, голямата стая. Ил. Волен, НС, 73. Той наблюдаваше през зирките на керемидите как зората се сипваше. Г. Караславов, Избр. съч. Х, 26. През зирките в дъсчената врата на плевнята се виждаше целият двор. В. Андреев, ПП, 8.  
https://ibl.bas.bg/rbe/lang/bg/%D0%B7%D0%B8%D1%80%D0%BA%D0%B0/

7. DOWNLOAD
    
https://github.com/Sanmayce/Zirka

2026-Jan-25
Kaze (sanmayce@sanmayce.com)
