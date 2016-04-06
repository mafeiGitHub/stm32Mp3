// memory.h
#pragma once

// sdram allocation
#define SDRAM_FRAME0    0xC0000000 // frameBuffer 272*480*4 = 0x7f800 = 512k-2048b leave bit of guard for clipping errors
#define SDRAM_FRAME1    0xC0080000

#define SDRAM_HEAP      0xC0100000 // sdram heap
#define SDRAM_HEAP_SIZE 0x00700000 // 7m
