// diskio.c
#include "string.h"
#include <stdlib.h>

#include "fatFs.h"
#include "diskio.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery_sd.h"
#elif STM32F769I_DISCO
  #include "stm32F769i_discovery_sd.h"
#endif

#include "cLcd.h"

static volatile DSTATUS Stat = STA_NOINIT;

//{{{
DSTATUS diskStatus() {

  Stat = STA_NOINIT;

  if (BSP_SD_GetStatus() == MSD_OK)
    Stat &= ~STA_NOINIT;

  return Stat;
  }
//}}}
//{{{
DSTATUS diskInitialize() {

  Stat = STA_NOINIT;

  if (BSP_SD_Init() == MSD_OK)
    Stat &= ~STA_NOINIT;

  return Stat;
  }
//}}}

//{{{
DRESULT diskIoctl (BYTE cmd, void* buff) {

  DRESULT res = RES_ERROR;

  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  switch (cmd) {
    // Make sure that no pending write process
    case CTRL_SYNC :
      res = RES_OK;
      break;

    // Get number of sectors on the disk (DWORD)
    case GET_SECTOR_COUNT : {
      SD_CardInfo CardInfo;
      BSP_SD_GetCardInfo (&CardInfo);
      *(DWORD*)buff = CardInfo.CardCapacity / SECTOR_SIZE;
      res = RES_OK;
      break;
      }

    // Get R/W sector size (WORD)
    case GET_SECTOR_SIZE :
      *(WORD*)buff = SECTOR_SIZE;
      res = RES_OK;
      break;

    // Get erase block size in unit of sector (DWORD)
    case GET_BLOCK_SIZE :
      *(DWORD*)buff = SECTOR_SIZE;
      res = RES_OK;
      break;

    default:
      res = RES_PARERR;
    }

  return res;
  }
//}}}
//{{{
DRESULT diskRead (BYTE* buffer, DWORD sector, UINT count) {

  if ((uint32_t)buffer & 0x03) {
    cLcd::debug ("diskRead align b:" + cLcd::hex ((int)buffer) + " sec:" + cLcd::dec (sector) + " num:" + cLcd::dec (count));

    // not 32bit aligned, dma fails,
    auto tempBuffer = (uint32_t*)pvPortMalloc (count * SECTOR_SIZE);

    // read into 32bit aligned tempBuffer
    auto result = BSP_SD_ReadBlocks_DMA (tempBuffer, (uint64_t)(sector * SECTOR_SIZE), SECTOR_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;

    // then cache fails on memcpy after dma read
    SCB_InvalidateDCache_by_Addr ((uint32_t*)((uint32_t)tempBuffer & 0xFFFFFFE0), (count * SECTOR_SIZE) + 32);
    memcpy (buffer, tempBuffer, count * SECTOR_SIZE);

    vPortFree (tempBuffer);
    return result;
    }

  else {
    cLcd::debug ("diskRead - sec:" + cLcd::dec (sector) + " num:" + cLcd::dec (count));
    auto result = BSP_SD_ReadBlocks_DMA ((uint32_t*)buffer, (uint64_t)(sector * SECTOR_SIZE), SECTOR_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
    SCB_InvalidateDCache_by_Addr ((uint32_t*)((uint32_t)buffer & 0xFFFFFFE0), (count * SECTOR_SIZE) + 32);
    return result;
    }
  }
//}}}
//{{{
DRESULT diskWrite (const BYTE* buffer, DWORD sector, UINT count) {
  return BSP_SD_WriteBlocks_DMA ((uint32_t*)buffer, (uint64_t)(sector * SECTOR_SIZE), SECTOR_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  }
//}}}
