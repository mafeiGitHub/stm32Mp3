// diskio.c
#include "string.h"
#include <stdlib.h>

#include "fatFs.h"
#include "diskio.h"

#include "../Bsp/stm32746g_discovery_sd.h"
#include "../Bsp/cLcd.h"

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
DRESULT diskRead (BYTE* buff, DWORD sector, UINT count) {

  if ((uint32_t)buff & 0x03) {
    //cLcd::debug ("diskRead align b:" + cLcd::hexStr ((int)buff) + " sec:" + cLcd::intStr (sector) + " num:" + cLcd::intStr (count));

    // not 32bit aligned, dma fails,
    auto tempBuffer = (uint32_t*)pvPortMalloc (count * SECTOR_SIZE);

    // read into 32bit aligned tempBuffer
    auto result = BSP_SD_ReadBlocks (tempBuffer, (uint64_t)(sector * SECTOR_SIZE), SECTOR_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;

    // then cache fails on memcpy after dma read
    SCB_InvalidateDCache();
    //SCB_InvalidateDCache_by_Addr ((uint32_t*)tempBuffer, count * SECTOR_SIZE);
    memcpy (buff, tempBuffer, count * SECTOR_SIZE);

    vPortFree (tempBuffer);
    return result;
    }

  else {
    auto result = BSP_SD_ReadBlocks ((uint32_t*)buff, (uint64_t)(sector * SECTOR_SIZE), SECTOR_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
    SCB_InvalidateDCache_by_Addr ((uint32_t*)buff, count * SECTOR_SIZE);
    return result;
    }
  }
//}}}
//{{{
DRESULT diskWrite (const BYTE* buff, DWORD sector, UINT count) {
  return BSP_SD_WriteBlocks ((uint32_t*)buff, (uint64_t)(sector * SECTOR_SIZE), SECTOR_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  }
//}}}
