/*{{{  includes*/
#include "usbd_msc_bot.h"
#include "usbd_msc.h"
#include "usbd_msc_scsi.h"
#include "usbd_ioreq.h"
/*}}}*/

/*{{{*/
void  MSC_BOT_SendCSW (USBD_HandleTypeDef  *pdev, uint8_t CSW_Status) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->csw.dSignature = USBD_BOT_CSW_SIGNATURE;
  hmsc->csw.bStatus = CSW_Status;
  hmsc->bot_state = USBD_BOT_IDLE;

  USBD_LL_Transmit (pdev, MSC_EPIN_ADDR, (uint8_t *)&hmsc->csw, USBD_BOT_CSW_LENGTH);

  /* Prepare EP to Receive next Cmd */
  USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t *)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
  }
/*}}}*/

/*{{{*/
static void MSC_BOT_Abort (USBD_HandleTypeDef  *pdev) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if ((hmsc->cbw.bmFlags == 0) && (hmsc->cbw.dDataLength != 0) && (hmsc->bot_status == USBD_BOT_STATUS_NORMAL) )
    USBD_LL_StallEP(pdev, MSC_EPOUT_ADDR );
  USBD_LL_StallEP(pdev, MSC_EPIN_ADDR);

  if(hmsc->bot_status == USBD_BOT_STATUS_ERROR)
    USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t *)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
  }
/*}}}*/
/*{{{*/
static void MSC_BOT_SendData(USBD_HandleTypeDef  *pdev, uint8_t* buf, uint16_t len) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  len = MIN (hmsc->cbw.dDataLength, len);
  hmsc->csw.dDataResidue -= len;
  hmsc->csw.bStatus = USBD_CSW_CMD_PASSED;
  hmsc->bot_state = USBD_BOT_SEND_DATA;

  USBD_LL_Transmit (pdev, MSC_EPIN_ADDR, buf, len);
  }
/*}}}*/
/*{{{*/
static void MSC_BOT_CBW_Decode (USBD_HandleTypeDef  *pdev) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->csw.dTag = hmsc->cbw.dTag;
  hmsc->csw.dDataResidue = hmsc->cbw.dDataLength;

  if ((USBD_LL_GetRxDataSize (pdev ,MSC_EPOUT_ADDR) != USBD_BOT_CBW_LENGTH) ||
      (hmsc->cbw.dSignature != USBD_BOT_CBW_SIGNATURE) || (hmsc->cbw.bLUN > 1) ||
      (hmsc->cbw.bCBLength < 1) || (hmsc->cbw.bCBLength > 16)) {
    SCSI_SenseCode(pdev, hmsc->cbw.bLUN, ILLEGAL_REQUEST, INVALID_CDB);
    hmsc->bot_status = USBD_BOT_STATUS_ERROR;
    MSC_BOT_Abort(pdev);
    }
  else {
    if (SCSI_ProcessCmd(pdev, hmsc->cbw.bLUN, &hmsc->cbw.CB[0]) < 0) {
      if (hmsc->bot_state == USBD_BOT_NO_DATA)
        MSC_BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
      else
        MSC_BOT_Abort(pdev);
      }
    /*Burst xfer handled internally*/
    else if ((hmsc->bot_state != USBD_BOT_DATA_IN) && (hmsc->bot_state != USBD_BOT_DATA_OUT) &&
             (hmsc->bot_state != USBD_BOT_LAST_DATA_IN)) {
      if (hmsc->bot_data_length > 0)
        MSC_BOT_SendData(pdev, hmsc->bot_data, hmsc->bot_data_length);
      else if (hmsc->bot_data_length == 0)
        MSC_BOT_SendCSW (pdev, USBD_CSW_CMD_PASSED);
      }
    }
  }
/*}}}*/

/*{{{*/
void MSC_BOT_Init (USBD_HandleTypeDef  *pdev) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->bot_state  = USBD_BOT_IDLE;
  hmsc->bot_status = USBD_BOT_STATUS_NORMAL;

  hmsc->scsi_sense_tail = 0;
  hmsc->scsi_sense_head = 0;

  ((USBD_StorageTypeDef *)pdev->pUserData)->Init(0);

  USBD_LL_FlushEP(pdev, MSC_EPOUT_ADDR);
  USBD_LL_FlushEP(pdev, MSC_EPIN_ADDR);

  /* Prapare EP to Receive First BOT Cmd */
  USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t *)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
  }
/*}}}*/
/*{{{*/
void MSC_BOT_Reset (USBD_HandleTypeDef  *pdev) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  hmsc->bot_state  = USBD_BOT_IDLE;
  hmsc->bot_status = USBD_BOT_STATUS_RECOVERY;

  /* Prapare EP to Receive First BOT Cmd */
  USBD_LL_PrepareReceive (pdev, MSC_EPOUT_ADDR, (uint8_t *)&hmsc->cbw, USBD_BOT_CBW_LENGTH);
  }
/*}}}*/
/*{{{*/
void MSC_BOT_DeInit (USBD_HandleTypeDef  *pdev) {
  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;
  hmsc->bot_state  = USBD_BOT_IDLE;
  }
/*}}}*/
/*{{{*/
void MSC_BOT_DataIn (USBD_HandleTypeDef  *pdev, uint8_t epnum) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  switch (hmsc->bot_state) {
    case USBD_BOT_DATA_IN:
      if (SCSI_ProcessCmd(pdev, hmsc->cbw.bLUN, &hmsc->cbw.CB[0]) < 0)
        MSC_BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
      break;

    case USBD_BOT_SEND_DATA:
    case USBD_BOT_LAST_DATA_IN:
      MSC_BOT_SendCSW (pdev, USBD_CSW_CMD_PASSED);
      break;

    default:
      break;
    }
  }
/*}}}*/
/*{{{*/
void MSC_BOT_DataOut (USBD_HandleTypeDef  *pdev, uint8_t epnum) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  switch (hmsc->bot_state) {
    case USBD_BOT_IDLE:
      MSC_BOT_CBW_Decode(pdev);
      break;

    case USBD_BOT_DATA_OUT:
      if(SCSI_ProcessCmd(pdev, hmsc->cbw.bLUN, &hmsc->cbw.CB[0]) < 0)
        MSC_BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
      break;

    default:
      break;
    }
  }
/*}}}*/

/*{{{*/
void  MSC_BOT_CplClrFeature (USBD_HandleTypeDef  *pdev, uint8_t epnum) {

  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  if (hmsc->bot_status == USBD_BOT_STATUS_ERROR ) {
    /* Bad CBW Signature */
    USBD_LL_StallEP(pdev, MSC_EPIN_ADDR);
    hmsc->bot_status = USBD_BOT_STATUS_NORMAL;
    }
  else if (((epnum & 0x80) == 0x80) && ( hmsc->bot_status != USBD_BOT_STATUS_RECOVERY))
    MSC_BOT_SendCSW (pdev, USBD_CSW_CMD_FAILED);
  }
/*}}}*/
