#pragma once
//{{{
#ifdef __cplusplus
 extern "C" {
#endif

//}}}
#include "usbd_core.h"

//{{{  usbd_bot defines
#define USBD_BOT_IDLE                      0       /* Idle state */
#define USBD_BOT_DATA_OUT                  1       /* Data Out state */
#define USBD_BOT_DATA_IN                   2       /* Data In state */
#define USBD_BOT_LAST_DATA_IN              3       /* Last Data In Last */
#define USBD_BOT_SEND_DATA                 4       /* Send Immediate data */
#define USBD_BOT_NO_DATA                   5       /* No data Stage */

#define USBD_BOT_CBW_SIGNATURE             0x43425355
#define USBD_BOT_CSW_SIGNATURE             0x53425355
#define USBD_BOT_CBW_LENGTH                31
#define USBD_BOT_CSW_LENGTH                13
#define USBD_BOT_MAX_DATA                  256

/* CSW Status Definitions */
#define USBD_CSW_CMD_PASSED                0x00
#define USBD_CSW_CMD_FAILED                0x01
#define USBD_CSW_PHASE_ERROR               0x02

/* BOT Status */
#define USBD_BOT_STATUS_NORMAL             0
#define USBD_BOT_STATUS_RECOVERY           1
#define USBD_BOT_STATUS_ERROR              2

#define USBD_DIR_IN                        0
#define USBD_DIR_OUT                       1
#define USBD_BOTH_DIR                      2
//}}}

typedef struct {
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataLength;
  uint8_t  bmFlags;
  uint8_t  bLUN;
  uint8_t  bCBLength;
  uint8_t  CB[16];
  uint8_t  ReservedForAlign;
  } USBD_MSC_BOT_CBWTypeDef;

typedef struct {
  uint32_t dSignature;
  uint32_t dTag;
  uint32_t dDataResidue;
  uint8_t  bStatus;
  uint8_t  ReservedForAlign[3];
  } USBD_MSC_BOT_CSWTypeDef;

void MSC_BOT_Init (USBD_HandleTypeDef* pdev);
void MSC_BOT_Reset (USBD_HandleTypeDef* pdev);
void MSC_BOT_DeInit (USBD_HandleTypeDef* pdev);
void MSC_BOT_DataIn (USBD_HandleTypeDef* pdev, uint8_t epnum);
void MSC_BOT_DataOut (USBD_HandleTypeDef* pdev, uint8_t epnum);
void MSC_BOT_SendCSW (USBD_HandleTypeDef* pdev, uint8_t CSW_Status);
void  MSC_BOT_CplClrFeature (USBD_HandleTypeDef* pdev, uint8_t epnum);

//{{{
#ifdef __cplusplus
}
#endif
//}}}
