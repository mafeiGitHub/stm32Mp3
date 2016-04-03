// sd_diskio.c
#include "../ff_gen_drv.h"

#define BLOCK_SIZE 512
static volatile DSTATUS Stat = STA_NOINIT;

/*{{{*/
DSTATUS SD_initialize (BYTE lun) {

  Stat = STA_NOINIT;
  if (BSP_SD_Init() == MSD_OK)
    Stat &= ~STA_NOINIT;
  return Stat;
  }
/*}}}*/
/*{{{*/
DSTATUS SD_status (BYTE lun) {

  Stat = STA_NOINIT;
  if (BSP_SD_GetStatus() == MSD_OK)
    Stat &= ~STA_NOINIT;
  return Stat;
  }
/*}}}*/
/*{{{*/
DRESULT SD_ioctl (BYTE lun, BYTE cmd, void *buff) {

  DRESULT res = RES_ERROR;
  SD_CardInfo CardInfo;
  if (Stat & STA_NOINIT) 
    return RES_NOTRDY;

  switch (cmd) {
    /* Make sure that no pending write process */
    case CTRL_SYNC :
      res = RES_OK;
      break;

    /* Get number of sectors on the disk (DWORD) */
    case GET_SECTOR_COUNT :
      BSP_SD_GetCardInfo(&CardInfo);
      *(DWORD*)buff = CardInfo.CardCapacity / BLOCK_SIZE;
      res = RES_OK;
      break;

    /* Get R/W sector size (WORD) */
    case GET_SECTOR_SIZE :
      *(WORD*)buff = BLOCK_SIZE;
      res = RES_OK;
      break;

    /* Get erase block size in unit of sector (DWORD) */
    case GET_BLOCK_SIZE :
      *(DWORD*)buff = BLOCK_SIZE;
      break;

    default:
      res = RES_PARERR;
    }

  return res;
  }
/*}}}*/

/*{{{*/
DRESULT SD_read (BYTE lun, BYTE* buff, DWORD sector, UINT count) {

  return BSP_SD_ReadBlocks ((uint32_t*)buff, (uint64_t)(sector*BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  //return BSP_SD_ReadBlocks_DMA ((uint32_t*)buff, (uint64_t)(sector*BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
  }
/*}}}*/
/*{{{*/
DRESULT SD_write (BYTE lun, const BYTE*  buff, DWORD sector, UINT count) {

  return BSP_SD_WriteBlocks((uint32_t*)buff, (uint64_t)(sector * BLOCK_SIZE), BLOCK_SIZE, count) == MSD_OK ? RES_OK : RES_ERROR;
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
