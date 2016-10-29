// memory.h - explicit memory placement
#pragma once

// DTCM         0x20000000 - 0x2000FFFF
#define AUDIO_BUFFER         0x20000000
#define AUDIO_BUFFER_HALF    0x20001200  // SIZE = 0x1200 = 1152*4
#define AUDIO_BUFFER_SIZE        0x2400  // SIZE = 0x2400 = 1152*8 = 9216 bytes

#define FATFS_BUFFER         0x20002400
#define FATFS_BUFFER_SIZE         0x200  // SIZE =  0x200 = 512 bytes - sector size

#define USB_BUFFER           0x20002600  // 620

#define EthRxDescripSection  0x20003000  // SIZE =   0xA0 bytes
#define EthRxBUF             0x20003100  // SIZE = 0x1DC4 bytes
#define EthTxDescripSection  0x20005000  // SIZE =   0xA0 bytes
#define EthTxBUF             0x20005100  // SIZE = 0x1DC4 bytes

#define DMA2D_BUFFER         0x20007000
#define DMA2D_BUFFER_SIZE        0x9000  // SIZE = rest of dtcm

#ifdef STM32F746G_DISCO
  // SDRAM        0xC0000000 - 0xC007FFFF
  #define SDRAM_FRAME0         0xC0000000
  #define SDRAM_FRAME_SIZE       0x07F800  // SIZE = 0x7F800 = 272*480*4 = 512k-2048b leave bit of guard for clipping errors
  #define SDRAM_FRAME1         0xC0080000
  #define SDRAM_HEAP           0xC0100000
  #define SDRAM_HEAP_SIZE        0x700000  // SIZE = 7m
#else
  // SDRAM        0xC0000000 - 0xC00FFFFF
  #define SDRAM_FRAME0         0xC0000000
  #define SDRAM_FRAME_SIZE       0x180000  // SIZE = 0x18000 = 800*480*4 = 0x177000
  #define SDRAM_FRAME1         0xC0180000
  #define SDRAM_HEAP           0xC0300000
  #define SDRAM_HEAP_SIZE        0xD00000  // SIZE = 13m
#endif
