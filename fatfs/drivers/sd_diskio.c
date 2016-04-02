// sd_diskio.c
#include <string.h>
#include "../ff_gen_drv.h"

#define BLOCK_SIZE 512

static volatile DSTATUS Stat = STA_NOINIT;

/*{{{*/
DSTATUS SD_initialize (BYTE lun) {
  return BSP_SD_Init() == MSD_OK ? Stat & ~STA_NOINIT : STA_NOINIT;
  }
/*}}}*/
/*{{{*/
DSTATUS SD_status (BYTE lun) {
  return BSP_SD_GetStatus() == MSD_OK ? Stat & ~STA_NOINIT : STA_NOINIT;
  }
/*}}}*/

/*{{{*/
DRESULT SD_read (BYTE lun, BYTE* buff, DWORD sector, UINT count) {

  return BSP_SD_ReadBlocks_DMA ((uint32_t*)buff, (uint64_t)(sector*BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  }
/*}}}*/
/*{{{*/
DRESULT SD_write (BYTE lun, const BYTE*  buff, DWORD sector, UINT count) {

  return BSP_SD_WriteBlocks((uint32_t*)buff, (uint64_t)(sector * BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  }
/*}}}*/
/*{{{*/
DRESULT SD_ioctl (BYTE lun, BYTE cmd, void *buff) {

  SD_CardInfo CardInfo;
  if (Stat & STA_NOINIT)
    return RES_NOTRDY;

  switch (cmd) {
    /* Make sure that no pending write process */
    case CTRL_SYNC :
      return RES_OK;

    /* Get number of sectors on the disk (DWORD) */
    case GET_SECTOR_COUNT :
      BSP_SD_GetCardInfo (&CardInfo);
      *(DWORD*)buff = CardInfo.CardCapacity / BLOCK_SIZE;
      return RES_OK;

    /* Get R/W sector size (WORD) */
    case GET_SECTOR_SIZE :
      *(WORD*)buff = BLOCK_SIZE;
      return RES_OK;

    /* Get erase block size in unit of sector (DWORD) */
    case GET_BLOCK_SIZE :
      *(DWORD*)buff = BLOCK_SIZE;
      return RES_ERROR;

    default:
      return RES_PARERR;
    }
  }
/*}}}*/

/*{{{*/
const Diskio_drvTypeDef  SD_Driver = {
  SD_initialize,
  SD_status,
  SD_read,
  SD_write,
  SD_ioctl,
  };
/*}}}*/
