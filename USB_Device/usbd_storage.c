// usbd_storage.c
/*{{{  includes*/
#include "usbd_storage.h"
#include "stm32746g_discovery_sd.h"
/*}}}*/

#define STORAGE_LUN_NBR 1
#define STORAGE_BLK_NBR 0x10000
#define STORAGE_BLK_SIZ 0x200

/*{{{*/
int8_t STORAGE_Init (uint8_t lun) {
  //BSP_SD_Init();
  return 0;
  }
/*}}}*/

/*{{{*/
int8_t STORAGE_GetCapacity (uint8_t lun, uint32_t *block_num, uint16_t *block_size) {

  HAL_SD_CardInfoTypedef info;

  if (BSP_SD_IsDetected() != SD_NOT_PRESENT) {
    BSP_SD_GetCardInfo (&info);
    *block_num = (info.CardCapacity) / STORAGE_BLK_SIZ  - 1;
    *block_size = STORAGE_BLK_SIZ;
    return 0;
    }

  return -1;
  }
/*}}}*/
/*{{{*/
int8_t STORAGE_IsReady (uint8_t lun) {

  static int8_t prev_status = 0;

  if (BSP_SD_IsDetected() != SD_NOT_PRESENT) {
    if (prev_status < 0) {
      BSP_SD_Init();
      prev_status = 0;
      }
    if (BSP_SD_GetStatus() == SD_TRANSFER_OK)
      return 0;
    }
  else if (prev_status == 0)
    prev_status = -1;

  return -1;
  }
/*}}}*/
int8_t STORAGE_IsWriteProtected (uint8_t lun) { return 0; }

/*{{{*/
int8_t STORAGE_Read (uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len) {

  if (BSP_SD_IsDetected() != SD_NOT_PRESENT) {
    BSP_SD_ReadBlocks_DMA ((uint32_t*)buf, blk_addr * STORAGE_BLK_SIZ, STORAGE_BLK_SIZ, blk_len);
    SCB_InvalidateDCache_by_Addr ((uint32_t*)((uint32_t)buf & 0xFFFFFFE0), (blk_len * STORAGE_BLK_SIZ) + 32);
    return 0;
    }

  return -1;
  }
/*}}}*/
/*{{{*/
int8_t STORAGE_Write (uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len) {

  if (BSP_SD_IsDetected() != SD_NOT_PRESENT) {
    BSP_SD_WriteBlocks_DMA ((uint32_t*)buf, blk_addr * STORAGE_BLK_SIZ, STORAGE_BLK_SIZ, blk_len);
    return 0;
    }
  return -1;
  }
/*}}}*/

int8_t STORAGE_GetMaxLun() { return(STORAGE_LUN_NBR - 1); }

/*{{{*/
int8_t STORAGE_Inquirydata[] = { /* 36 */
  /* LUN 0 */
  0x00,
  0x80,
  0x02,
  0x02,
  (STANDARD_INQUIRY_DATA_LEN - 5),
  0x00,
  0x00,
  0x00,
  'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ', /* Manufacturer: 8 bytes  */
  'P', 'r', 'o', 'd', 'u', 'c', 't', ' ', /* Product     : 16 Bytes */
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  '0', '.', '0','1',                      /* Version     : 4 Bytes  */
  };
/*}}}*/

/*{{{*/
USBD_StorageTypeDef USBD_DISK_fops = {
  STORAGE_Init,
  STORAGE_GetCapacity,
  STORAGE_IsReady,
  STORAGE_IsWriteProtected,
  STORAGE_Read,
  STORAGE_Write,
  STORAGE_GetMaxLun,
  STORAGE_Inquirydata,
};
/*}}}*/
