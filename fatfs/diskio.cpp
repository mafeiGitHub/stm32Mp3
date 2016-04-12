// diskio.c
#include <stdlib.h>
#include "fatFs.h"
#include "diskio.h"

#include "../Bsp/stm32746g_discovery_sd.h"
#include "../Bsp/cLcd.h"

#define BLOCK_SIZE 512

static volatile DSTATUS Stat = STA_NOINIT;

//{{{
DSTATUS diskStatus (BYTE pdrv) {

  Stat = STA_NOINIT;

  if (BSP_SD_GetStatus() == MSD_OK)
    Stat &= ~STA_NOINIT;

  return Stat;
  }
//}}}
//{{{
DSTATUS diskInitialize (BYTE pdrv) {

  Stat = STA_NOINIT;

  if (BSP_SD_Init() == MSD_OK)
    Stat &= ~STA_NOINIT;

  return Stat;
  }
//}}}
//{{{
DRESULT diskIoctl (BYTE pdrv, BYTE cmd, void* buff) {

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
      *(DWORD*)buff = CardInfo.CardCapacity / BLOCK_SIZE;
      res = RES_OK;
      break;
      }

    // Get R/W sector size (WORD)
    case GET_SECTOR_SIZE :
      *(WORD*)buff = BLOCK_SIZE;
      res = RES_OK;
      break;

    // Get erase block size in unit of sector (DWORD)
    case GET_BLOCK_SIZE :
      *(DWORD*)buff = BLOCK_SIZE;
      res = RES_OK;
      break;

    default:
      res = RES_PARERR;
    }

  return res;
  }
//}}}

//{{{
DRESULT diskRead (BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {

  //if ((uint32_t)buff < 0xC0000000U)
  //  cLcd::debug ("diskRead b:" + cLcd::hexStr ((int)buff) + " s:" + cLcd::intStr (sector) + " c:" + cLcd::intStr (count));

  auto result = BSP_SD_ReadBlocks ((uint32_t*)buff, (uint64_t)(sector * BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  //SCB_InvalidateDCache();
  //SCB_InvalidateDCache_by_Addr ((uint32_t*)buff, count * BLOCK_SIZE);
  //SCB_CleanDCache_by_Addr ((uint32_t*)buff, count * BLOCK_SIZE);
  //SCB_CleanInvalidateDCache_by_Addr ((uint32_t*)buff, count * BLOCK_SIZE);
  return result;
  }
//}}}
//{{{
DRESULT diskWrite (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
  return BSP_SD_WriteBlocks ((uint32_t*)buff, (uint64_t)(sector * BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  }
//}}}
