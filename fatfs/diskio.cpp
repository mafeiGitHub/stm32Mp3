// diskio.c
//{{{  includes
#include "string.h"
#include <stdlib.h>

#include "fatFs.h"
#include "diskio.h"

#include "cSd.h"

#include "cLcd.h"
//}}}

static volatile DSTATUS Stat = STA_NOINIT;

//{{{
DSTATUS diskStatus() {

  return SD_GetStatus() == MSD_OK ? 0 : STA_NOINIT;
  }
//}}}
//{{{
DSTATUS diskInitialize() {

  return SD_GetStatus() == MSD_OK ? 0 : STA_NOINIT;
  }
//}}}

//{{{
DRESULT diskIoctl (BYTE cmd, void* buff) {

  DRESULT res = RES_ERROR;

  switch (cmd) {
    // Make sure that no pending write process
    case CTRL_SYNC :
      res = RES_OK;
      break;

    // Get number of sectors on the disk (DWORD)
    case GET_SECTOR_COUNT : {
      SD_CardInfo CardInfo;
      SD_GetCardInfo (&CardInfo);
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
    auto tempBuffer = (uint8_t*)pvPortMalloc (count * SECTOR_SIZE);

    // read into 32bit aligned tempBuffer
    auto result = SD_ReadBlocks (tempBuffer, (uint64_t)(sector * SECTOR_SIZE), count) == MSD_OK ? RES_OK : RES_ERROR;
    memcpy (buffer, tempBuffer, count * SECTOR_SIZE);

    vPortFree (tempBuffer);
    return result;
    }

  else
    //cLcd::debug ("diskRead - sec:" + cLcd::dec (sector) + " num:" + cLcd::dec (count));
    return  SD_ReadBlocks ((uint8_t*)buffer, (uint64_t)(sector * SECTOR_SIZE), count) == MSD_OK ? RES_OK : RES_ERROR;
  }
//}}}
//{{{
DRESULT diskWrite (const BYTE* buffer, DWORD sector, UINT count) {
  return SD_WriteBlocks ((uint8_t*)buffer, (uint64_t)(sector * SECTOR_SIZE), count) == MSD_OK ? RES_OK : RES_ERROR;
  }
//}}}
