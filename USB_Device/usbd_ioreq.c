#include "usbd_ioreq.h"

/*{{{*/
USBD_StatusTypeDef USBD_CtlSendData (USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t len) {
  /* Set EP0 State */
  pdev->ep0_state          = USBD_EP0_DATA_IN;
  pdev->ep_in[0].total_length = len;
  pdev->ep_in[0].rem_length   = len;

 /* Start the transfer */
  USBD_LL_Transmit (pdev, 0x00, pbuf, len);

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_CtlContinueSendData (USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t len) {

  /* Start the next transfer */
  USBD_LL_Transmit (pdev, 0x00, pbuf, len);
  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_CtlPrepareRx (USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t len) {

  /* Set EP0 State */
  pdev->ep0_state = USBD_EP0_DATA_OUT;
  pdev->ep_out[0].total_length = len;
  pdev->ep_out[0].rem_length   = len;
  /* Start the transfer */
  USBD_LL_PrepareReceive (pdev, 0, pbuf, len);

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_CtlContinueRx (USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint16_t len) {
  USBD_LL_PrepareReceive (pdev, 0, pbuf, len);
  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_CtlSendStatus (USBD_HandleTypeDef *pdev) {

  /* Set EP0 State */
  pdev->ep0_state = USBD_EP0_STATUS_IN;

  /* Start the transfer */
  USBD_LL_Transmit (pdev, 0x00, NULL, 0);

  return USBD_OK;
  }
/*}}}*/
/*{{{*/
USBD_StatusTypeDef USBD_CtlReceiveStatus (USBD_HandleTypeDef *pdev) {

  /* Set EP0 State */
  pdev->ep0_state = USBD_EP0_STATUS_OUT;

  /* Start the transfer */
  USBD_LL_PrepareReceive (pdev, 0, NULL, 0);

  return USBD_OK;
  }
/*}}}*/

/*{{{*/
uint16_t USBD_GetRxCount (USBD_HandleTypeDef *pdev , uint8_t ep_addr) {
  return USBD_LL_GetRxDataSize (pdev, ep_addr);
  }
/*}}}*/
