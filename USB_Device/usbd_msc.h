#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif
//}}}
#include  "usbd_def.h"

extern USBD_ClassTypeDef USBD_MSC;

//{{{  struct USBD_StorageTypeDef
typedef struct _USBD_STORAGE {
  int8_t (*GetCapacity) (uint8_t lun, uint32_t* block_num, uint16_t* block_size);
  int8_t (*IsReady) (uint8_t lun);
  int8_t (*Read) (uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len);
  int8_t (*Write)(uint8_t lun, uint8_t* buf, uint32_t blk_addr, uint16_t blk_len);
  int8_t *pInquiry;
  } USBD_StorageTypeDef;
//}}}
void USBD_MSC_RegisterStorage (USBD_HandleTypeDef* pdev, USBD_StorageTypeDef* fops);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
